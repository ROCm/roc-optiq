// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_rocprof_sys_backend.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_json_utils.h"
#include "imgui.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace RocProfVis
{
namespace View
{

// ==================================================================================
// Static checkbox entry tables
// ==================================================================================

CheckboxEntry const kRocmDomains[] = {
    {"hip_runtime_api",        "HIP Runtime API",        true},
    {"marker_api",             "Marker API (ROCTX)",     true},
    {"kernel_dispatch",        "Kernel Dispatch",        true},
    {"memory_copy",            "Memory Copy",            true},
    {"scratch_memory",         "Scratch Memory",         true},
    {"memory_allocation",      "Memory Allocation",      false},
    {"hip_api",                "HIP API (all)",          false},
    {"hip_compiler_api",       "HIP Compiler API",       false},
    {"hip_compiler_api_ext",   "HIP Compiler API Ext",   false},
    {"hip_runtime_api_ext",    "HIP Runtime API Ext",    false},
    {"hip_stream",             "HIP Stream",             false},
    {"hsa_api",                "HSA API (all)",          false},
    {"hsa_core_api",           "HSA Core API",           false},
    {"hsa_amd_ext_api",        "HSA AMD Ext API",        false},
    {"hsa_image_ext_api",      "HSA Image Ext API",      false},
    {"hsa_finalize_ext_api",   "HSA Finalize Ext API",   false},
    {"marker_core_range_api",  "Marker Core Range API",  false},
    {"rccl_api",               "RCCL API",               false},
    {"ompt",                   "OpenMP (OMPT)",          false},
    {"rocdecode_api",          "ROCdecode API",          false},
    {"rocdecode_api_ext",      "ROCdecode API Ext",      false},
    {"rocjpeg_api",            "ROCjpeg API",            false},
    {"runtime_initialization", "Runtime Init",           false},
    {"kfd_page_migrate",       "KFD Page Migration",     false},
    {"kfd_page_fault",         "KFD Page Fault",         false},
    {"kfd_queue",              "KFD Queue",              false},
    {"kfd_event_page_migrate", "KFD Event Page Migrate", false},
    {"kfd_event_page_fault",   "KFD Event Page Fault",   false},
    {"kfd_event_queue",        "KFD Event Queue",        false},
    {"kfd_event_unmap_from_gpu", "KFD Event Unmap GPU",  false},
    {"kfd_event_dropped_events", "KFD Dropped Events",   false},
};
size_t const kRocmDomainsCount = sizeof(kRocmDomains) / sizeof(kRocmDomains[0]);

CheckboxEntry const kAmdSmiMetrics[] = {
    {"busy",          "GPU Busy %",    true},
    {"temp",          "Temperature",   true},
    {"power",         "Power",         true},
    {"mem_usage",     "Memory Usage",  true},
    {"vcn_activity",  "VCN Activity",  false},
    {"jpeg_activity", "JPEG Activity", false},
    {"xgmi",          "xGMI",          false},
    {"pcie",          "PCIe",          false},
    {"sdma_usage",    "SDMA Usage",    false},
};
size_t const kAmdSmiMetricsCount = sizeof(kAmdSmiMetrics) / sizeof(kAmdSmiMetrics[0]);

CheckboxEntry const kPerfettoCategories[] = {
    {"host",                    "Host",                    false},
    {"user",                    "User",                    false},
    {"python",                  "Python",                  false},
    {"rocm",                    "ROCm (all)",              false},
    {"rocm_hip_api",            "ROCm HIP API",            false},
    {"rocm_hsa_api",            "ROCm HSA API",            false},
    {"rocm_kernel_dispatch",    "ROCm Kernel Dispatch",    false},
    {"rocm_memory_copy",        "ROCm Memory Copy",        false},
    {"rocm_memory_allocate",    "ROCm Memory Allocate",    false},
    {"rocm_hip_stream",         "ROCm HIP Stream",         false},
    {"rocm_scratch_memory",     "ROCm Scratch Memory",     false},
    {"rocm_page_migration",     "ROCm Page Migration",     false},
    {"rocm_counter_collection", "ROCm Counter Collection", false},
    {"rocm_marker_api",         "ROCm Marker API",         false},
    {"rocm_rocdecode_api",      "ROCm ROCdecode API",      false},
    {"rocm_rocjpeg_api",        "ROCm ROCjpeg API",        false},
    {"rocm_rccl_api",           "ROCm RCCL API",           false},
    {"rocm_ompt_api",           "ROCm OMPT API",           false},
    {"amd_smi",                 "AMD SMI",                 false},
    {"rccl",                    "RCCL",                    false},
    {"pthread",                 "Pthreads",                false},
    {"kokkos",                  "Kokkos",                  false},
    {"mpi",                     "MPI",                     false},
    {"sampling",                "Sampling",                false},
    {"timer_sampling",          "Timer Sampling",          false},
    {"overflow_sampling",       "Overflow Sampling",       false},
    {"process_sampling",        "Process Sampling",        false},
    {"causal",                  "Causal",                  false},
    {"timemory",                "Timemory",                false},
};
size_t const kPerfettoCategoriesCount =
    sizeof(kPerfettoCategories) / sizeof(kPerfettoCategories[0]);

// Descriptions mirror the rocprof-sys preset JSONs shipped in
// share/rocprofiler-systems/presets/*.json (metadata.description / use_case).
// The authoritative text lives with the installed tool; these are bundled
// copies so users see guidance without a rocprof-sys install.
RocprofPresetEntry const kRocprofSysPresets[] = {
    {"balanced",          "Tracing + call-stack sampling for a solid overview at moderate overhead. Good default."},
    {"profile-only",      "Flat call-stack profile only. Lowest overhead, no timeline."},
    {"detailed",          "Comprehensive: tracing, sampling, and full system + GPU metrics. Highest overhead."},
    {"trace-gpu",         "GPU workload timeline: host + device activity and kernel dispatch."},
    {"workload-trace",    "AI/ML/GPU workloads, including RCCL collective communication."},
    {"trace-hw-counters", "Collect ROCm hardware counters (PMC) alongside tracing."},
    {"trace-hpc",         "HPC apps: MPI, OpenMP, Kokkos and RCCL with hardware counters."},
    {"trace-openmp",      "OpenMP offload workloads with HSA domains."},
    {"profile-mpi",       "MPI communication latency profiling."},
    {"sys-trace",         "System-level tracing of the whole application."},
    {"runtime-trace",     "ROCm/HIP runtime API tracing."},
};
size_t const kRocprofSysPresetsCount =
    sizeof(kRocprofSysPresets) / sizeof(kRocprofSysPresets[0]);

namespace
{

// ==================================================================================
// ImGui helpers
// ==================================================================================

void HelpMarker(char const* env_var, char const* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(env_var);
        ImGui::Separator();
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Shared control widths so combos/inputs across the advanced tabs are neither
// full-window-wide nor ragged.
constexpr float kFieldLabelW = 150.0f;  // label column width (aligns all inputs)
constexpr float kNumW        = 130.0f;  // numeric inputs
constexpr float kComboW      = 200.0f;  // dropdowns
constexpr float kTextW       = 260.0f;  // single-value text
constexpr float kListW       = 340.0f;  // comma-separated list text

// Checkbox lists lay out as a responsive grid clamped to this many columns.
constexpr float kCheckboxColWidth = 190.0f;
constexpr int   kCheckboxMaxCols  = 4;

// General tab: the preset row and the side-by-side OUTPUT FORMAT / TRACE WINDOW
// split (output takes a narrow fixed column so the trace window keeps Delay and
// Duration on one line).
constexpr float kPresetLabelW    = 110.0f;
constexpr float kPresetComboW    = 240.0f;
constexpr float kOutputColW      = 160.0f;
constexpr float kTraceInlineNumW = 85.0f;   // Delay / Duration inputs (share a row)
constexpr float kTraceFieldGapX  = 16.0f;   // gap between Delay and Duration
constexpr float kTraceRegionTrail = 26.0f;  // width reserved for the Region (?)

// Renders a label in a fixed-width column and sizes the next item so a whole
// column of controls lines up regardless of label length.
void FieldLabel(const char* label, float item_w, float label_w = kFieldLabelW)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(item_w);
}

// Returns true if any checkbox in the grid was toggled this frame.
bool RenderCheckboxMap(
    std::map<std::string, bool>& map,
    CheckboxEntry const* entries,
    size_t count,
    char const* id_prefix)
{
    bool changed = false;
    // Long checkbox lists read better as a responsive grid than one tall column.
    int cols = static_cast<int>(ImGui::GetContentRegionAvail().x / kCheckboxColWidth);
    cols     = std::clamp(cols, 1, kCheckboxMaxCols);
    if (count > 0 && static_cast<size_t>(cols) > count)
    {
        cols = static_cast<int>(count);
    }

    std::string table_id = std::string("cbgrid_") + id_prefix;
    if (ImGui::BeginTable(table_id.c_str(), cols, ImGuiTableFlags_None))
    {
        for (size_t i = 0; i < count; i++)
        {
            ImGui::TableNextColumn();

            bool val = false;
            auto it  = map.find(entries[i].id);
            if (it != map.end())
            {
                val = it->second;
            }
            std::string label =
                std::string(entries[i].display_name) + "##" + id_prefix + entries[i].id;
            if (ImGui::Checkbox(label.c_str(), &val))
            {
                map[entries[i].id] = val;
                changed            = true;
            }
        }
        ImGui::EndTable();
    }
    return changed;
}

bool AnyEnabled(std::map<std::string, bool> const& m)
{
    for (auto const& kv : m)
    {
        if (kv.second)
            return true;
    }
    return false;
}

const ImVec4 kPresetLockColor(1.0f, 0.8f, 0.0f, 1.0f);  // amber "locked by preset"

// Advanced tabs are read-only while a built-in preset is active. Draws the
// "controlled by preset" banner (when one is set) and opens a BeginDisabled()
// scope that the caller closes with ImGui::EndDisabled().
void BeginPresetLockedSection(std::string const& preset)
{
    const bool has_preset = !preset.empty();
    if (has_preset)
    {
        ImGui::TextColored(kPresetLockColor,
                           "Preset \"%s\" controls these settings.", preset.c_str());
        ImGui::TextDisabled("Clear the preset or use Raw Env Vars to override.");
        ImGui::Spacing();
    }
    ImGui::BeginDisabled(has_preset);
}

// ==================================================================================
// JSON helpers
// ==================================================================================

// Thin forwarders to the shared json_utils helpers, preserving the historic
// names/signatures used throughout the RocprofSysSettings (de)serialization.
bool safe_bool(jt::Json& j, char const* key, bool def)
{
    return JsonUtils::GetBool(j, key, def);
}

double safe_double(jt::Json& j, char const* key, double def)
{
    return JsonUtils::GetDouble(j, key, def);
}

int32_t safe_int(jt::Json& j, char const* key, int32_t def)
{
    return JsonUtils::GetInt(j, key, def);
}

std::string safe_string(jt::Json& j, char const* key, char const* def)
{
    return JsonUtils::GetString(j, key, def);
}

jt::Json map_to_json(std::map<std::string, bool> const& m)
{
    return JsonUtils::BoolMapToJson(m);
}

std::map<std::string, bool> json_to_map(
    jt::Json& j,
    char const* key,
    std::map<std::string, bool> const& defaults)
{
    return JsonUtils::JsonToBoolMap(j, key, defaults);
}

} // namespace

// ==================================================================================
// RocprofSysSettings helpers
// ==================================================================================

std::map<std::string, bool> RocprofSysSettings::BuildDefaultMap(
    CheckboxEntry const* entries, size_t count)
{
    std::map<std::string, bool> m;
    for (size_t i = 0; i < count; i++)
    {
        m[entries[i].id] = entries[i].default_on;
    }
    return m;
}

std::string RocprofSysBackend::JoinEnabledKeys(
    std::map<std::string, bool> const& m,
    std::string const& custom)
{
    std::string result;
    for (auto const& kv : m)
    {
        if (kv.second)
        {
            if (!result.empty())
                result += ",";
            result += kv.first;
        }
    }
    if (!custom.empty())
    {
        if (!result.empty())
            result += ",";
        result += custom;
    }
    return result;
}

// ==================================================================================
// RocprofSysSettings JSON round-trip
// ==================================================================================

RocprofSysSettings RocprofSysSettings::FromJson(jt::Json const& json)
{
    RocprofSysSettings s;
    jt::Json& j = const_cast<jt::Json&>(json);

    auto default_domains = BuildDefaultMap(kRocmDomains, kRocmDomainsCount);
    auto default_smi     = BuildDefaultMap(kAmdSmiMetrics, kAmdSmiMetricsCount);
    std::map<std::string, bool> empty_map;

    // Preset
    s.rocprof_preset = safe_string(j, "rocprof_preset", "");

    // General (ignore legacy "mode" field from old Optiq JSON presets)
    s.trace_delay    = safe_double(j, "trace_delay", 0.0);
    s.trace_duration = safe_double(j, "trace_duration", 0.0);
    s.trace_region   = safe_string(j, "trace_region", "");

    // Backends
    s.trace_backend        = safe_bool(j, "trace", true);
    s.profile              = safe_bool(j, "profile", false);
    s.flat_profile         = safe_bool(j, "flat_profile", false);
    s.use_rocpd            = safe_bool(j, "use_rocpd", true);
    s.use_sampling         = safe_bool(j, "use_sampling", false);
    s.use_process_sampling = safe_bool(j, "use_process_sampling", true);
    s.use_amd_smi          = safe_bool(j, "use_amd_smi", true);
    // Sampling
    s.sampling_freq            = safe_double(j, "sampling_freq", 300.0);
    s.sampling_cputime         = safe_bool(j, "sampling_cputime", false);
    s.sampling_realtime        = safe_bool(j, "sampling_realtime", false);
    s.sampling_overflow        = safe_bool(j, "sampling_overflow", false);
    s.sampling_duration        = safe_double(j, "sampling_duration", 0.0);
    s.sampling_allocator_size  = safe_int(j, "sampling_allocator_size", 8);
    s.sampling_overflow_event  = safe_string(j, "sampling_overflow_event",
                                             "perf::PERF_COUNT_HW_CACHE_REFERENCES");
    s.sampling_keep_internal   = safe_bool(j, "sampling_keep_internal", true);
    s.sampling_include_inlines = safe_bool(j, "sampling_include_inlines", false);

    // ROCm
    s.rocm_domains        = json_to_map(j, "rocm_domains", default_domains);
    s.rocm_domains_custom = safe_string(j, "rocm_domains_custom", "");
    s.rocm_events         = safe_string(j, "rocm_events", "");
    s.rocm_group_by_queue = safe_bool(j, "rocm_group_by_queue", false);

    // Perfetto
    s.perfetto_backend         = safe_string(j, "perfetto_backend", "inprocess");
    s.perfetto_buffer_size_kb  = safe_int(j, "perfetto_buffer_size_kb", 1024000);
    s.perfetto_flush_period_ms = safe_int(j, "perfetto_flush_period_ms", 10000);
    s.perfetto_fill_policy     = safe_string(j, "perfetto_fill_policy", "discard");
    s.perfetto_annotations     = safe_bool(j, "perfetto_annotations", true);
    s.perfetto_combine_traces  = safe_bool(j, "perfetto_combine_traces", false);
    s.enable_categories        = json_to_map(j, "enable_categories", empty_map);
    s.disable_categories       = json_to_map(j, "disable_categories", empty_map);
    s.perfetto_file            = safe_string(j, "perfetto_file", "perfetto-trace.proto");

    // Process Sampling
    s.cpu_freq_enabled     = safe_bool(j, "cpu_freq_enabled", false);
    s.amd_smi_metrics      = json_to_map(j, "amd_smi_metrics", default_smi);
    s.amd_smi_metrics_custom = safe_string(j, "amd_smi_metrics_custom", "");
    s.sampling_cpus        = safe_string(j, "sampling_cpus", "none");
    s.sampling_gpus        = safe_string(j, "sampling_gpus", "all");
    s.use_ainic            = safe_bool(j, "use_ainic", false);

    // Parallelism
    s.use_mpip    = safe_bool(j, "use_mpip", false);
    s.use_ucx     = safe_bool(j, "use_ucx", false);
    s.use_shmem   = safe_bool(j, "use_shmem", false);
    s.use_rcclp   = safe_bool(j, "use_rcclp", false);
    s.use_ompt    = safe_bool(j, "use_ompt", false);
    s.use_kokkosp = safe_bool(j, "use_kokkosp", false);

    // Advanced
    s.config_file         = safe_string(j, "config_file", "");
    s.log_level           = safe_string(j, "log_level", "info");
    s.log_file            = safe_string(j, "log_file", "rocprof-sys-log.txt");
    s.tmpdir              = safe_string(j, "tmpdir", "");
    s.use_pid             = safe_bool(j, "use_pid", true);

    // Instrument
    s.instr_include    = safe_string(j, "instr_include", "");
    s.instr_exclude    = safe_string(j, "instr_exclude", "");
    s.min_instructions = safe_int(j, "min_instructions", 0);

    return s;
}

jt::Json RocprofSysSettings::ToJson() const
{
    jt::Json p;

    // Preset
    if (!rocprof_preset.empty())
        p["rocprof_preset"] = rocprof_preset;

    // General
    p["trace_delay"]    = trace_delay;
    p["trace_duration"] = trace_duration;
    p["trace_region"]   = trace_region;

    // Backends
    p["trace"]                = trace_backend;
    p["profile"]              = profile;
    p["flat_profile"]         = flat_profile;
    p["use_rocpd"]            = use_rocpd;
    p["use_sampling"]         = use_sampling;
    p["use_process_sampling"] = use_process_sampling;
    p["use_amd_smi"]          = use_amd_smi;

    // Sampling
    p["sampling_freq"]            = sampling_freq;
    p["sampling_cputime"]         = sampling_cputime;
    p["sampling_realtime"]        = sampling_realtime;
    p["sampling_overflow"]        = sampling_overflow;
    p["sampling_duration"]        = sampling_duration;
    p["sampling_allocator_size"]  = static_cast<long>(sampling_allocator_size);
    p["sampling_overflow_event"]  = sampling_overflow_event;
    p["sampling_keep_internal"]   = sampling_keep_internal;
    p["sampling_include_inlines"] = sampling_include_inlines;

    // ROCm
    p["rocm_domains"]        = map_to_json(rocm_domains);
    p["rocm_domains_custom"] = rocm_domains_custom;
    p["rocm_events"]         = rocm_events;
    p["rocm_group_by_queue"] = rocm_group_by_queue;

    // Perfetto
    p["perfetto_backend"]         = perfetto_backend;
    p["perfetto_buffer_size_kb"]  = static_cast<long>(perfetto_buffer_size_kb);
    p["perfetto_flush_period_ms"] = static_cast<long>(perfetto_flush_period_ms);
    p["perfetto_fill_policy"]     = perfetto_fill_policy;
    p["perfetto_annotations"]     = perfetto_annotations;
    p["perfetto_combine_traces"]  = perfetto_combine_traces;
    p["enable_categories"]        = map_to_json(enable_categories);
    p["disable_categories"]       = map_to_json(disable_categories);
    p["perfetto_file"]            = perfetto_file;

    // Process Sampling
    p["cpu_freq_enabled"]      = cpu_freq_enabled;
    p["amd_smi_metrics"]       = map_to_json(amd_smi_metrics);
    p["amd_smi_metrics_custom"] = amd_smi_metrics_custom;
    p["sampling_cpus"]         = sampling_cpus;
    p["sampling_gpus"]         = sampling_gpus;
    p["use_ainic"]             = use_ainic;

    // Parallelism
    p["use_mpip"]    = use_mpip;
    p["use_ucx"]     = use_ucx;
    p["use_shmem"]   = use_shmem;
    p["use_rcclp"]   = use_rcclp;
    p["use_ompt"]    = use_ompt;
    p["use_kokkosp"] = use_kokkosp;

    // Advanced
    p["config_file"]         = config_file;
    p["log_level"]           = log_level;
    p["log_file"]            = log_file;
    p["tmpdir"]              = tmpdir;
    p["use_pid"]             = use_pid;

    // Instrument
    p["instr_include"]    = instr_include;
    p["instr_exclude"]    = instr_exclude;
    p["min_instructions"] = static_cast<long>(min_instructions);

    return p;
}

// ==================================================================================
// RocprofSysBackend core
// ==================================================================================

RocprofSysBackend::RocprofSysBackend()
{
}

const char* RocprofSysBackend::Id() const
{
    return "rocprof-sys";
}

const char* RocprofSysBackend::DisplayName() const
{
    return "ROCm Systems Profiler";
}

std::vector<ToolOption> RocprofSysBackend::GetTools() const
{
    return {
        {kRPVProfilerToolRocprofSysRun,        "Run"},
        {kRPVProfilerToolRocprofSysSample,     "Sample"},
        {kRPVProfilerToolRocprofSysInstrument, "Instrument"},
    };
}

std::vector<TabDescriptor> RocprofSysBackend::GetTabs(rocprofvis_profiler_tool_t tool) const
{
    std::vector<TabDescriptor> tabs;

    // General: the everyday controls (preset, output format, trace window).
    // Always visible in the launcher.
    tabs.push_back({"general", "General", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderBackendsTab();
    }, false});

    // Advanced: power-user detail, grouped by domain and tucked under the
    // collapsible "Advanced Options" section so the common case stays simple.
    tabs.push_back({"sampling", "Sampling", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderSamplingTab(); }, true});
    tabs.push_back({"rocm", "ROCm", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderRocmTab(); }, true});
    tabs.push_back({"perfetto", "Perfetto", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderPerfettoTab(); }, true});
    tabs.push_back({"process_sampling", "Process Sampling", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderProcessSamplingTab(); },
        true});
    tabs.push_back({"parallelism", "Parallelism", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderParallelismTab(); }, true});

    if (tool == kRPVProfilerToolRocprofSysInstrument)
    {
        tabs.push_back({"instrument", "Instrument", [this]() {
            return const_cast<RocprofSysBackend*>(this)->RenderInstrumentTab(); },
            true});
    }

    tabs.push_back({"advanced", "Config & Logging", [this]() {
        return const_cast<RocprofSysBackend*>(this)->RenderAdvancedTab(); }, true});

    return tabs;
}

