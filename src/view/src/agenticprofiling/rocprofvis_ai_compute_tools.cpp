// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that read a compute workload, and the formatters that turn what
// they read into text. Nothing here touches TraceDataModel: a compute trace has
// one, but it is empty, so reading it would answer confidently that the trace
// has no tracks and no events.
//
// Five of the eight answer from memory. A compute trace loads its whole
// workload/kernel/metric-catalogue/roofline tree up front, so kernel names,
// dispatch statistics, units and descriptions cost nothing to read and need no
// request id. kernel_triage and get_metrics query through the assistant's own
// client id - FetchMetrics keys both the request slot and the ComputeDataModel
// store off that, which is what lets the assistant read a metric table while
// the user is looking at one. kernel_pc_samples reads PC samples through the
// ISA View's slots instead, and never while the view has one of its own in
// flight.
#include "rocprofvis_ai_tools_internal.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json.h"

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_requests.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr size_t ASSISTANT_COMPUTE_MAX_RESULT_CHARS = 20000;
constexpr size_t ASSISTANT_COMPUTE_TOP_KERNELS      = 10;
constexpr size_t ASSISTANT_COMPUTE_MAX_KERNELS      = 200;
// Catalogue rows one list_metrics call may name. Low on purpose: a search that
// matches more than this is too broad to be useful, and the model is better
// served by being told to narrow it than by a page of near-misses.
constexpr size_t ASSISTANT_COMPUTE_MAX_METRIC_ROWS = 60;
// Longest metric selector list one get_metrics call may carry. A category on
// its own can expand to hundreds of entries, so the cap is on selectors rather
// than on what they resolve to.
constexpr size_t ASSISTANT_COMPUTE_MAX_SELECTORS = 16;
// A metric id is category.table.entry; a shorter one selects everything under
// it.
constexpr size_t ASSISTANT_METRIC_ID_PARTS = 3;

// Instructions one kernel_pc_samples call lists, most-sampled first.
constexpr size_t ASSISTANT_PC_DEFAULT_INSTRUCTIONS = 10;
constexpr size_t ASSISTANT_PC_MAX_INSTRUCTIONS     = 30;
// Source files whose line tables one call reads to put line numbers on the
// instructions it lists. Each is a read of its own, and hot instructions rarely
// span more than the kernel's file and a header or two.
constexpr size_t ASSISTANT_PC_MAX_SOURCE_FILES = 4;
// Reasons listed against one instruction, most common first.
constexpr size_t ASSISTANT_PC_REASONS_PER_INSTRUCTION = 3;
// Longest source line quoted beside an instruction.
constexpr size_t ASSISTANT_PC_MAX_CODE_CHARS = 100;
// Marks the assistant's PC-sampling reads for the ISA View, which drops any
// completion whose generation is not its own. The view counts its generations
// up from zero, one per kernel selection, so it never reaches this one.
constexpr uint32_t    ASSISTANT_PC_SAMPLING_GENERATION = UINT32_MAX;
constexpr const char* ASSISTANT_NO_PC_SAMPLES =
    "No PC sampling data was found for that kernel. rocprof-compute records PC "
    "samples in a capture of their own, apart from the hardware counters, so a "
    "trace usually carries one or the other.";
constexpr const char* ASSISTANT_PC_SAMPLES_UNSUPPORTED =
    "This trace has no PC samples to read: it was written with a compute schema "
    "older than 2.2, and Optiq reads PC samples only from 2.2 on. That is the "
    "trace, not a failed read, so do not retry it.";

/*
 * The tables a first look at any kernel needs, by name rather than by id.
 *
 * Between them they carry the launch geometry, the register and scratch
 * allocation, the speed-of-light summary, the arithmetic split by precision,
 * the instruction mix, and the two efficiency rates - coalescing and LDS bank
 * conflicts - that name a memory problem instead of merely reporting that
 * memory was slow. That is the standard triage set for this profiler, not a
 * guess at what one workload will need.
 *
 * Matched against the catalogue's own table names because the dotted ids move
 * with the accelerator and the profiling mode, while these names do not. A
 * table this trace did not record simply contributes no selector.
 */
constexpr const char* ASSISTANT_TRIAGE_TABLE_NAMES[] = {
    "System Speed-of-Light",           "Wavefront Launch Stats",
    "Wavefront Runtime Stats",         "Roofline Performance Rates",
    "Overall Instruction Mix",         "VALU Arithmetic Instruction Mix",
    "VMEM Instruction Mix",            "vL1D Speed-of-Light",
    "LDS Speed-of-Light",
    // These three name a defect outright rather than describing a symptom:
    // which resource capped residency, a reduction serialised on one LDS
    // address, and scalar access that should have been vectorised.
    "Workgroup Manager - Resource Allocation",
    "LDS Statistics",
    "Vector L1 data-return path or Texture Data (TD)",
};

// Display names for the roofline enums, kept in step with the ones the Roofline
// widget draws so the model and the user name the same line.
constexpr const char* ASSISTANT_CEILING_COMPUTE_NAMES[] = {
    "MFMA FP4",  "MFMA FP6",  "MFMA FP8",  "VALU I8",   "MFMA I8",
    "VALU FP16", "MFMA FP16", "MFMA BF16", "VALU FP32", "MFMA FP32",
    "VALU I32",  "VALU FP64", "MFMA FP64", "VALU I64",
};
constexpr const char* ASSISTANT_CEILING_BANDWIDTH_NAMES[] = { "HBM", "L2", "L1", "LDS" };

const char*
CeilingComputeName(rocprofvis_controller_roofline_ceiling_compute_type_t type)
{
    const size_t index = static_cast<size_t>(type);
    if(index >= sizeof(ASSISTANT_CEILING_COMPUTE_NAMES) /
                    sizeof(ASSISTANT_CEILING_COMPUTE_NAMES[0]))
    {
        return "unknown";
    }
    return ASSISTANT_CEILING_COMPUTE_NAMES[index];
}

const char*
CeilingBandwidthName(rocprofvis_controller_roofline_ceiling_bandwidth_type_t type)
{
    const size_t index = static_cast<size_t>(type);
    if(index >= sizeof(ASSISTANT_CEILING_BANDWIDTH_NAMES) /
                    sizeof(ASSISTANT_CEILING_BANDWIDTH_NAMES[0]))
    {
        return "unknown";
    }
    return ASSISTANT_CEILING_BANDWIDTH_NAMES[index];
}

const char*
IntensityName(rocprofvis_controller_roofline_kernel_intensity_type_t type)
{
    switch(type)
    {
        case kRPVControllerRooflineKernelIntensityTypeHBM: return "HBM";
        case kRPVControllerRooflineKernelIntensityTypeL2: return "L2";
        case kRPVControllerRooflineKernelIntensityTypeL1: return "L1";
        case kRPVControllerRooflineKernelIntensityTypeLDS: return "LDS";
    }
    return "unknown";
}

std::string
TrimComputeResult(std::string text)
{
    return TrimAssistantText(std::move(text), ASSISTANT_COMPUTE_MAX_RESULT_CHARS,
                             "\n... truncated. Narrow the request and ask again ...\n");
}

// Prints a metric value at a fixed precision rather than through the UI's unit
// formatter: these carry their own units from the catalogue, and a counter, a
// percentage and a GB/s figure must not all be reformatted as a time.
std::string
FormatMetricNumber(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.4g", value);
    return std::string(buffer);
}

ComputeDataModel&
Model(const AssistantToolContext& context)
{
    return context.data_provider->ComputeModel();
}

// The request slot kernel_triage and get_metrics share. Derived from the
// assistant's client id, so it can never be the one a metric table is waiting on.
uint64_t
ComputeMetricsRequestId()
{
    return RequestIdBuilder::MakeClientRequestId(RequestType::kFetchMetrics,
                                                 DataProvider::ASSISTANT_CLIENT_ID);
}

// Whether the model passed an argument at all. A null counts as leaving it out,
// since that is how some models spell an optional argument they are not using.
bool
HasArg(const jt::Json& args, const char* key)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    return mutable_args.contains(key) && !mutable_args[key].isNull();
}

/*
 * Which workload a tool is talking about.
 *
 * An explicit workload_id wins, then whichever one the toolbar has selected,
 * then the only one there is. Most traces carry a single workload, so falling
 * through to the first keeps every tool callable with no arguments at all. An
 * id that was passed but names nothing is refused rather than replaced by the
 * selection, which would answer about a workload nobody asked for.
 */
