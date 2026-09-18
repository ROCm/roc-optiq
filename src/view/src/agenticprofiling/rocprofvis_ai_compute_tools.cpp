// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that read a compute workload, and the formatters that turn what
// they read into text. Nothing here touches TraceDataModel: a compute trace has
// one, but it is empty, so reading it would answer confidently that the trace
// has no tracks and no events.
//
// Five of the six answer from memory. A compute trace loads its whole
// workload/kernel/metric-catalogue/roofline tree up front, so kernel names,
// dispatch statistics, units and descriptions cost nothing to read and need no
// request id. Only get_metrics queries, and it does so through the assistant's
// own client id - FetchMetrics keys both the request slot and the
// ComputeDataModel store off that, which is what lets the assistant read a
// metric table while the user is looking at one.
#include "rocprofvis_ai_tools_internal.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
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

// The request slot get_metrics owns. Derived from the assistant's client id, so
// it can never be the one a metric table is waiting on.
uint64_t
ComputeMetricsRequestId()
{
    return RequestIdBuilder::MakeClientRequestId(RequestType::kFetchMetrics,
                                                 DataProvider::ASSISTANT_CLIENT_ID);
}

/*
 * Which workload a tool is talking about.
 *
 * An explicit workload_id wins, then whichever one the toolbar has selected,
 * then the only one there is. Most traces carry a single workload, so falling
 * through to the first keeps every tool callable with no arguments at all.
 */