// ==================================================================================
// Validation
// ==================================================================================

std::string RocprofSysBackend::Validate(LaunchConfig const& config) const
{
    if (config.target.executable.empty())
    {
        return "Target executable is required";
    }
    if (config.target.output_directory.empty())
    {
        return "Output directory is required";
    }

    // Hierarchical vs flat profile
    if (m_settings.profile && m_settings.flat_profile)
    {
        return "Cannot enable both hierarchical profile and flat profile";
    }

    // Enable + disable categories mutual exclusion
    if (AnyEnabled(m_settings.enable_categories) &&
        AnyEnabled(m_settings.disable_categories))
    {
        return "Cannot specify both enable and disable category lists "
               "(they are mutually exclusive)";
    }

    // Overflow event duplicate in extra_env
    auto it = config.extra_env.find("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT");
    if (it != config.extra_env.end() &&
        !it->second.empty() &&
        it->second != m_settings.sampling_overflow_event)
    {
        return "Raw env var ROCPROFSYS_SAMPLING_OVERFLOW_EVENT conflicts with the "
               "curated overflow event setting (would cause a runtime error)";
    }

    return "";
}

void RocprofSysBackend::LoadSettings(jt::Json const& payload)
{
    m_settings = RocprofSysSettings::FromJson(payload);
}