const WorkloadInfo*
ResolveWorkload(const AssistantToolContext& context, const jt::Json& args,
                std::string& error_out)
{
    ComputeDataModel& model = Model(context);

    if(HasArg(args, "workload_id"))
    {
        const uint64_t      requested = JsonU64(args, "workload_id", UINT64_MAX);
        const WorkloadInfo* workload =
            requested <= UINT32_MAX ? model.GetWorkload(static_cast<uint32_t>(requested))
                                    : nullptr;
        if(workload == nullptr)
        {
            error_out = "That workload_id is not in this trace. compute_overview lists "
                        "the workloads it has.";
        }
        return workload;
    }

    if(context.compute_selection != nullptr)
    {
        const uint32_t selected = context.compute_selection->GetSelectedWorkload();
        if(selected != ComputeSelection::INVALID_SELECTION_ID)
        {
            const WorkloadInfo* workload = model.GetWorkload(selected);
            if(workload != nullptr)
            {
                return workload;
            }
        }
    }

    const std::vector<const WorkloadInfo*>& workloads = model.GetWorkloadList();
    if(workloads.empty())
    {
        error_out = "This compute trace has no workloads in it.";
        return nullptr;
    }
    return workloads.front();
}

// Finds a kernel by exact name first, then by unique substring. Names come back
// mangled and are long, so requiring the whole string would make the tool
// unusable; resolving an ambiguous fragment would answer about the wrong
// kernel, so that is refused instead.
const KernelInfo*
KernelByName(const WorkloadInfo& workload, const std::string& name, bool& ambiguous_out)
{
    ambiguous_out = false;
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel != nullptr && kernel->name == name)
        {
            return kernel;
        }
    }

    const std::string  needle = Core::String::to_lower_copy(name);
    const KernelInfo*  match  = nullptr;
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel == nullptr ||
           Core::String::to_lower_copy(kernel->name).find(needle) == std::string::npos)
        {
            continue;
        }
        if(match != nullptr)
        {
            ambiguous_out = true;
            return nullptr;
        }
        match = kernel;
    }
    return match;
}

/*
 * Which kernel a tool is talking about: the one it names, else the one the
 * toolbar has selected.
 *
 * named_out separates "no kernel was named and none is selected" from "a kernel
 * was named and does not exist", which the caller has to report differently. A
 * kernel_id that cannot be read - a name passed in the id field, say - counts as
 * named: falling back to the selection would answer about a kernel the model
 * did not ask for.
 */
const KernelInfo*
ResolveKernel(const AssistantToolContext& context, const WorkloadInfo& workload,
              const jt::Json& args, bool& named_out, std::string& error_out)
{
    named_out = false;

    if(HasArg(args, "kernel_id"))
    {
        named_out                = true;
        const uint64_t requested = JsonU64(args, "kernel_id", UINT64_MAX);
        if(requested > UINT32_MAX)
        {
            error_out = "kernel_id must be a kernel's number from list_kernels. Pass a "
                        "name as kernel_name instead.";
            return nullptr;
        }
        const KernelInfo* kernel =
            Model(context).GetKernelInfo(workload.id, static_cast<uint32_t>(requested));
        if(kernel == nullptr)
        {
            error_out = "No kernel with id " + std::to_string(requested) +
                        " in this workload. Call list_kernels for the ids.";
        }
        return kernel;
    }

    const std::string name = JsonUtils::GetString(args, "kernel_name", "");
    if(!name.empty())
    {
        named_out      = true;
        bool ambiguous = false;
        const KernelInfo* kernel = KernelByName(workload, name, ambiguous);
        if(kernel == nullptr)
        {
            error_out = ambiguous ? "\"" + name +
                                        "\" matches more than one kernel. Pass a "
                                        "kernel_id from list_kernels instead."
                                  : "No kernel matching \"" + name +
                                        "\" in this workload. Call list_kernels.";
        }
        return kernel;
    }

    if(context.compute_selection != nullptr)
    {
        const uint32_t selected = context.compute_selection->GetSelectedKernel();
        if(selected != ComputeSelection::INVALID_SELECTION_ID)
        {
            return Model(context).GetKernelInfo(workload.id, selected);
        }
    }
    return nullptr;
}

// The two-column name/value tables a workload carries for system info and
// profiling config. Column 0 is names and column 1 values; anything else is
// skipped rather than guessed at.
void
AppendNameValueTable(std::ostringstream& out, const char* label,
                     const std::vector<std::vector<std::string>>& table)
{
    if(table.size() != 2 || table[0].size() != table[1].size() || table[0].empty())
    {
        return;
    }
    out << label << ":\n";
    for(size_t i = 0; i < table[0].size(); ++i)
    {
        out << "  " << table[0][i] << ": " << table[1][i] << "\n";
    }
}

void
AppendKernelRow(std::ostringstream& out, const KernelInfo& kernel)
{
    out << "  kernel_id=" << kernel.id
        << " invocations=" << kernel.dispatch_metrics[KernelInfo::InvocationCount]
        << " duration_total_ns=" << kernel.dispatch_metrics[KernelInfo::DurationTotal]
        << " duration_mean_ns=" << kernel.dispatch_metrics[KernelInfo::DurationMean]
        << " duration_median_ns=" << kernel.dispatch_metrics[KernelInfo::DurationMedian]
        << " duration_min_ns=" << kernel.dispatch_metrics[KernelInfo::DurationMin]
        << " duration_max_ns=" << kernel.dispatch_metrics[KernelInfo::DurationMax]
        << " name=\"" << kernel.name << "\"\n";
}

// One kernel's position on the roofline, at each memory level the trace
// measured. Walks the enum in order so repeated answers agree with each other.
void
AppendKernelIntensities(std::ostringstream& out, const KernelInfo& kernel)
{
    if(kernel.roofline.intensities.empty())
    {
        out << "kernel_intensities: (none recorded for this kernel)\n";
        return;
    }

    out << "kernel_intensities:\n";
    for(uint32_t i = kRPVControllerRooflineKernelIntensityTypeHBM;
        i <= kRPVControllerRooflineKernelIntensityTypeLDS; ++i)
    {
        const rocprofvis_controller_roofline_kernel_intensity_type_t type =
            static_cast<rocprofvis_controller_roofline_kernel_intensity_type_t>(i);
        if(kernel.roofline.intensities.count(type) == 0)
        {
            continue;
        }
        const KernelInfo::Roofline::Intensity& intensity =
            kernel.roofline.intensities.at(type);
        out << "  " << IntensityName(type) << ": arithmetic_intensity="
            << FormatMetricNumber(intensity.position.x)
            << " performance=" << FormatMetricNumber(intensity.position.y) << "\n";
    }
}

uint64_t
TotalKernelTimeNs(const WorkloadInfo& workload)
{
    uint64_t total = 0;
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel != nullptr)
        {
            total += kernel->dispatch_metrics[KernelInfo::DurationTotal];
        }
    }
    return total;
}

std::string
FormatPercentOf(uint64_t part, uint64_t whole)
{
    if(whole == 0)
    {
        return "0.0";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f",
                  static_cast<double>(part) / static_cast<double>(whole) * 100.0);
    return std::string(buffer);
}

// One part of a dotted id: digits only, and small enough for a uint32_t.
bool
ParseMetricIdPart(const std::string& text, size_t begin, size_t end, uint32_t& out)
{
    if(begin == end)
    {
        return false;
    }
    uint64_t value = 0;
    for(size_t i = begin; i < end; ++i)
    {
        if(text[i] < '0' || text[i] > '9')
        {
            return false;
        }
        value = value * 10 + static_cast<uint64_t>(text[i] - '0');
        if(value > UINT32_MAX)
        {
            return false;
        }
    }
    out = static_cast<uint32_t>(value);
    return true;
}

// Reads "2", "2.1" or "2.1.4". An empty part - "2..1", or a trailing dot - is
// refused, and so is a fourth part.
bool
ParseDottedMetricId(const std::string& text, MetricsRequestParams::MetricID& out)
{
    uint32_t parts[ASSISTANT_METRIC_ID_PARTS] = {};
    size_t   count                            = 0;
    size_t   begin                            = 0;
    bool     more                             = true;
    while(more)
    {
        const size_t dot = text.find('.', begin);
        const size_t end = dot == std::string::npos ? text.size() : dot;
        if(count == ASSISTANT_METRIC_ID_PARTS ||
           !ParseMetricIdPart(text, begin, end, parts[count]))
        {
            return false;
        }
        ++count;
        more  = dot != std::string::npos;
        begin = end + 1;
    }

    out             = MetricsRequestParams::MetricID{};
    out.category_id = parts[0];
    if(count > 1)
    {
        out.table_id = parts[1];
    }
    if(count > 2)
    {
        out.entry_id = parts[2];
    }
    return true;
}