const WorkloadInfo*
ResolveWorkload(const AssistantToolContext& context, const jt::Json& args)
{
    ComputeDataModel& model = Model(context);

    const uint64_t requested = JsonU64(args, "workload_id", UINT64_MAX);
    if(requested != UINT64_MAX)
    {
        return model.GetWorkload(static_cast<uint32_t>(requested));
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
    return workloads.empty() ? nullptr : workloads.front();
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
 * Which kernel a tool is talking about, and whether it was given one at all.
 *
 * found_out separates "no kernel was named, use workload scope" from "a kernel
 * was named and does not exist", which the caller has to report differently.
 */
const KernelInfo*
ResolveKernel(const AssistantToolContext& context, const WorkloadInfo& workload,
              const jt::Json& args, bool& named_out, std::string& error_out)
{
    named_out = false;

    const uint64_t requested_id = JsonU64(args, "kernel_id", UINT64_MAX);
    if(requested_id != UINT64_MAX)
    {
        named_out = true;
        const KernelInfo* kernel =
            Model(context).GetKernelInfo(workload.id, static_cast<uint32_t>(requested_id));
        if(kernel == nullptr)
        {
            error_out = "No kernel with id " + std::to_string(requested_id) +
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

/*
 * Reads a dotted-id argument into the selectors FetchMetrics takes.
 *
 * Each is an id from list_metrics: "2" is a whole category, "2.1" a table,
 * "2.1.4" one entry. A name is refused rather than guessed at - the catalogue
 * is per-workload and its names are free-form, so matching one loosely here
 * would fetch a metric the model did not ask for.
 *
 * Shared by get_metrics and by list_metrics' describe mode, so the two accept
 * exactly the same ids and a model that has learned one has learned both.
 */
bool
ParseMetricSelectors(const jt::Json& args, const char* key,
                     std::vector<MetricsRequestParams::MetricID>& out,
                     std::string& error_out)
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

        const std::string text     = Core::String::trim_copy(entry.getString());
        uint32_t          parts[3] = { 0, 0, 0 };
        size_t            count    = 0;
        size_t            pos      = 0;
        bool              ok       = !text.empty();

        while(ok && pos < text.size() && count < 3)
        {
            size_t   digits = 0;
            uint64_t value  = 0;
            while(pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
            {
                value = value * 10 + static_cast<uint64_t>(text[pos] - '0');
                if(value > UINT32_MAX)
                {
                    ok = false;
                    break;
                }
                ++pos;
                ++digits;
            }
            if(!ok || digits == 0)
            {
                ok = false;
                break;
            }
            parts[count++] = static_cast<uint32_t>(value);
            if(pos < text.size())
            {
                if(text[pos] != '.')
                {
                    ok = false;
                    break;
                }
                ++pos;
            }
        }

        if(!ok || pos != text.size() || count == 0)
        {
            error_out = "\"" + text +
                        "\" is not a metric id. Use the dotted ids list_metrics "
                        "printed, such as \"2.1\" or \"2.1.4\".";
            return false;
        }

        MetricsRequestParams::MetricID selector{};
        selector.category_id = parts[0];
        if(count > 1)
        {
            selector.table_id = parts[1];
        }
        if(count > 2)
        {
            selector.entry_id = parts[2];
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

// --- Tools ---------------------------------------------------------------

AssistantToolStartResult
ToolComputeOverview(const AssistantToolContext& context, const jt::Json& args,
                    const std::string&)
{
    ComputeDataModel&                       model     = Model(context);
    const std::vector<const WorkloadInfo*>& workloads = model.GetWorkloadList();
    if(workloads.empty())
    {
        return DoneResult("This compute trace has no workloads in it.", "No workloads");
    }

    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
    }

    std::ostringstream out;
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
           "dispatch. Hardware metrics are not here - call list_metrics then "
           "get_metrics.\n";

    return DoneResult(TrimComputeResult(out.str()), "Read the workload overview");
}

AssistantToolStartResult
ToolListKernels(const AssistantToolContext& context, const jt::Json& args,
                const std::string&)
{
    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
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
    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
    }

    bool        named = false;
    std::string error;
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
    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
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
        std::string                                 error;
        if(!ParseMetricSelectors(args, "describe", selectors, error))
        {
            return DoneResult(error, "Bad describe");
        }

        size_t shown = 0;
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
            if(!wanted || shown >= ASSISTANT_COMPUTE_MAX_METRIC_ROWS)
            {
                continue;
            }
            ++shown;
            out << "  " << entry.category_id << "." << entry.table_id << "." << entry.id
                << "  " << entry.name;
            if(!entry.unit.empty())
            {
                out << "  [" << entry.unit << "]";
            }
            out << "\n";
            if(!entry.description.empty())
            {
                out << "    " << entry.description << "\n";
            }
        }
        if(shown == 0)
        {
            out << "  (no metric in this workload has any of those ids)\n";
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
            out << "  " << entry.category_id << "." << entry.table_id << "." << entry.id
                << "  " << entry.name;
            if(!entry.unit.empty())
            {
                out << "  [" << entry.unit << "]";
            }
            out << "\n";
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
    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
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

    bool        named = false;
    std::string error;
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

AssistantToolStartResult
ToolGetMetrics(const AssistantToolContext& context, const jt::Json& args,
               const std::string&)
{
    const WorkloadInfo* workload = ResolveWorkload(context, args);
    if(workload == nullptr)
    {
        return DoneResult("That workload id is not in this trace.", "Unknown workload");
    }

    std::vector<MetricsRequestParams::MetricID> selectors;
    std::string                                 error;
    if(!ParseMetricSelectors(args, "metrics", selectors, error))
    {
        return DoneResult(error, "Bad metrics");
    }

    bool              named  = false;
    const KernelInfo* kernel = ResolveKernel(context, *workload, args, named, error);
    if(kernel == nullptr && named)
    {
        return DoneResult(error, "No kernel");
    }

    // Our own slot, so a pending request here is a previous get_metrics rather
    // than a widget's. Wait it out and run again instead of formatting values
    // that answer the earlier call.
    const uint64_t request_id = ComputeMetricsRequestId();
    if(context.data_provider->IsRequestPending(request_id))
    {
        AssistantToolStartResult waiting;
        waiting.pending       = true;
        waiting.started_fetch = false;
        waiting.request_ids.push_back(request_id);
        waiting.fetch.kind  = AssistantFetchKind::kComputeMetrics;
        waiting.status_line = "Waiting for the previous metric fetch...";
        return waiting;
    }

    ComputeDataModel& model = Model(context);
    std::vector<uint32_t> kernel_ids;
    if(kernel != nullptr)
    {
        kernel_ids.push_back(kernel->id);
        // Clearing first is what lets the formatter print everything in the
        // store rather than having to carry the selector list through the wait:
        // whatever is there afterwards is what this call asked for.
        model.ClearKernelMetricValues(DataProvider::ASSISTANT_CLIENT_ID, kernel->id);
    }
    else
    {
        model.ClearWorkloadMetricValues(DataProvider::ASSISTANT_CLIENT_ID, workload->id);
    }

    if(!context.data_provider->FetchMetrics(
           MetricsRequestParams(workload->id, kernel_ids, selectors,
                                DataProvider::ASSISTANT_CLIENT_ID)))
    {
        return DoneResult("The metric fetch was refused, so nothing was read. The "
                          "trace may still be loading.",
                          "Fetch refused");
    }

    AssistantToolStartResult result;
    result.pending       = true;
    result.started_fetch = true;
    result.request_ids.push_back(request_id);
    result.fetch.kind        = AssistantFetchKind::kComputeMetrics;
    result.fetch.workload_id = workload->id;
    result.fetch.kernel_id =
        kernel != nullptr ? kernel->id : ASSISTANT_COMPUTE_WORKLOAD_SCOPE;
    result.status_line = "Reading metric values...";
    return result;
}

const AssistantToolEntry k_compute_tool_handlers[] = {
    { "compute_overview", ToolComputeOverview },
    { "list_kernels", ToolListKernels },
    { "kernel_summary", ToolKernelSummary },
    { "list_metrics", ToolListMetrics },
    { "kernel_roofline", ToolKernelRoofline },
    { "get_metrics", ToolGetMetrics },
};

}  // namespace

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
               "No values came back for those metric ids. Either this workload does "
               "not record them or the ids were wrong - call list_metrics to see what "
               "it has. Workload-scope values also need a newer compute schema than "
               "some traces carry.\n";
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
    ComputeDataModel&                       model     = context.data_provider->ComputeModel();
    const std::vector<const WorkloadInfo*>& workloads = model.GetWorkloadList();

    std::ostringstream out;
    out << "trace_name: " << context.trace_name << "\n";
    out << "kind: compute_trace\n";
    out << "workload_count: " << workloads.size() << "\n";

    const WorkloadInfo* workload = nullptr;
    if(context.compute_selection != nullptr)
    {
        const uint32_t selected = context.compute_selection->GetSelectedWorkload();
        if(selected != ComputeSelection::INVALID_SELECTION_ID)
        {
            workload = model.GetWorkload(selected);
        }
    }
    if(workload == nullptr && !workloads.empty())
    {
        workload = workloads.front();
    }
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
           "NOT in this briefing. Call compute_overview, list_kernels, list_metrics, "
           "and get_metrics to see them.\n";
    return out.str();
}

}  // namespace View
}  // namespace RocProfVis