jt::Json RocprofSysBackend::SaveSettings() const
{
    return m_settings.ToJson();
}

// ==================================================================================
// Warnings & hints
// ==================================================================================

std::vector<WarningMessage> RocprofSysBackend::GetWarnings(
    LaunchConfig const& config) const
{
    std::vector<WarningMessage> warnings;

    // Trace + profile both on
    if (m_settings.trace_backend && (m_settings.profile || m_settings.flat_profile))
    {
        warnings.push_back({WarningMessage::kWarning,
            "Both trace and profile backends are enabled -- this increases overhead"});
    }

    // MPI + instrument tool
    if (m_settings.use_mpip && config.tool == kRPVProfilerToolRocprofSysInstrument)
    {
        warnings.push_back({WarningMessage::kWarning,
            "Runtime instrumentation is incompatible with MPI spawn. "
            "Consider binary rewrite mode or use the Sample tool"});
    }

    // Tool routing: run + sampling
    if (config.tool == kRPVProfilerToolRocprofSysRun && m_settings.use_sampling &&
        !m_settings.trace_backend)
    {
        warnings.push_back({WarningMessage::kInfo,
            "Consider using the Sample tool for uninstrumented sampling"});
    }

    // Deprecated env aliases in raw env vars
    auto perfetto_it = config.extra_env.find("ROCPROFSYS_USE_PERFETTO");
    if (perfetto_it != config.extra_env.end())
    {
        warnings.push_back({WarningMessage::kWarning,
            "ROCPROFSYS_USE_PERFETTO is deprecated -- use ROCPROFSYS_TRACE instead"});
    }
    auto timemory_it = config.extra_env.find("ROCPROFSYS_USE_TIMEMORY");
    if (timemory_it != config.extra_env.end())
    {
        warnings.push_back({WarningMessage::kWarning,
            "ROCPROFSYS_USE_TIMEMORY is deprecated -- use ROCPROFSYS_PROFILE instead"});
    }

    return warnings;
}