// Whether a selector names something this workload's catalogue has. Checked
// before fetching, so a wrong id is reported as wrong instead of coming back as
// an empty result that reads like a metric the trace does not record.
bool
SelectorInCatalogue(const AvailableMetrics&               catalogue,
                    const MetricsRequestParams::MetricID& selector)
{
    auto category = catalogue.tree.find(selector.category_id);
    if(category == catalogue.tree.end())
    {
        return false;
    }
    if(!selector.table_id.has_value())
    {
        return true;
    }
    auto table = category->second.tables.find(selector.table_id.value());
    if(table == category->second.tables.end())
    {
        return false;
    }
    return !selector.entry_id.has_value() ||
           table->second.entries.count(selector.entry_id.value()) > 0;
}

/*
 * Reads a dotted-id argument into the selectors FetchMetrics takes.
 *
 * Each is an id from list_metrics: "2" is a whole category, "2.1" a table,
 * "2.1.4" one entry. A name is refused rather than guessed at - the catalogue
 * is per-workload and its names are free-form, so matching one loosely here
 * would fetch a metric the model did not ask for - and so is an id this
 * workload's catalogue does not have.
 *
 * Shared by get_metrics and by list_metrics' describe mode, so the two accept
 * exactly the same ids and a model that has learned one has learned both.
 */
bool
ParseMetricSelectors(const jt::Json& args, const char* key,
                     const AvailableMetrics&                      catalogue,
                     std::vector<MetricsRequestParams::MetricID>& out,
                     std::string&                                 error_out)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains(key) || !mutable_args[key].isArray())
    {
        error_out = std::string(key) +
                    " must be an array of dotted ids from list_metrics, for "
                    "example [\"2.1\"] for a whole table or [\"2.1.4\"] for one "
                    "entry.";
        return false;
    }

    std::vector<jt::Json>& entries = mutable_args[key].getArray();
    if(!CheckArrayLength(entries, key, error_out))
    {
        return false;
    }
    if(entries.size() > ASSISTANT_COMPUTE_MAX_SELECTORS)
    {
        error_out = std::string(key) + " has too many entries. Use at most " +
                    std::to_string(ASSISTANT_COMPUTE_MAX_SELECTORS) +
                    ", and prefer a whole table id over listing its entries.";
        return false;
    }

    for(jt::Json& entry : entries)
    {
        if(!entry.isString())
        {
            error_out = std::string("Every entry of ") + key +
                        " must be a dotted id string from list_metrics, such as "
                        "\"2.1.4\".";
            return false;
        }

        const std::string              text = Core::String::trim_copy(entry.getString());
        MetricsRequestParams::MetricID selector{};
        if(!ParseDottedMetricId(text, selector))
        {
            error_out = "\"" + text +
                        "\" is not a metric id. Use the dotted ids list_metrics "
                        "printed, such as \"2.1\" or \"2.1.4\".";
            return false;
        }
        if(!SelectorInCatalogue(catalogue, selector))
        {
            error_out = "\"" + text +
                        "\" is not in this workload's metric catalogue. Call "
                        "list_metrics for the ids it does have.";
            return false;
        }
        out.push_back(selector);
    }

    if(out.empty())
    {
        error_out = std::string(key) + " was empty. Pass at least one id from "
                                       "list_metrics.";
        return false;
    }
    return true;
}

// Whether one catalogue entry falls under a selector. A selector with no table
// covers a whole category, one with no entry a whole table.
bool
EntryMatchesSelector(const AvailableMetrics::Entry&        entry,
                     const MetricsRequestParams::MetricID& selector)
{
    if(entry.category_id != selector.category_id)
    {
        return false;
    }
    if(selector.table_id.has_value() && entry.table_id != selector.table_id.value())
    {
        return false;
    }
    if(selector.entry_id.has_value() && entry.id != selector.entry_id.value())
    {
        return false;
    }
    return true;
}

// One catalogue row as list_metrics prints it: id, name, and the unit when there
// is one, then the description on its own line when that was asked for.
void
AppendCatalogueEntry(std::ostringstream& out, const AvailableMetrics::Entry& entry,
                     bool with_description)
{
    out << "  " << entry.category_id << "." << entry.table_id << "." << entry.id << "  "
        << entry.name;
    if(!entry.unit.empty())
    {
        out << "  [" << entry.unit << "]";
    }
    out << "\n";
    if(with_description && !entry.description.empty())
    {
        out << "    " << entry.description << "\n";
    }
}

// --- Tools ---------------------------------------------------------------

AssistantToolStartResult
ToolComputeOverview(const AssistantToolContext& context, const jt::Json& args,
                    const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    const std::vector<const WorkloadInfo*>& workloads = Model(context).GetWorkloadList();
    std::ostringstream                      out;
    out << "workload_count: " << workloads.size() << "\n";
    if(workloads.size() > 1)
    {
        out << "workloads:\n";
        for(const WorkloadInfo* entry : workloads)
        {
            if(entry != nullptr)
            {
                out << "  workload_id=" << entry->id << " kernels="
                    << entry->ordered_kernels.size() << " name=\"" << entry->name
                    << "\"\n";
            }
        }
    }

    out << "workload_id: " << workload->id << "\n";
    out << "workload_name: " << workload->name << "\n";
    AppendNameValueTable(out, "system_info", workload->system_info);
    AppendNameValueTable(out, "profiling_config", workload->profiling_config);

    const uint64_t total_ns = TotalKernelTimeNs(*workload);
    out << "kernel_count: " << workload->ordered_kernels.size() << "\n";
    out << "kernel_time_total_ns: " << total_ns << "\n";
    out << "metric_count: " << workload->available_metrics.list.size() << "\n";

    std::vector<const KernelInfo*> kernels = workload->ordered_kernels;
    std::sort(kernels.begin(), kernels.end(),
              [](const KernelInfo* a, const KernelInfo* b) {
                  return a->dispatch_metrics[KernelInfo::DurationTotal] >
                         b->dispatch_metrics[KernelInfo::DurationTotal];
              });

    const size_t shown = std::min(kernels.size(), ASSISTANT_COMPUTE_TOP_KERNELS);
    out << "top_kernels_by_total_time:\n";
    for(size_t i = 0; i < shown; ++i)
    {
        out << "  " << (i + 1) << ". share_pct="
            << FormatPercentOf(kernels[i]->dispatch_metrics[KernelInfo::DurationTotal],
                               total_ns)
            << "\n";
        AppendKernelRow(out, *kernels[i]);
    }
    if(kernels.size() > shown)
    {
        out << "  ... " << (kernels.size() - shown)
            << " more kernels, see list_kernels\n";
    }
    out << "note: these are aggregate dispatch statistics. A compute trace has no "
           "timeline, so there is no ordering or start time for an individual "
           "dispatch.";
    if(workload->available_metrics.list.empty())
    {
        out << " This workload records no hardware counters, so list_metrics, "
               "kernel_triage and get_metrics have nothing to read; kernel_pc_samples "
               "reads its PC samples if it has any.\n";
    }
    else
    {
        out << " Hardware metrics are not here - kernel_triage reads the standard "
               "panel for one kernel, and get_metrics whatever it leaves open.\n";
    }

    return DoneResult(TrimComputeResult(out.str()), "Read the workload overview");
}