std::vector<std::string> RocprofSysBackend::GetSummaryTags(
    LaunchConfig const& config) const
{
    (void)config;
    std::vector<std::string> tags;

    // Output format (what the run will produce).
    std::string output;
    if (m_settings.trace_backend && m_settings.use_rocpd)
    {
        output = "Perfetto + ROCpd";
    }
    else if (m_settings.trace_backend)
    {
        output = "Perfetto trace";
    }
    else if (m_settings.use_rocpd)
    {
        output = "ROCpd database";
    }
    else
    {
        output = "No trace output";
    }
    tags.push_back(output);

    // Active preset (or Custom when none is selected).
    tags.push_back(m_settings.rocprof_preset.empty()
                       ? std::string("Custom")
                       : m_settings.rocprof_preset);

    return tags;
}

// ==================================================================================
// FlattenToExecution -- only emits non-default values
// ==================================================================================

void RocprofSysBackend::FlattenToExecution(
    LaunchConfig const& config,
    std::vector<std::pair<std::string, std::string>>& env_out,
    std::vector<std::string>& argv_out) const
{
    RocprofSysSettings defaults = RocprofSysSettings::FromJson(jt::Json());

    auto emit_bool = [&](char const* env_name, bool val, bool def)
    {
        if (val != def)
            env_out.emplace_back(env_name, val ? "true" : "false");
    };

    auto emit_string = [&](char const* env_name,
                           std::string const& val,
                           std::string const& def)
    {
        if (val != def && !val.empty())
            env_out.emplace_back(env_name, val);
    };

    auto emit_double = [&](char const* env_name, double val, double def)
    {
        if (val != def)
        {
            std::ostringstream oss;
            oss << val;
            env_out.emplace_back(env_name, oss.str());
        }
    };

    auto emit_int = [&](char const* env_name, int32_t val, int32_t def)
    {
        if (val != def)
            env_out.emplace_back(env_name, std::to_string(val));
    };

    // Emit --preset= in argv when a built-in preset is selected
    if (!m_settings.rocprof_preset.empty())
    {
        argv_out.push_back("--preset=" + m_settings.rocprof_preset);
    }

    // Output path (always emit if set)
    if (!config.target.output_directory.empty())
    {
        env_out.emplace_back("ROCPROFSYS_OUTPUT_PATH", config.target.output_directory);
    }

    // Legacy ROCPROFSYS_MODE is intentionally not emitted; the individual
    // backend toggles drive behavior instead.
    emit_double("ROCPROFSYS_TRACE_DELAY", m_settings.trace_delay, defaults.trace_delay);
    emit_double("ROCPROFSYS_TRACE_DURATION",
                m_settings.trace_duration, defaults.trace_duration);
    emit_string("ROCPROFSYS_TRACE_REGION",
                m_settings.trace_region, defaults.trace_region);

    // Output format — always emit so presets don't override user intent
    auto emit_bool_always = [&](char const* env_name, bool val)
    {
        env_out.emplace_back(env_name, val ? "true" : "false");
    };

    emit_bool_always("ROCPROFSYS_PROFILE", m_settings.profile);
    emit_bool_always("ROCPROFSYS_FLAT_PROFILE", m_settings.flat_profile);
    emit_bool_always("ROCPROFSYS_USE_ROCPD", m_settings.use_rocpd);

    // Perfetto tracing — emit as CLI arg so it can override --preset precedence
    bool has_preset = !m_settings.rocprof_preset.empty();
    if (has_preset || m_settings.trace_backend != defaults.trace_backend)
    {
        argv_out.push_back(
            std::string("--trace=") + (m_settings.trace_backend ? "true" : "false"));
    }

    // Collection — defer to preset when at default
    emit_bool("ROCPROFSYS_USE_SAMPLING", m_settings.use_sampling, defaults.use_sampling);
    emit_bool("ROCPROFSYS_USE_PROCESS_SAMPLING",
              m_settings.use_process_sampling, defaults.use_process_sampling);
    emit_bool("ROCPROFSYS_USE_AMD_SMI", m_settings.use_amd_smi, defaults.use_amd_smi);

    // Sampling
    emit_double("ROCPROFSYS_SAMPLING_FREQ",
                m_settings.sampling_freq, defaults.sampling_freq);
    emit_bool("ROCPROFSYS_SAMPLING_CPUTIME",
              m_settings.sampling_cputime, defaults.sampling_cputime);
    emit_bool("ROCPROFSYS_SAMPLING_REALTIME",
              m_settings.sampling_realtime, defaults.sampling_realtime);
    emit_bool("ROCPROFSYS_SAMPLING_OVERFLOW",
              m_settings.sampling_overflow, defaults.sampling_overflow);
    emit_double("ROCPROFSYS_SAMPLING_DURATION",
                m_settings.sampling_duration, defaults.sampling_duration);
    emit_int("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE",
             m_settings.sampling_allocator_size, defaults.sampling_allocator_size);
    emit_string("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT",
                m_settings.sampling_overflow_event, defaults.sampling_overflow_event);
    emit_bool("ROCPROFSYS_SAMPLING_KEEP_INTERNAL",
              m_settings.sampling_keep_internal, defaults.sampling_keep_internal);
    emit_bool("ROCPROFSYS_SAMPLING_INCLUDE_INLINES",
              m_settings.sampling_include_inlines, defaults.sampling_include_inlines);

    // ROCm domains
    std::string domains_str = JoinEnabledKeys(
        m_settings.rocm_domains, m_settings.rocm_domains_custom);
    std::string default_domains_str = JoinEnabledKeys(
        defaults.rocm_domains, defaults.rocm_domains_custom);
    if (domains_str != default_domains_str)
    {
        env_out.emplace_back("ROCPROFSYS_ROCM_DOMAINS", domains_str);
    }

    emit_string("ROCPROFSYS_ROCM_EVENTS", m_settings.rocm_events, defaults.rocm_events);
    emit_bool("ROCPROFSYS_ROCM_GROUP_BY_QUEUE",
              m_settings.rocm_group_by_queue, defaults.rocm_group_by_queue);

    // Perfetto
    emit_string("ROCPROFSYS_PERFETTO_BACKEND",
                m_settings.perfetto_backend, defaults.perfetto_backend);
    emit_int("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB",
             m_settings.perfetto_buffer_size_kb, defaults.perfetto_buffer_size_kb);
    emit_int("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS",
             m_settings.perfetto_flush_period_ms, defaults.perfetto_flush_period_ms);
    emit_string("ROCPROFSYS_PERFETTO_FILL_POLICY",
                m_settings.perfetto_fill_policy, defaults.perfetto_fill_policy);
    emit_bool("ROCPROFSYS_PERFETTO_ANNOTATIONS",
              m_settings.perfetto_annotations, defaults.perfetto_annotations);
    emit_bool("ROCPROFSYS_PERFETTO_COMBINE_TRACES",
              m_settings.perfetto_combine_traces, defaults.perfetto_combine_traces);

    std::string en_cat = JoinEnabledKeys(m_settings.enable_categories, "");
    if (!en_cat.empty())
        env_out.emplace_back("ROCPROFSYS_ENABLE_CATEGORIES", en_cat);

    std::string dis_cat = JoinEnabledKeys(m_settings.disable_categories, "");
    if (!dis_cat.empty())
        env_out.emplace_back("ROCPROFSYS_DISABLE_CATEGORIES", dis_cat);

    emit_string("ROCPROFSYS_PERFETTO_FILE",
                m_settings.perfetto_file, defaults.perfetto_file);

    // Process Sampling
    emit_bool("ROCPROFSYS_CPU_FREQ_ENABLED",
              m_settings.cpu_freq_enabled, defaults.cpu_freq_enabled);

    std::string metrics_str = JoinEnabledKeys(
        m_settings.amd_smi_metrics, m_settings.amd_smi_metrics_custom);
    std::string default_metrics_str = JoinEnabledKeys(
        defaults.amd_smi_metrics, defaults.amd_smi_metrics_custom);
    if (metrics_str != default_metrics_str)
    {
        env_out.emplace_back("ROCPROFSYS_AMD_SMI_METRICS", metrics_str);
    }

    emit_string("ROCPROFSYS_SAMPLING_CPUS",
                m_settings.sampling_cpus, defaults.sampling_cpus);
    emit_string("ROCPROFSYS_SAMPLING_GPUS",
                m_settings.sampling_gpus, defaults.sampling_gpus);
    emit_bool("ROCPROFSYS_USE_AINIC", m_settings.use_ainic, defaults.use_ainic);

    // Parallelism
    emit_bool("ROCPROFSYS_USE_MPIP", m_settings.use_mpip, defaults.use_mpip);
    emit_bool("ROCPROFSYS_USE_UCX", m_settings.use_ucx, defaults.use_ucx);
    emit_bool("ROCPROFSYS_USE_SHMEM", m_settings.use_shmem, defaults.use_shmem);
    emit_bool("ROCPROFSYS_USE_RCCLP", m_settings.use_rcclp, defaults.use_rcclp);
    emit_bool("ROCPROFSYS_USE_OMPT", m_settings.use_ompt, defaults.use_ompt);
    emit_bool("ROCPROFSYS_USE_KOKKOSP", m_settings.use_kokkosp, defaults.use_kokkosp);

    // Advanced
    emit_string("ROCPROFSYS_CONFIG_FILE",
                m_settings.config_file, defaults.config_file);
    emit_string("ROCPROFSYS_LOG_LEVEL", m_settings.log_level, defaults.log_level);
    emit_string("ROCPROFSYS_LOG_FILE", m_settings.log_file, defaults.log_file);
    emit_string("ROCPROFSYS_TMPDIR", m_settings.tmpdir, defaults.tmpdir);
    // MPI profiling (MPIP) is incompatible with per-process PID-suffixed output,
    // so PID suffixing is forced off whenever MPI is enabled.
    bool effective_use_pid = m_settings.use_pid && !m_settings.use_mpip;
    emit_bool("ROCPROFSYS_USE_PID", effective_use_pid, defaults.use_pid);

    // Instrument args
    if (config.tool == kRPVProfilerToolRocprofSysInstrument)
    {
        if (!m_settings.instr_include.empty())
        {
            argv_out.push_back("-I");
            argv_out.push_back(m_settings.instr_include);
        }
        if (!m_settings.instr_exclude.empty())
        {
            argv_out.push_back("-E");
            argv_out.push_back(m_settings.instr_exclude);
        }
        if (m_settings.min_instructions > 0)
        {
            argv_out.push_back("--min-instructions");
            argv_out.push_back(std::to_string(m_settings.min_instructions));
        }
    }

    // The user's raw escape hatch goes last among the profiler's own flags, so
    // it can override anything emitted above, but still ahead of the "--"
    // separator - past that point the tokens belong to the target, not to
    // rocprof-sys.
    for (auto const& arg : config.extra_argv)
    {
        argv_out.push_back(arg);
    }

    if (!config.target.output_directory.empty())
    {
        argv_out.push_back("--output");
        argv_out.push_back(config.target.output_directory);
    }

    // Everything after "--" is the command rocprof-sys should run.
    if (!config.target.executable.empty())
    {
        argv_out.push_back("--");
        argv_out.push_back(config.target.executable);

        for (auto const& arg : SplitArguments(config.target.arguments))
        {
            argv_out.push_back(arg);
        }
    }
}