AssistantToolStartResult
ToolListKernels(const AssistantToolContext& context, const jt::Json& args,
                const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    std::vector<const KernelInfo*> kernels;
    const std::string              filter =
        Core::String::to_lower_copy(JsonUtils::GetString(args, "name_contains", ""));
    for(const KernelInfo* kernel : workload->ordered_kernels)
    {
        if(kernel == nullptr)
        {
            continue;
        }
        if(!filter.empty() &&
           Core::String::to_lower_copy(kernel->name).find(filter) == std::string::npos)
        {
            continue;
        }
        kernels.push_back(kernel);
    }

    const std::string sort_by =
        Core::String::to_lower_copy(JsonUtils::GetString(args, "sort_by", "duration_total"));
    const bool descending =
        Core::String::to_lower_copy(JsonUtils::GetString(args, "sort_order", "desc")) !=
        "asc";

    KernelInfo::DispatchMetric metric   = KernelInfo::DurationTotal;
    bool                       by_name  = false;
    if(sort_by == "name")
    {
        by_name = true;
    }
    else if(sort_by == "invocations")
    {
        metric = KernelInfo::InvocationCount;
    }
    else if(sort_by == "duration_min")
    {
        metric = KernelInfo::DurationMin;
    }
    else if(sort_by == "duration_max")
    {
        metric = KernelInfo::DurationMax;
    }
    else if(sort_by == "duration_mean")
    {
        metric = KernelInfo::DurationMean;
    }
    else if(sort_by == "duration_median")
    {
        metric = KernelInfo::DurationMedian;
    }
    else if(sort_by != "duration_total")
    {
        return DoneResult(
            "Unknown sort_by \"" + sort_by +
                "\". Use name, invocations, duration_total, duration_mean, "
                "duration_median, duration_min, or duration_max.",
            "Bad sort_by");
    }

    std::sort(kernels.begin(), kernels.end(),
              [&](const KernelInfo* a, const KernelInfo* b) {
                  if(by_name)
                  {
                      return descending ? a->name > b->name : a->name < b->name;
                  }
                  return descending ? a->dispatch_metrics[metric] > b->dispatch_metrics[metric]
                                    : a->dispatch_metrics[metric] < b->dispatch_metrics[metric];
              });

    const int32_t requested = JsonUtils::GetInt(
        args, "limit", static_cast<int32_t>(ASSISTANT_DEFAULT_ROW_LIMIT));
    const size_t limit =
        requested <= 0 ? ASSISTANT_DEFAULT_ROW_LIMIT
                       : std::min(static_cast<size_t>(requested),
                                  ASSISTANT_COMPUTE_MAX_KERNELS);
    const size_t offset = static_cast<size_t>(JsonU64(args, "offset", 0));

    std::ostringstream out;
    out << "workload_id: " << workload->id << "\n";
    out << "matching_kernels: " << kernels.size() << "\n";
    const uint64_t total_ns = TotalKernelTimeNs(*workload);
    out << "kernel_time_total_ns: " << total_ns << "\n";
    if(kernels.empty())
    {
        out << "kernels: (none)\n";
        out << (filter.empty() ? "This workload has no kernels in it.\n"
                               : "Nothing matches that name_contains. Call again "
                                 "without it to see every kernel.\n");
        return DoneResult(out.str(), "Listed kernels");
    }

    out << "kernels:\n";
    if(offset >= kernels.size())
    {
        out << "  (offset " << offset << " is past the end of " << kernels.size()
            << " matching kernels)\n";
        return DoneResult(out.str(), "Listed kernels");
    }
    const size_t end = std::min(kernels.size(), offset + limit);
    for(size_t i = offset; i < end; ++i)
    {
        AppendKernelRow(out, *kernels[i]);
    }
    if(end < kernels.size())
    {
        out << "  ... " << (kernels.size() - end) << " more, raise limit or page with "
                                                     "offset\n";
    }

    return DoneResult(TrimComputeResult(out.str()), "Listed kernels");
}

AssistantToolStartResult
ToolKernelSummary(const AssistantToolContext& context, const jt::Json& args,
                  const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr)
    {
        return DoneResult(named ? error
                                : "kernel_summary needs a kernel_id or kernel_name, and "
                                  "no kernel is selected. Call list_kernels first.",
                          "No kernel");
    }

    const uint64_t total_ns = TotalKernelTimeNs(*workload);

    std::ostringstream out;
    out << "workload_id: " << workload->id << "\n";
    out << "kernel_id: " << kernel->id << "\n";
    out << "kernel_name: " << kernel->name << "\n";
    out << "invocations: " << kernel->dispatch_metrics[KernelInfo::InvocationCount]
        << "\n";
    out << "duration_total_ns: " << kernel->dispatch_metrics[KernelInfo::DurationTotal]
        << "\n";
    out << "share_of_kernel_time_pct: "
        << FormatPercentOf(kernel->dispatch_metrics[KernelInfo::DurationTotal], total_ns)
        << "\n";
    out << "duration_mean_ns: " << kernel->dispatch_metrics[KernelInfo::DurationMean]
        << "\n";
    out << "duration_median_ns: " << kernel->dispatch_metrics[KernelInfo::DurationMedian]
        << "\n";
    out << "duration_min_ns: " << kernel->dispatch_metrics[KernelInfo::DurationMin]
        << "\n";
    out << "duration_max_ns: " << kernel->dispatch_metrics[KernelInfo::DurationMax]
        << "\n";

    if(!kernel->roofline.intensities.empty())
    {
        AppendKernelIntensities(out, *kernel);
        out << "note: call kernel_roofline for the ceilings these sit under.\n";
    }

    return DoneResult(TrimComputeResult(out.str()), "Read the kernel summary");
}

AssistantToolStartResult
ToolListMetrics(const AssistantToolContext& context, const jt::Json& args,
                const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    const AvailableMetrics& metrics = workload->available_metrics;
    if(metrics.list.empty())
    {
        return DoneResult("This workload carries no metric definitions, so only the "
                          "kernel dispatch statistics are available.",
                          "No metrics");
    }

    std::ostringstream out;
    out << "workload_id: " << workload->id << "\n";
    out << "metric_id_format: category.table.entry\n";

    // Descriptions, for ids the model already has. Asked for by name because
    // they are the expensive half of the catalogue and are rarely needed: a
    // metric's name is usually enough to interpret it.
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(mutable_args.contains("describe"))
    {
        std::vector<MetricsRequestParams::MetricID> selectors;
        if(!ParseMetricSelectors(args, "describe", metrics, selectors, error))
        {
            return DoneResult(error, "Bad describe");
        }

        size_t matched = 0;
        for(const AvailableMetrics::Entry& entry : metrics.list)
        {
            bool wanted = false;
            for(const MetricsRequestParams::MetricID& selector : selectors)
            {
                if(EntryMatchesSelector(entry, selector))
                {
                    wanted = true;
                    break;
                }
            }
            if(!wanted)
            {
                continue;
            }
            ++matched;
            if(matched <= ASSISTANT_COMPUTE_MAX_METRIC_ROWS)
            {
                AppendCatalogueEntry(out, entry, true);
            }
        }
        if(matched == 0)
        {
            out << "  (no metrics are listed under those ids)\n";
        }
        else if(matched > ASSISTANT_COMPUTE_MAX_METRIC_ROWS)
        {
            out << "note: " << (matched - ASSISTANT_COMPUTE_MAX_METRIC_ROWS)
                << " more matched than are listed. Describe fewer ids at a time.\n";
        }
        return DoneResult(TrimComputeResult(out.str()), "Described metrics");
    }

    // By word, over the flat catalogue. Names and units only - enough to pick
    // an id out and pass it to get_metrics, which labels its values anyway.
    const std::string search =
        Core::String::to_lower_copy(JsonUtils::GetString(args, "search", ""));
    if(!search.empty())
    {
        size_t matched = 0;
        size_t shown   = 0;
        for(const AvailableMetrics::Entry& entry : metrics.list)
        {
            if(Core::String::to_lower_copy(entry.name).find(search) ==
                   std::string::npos &&
               Core::String::to_lower_copy(entry.description).find(search) ==
                   std::string::npos)
            {
                continue;
            }
            ++matched;
            if(shown >= ASSISTANT_COMPUTE_MAX_METRIC_ROWS)
            {
                continue;
            }
            ++shown;
            AppendCatalogueEntry(out, entry, false);
        }

        if(matched == 0)
        {
            out << "  (nothing matches \"" << JsonUtils::GetString(args, "search", "")
                << "\" - call with no arguments to see the tables, or try a "
                   "broader word)\n";
            return DoneResult(out.str(), "Listed metrics");
        }
        out << "matched_metrics: " << matched << "\n";
        if(matched > shown)
        {
            out << "note: " << (matched - shown)
                << " more matched than are listed. Use a narrower word.\n";
        }
        out << "note: pass these ids to get_metrics, or to describe if you need to "
               "know what one measures.\n";
        return DoneResult(TrimComputeResult(out.str()), "Listed metrics");
    }

    // The default, and the cheap one: which tables exist, not what is in them.
    // A full catalogue is hundreds of entries and the model needs a table id to
    // fetch, so listing every entry up front spends a lot of context on rows it
    // will not use. It narrows with search or fetches a whole table from here.
    out << "tables:\n";
    for(const AvailableMetrics::Category* category : metrics.ordered_categories)
    {
        if(category == nullptr)
        {
            continue;
        }
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            if(table == nullptr)
            {
                continue;
            }
            out << "  " << category->id << "." << table->id << "  " << category->name
                << " / " << table->name << "  (" << table->ordered_entries.size()
                << " metrics)";
            if(!table->value_names.empty())
            {
                out << "  value_names:";
                for(const std::string& value_name : table->value_names)
                {
                    out << " " << value_name;
                }
            }
            out << "\n";
        }
    }
    out << "total_metrics: " << metrics.list.size() << "\n";
    out << "note: this is the shape of the catalogue, not its contents. Pass a "
           "table id to get_metrics to read all of it, call again with search to "
           "find one metric by word, or with describe to see what given ids "
           "measure. A metric that is not in this workload does not exist in this "
           "trace, whatever the architecture usually records.\n";

    return DoneResult(TrimComputeResult(out.str()), "Listed metrics");
}

AssistantToolStartResult
ToolKernelRoofline(const AssistantToolContext& context, const jt::Json& args,
                   const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    if(workload->roofline.ceiling_bandwidth.empty() &&
       workload->roofline.ceiling_compute.empty())
    {
        return DoneResult("This workload has no roofline data, so there is no "
                          "memory-bound or compute-bound verdict to read off one.",
                          "No roofline");
    }

    std::ostringstream out;
    out << "workload_id: " << workload->id << "\n";
    out << "axes: x=arithmetic_intensity(FLOP/byte) y=performance(GFLOP/s)\n";

    // Both ceiling maps are two indexes over the same objects, and a ceiling's
    // throughput does not vary with what it is paired against - which is why
    // the Roofline widget takes begin()->second too. Printing the cross product
    // would repeat every figure once per pairing.
    //
    // Walking the enum ranges rather than the maps keeps the order stable from
    // one answer to the next; an unordered_map would reorder the lines.
    out << "bandwidth_ceilings_gb_per_s:\n";
    for(uint32_t i = __KRPVControllerRooflineCeilingBandwidthTypeFirst;
        i < __KRPVControllerRooflineCeilingBandwidthTypeLast; ++i)
    {
        const rocprofvis_controller_roofline_ceiling_bandwidth_type_t type =
            static_cast<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>(i);
        if(workload->roofline.ceiling_bandwidth.count(type) == 0 ||
           workload->roofline.ceiling_bandwidth.at(type).empty())
        {
            continue;
        }
        out << "  " << CeilingBandwidthName(type) << ": "
            << FormatMetricNumber(
                   workload->roofline.ceiling_bandwidth.at(type).begin()->second.throughput)
            << "\n";
    }

    out << "compute_ceilings_gflop_per_s:\n";
    for(uint32_t i = __KRPVControllerRooflineCeilingComputeTypeFirst;
        i < __KRPVControllerRooflineCeilingComputeTypeLast; ++i)
    {
        const rocprofvis_controller_roofline_ceiling_compute_type_t type =
            static_cast<rocprofvis_controller_roofline_ceiling_compute_type_t>(i);
        if(workload->roofline.ceiling_compute.count(type) == 0 ||
           workload->roofline.ceiling_compute.at(type).empty())
        {
            continue;
        }
        out << "  " << CeilingComputeName(type) << ": "
            << FormatMetricNumber(
                   workload->roofline.ceiling_compute.at(type).begin()->second.throughput)
            << "\n";
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr && named)
    {
        return DoneResult(error, "No kernel");
    }
    if(kernel != nullptr)
    {
        out << "kernel_id: " << kernel->id << "\n";
        out << "kernel_name: " << kernel->name << "\n";
        AppendKernelIntensities(out, *kernel);
    }
    else
    {
        out << "note: no kernel was named and none is selected, so only the "
               "workload's ceilings are shown. Pass kernel_id to place a kernel "
               "against them.\n";
    }

    return DoneResult(TrimComputeResult(out.str()), "Read the roofline");
}

// Every triage table this workload actually recorded, as table-wide selectors.
void
CollectTriageSelectors(const WorkloadInfo&                          workload,
                       std::vector<MetricsRequestParams::MetricID>& out)
{
    std::vector<std::string> wanted;
    for(const char* name : ASSISTANT_TRIAGE_TABLE_NAMES)
    {
        wanted.push_back(Core::String::to_lower_copy(name));
    }

    for(const AvailableMetrics::Category* category :
        workload.available_metrics.ordered_categories)
    {
        if(category == nullptr)
        {
            continue;
        }
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            if(table == nullptr || out.size() >= ASSISTANT_COMPUTE_MAX_SELECTORS ||
               std::find(wanted.begin(), wanted.end(),
                         Core::String::to_lower_copy(table->name)) == wanted.end())
            {
                continue;
            }
            MetricsRequestParams::MetricID selector{};
            selector.category_id = category->id;
            selector.table_id    = table->id;
            out.push_back(selector);
        }
    }
}

/*
 * Starts a metric read into the assistant's own store, or parks behind the one
 * already in flight there. A null kernel reads the workload's own values.
 *
 * The slot is the assistant's alone, so a pending request in it is an earlier
 * call of ours rather than a widget's: wait that out and run again instead of
 * formatting values that answer the earlier call. Clearing the scope first is
 * what lets the formatter print everything in the store rather than carrying
 * the selector list through the wait - whatever is there afterwards is what
 * this call asked for.
 */
AssistantToolStartResult
StartComputeMetricsFetch(const AssistantToolContext& context,
                         const WorkloadInfo& workload, const KernelInfo* kernel,
                         const std::vector<MetricsRequestParams::MetricID>& selectors,
                         const char*                                        status_line)
{
    AssistantToolStartResult result;
    result.pending    = true;
    result.fetch.kind = AssistantFetchKind::kComputeMetrics;
    result.request_ids.push_back(ComputeMetricsRequestId());
    if(context.data_provider->IsRequestPending(result.request_ids.front()))
    {
        result.status_line = "Waiting for the previous metric fetch...";
        return result;
    }

    ComputeDataModel&     model = Model(context);
    std::vector<uint32_t> kernel_ids;
    if(kernel != nullptr)
    {
        kernel_ids.push_back(kernel->id);
        model.ClearKernelMetricValues(DataProvider::ASSISTANT_CLIENT_ID, kernel->id);
    }
    else
    {
        model.ClearWorkloadMetricValues(DataProvider::ASSISTANT_CLIENT_ID, workload.id);
    }

    if(!context.data_provider->FetchMetrics(MetricsRequestParams(
           workload.id, kernel_ids, selectors, DataProvider::ASSISTANT_CLIENT_ID)))
    {
        return DoneResult("The metric fetch could not be started, so nothing was read.",
                          "Fetch refused");
    }

    result.started_fetch     = true;
    result.fetch.workload_id = workload.id;
    result.fetch.kernel_id =
        kernel != nullptr ? kernel->id : ASSISTANT_COMPUTE_WORKLOAD_SCOPE;
    result.status_line = status_line;
    return result;
}

/*
 * One kernel's whole diagnostic panel, in a single query.
 *
 * The alternative - searching the catalogue a word at a time and fetching each
 * table as the thought occurs - costs a round trip per idea and only ever finds
 * the defect that was already suspected. Reading the standard set at once puts
 * scratch beside coalescing beside the precision split, so the evidence for the
 * defect that is actually present is there whether or not it was looked for.
 */
AssistantToolStartResult
ToolKernelTriage(const AssistantToolContext& context, const jt::Json& args,
                 const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr)
    {
        return DoneResult(named ? error
                                : "kernel_triage needs a kernel_id or kernel_name, and "
                                  "no kernel is selected. Call list_kernels first.",
                          "No kernel");
    }

    std::vector<MetricsRequestParams::MetricID> selectors;
    CollectTriageSelectors(*workload, selectors);
    if(selectors.empty())
    {
        return DoneResult("This workload recorded none of the standard triage tables, "
                          "so there is no panel to read. Call list_metrics to see what "
                          "it does carry.",
                          "No triage tables");
    }

    return StartComputeMetricsFetch(context, *workload, kernel, selectors,
                                    "Reading the triage panel...");
}

AssistantToolStartResult
ToolGetMetrics(const AssistantToolContext& context, const jt::Json& args,
               const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    std::vector<MetricsRequestParams::MetricID> selectors;
    if(!ParseMetricSelectors(args, "metrics", workload->available_metrics, selectors,
                             error))
    {
        return DoneResult(error, "Bad metrics");
    }

    // Asked for by name rather than by leaving the kernel out: selecting a
    // workload also selects its first kernel, so "no kernel given" would always
    // land on the toolbar's kernel and the workload's own values would be out
    // of reach.
    const std::string scope =
        Core::String::to_lower_copy(JsonUtils::GetString(args, "scope", "kernel"));
    if(scope == "workload")
    {
        if(HasArg(args, "kernel_id") ||
           !JsonUtils::GetString(args, "kernel_name", "").empty())
        {
            return DoneResult("scope=\"workload\" reads the workload's own values, so it "
                              "takes no kernel_id or kernel_name. Drop them, or use "
                              "scope=\"kernel\".",
                              "Bad scope");
        }
        return StartComputeMetricsFetch(context, *workload, nullptr, selectors,
                                        "Reading workload metric values...");
    }
    if(scope != "kernel")
    {
        return DoneResult("scope must be \"kernel\" or \"workload\".", "Bad scope");
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr)
    {
        return DoneResult(named ? error
                                : "get_metrics needs a kernel_id or kernel_name, and no "
                                  "kernel is selected. Call list_kernels, or pass "
                                  "scope=\"workload\" for the workload's own values.",
                          "No kernel");
    }
    return StartComputeMetricsFetch(context, *workload, kernel, selectors,
                                    "Reading metric values...");
}

// One sampled instruction, summed over every sample state that names it.
struct PcInstructionSamples
{
    uint64_t                        instruction_uuid = 0;
    uint64_t                        total            = 0;
    uint64_t                        issued           = 0;
    uint64_t                        stalled          = 0;
    std::map<std::string, uint64_t> reasons;
};