// ==================================================================================
// ParseTraceOutputPath
// ==================================================================================

std::string RocprofSysBackend::ParseTraceOutputPath(std::string const& profiler_stdout) const
{
    // rocprof-sys reports the rocpd database path it produced in a couple of
    // places, e.g.:
    //   [...]database.cpp:151 database][info] Database: /path/rocpd-<pid>-0.db
    //   Output Summary box: "RocPD database" -> "File: /path/rocpd-<pid>-0.db"
    // We can't predict the exact filename (timestamp folder + PID suffix), so
    // we scrape the path the tool actually printed.
    //
    // Every candidate resolves to the LAST match in the stream, never the
    // first. What we are handed is the whole child process tree's stdout and
    // stderr, so the profiled application's own output is interleaved with the
    // tool's and can name an unrelated database - profiling Optiq with Optiq
    // logs "Opening file: <trace>.db" for whatever the user opens in the child.
    // rocprof-sys reports its path during finalization, after the target has
    // exited, so the tool's report is always the later one.
    //
    // Preference order, each resolved last-match: a path labelled "Database:",
    // then one labelled "File:" (the Output Summary box), then any ".db" token
    // for a tool that labels neither. The label has to appear before the path
    // on the line, which is what keeps a lower-case "Opening file:" from
    // passing itself off as the summary label.

    struct DbToken
    {
        std::string text;
        size_t      start = std::string::npos;
    };

    // Extracts the last whitespace/quote-delimited token ending in ".db" from a
    // single line, or an empty token if none. Trims surrounding quotes and
    // punctuation.
    auto extract_db_token = [](std::string const& line) -> DbToken
    {
        const std::string ext = ".db";
        DbToken best;
        size_t pos = 0;
        while ((pos = line.find(ext, pos)) != std::string::npos)
        {
            size_t end = pos + ext.size();
            // The match must be a real extension: end of line or followed by a
            // delimiter (not another path char like a letter, which would make
            // it e.g. ".dbx").
            if (end < line.size())
            {
                char next = line[end];
                bool is_delim = std::isspace(static_cast<unsigned char>(next)) ||
                                next == '\'' || next == '"' || next == ',' ||
                                next == ')' || next == ']';
                if (!is_delim)
                {
                    pos = end;
                    continue;
                }
            }

            // Walk backwards to the start of the token (first delimiter before
            // the path).
            size_t start = pos;
            while (start > 0)
            {
                char c = line[start - 1];
                if (std::isspace(static_cast<unsigned char>(c)) || c == '\'' ||
                    c == '"' || c == '`' || c == '|')
                {
                    break;
                }
                --start;
            }

            best.text  = line.substr(start, end - start);
            best.start = start;
            pos = end;
        }
        return best;
    };

    std::string labelled_database;
    std::string labelled_file;
    std::string unlabelled;
    std::istringstream stream(profiler_stdout);
    std::string line;
    while (std::getline(stream, line))
    {
        DbToken token = extract_db_token(line);
        if (token.text.empty())
        {
            continue;
        }

        if (line.rfind("Database:", token.start) != std::string::npos)
        {
            labelled_database = token.text;
        }
        else if (line.rfind("File:", token.start) != std::string::npos)
        {
            labelled_file = token.text;
        }
        unlabelled = token.text;
    }

    if (!labelled_database.empty())
    {
        return labelled_database;
    }
    if (!labelled_file.empty())
    {
        return labelled_file;
    }
    return unlabelled;
}

// ==================================================================================
// ExportCfg
// ==================================================================================