/*
 * Which layers this call has submitted a read for.
 *
 * The tool re-enters once per layer, and a layer that failed stays kFailed on
 * the kernel, where the ISA View's reads land too. Retrying a failed layer at
 * most once per call is what stops a lasting failure from being fetched again
 * on every re-entry until the wait times out. The panel clears this whenever a
 * call ends, through ResetAssistantPcRead, so the next call retries once more.
 */
struct PcReadAttempt
{
    const DataProvider* provider    = nullptr;
    uint32_t            workload_id = 0;
    uint32_t            kernel_id   = 0;
    bool                open        = false;
    bool                stalls      = false;
    bool                isa         = false;
    bool                source      = false;
};

PcReadAttempt g_pc_read_attempt;

bool
PcReadContinuing(const DataProvider* provider, uint32_t workload_id, uint32_t kernel_id)
{
    PcReadAttempt& attempt = g_pc_read_attempt;
    if(attempt.open && attempt.provider == provider && attempt.workload_id == workload_id &&
       attempt.kernel_id == kernel_id)
    {
        return true;
    }
    attempt             = PcReadAttempt{};
    attempt.open        = true;
    attempt.provider    = provider;
    attempt.workload_id = workload_id;
    attempt.kernel_id   = kernel_id;
    return false;
}

bool
PcLayerAlreadyTried(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kStalls: return g_pc_read_attempt.stalls;
        case PcSamplingLayer::kIsa: return g_pc_read_attempt.isa;
        case PcSamplingLayer::kSource: return g_pc_read_attempt.source;
    }
    return false;
}

void
MarkPcLayerTried(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kStalls:
            g_pc_read_attempt.stalls = true;
            break;
        case PcSamplingLayer::kIsa:
            g_pc_read_attempt.isa = true;
            break;
        case PcSamplingLayer::kSource:
            g_pc_read_attempt.source = true;
            break;
    }
}

// A layer still to read: never read, or failed and not yet retried by this call.
bool
PcLayerOutstanding(PcSamplingLayerState state, PcSamplingLayer layer)
{
    if(state == PcSamplingLayerState::kNotRead)
    {
        return true;
    }
    return state == PcSamplingLayerState::kFailed && !PcLayerAlreadyTried(layer);
}

// The slot each layer is read through, shared with the ISA View.
uint64_t
PcSamplingRequestId(PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kSource:
            return DataProvider::FETCH_PC_SAMPLING_SOURCE_REQUEST_ID;
        case PcSamplingLayer::kStalls:
            return DataProvider::FETCH_PC_SAMPLING_STALLS_REQUEST_ID;
        case PcSamplingLayer::kIsa: break;
    }
    return DataProvider::FETCH_PC_SAMPLING_ISA_REQUEST_ID;
}

/*
 * Reads one layer of a kernel's PC samples, or waits for its slot to clear.
 *
 * The ISA View reads through the same slots, and starting a read while one of
 * its own is in flight would cancel the view's and leave it waiting on a reply
 * that never comes - so a busy slot is waited out, never taken. Either way the
 * wait is parked as someone else's fetch, which has the panel run the tool
 * again once the slot clears; that run picks up whatever landed and moves on to
 * the next layer it still needs. Only a read this call actually submitted
 * counts as its retry: a wait behind the view's read, which may be for another
 * kernel, does not.
 */
AssistantToolStartResult
ReadPcSamplingLayer(const AssistantToolContext& context, const WorkloadInfo& workload,
                    const KernelInfo& kernel, PcSamplingLayer layer,
                    uint64_t source_file_uuid)
{
    AssistantToolStartResult result;
    result.pending     = true;
    result.fetch.kind  = AssistantFetchKind::kPcSampling;
    result.status_line = "Reading PC samples...";
    result.request_ids.push_back(PcSamplingRequestId(layer));
    if(context.data_provider->IsRequestPending(result.request_ids.front()))
    {
        return result;
    }
    // Every kernel owns a PC-sampling object, so a refusal here is never "this
    // trace has no samples": it is a submit that did not start.
    if(!context.data_provider->FetchPcSampling(
           PcSamplingRequestParams(layer, workload.id, kernel.id, source_file_uuid,
                                   ASSISTANT_PC_SAMPLING_GENERATION, 0)))
    {
        return DoneResult(layer == PcSamplingLayer::kStalls
                              ? "The PC samples for that kernel could not be read. That "
                                "is an error reading the trace, not a sign it has none."
                              : "The PC samples for that kernel could not be read in "
                                "full.",
                          "PC samples unreadable");
    }
    MarkPcLayerTried(layer);
    return result;
}

// Every sampled instruction, most-sampled first; ties keep the order the trace
// lists them in, so the ranking is the same from one call to the next.
std::vector<PcInstructionSamples>
RankPcSamples(const PcSamplingData& data)
{
    std::vector<PcInstructionSamples>    ranked;
    std::unordered_map<uint64_t, size_t> index_by_instruction;
    for(const PcSampleState& state : data.pc_sample_states)
    {
        auto found = index_by_instruction.find(state.instruction_uuid);
        if(found == index_by_instruction.end())
        {
            found =
                index_by_instruction.emplace(state.instruction_uuid, ranked.size()).first;
            ranked.emplace_back();
            ranked.back().instruction_uuid = state.instruction_uuid;
        }
        PcInstructionSamples& entry = ranked[found->second];
        entry.total += state.total_count;
        entry.issued += state.issue_count;
        entry.stalled += state.stall_count;
        for(const PcSampleState::Reason& reason : state.reasons)
        {
            entry.reasons[reason.name] += reason.count;
        }
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const PcInstructionSamples& a, const PcInstructionSamples& b) {
                         return a.total > b.total;
                     });
    return ranked;
}

// Where each instruction came from, taking frame 0 as the ISA View does so the
// line named here is the line the user sees there.
std::unordered_map<uint64_t, const InstructionSourceLine*>
SourceByInstruction(const PcSamplingData& data)
{
    std::unordered_map<uint64_t, const InstructionSourceLine*> located;
    for(const InstructionSourceLine& mapping : data.instruction_source_lines)
    {
        if(mapping.frame_index == 0)
        {
            located.emplace(mapping.instruction_uuid, &mapping);
        }
    }
    return located;
}

// The first file the listed instructions map to whose line table has not been
// read yet, or zero once every one of them has.
uint64_t
NextSourceFileToRead(
    const PcSamplingData& data, const std::vector<PcInstructionSamples>& ranked,
    const std::unordered_map<uint64_t, const InstructionSourceLine*>& located,
    size_t                                                            shown)
{
    std::vector<uint64_t> files;
    for(size_t i = 0; i < shown && files.size() < ASSISTANT_PC_MAX_SOURCE_FILES; ++i)
    {
        auto found = located.find(ranked[i].instruction_uuid);
        if(found == located.end() || found->second->source_file_uuid == 0 ||
           std::find(files.begin(), files.end(), found->second->source_file_uuid) !=
               files.end())
        {
            continue;
        }
        files.push_back(found->second->source_file_uuid);
    }
    for(uint64_t file : files)
    {
        // A file that failed is settled for this call too, or the re-entry
        // after the failure would fetch it again until the wait timed out.
        // The next call clears source_files_failed and tries it once more.
        if(std::find(data.source_files_read.begin(), data.source_files_read.end(),
                     file) == data.source_files_read.end() &&
           std::find(data.source_files_failed.begin(), data.source_files_failed.end(),
                     file) == data.source_files_failed.end())
        {
            return file;
        }
    }
    return 0;
}

// Reasons most common first, each against the samples it is a share of.
std::vector<std::pair<std::string, uint64_t>>
OrderedReasons(const std::map<std::string, uint64_t>& reasons)
{
    std::vector<std::pair<std::string, uint64_t>> ordered(reasons.begin(), reasons.end());
    std::stable_sort(
        ordered.begin(), ordered.end(),
        [](const std::pair<std::string, uint64_t>& a,
           const std::pair<std::string, uint64_t>& b) { return a.second > b.second; });
    return ordered;
}