std::string RocprofSysBackend::ExportCfg() const
{
    std::ostringstream cfg;
    cfg << "# ROCm Systems Profiler configuration\n";
    cfg << "# Generated by ROCm Optiq\n\n";

    auto emit_bool = [&](char const* env_name, bool val)
    {
        cfg << env_name << " = " << (val ? "true" : "false") << "\n";
    };

    auto emit_string = [&](char const* env_name, std::string const& val)
    {
        if (!val.empty())
            cfg << env_name << " = " << val << "\n";
    };

    auto emit_double = [&](char const* env_name, double val)
    {
        cfg << env_name << " = " << val << "\n";
    };

    auto emit_int = [&](char const* env_name, int32_t val)
    {
        cfg << env_name << " = " << val << "\n";
    };

    emit_double("ROCPROFSYS_TRACE_DELAY", m_settings.trace_delay);
    emit_double("ROCPROFSYS_TRACE_DURATION", m_settings.trace_duration);
    emit_string("ROCPROFSYS_TRACE_REGION", m_settings.trace_region);
    emit_bool("ROCPROFSYS_TRACE", m_settings.trace_backend);
    emit_bool("ROCPROFSYS_PROFILE", m_settings.profile);
    emit_bool("ROCPROFSYS_FLAT_PROFILE", m_settings.flat_profile);
    emit_bool("ROCPROFSYS_USE_ROCPD", m_settings.use_rocpd);
    emit_bool("ROCPROFSYS_USE_SAMPLING", m_settings.use_sampling);
    emit_bool("ROCPROFSYS_USE_PROCESS_SAMPLING", m_settings.use_process_sampling);
    emit_bool("ROCPROFSYS_USE_AMD_SMI", m_settings.use_amd_smi);
    emit_double("ROCPROFSYS_SAMPLING_FREQ", m_settings.sampling_freq);
    emit_bool("ROCPROFSYS_SAMPLING_CPUTIME", m_settings.sampling_cputime);
    emit_bool("ROCPROFSYS_SAMPLING_REALTIME", m_settings.sampling_realtime);
    emit_bool("ROCPROFSYS_SAMPLING_OVERFLOW", m_settings.sampling_overflow);
    emit_double("ROCPROFSYS_SAMPLING_DURATION", m_settings.sampling_duration);
    emit_int("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE", m_settings.sampling_allocator_size);
    emit_string("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT", m_settings.sampling_overflow_event);
    emit_bool("ROCPROFSYS_SAMPLING_KEEP_INTERNAL", m_settings.sampling_keep_internal);
    emit_bool("ROCPROFSYS_SAMPLING_INCLUDE_INLINES", m_settings.sampling_include_inlines);
    emit_string("ROCPROFSYS_ROCM_DOMAINS",
                JoinEnabledKeys(m_settings.rocm_domains, m_settings.rocm_domains_custom));
    emit_string("ROCPROFSYS_ROCM_EVENTS", m_settings.rocm_events);
    emit_bool("ROCPROFSYS_ROCM_GROUP_BY_QUEUE", m_settings.rocm_group_by_queue);
    emit_string("ROCPROFSYS_PERFETTO_BACKEND", m_settings.perfetto_backend);
    emit_int("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB", m_settings.perfetto_buffer_size_kb);
    emit_int("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS", m_settings.perfetto_flush_period_ms);
    emit_string("ROCPROFSYS_PERFETTO_FILL_POLICY", m_settings.perfetto_fill_policy);
    emit_bool("ROCPROFSYS_PERFETTO_ANNOTATIONS", m_settings.perfetto_annotations);
    emit_bool("ROCPROFSYS_PERFETTO_COMBINE_TRACES", m_settings.perfetto_combine_traces);
    emit_string("ROCPROFSYS_ENABLE_CATEGORIES",
                JoinEnabledKeys(m_settings.enable_categories, ""));
    emit_string("ROCPROFSYS_DISABLE_CATEGORIES",
                JoinEnabledKeys(m_settings.disable_categories, ""));
    emit_string("ROCPROFSYS_PERFETTO_FILE", m_settings.perfetto_file);
    emit_bool("ROCPROFSYS_CPU_FREQ_ENABLED", m_settings.cpu_freq_enabled);
    emit_string("ROCPROFSYS_AMD_SMI_METRICS",
                JoinEnabledKeys(m_settings.amd_smi_metrics,
                                m_settings.amd_smi_metrics_custom));
    emit_string("ROCPROFSYS_SAMPLING_CPUS", m_settings.sampling_cpus);
    emit_string("ROCPROFSYS_SAMPLING_GPUS", m_settings.sampling_gpus);
    emit_bool("ROCPROFSYS_USE_AINIC", m_settings.use_ainic);
    emit_bool("ROCPROFSYS_USE_MPIP", m_settings.use_mpip);
    emit_bool("ROCPROFSYS_USE_UCX", m_settings.use_ucx);
    emit_bool("ROCPROFSYS_USE_SHMEM", m_settings.use_shmem);
    emit_bool("ROCPROFSYS_USE_RCCLP", m_settings.use_rcclp);
    emit_bool("ROCPROFSYS_USE_OMPT", m_settings.use_ompt);
    emit_bool("ROCPROFSYS_USE_KOKKOSP", m_settings.use_kokkosp);
    emit_string("ROCPROFSYS_LOG_LEVEL", m_settings.log_level);
    emit_string("ROCPROFSYS_LOG_FILE", m_settings.log_file);
    emit_string("ROCPROFSYS_TMPDIR", m_settings.tmpdir);
    emit_bool("ROCPROFSYS_USE_PID", m_settings.use_pid && !m_settings.use_mpip);

    return cfg.str();
}

// ==================================================================================
// Tab render functions
// ==================================================================================

bool RocprofSysBackend::RenderGeneralTraceOptions()
{
    bool changed = false;

    // Delay + Duration share a row; Region takes the next.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Delay (s)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kTraceInlineNumW);
    changed |=
        ImGui::InputDouble("##TraceDelay", &m_settings.trace_delay, 0.0, 0.0, "%.2f");

    ImGui::SameLine(0.0f, kTraceFieldGapX);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Duration (s)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kTraceInlineNumW);
    changed |= ImGui::InputDouble("##TraceDuration", &m_settings.trace_duration, 0.0,
                                  0.0, "%.2f");

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Region");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-kTraceRegionTrail);
    changed |= InputTextStringWithHint("##TraceRegion",
                                       "ROCTX regions (comma-separated)",
                                       m_settings.trace_region);
    HelpMarker("ROCPROFSYS_TRACE_REGION",
               "Comma-separated ROCTX region names for selective tracing");

    return changed;
}

bool RocprofSysBackend::RenderBackendsTab()
{
    bool changed = false;

    // Headline choice: the built-in rocprof-sys preset.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preset");
    ImGui::SameLine(kPresetLabelW);

    const char* preset_label =
        m_settings.rocprof_preset.empty() ? "Custom" : m_settings.rocprof_preset.c_str();

    ImGui::SetNextItemWidth(kPresetComboW);
    if (ImGui::BeginCombo("##RocprofPresetCombo", preset_label))
    {
        if (ImGui::Selectable("Custom", m_settings.rocprof_preset.empty()))
        {
            m_settings.rocprof_preset.clear();
            changed = true;
        }
        for (size_t i = 0; i < kRocprofSysPresetsCount; i++)
        {
            bool selected = (m_settings.rocprof_preset == kRocprofSysPresets[i].name);
            if (ImGui::Selectable(kRocprofSysPresets[i].name, selected))
            {
                m_settings.rocprof_preset = kRocprofSysPresets[i].name;
                changed                   = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", kRocprofSysPresets[i].description);
            }
        }
        ImGui::EndCombo();
    }
    HelpMarker("Profiling preset",
               "Pick a built-in rocprof-sys preset to configure collection defaults. "
               "When a preset is active, detailed settings are locked to preset values. "
               "Choose Custom to unlock every control.");

    bool has_preset = !m_settings.rocprof_preset.empty();

    // Show the active preset's description inline so its intent is visible.
    if (has_preset)
    {
        for (size_t i = 0; i < kRocprofSysPresetsCount; i++)
        {
            if (m_settings.rocprof_preset == kRocprofSysPresets[i].name)
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextWrapped("%s", kRocprofSysPresets[i].description);
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                break;
            }
        }
    }

    // Output format and the trace window sit side by side.
    if (ImGui::BeginTable("general_split", 2, ImGuiTableFlags_None))
    {
        ImGui::TableSetupColumn("##out", ImGuiTableColumnFlags_WidthFixed, kOutputColW);
        ImGui::TableSetupColumn("##trace", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        LaunchSubHeader("OUTPUT FORMAT");
        changed |= ToggleSwitch("Perfetto trace", &m_settings.trace_backend);
        HelpMarker("ROCPROFSYS_TRACE", "Enable the Perfetto trace backend");
        changed |= ToggleSwitch("ROCpd database", &m_settings.use_rocpd);
        HelpMarker("ROCPROFSYS_USE_ROCPD", "Enable ROCpd SQLite output");

        ImGui::TableNextColumn();
        ImGui::BeginDisabled(has_preset);
        LaunchSubHeader("TRACE WINDOW");
        changed |= RenderGeneralTraceOptions();
        ImGui::EndDisabled();

        ImGui::EndTable();
    }

    if (has_preset)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Preset \"%s\" controls the locked options above. "
                            "Choose Custom to edit them.",
                            m_settings.rocprof_preset.c_str());
    }

    return changed;
}