// A path's last component, which is how rows name their file; the full paths
// are listed once underneath them.
std::string
FileName(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string
FormatPcSamples(const WorkloadInfo& workload, const KernelInfo& kernel,
                const std::vector<PcInstructionSamples>&                          ranked,
                const std::unordered_map<uint64_t, const InstructionSourceLine*>& located,
                size_t                                                            shown)
{
    const PcSamplingData& data = kernel.pc_sampling_data;

    std::unordered_map<uint64_t, const std::string*> isa_by_instruction;
    for(const CodeObjectStore& code_object : data.code_objects)
    {
        for(const KernelSymbol& symbol : code_object.kernel_symbols)
        {
            for(const InstructionLine& line : symbol.instruction_lines)
            {
                isa_by_instruction.emplace(line.instruction_uuid, &line.instruction);
            }
        }
    }
    std::unordered_map<uint64_t, const SourceFile*> files_by_uuid;
    std::unordered_map<uint64_t, const SourceLine*> lines_by_uuid;
    for(const SourceFile& file : data.source_files)
    {
        files_by_uuid.emplace(file.source_file_uuid, &file);
        for(const SourceLine& line : file.source_lines)
        {
            lines_by_uuid.emplace(line.source_line_uuid, &line);
        }
    }

    uint64_t                        total   = 0;
    uint64_t                        issued  = 0;
    uint64_t                        stalled = 0;
    std::map<std::string, uint64_t> reasons;
    for(const PcInstructionSamples& entry : ranked)
    {
        total += entry.total;
        issued += entry.issued;
        stalled += entry.stalled;
        for(const std::pair<const std::string, uint64_t>& reason : entry.reasons)
        {
            reasons[reason.first] += reason.second;
        }
    }

    std::ostringstream out;
    out << "workload_id: " << workload.id << "\n";
    out << "kernel_id: " << kernel.id << "\n";
    out << "kernel_name: " << kernel.name << "\n";
    out << "samples_total: " << total << "\n";
    out << "samples_issued: " << issued << " (" << FormatPercentOf(issued, total)
        << "%)\n";
    out << "samples_stalled: " << stalled << " (" << FormatPercentOf(stalled, total)
        << "%)\n";
    out << "sampled_instructions: " << ranked.size() << "\n";
    if(!reasons.empty())
    {
        out << "reasons_all_samples:\n";
        for(const std::pair<std::string, uint64_t>& reason : OrderedReasons(reasons))
        {
            out << "  " << reason.first << ": " << reason.second << " ("
                << FormatPercentOf(reason.second, total) << "%)\n";
        }
    }

    std::vector<const SourceFile*> files_named;
    uint64_t                       listed = 0;
    out << "top_instructions:\n";
    for(size_t i = 0; i < shown; ++i)
    {
        const PcInstructionSamples& entry = ranked[i];
        listed += entry.total;

        std::string at = "?";
        std::string code;
        auto        where = located.find(entry.instruction_uuid);
        if(where != located.end())
        {
            auto       file     = files_by_uuid.find(where->second->source_file_uuid);
            auto       line     = lines_by_uuid.find(where->second->source_line_uuid);
            const bool has_file = file != files_by_uuid.end();
            const bool has_line = line != lines_by_uuid.end();
            const bool numbered = has_line && line->second->line_number != 0;
            at =
                (has_file ? FileName(file->second->file_path) : std::string("?")) + ":" +
                (numbered ? std::to_string(line->second->line_number) : std::string("?"));
            if(has_file && std::find(files_named.begin(), files_named.end(),
                                     file->second) == files_named.end())
            {
                files_named.push_back(file->second);
            }
            if(has_line)
            {
                code = Core::String::trim_copy(line->second->content);
                if(code.size() > ASSISTANT_PC_MAX_CODE_CHARS)
                {
                    code = code.substr(0, ASSISTANT_PC_MAX_CODE_CHARS) + "...";
                }
            }
        }
        auto isa = isa_by_instruction.find(entry.instruction_uuid);

        out << "  " << (i + 1) << ". samples=" << entry.total << " ("
            << FormatPercentOf(entry.total, total) << "%) issued=" << entry.issued
            << " stalled=" << entry.stalled << " at=" << at << " isa=\""
            << (isa != isa_by_instruction.end() ? *isa->second : std::string("?"))
            << "\"";
        const std::vector<std::pair<std::string, uint64_t>> own =
            OrderedReasons(entry.reasons);
        for(size_t r = 0; r < own.size() && r < ASSISTANT_PC_REASONS_PER_INSTRUCTION; ++r)
        {
            out << (r == 0 ? " reasons: " : " ") << own[r].first << "=" << own[r].second;
        }
        if(!code.empty())
        {
            out << " code=\"" << code << "\"";
        }
        out << "\n";
    }
    if(ranked.size() > shown)
    {
        out << "  ... " << (ranked.size() - shown) << " more sampled instructions hold "
            << (total - listed) << " samples\n";
    }
    if(!files_named.empty())
    {
        out << "source_files:\n";
        for(const SourceFile* file : files_named)
        {
            out << "  " << FileName(file->file_path) << " = " << file->file_path << "\n";
        }
    }
    if(data.isa_state != PcSamplingLayerState::kRead)
    {
        out << "note: the ISA text could not be read, so instructions show as ?.\n";
    }
    if(data.source_state != PcSamplingLayerState::kRead)
    {
        out << "note: the source mapping could not be read, so no lines are named.\n";
    }
    else
    {
        // Only the files a listed instruction sits in: a failure elsewhere,
        // the ISA View's included, changes nothing in this answer.
        std::vector<const SourceFile*> unread;
        for(const SourceFile* file : files_named)
        {
            if(std::find(data.source_files_failed.begin(), data.source_files_failed.end(),
                         file->source_file_uuid) != data.source_files_failed.end())
            {
                unread.push_back(file);
            }
        }
        if(!unread.empty())
        {
            out << "note: the lines of";
            for(size_t i = 0; i < unread.size(); ++i)
            {
                out << (i == 0 ? " " : ", ") << FileName(unread[i]->file_path);
            }
            out << " could not be read, so instructions there show a line of ?. Say "
                   "so rather than guessing at the line.\n";
        }
    }
    out << "note: a sample is where one wave was when the profiler looked, so where "
           "samples pile up is where the kernel spends its time. Stalled samples on an "
           "s_waitcnt are waves waiting for memory operations issued before it; stalled "
           "samples on a load or store itself mean the memory pipeline was not taking "
           "new work. Samples say where the time went, not why memory was slow - "
           "coalescing and cache behaviour need the hardware counter capture.\n";
    return TrimComputeResult(out.str());
}

/*
 * Where one kernel's waves were when the profiler sampled them. PC sampling is
 * a capture of its own, so a trace either has it or it does not: this reads it
 * when it is there and says plainly when it is not.
 */
AssistantToolStartResult
ToolKernelPcSamples(const AssistantToolContext& context, const jt::Json& args,
                    const std::string&)
{
    std::string         error;
    const WorkloadInfo* workload = ResolveWorkload(context, args, error);
    if(workload == nullptr)
    {
        return DoneResult(error, "Unknown workload");
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr)
    {
        return DoneResult(named
                              ? error
                              : "kernel_pc_samples needs a kernel_id or kernel_name, and "
                                "no kernel is selected. Call list_kernels first.",
                          "No kernel");
    }

    // The counts come first: they are what says whether there is anything to
    // read at all, and a kernel without samples needs neither other layer.
    // A fresh call clears files a previous read failed on, so they are tried
    // again; a re-entry keeps them, or the failure would be fetched in a loop.
    KernelInfo* mutable_kernel =
        Model(context).GetKernelInfoMutable(workload->id, kernel->id);
    if(mutable_kernel == nullptr)
    {
        return DoneResult("That kernel is no longer loaded.", "No kernel");
    }
    PcSamplingData& data = mutable_kernel->pc_sampling_data;
    if(!PcReadContinuing(context.data_provider, workload->id, kernel->id))
    {
        data.source_files_failed.clear();
    }

    if(PcLayerOutstanding(data.stalls_state, PcSamplingLayer::kStalls))
    {
        return ReadPcSamplingLayer(context, *workload, *kernel, PcSamplingLayer::kStalls,
                                   0);
    }
    if(data.stalls_state == PcSamplingLayerState::kUnsupported)
    {
        return DoneResult(ASSISTANT_PC_SAMPLES_UNSUPPORTED, "No PC samples");
    }
    if(data.stalls_state == PcSamplingLayerState::kFailed)
    {
        return DoneResult(
            "The PC samples for that kernel could not be read. That is an error "
            "reading the trace, not a sign it has no samples - tell the user the "
            "read failed rather than that the capture has none.",
            "PC samples unreadable");
    }
    if(data.pc_sample_states.empty())
    {
        return DoneResult(ASSISTANT_NO_PC_SAMPLES, "No PC samples");
    }
    if(PcLayerOutstanding(data.isa_state, PcSamplingLayer::kIsa))
    {
        return ReadPcSamplingLayer(context, *workload, *kernel, PcSamplingLayer::kIsa, 0);
    }
    if(PcLayerOutstanding(data.source_state, PcSamplingLayer::kSource))
    {
        return ReadPcSamplingLayer(context, *workload, *kernel, PcSamplingLayer::kSource,
                                   0);
    }

    const int32_t requested = JsonUtils::GetInt(
        args, "limit", static_cast<int32_t>(ASSISTANT_PC_DEFAULT_INSTRUCTIONS));
    const size_t limit = requested <= 0 ? ASSISTANT_PC_DEFAULT_INSTRUCTIONS
                                        : std::min(static_cast<size_t>(requested),
                                                   ASSISTANT_PC_MAX_INSTRUCTIONS);

    const std::vector<PcInstructionSamples> ranked = RankPcSamples(data);
    const size_t                            shown  = std::min(ranked.size(), limit);
    const std::unordered_map<uint64_t, const InstructionSourceLine*> located =
        SourceByInstruction(data);
    if(data.source_state == PcSamplingLayerState::kRead)
    {
        const uint64_t unread = NextSourceFileToRead(data, ranked, located, shown);
        if(unread != 0)
        {
            return ReadPcSamplingLayer(context, *workload, *kernel,
                                       PcSamplingLayer::kSource, unread);
        }
    }

    return DoneResult(FormatPcSamples(*workload, *kernel, ranked, located, shown),
                      "Read the PC samples");
}

const AssistantToolEntry k_compute_tool_handlers[] = {
    { "compute_overview", ToolComputeOverview },
    { "list_kernels", ToolListKernels },
    { "kernel_summary", ToolKernelSummary },
    { "list_metrics", ToolListMetrics },
    { "kernel_roofline", ToolKernelRoofline },
    { "kernel_triage", ToolKernelTriage },
    { "get_metrics", ToolGetMetrics },
    { "kernel_pc_samples", ToolKernelPcSamples },
};

}  // namespace