bool RocprofSysBackend::RenderSamplingTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    LaunchSubHeader("ENABLE");
    changed |= ToggleSwitch("Call-stack sampling", &m_settings.use_sampling);
    HelpMarker("ROCPROFSYS_USE_SAMPLING", "Enable call-stack sampling");
    changed |= ToggleSwitch("Process / system sampling",
                            &m_settings.use_process_sampling);
    HelpMarker("ROCPROFSYS_USE_PROCESS_SAMPLING",
               "Background process/system metrics (CPU freq, memory, GPU via SMI)");

    ImGui::Spacing();
    ImGui::Separator();

    FieldLabel("Frequency (Hz)", kNumW);
    changed |= ImGui::InputDouble("##SampFreq", &m_settings.sampling_freq, 10.0, 100.0,
                                  "%.0f");
    HelpMarker("ROCPROFSYS_SAMPLING_FREQ", "Software interrupts per second");

    ImGui::Separator();
    ImGui::Text("Timer Sources:");

    changed |= ImGui::Checkbox("CPU Time", &m_settings.sampling_cputime);
    HelpMarker("ROCPROFSYS_SAMPLING_CPUTIME",
               "Sample on CPU-time timer (ITIMER_PROF)");

    changed |= ImGui::Checkbox("Real Time", &m_settings.sampling_realtime);
    HelpMarker("ROCPROFSYS_SAMPLING_REALTIME",
               "Sample on real-time timer (ITIMER_REAL)");

    changed |= ImGui::Checkbox("Hardware Overflow", &m_settings.sampling_overflow);
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW",
               "Sample on hardware counter overflow");

    ImGui::Separator();

    FieldLabel("Duration (s)", kNumW);
    changed |= ImGui::InputDouble("##SampDur", &m_settings.sampling_duration, 0.0, 0.0,
                                  "%.2f");
    HelpMarker("ROCPROFSYS_SAMPLING_DURATION",
               "Stop sampling after N seconds (0 = unlimited)");

    int alloc_sz = m_settings.sampling_allocator_size;
    FieldLabel("Allocator Size", kNumW);
    if (ImGui::InputInt("##AllocSz", &alloc_sz))
    {
        m_settings.sampling_allocator_size = alloc_sz;
        changed                            = true;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE",
               "Threads per background allocator");

    FieldLabel("Overflow Event", kTextW);
    changed |= InputTextString("##OverflowEvt", m_settings.sampling_overflow_event);
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT",
               "Linux perf metric name for overflow sampling");

    ImGui::Separator();

    changed |=
        ImGui::Checkbox("Keep Internal Frames", &m_settings.sampling_keep_internal);
    HelpMarker("ROCPROFSYS_SAMPLING_KEEP_INTERNAL",
               "Show rocprof-sys frames in call stacks");

    changed |=
        ImGui::Checkbox("Include Inline Entries", &m_settings.sampling_include_inlines);
    HelpMarker("ROCPROFSYS_SAMPLING_INCLUDE_INLINES",
               "Include inline function entries in stacks");

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderRocmTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    ImGui::Text("ROCm Domains:");
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS",
               "ROCm SDK domains to trace (checked = enabled)");

    changed |= RenderCheckboxMap(m_settings.rocm_domains,
                                 kRocmDomains, kRocmDomainsCount, "rd_");

    ImGui::Spacing();
    FieldLabel("Custom Domains", kListW);
    changed |= InputTextString("##RocmDomainsCustom", m_settings.rocm_domains_custom);
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS",
               "Additional comma-separated domain names not in the list above");

    ImGui::Separator();

    FieldLabel("Hardware Counters", kListW);
    changed |= InputTextString("##RocmEvents", m_settings.rocm_events);
    HelpMarker("ROCPROFSYS_ROCM_EVENTS",
               "HW counters (use :device=N syntax for specific GPU)");

    changed |= ImGui::Checkbox("Group by Queue", &m_settings.rocm_group_by_queue);
    HelpMarker("ROCPROFSYS_ROCM_GROUP_BY_QUEUE",
               "Group by HSA queue instead of HIP stream");

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderPerfettoTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    const char* backends[] = {"inprocess", "system", "all"};
    int backend_idx = 0;
    for (int i = 0; i < IM_ARRAYSIZE(backends); i++)
    {
        if (m_settings.perfetto_backend == backends[i])
        {
            backend_idx = i;
            break;
        }
    }
    FieldLabel("Backend", kComboW);
    if (ImGui::Combo("##PerfBackend", &backend_idx, backends, IM_ARRAYSIZE(backends)))
    {
        m_settings.perfetto_backend = backends[backend_idx];
        changed                     = true;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BACKEND", "Perfetto tracing backend mode");

    int buf_kb = m_settings.perfetto_buffer_size_kb;
    FieldLabel("Buffer Size (KB)", kNumW);
    if (ImGui::InputInt("##BufKB", &buf_kb, 1024, 10240))
    {
        m_settings.perfetto_buffer_size_kb = buf_kb;
        changed                            = true;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB",
               "Perfetto shared memory buffer size");

    int flush_ms = m_settings.perfetto_flush_period_ms;
    FieldLabel("Flush Period (ms)", kNumW);
    if (ImGui::InputInt("##FlushMs", &flush_ms, 1000, 5000))
    {
        m_settings.perfetto_flush_period_ms = flush_ms;
        changed                             = true;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS",
               "Flush interval in milliseconds");

    const char* policies[] = {"discard", "fill"};
    int policy_idx = (m_settings.perfetto_fill_policy == "fill") ? 1 : 0;
    FieldLabel("Fill Policy", kComboW);
    if (ImGui::Combo("##FillPolicy", &policy_idx, policies, IM_ARRAYSIZE(policies)))
    {
        m_settings.perfetto_fill_policy = policies[policy_idx];
        changed                         = true;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILL_POLICY",
               "Buffer fill policy: discard old or stop writing");

    changed |= ImGui::Checkbox("Annotations", &m_settings.perfetto_annotations);
    HelpMarker("ROCPROFSYS_PERFETTO_ANNOTATIONS",
               "Function argument annotations (larger traces)");

    changed |= ImGui::Checkbox("Combine Traces", &m_settings.perfetto_combine_traces);
    HelpMarker("ROCPROFSYS_PERFETTO_COMBINE_TRACES",
               "Combine per-process traces into one file");

    ImGui::Separator();

    // Category enable/disable with mutual exclusion
    bool has_enable  = AnyEnabled(m_settings.enable_categories);
    bool has_disable = AnyEnabled(m_settings.disable_categories);

    ImGui::Text("Enable Categories (allowlist):");
    HelpMarker("ROCPROFSYS_ENABLE_CATEGORIES",
               "Perfetto categories to enable (mutually exclusive with disable list)");
    if (has_disable)
    {
        ImGui::BeginDisabled();
        ImGui::TextDisabled("(disabled -- clear disable list first)");
    }
    changed |= RenderCheckboxMap(m_settings.enable_categories,
                                 kPerfettoCategories, kPerfettoCategoriesCount,
                                 "encat_");
    if (has_disable)
        ImGui::EndDisabled();

    ImGui::Spacing();

    ImGui::Text("Disable Categories (denylist):");
    HelpMarker("ROCPROFSYS_DISABLE_CATEGORIES",
               "Perfetto categories to disable (mutually exclusive with enable list)");
    if (has_enable)
    {
        ImGui::BeginDisabled();
        ImGui::TextDisabled("(disabled -- clear enable list first)");
    }
    changed |= RenderCheckboxMap(m_settings.disable_categories,
                                 kPerfettoCategories, kPerfettoCategoriesCount,
                                 "discat_");
    if (has_enable)
        ImGui::EndDisabled();

    ImGui::Separator();

    FieldLabel("Output File", kTextW);
    changed |= InputTextString("##PerfFile", m_settings.perfetto_file);
    HelpMarker("ROCPROFSYS_PERFETTO_FILE", "Output filename for perfetto trace");

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderProcessSamplingTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    LaunchSubHeader("ENABLE");
    changed |= ToggleSwitch("AMD SMI GPU metrics", &m_settings.use_amd_smi);
    HelpMarker("ROCPROFSYS_USE_AMD_SMI", "GPU metrics via AMD SMI");

    ImGui::Spacing();
    ImGui::Separator();

    changed |= ImGui::Checkbox("CPU Frequency / Mem / Context Switches",
                               &m_settings.cpu_freq_enabled);
    HelpMarker("ROCPROFSYS_CPU_FREQ_ENABLED",
               "Enable CPU frequency, memory, and context switch sampling");

    ImGui::Separator();

    ImGui::Text("AMD SMI Metrics:");
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS",
               "GPU metrics to collect (checked = enabled)");

    changed |= RenderCheckboxMap(m_settings.amd_smi_metrics,
                                 kAmdSmiMetrics, kAmdSmiMetricsCount, "smi_");

    ImGui::Spacing();
    FieldLabel("Custom Metrics", kListW);
    changed |= InputTextString("##SmiMetricsCustom",
                               m_settings.amd_smi_metrics_custom);
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS",
               "Additional comma-separated metric names not in the list above");

    ImGui::Separator();

    FieldLabel("CPUs", kTextW);
    changed |= InputTextString("##SampCPUs", m_settings.sampling_cpus);
    HelpMarker("ROCPROFSYS_SAMPLING_CPUS",
               "CPU list for frequency sampling ('none', 'all', or index list)");

    FieldLabel("GPUs", kTextW);
    changed |= InputTextString("##SampGPUs", m_settings.sampling_gpus);
    HelpMarker("ROCPROFSYS_SAMPLING_GPUS",
               "AMD SMI device indices for GPU sampling");

    changed |= ImGui::Checkbox("AI NIC Metrics", &m_settings.use_ainic);
    HelpMarker("ROCPROFSYS_USE_AINIC", "Enable AI NIC metrics collection");

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderParallelismTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    auto toggle = [&changed](char const* label, bool& val, char const* env,
                             char const* help)
    {
        ImGui::TableNextColumn();
        changed |= ToggleSwitch(label, &val);
        HelpMarker(env, help);
    };

    if (ImGui::BeginTable("parallelism_grid", 2, ImGuiTableFlags_None))
    {
        toggle("MPI (MPIP)", m_settings.use_mpip,
               "ROCPROFSYS_USE_MPIP", "Profile MPI functions");
        toggle("UCX", m_settings.use_ucx,
               "ROCPROFSYS_USE_UCX", "UCX communication wrappers");
        toggle("OpenSHMEM", m_settings.use_shmem,
               "ROCPROFSYS_USE_SHMEM", "OpenSHMEM profiling");
        toggle("RCCL", m_settings.use_rcclp,
               "ROCPROFSYS_USE_RCCLP", "RCCL collective communication profiling");
        toggle("OpenMP (OMPT)", m_settings.use_ompt,
               "ROCPROFSYS_USE_OMPT", "OpenMP Tools interface");
        toggle("Kokkos", m_settings.use_kokkosp,
               "ROCPROFSYS_USE_KOKKOSP", "Kokkos Tools callback interface");
        ImGui::EndTable();
    }

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderInstrumentTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    ImGui::Text("Binary Instrumentation Options");
    ImGui::Separator();

    FieldLabel("Include Regex", kListW);
    changed |= InputTextString("##InstrInclude", m_settings.instr_include);
    HelpMarker("-I / --function-include",
               "Regex for functions to include in instrumentation");

    FieldLabel("Exclude Regex", kListW);
    changed |= InputTextString("##InstrExclude", m_settings.instr_exclude);
    HelpMarker("-E / --function-exclude",
               "Regex for functions to exclude from instrumentation");

    int min_instr = m_settings.min_instructions;
    FieldLabel("Min Instructions", kNumW);
    if (ImGui::InputInt("##MinInstr", &min_instr))
    {
        if (min_instr < 0) min_instr = 0;
        m_settings.min_instructions = min_instr;
        changed                     = true;
    }
    HelpMarker("--min-instructions",
               "Minimum instruction count for a function to be instrumented");

    ImGui::EndDisabled();

    return changed;
}