void
ResetAssistantPcRead()
{
    g_pc_read_attempt = PcReadAttempt{};
}

// The compute half of the tool set, for StartAssistantTool to search.
AssistantToolTable
GetAssistantComputeToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_compute_tool_handlers;
    table.count   = sizeof(k_compute_tool_handlers) / sizeof(k_compute_tool_handlers[0]);
    return table;
}

/*
 * Formats the metric values that just landed in the assistant's own store.
 *
 * Walks the catalogue rather than the store's own flat list so the output comes
 * out in category/table/entry order and carries each metric's name and unit.
 * The store was cleared before the fetch, so an entry with a value here is one
 * this call asked for.
 */
std::string
FinishAssistantComputeFetch(const AssistantToolContext& context,
                            const AssistantFetchState&  fetch)
{
    ComputeDataModel&   model    = context.data_provider->ComputeModel();
    const WorkloadInfo* workload = model.GetWorkload(fetch.workload_id);
    if(workload == nullptr)
    {
        return "That workload is no longer loaded.";
    }
    // A failed read leaves the store as empty as a read of metrics the trace
    // lacks, and the prompt tells the model to take an empty result as the
    // latter, so the two have to be told apart here.
    if(!context.data_provider->LastMetricsFetchSucceeded(
           DataProvider::ASSISTANT_CLIENT_ID))
    {
        return "The metric query failed, so nothing was read. That is an error reading "
               "the trace, not a sign it lacks these metrics - tell the user the read "
               "failed rather than drawing anything from the gap.";
    }

    const bool     workload_scope = fetch.kernel_id == ASSISTANT_COMPUTE_WORKLOAD_SCOPE;
    const uint64_t store_id       = DataProvider::ASSISTANT_CLIENT_ID;

    std::ostringstream out;
    out << "workload_id: " << workload->id << "\n";
    if(workload_scope)
    {
        out << "scope: workload\n";
    }
    else
    {
        const KernelInfo* kernel = model.GetKernelInfo(workload->id, fetch.kernel_id);
        out << "scope: kernel\n";
        out << "kernel_id: " << fetch.kernel_id << "\n";
        if(kernel != nullptr)
        {
            out << "kernel_name: " << kernel->name << "\n";
        }
    }

    size_t printed = 0;
    for(const AvailableMetrics::Category* category : workload->available_metrics
                                                         .ordered_categories)
    {
        if(category == nullptr)
        {
            continue;
        }
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            if(table == nullptr)
            {
                continue;
            }
            std::ostringstream table_out;
            bool               table_has_rows = false;

            for(const AvailableMetrics::Entry* entry : table->ordered_entries)
            {
                if(entry == nullptr)
                {
                    continue;
                }
                const std::shared_ptr<MetricValue> value =
                    workload_scope
                        ? model.GetWorkloadMetricValue(store_id, workload->id,
                                                       category->id, table->id, entry->id)
                        : model.GetKernelMetricValue(store_id, fetch.kernel_id,
                                                     category->id, table->id, entry->id);
                if(value == nullptr || value->values.empty())
                {
                    continue;
                }

                table_has_rows = true;
                ++printed;
                table_out << "    " << category->id << "." << table->id << "."
                          << entry->id << "  " << entry->name;
                if(!entry->unit.empty())
                {
                    table_out << " [" << entry->unit << "]";
                }
                table_out << ":";

                // Print in the table's own value-name order; MetricValue::values
                // is unordered, so iterating it directly would reorder the
                // columns from one answer to the next.
                bool any = false;
                for(const std::string& value_name : table->value_names)
                {
                    const std::unordered_map<std::string, double>::const_iterator found =
                        value->values.find(value_name);
                    if(found != value->values.end())
                    {
                        table_out << " " << value_name << "="
                                  << FormatMetricNumber(found->second);
                        any = true;
                    }
                }
                if(!any)
                {
                    for(const std::pair<const std::string, double>& pair : value->values)
                    {
                        table_out << " " << pair.first << "="
                                  << FormatMetricNumber(pair.second);
                    }
                }
                table_out << "\n";
            }

            if(table_has_rows)
            {
                out << "  " << category->name << " / " << table->name << "\n";
                out << table_out.str();
            }
        }
    }

    if(printed == 0)
    {
        return out.str() +
               "No values came back for those metric ids. The catalogue lists them, but "
               "this trace recorded no values for them at this scope - say so rather "
               "than retrying. Workload-scope values also need a newer compute schema "
               "than some traces carry.\n";
    }

    out << "metrics_returned: " << printed << "\n";
    return TrimComputeResult(out.str());
}

/*
 * The short compute description the model sees before it asks anything.
 *
 * Deliberately incomplete in the same way the system briefing is: it carries
 * the shape of the trace and what the user is looking at, but no kernel names
 * and no metric values, so answering from it alone is visibly guessing.
 */
std::string
BuildAssistantComputeBriefing(const AssistantToolContext& context)
{
    ComputeDataModel& model = context.data_provider->ComputeModel();

    std::ostringstream out;
    out << "trace_name: " << context.trace_name << "\n";
    out << "kind: compute_trace\n";
    out << "workload_count: " << model.GetWorkloadList().size() << "\n";

    // The same workload the tools fall back to with no workload_id, so the
    // briefing and the first answer describe one workload.
    jt::Json no_args;
    no_args.setObject();
    std::string         no_workload;
    const WorkloadInfo* workload = ResolveWorkload(context, no_args, no_workload);
    if(workload == nullptr)
    {
        out << "note: no workload is loaded yet.\n";
        return out.str();
    }

    out << "selected_workload_id: " << workload->id << "\n";
    out << "selected_workload_name: " << workload->name << "\n";
    out << "kernel_count: " << workload->ordered_kernels.size() << "\n";
    out << "metric_count: " << workload->available_metrics.list.size() << "\n";

    // The GPU decides which metrics exist at all, so it is worth the tokens
    // here even though compute_overview repeats it.
    if(workload->system_info.size() == 2 &&
       workload->system_info[0].size() == workload->system_info[1].size())
    {
        for(size_t i = 0; i < workload->system_info[0].size(); ++i)
        {
            const std::string key = Core::String::to_lower_copy(workload->system_info[0][i]);
            if(key.find("gpu") != std::string::npos ||
               key.find("arch") != std::string::npos ||
               key.find("cu") != std::string::npos)
            {
                out << "  " << workload->system_info[0][i] << ": "
                    << workload->system_info[1][i] << "\n";
            }
        }
    }

    if(context.compute_selection != nullptr)
    {
        const uint32_t kernel_id = context.compute_selection->GetSelectedKernel();
        if(kernel_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            const KernelInfo* kernel = model.GetKernelInfo(workload->id, kernel_id);
            out << "selected_kernel_id: " << kernel_id << "\n";
            if(kernel != nullptr)
            {
                out << "selected_kernel_name: " << kernel->name << "\n";
            }
        }
    }

    out << "note: kernel names, dispatch statistics, and hardware metric values are "
           "NOT in this briefing. Call compute_overview, list_kernels, kernel_triage, "
           "and get_metrics to see them.\n";
    return out.str();
}

}  // namespace View
}  // namespace RocProfVis