bool RocprofSysBackend::RenderAdvancedTab()
{
    bool changed = false;

    BeginPresetLockedSection(m_settings.rocprof_preset);

    LaunchSubHeader("SUMMARY PROFILE");
    int profile_mode = 0;
    if (m_settings.profile)      profile_mode = 1;
    if (m_settings.flat_profile) profile_mode = 2;

    if (ImGui::RadioButton("None##Profile", profile_mode == 0))
    {
        m_settings.profile      = false;
        m_settings.flat_profile = false;
        changed                 = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Hierarchical", profile_mode == 1))
    {
        m_settings.profile      = true;
        m_settings.flat_profile = false;
        changed                 = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Flat", profile_mode == 2))
    {
        m_settings.profile      = false;
        m_settings.flat_profile = true;
        changed                 = true;
    }
    ImGui::SameLine();
    HelpMarker("ROCPROFSYS_PROFILE / ROCPROFSYS_FLAT_PROFILE",
               "Timemory summary profile mode (hierarchical or flat call stacks)");

    ImGui::Spacing();
    ImGui::Separator();

    FieldLabel("Config File", kListW);
    changed |= InputTextString("##CfgFile", m_settings.config_file);
    HelpMarker("ROCPROFSYS_CONFIG_FILE",
               "Path to rocprof-sys configuration file (overrides individual settings)");
    ImGui::TextDisabled("For full Perfetto control, point to a config file.");

    ImGui::Separator();

    const char* levels[] = {"trace", "debug", "info", "warning", "error", "critical"};
    int level_idx = 2;  // default: "info"
    for (int i = 0; i < IM_ARRAYSIZE(levels); i++)
    {
        if (m_settings.log_level == levels[i])
        {
            level_idx = i;
            break;
        }
    }
    FieldLabel("Log Level", kComboW);
    if (ImGui::Combo("##LogLevel", &level_idx, levels, IM_ARRAYSIZE(levels)))
    {
        m_settings.log_level = levels[level_idx];
        changed              = true;
    }
    HelpMarker("ROCPROFSYS_LOG_LEVEL", "Logging verbosity level");

    FieldLabel("Log File", kTextW);
    changed |= InputTextString("##LogFile", m_settings.log_file);
    HelpMarker("ROCPROFSYS_LOG_FILE",
               "Log file name (empty disables file logging)");

    FieldLabel("Temp Directory", kTextW);
    changed |= InputTextString("##TmpDir", m_settings.tmpdir);
    HelpMarker("ROCPROFSYS_TMPDIR",
               "Base directory for temporary/spill files");

    // MPI profiling (MPIP) forces PID suffixing off. Show the toggle off and
    // disabled while MPI is enabled, but keep the user's underlying preference
    // so it returns when MPI is turned back off. FlattenToExecution enforces the
    // effective value regardless of this view.
    const bool pid_locked_by_mpi = m_settings.use_mpip;
    bool       pid_display       = m_settings.use_pid && !pid_locked_by_mpi;
    ImGui::BeginDisabled(pid_locked_by_mpi);
    if (ImGui::Checkbox("Append process ID to output files", &pid_display))
    {
        m_settings.use_pid = pid_display;
        changed            = true;
    }
    ImGui::EndDisabled();
    HelpMarker("ROCPROFSYS_USE_PID", "Suffix output filenames with the process ID");
    if (pid_locked_by_mpi)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(off: forced by MPI profiling)");
    }

    ImGui::EndDisabled();

    return changed;
}

} // namespace View
} // namespace RocProfVis
