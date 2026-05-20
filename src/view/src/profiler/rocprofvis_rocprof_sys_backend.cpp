// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_rocprof_sys_backend.h"
#include "rocprofvis_gui_helpers.h"
#include "imgui.h"
#include <sstream>

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

void WarningText(char const* text)
{
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "(!) %s", text);
}

int StringResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        std::string* str = static_cast<std::string*>(data->UserData);
        str->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = str->data();
    }
    return 0;
}

bool InputTextString(char const* label, std::string& str,
                     ImGuiInputTextFlags flags = 0)
{
    str.reserve(std::max(str.size() + 1, static_cast<size_t>(256)));
    return ImGui::InputText(
        label, str.data(), str.capacity() + 1,
        flags | ImGuiInputTextFlags_CallbackResize,
        StringResizeCallback, static_cast<void*>(&str));
}

void RenderCheckboxMap(
    std::map<std::string, bool>& map,
    CheckboxEntry const* entries,
    size_t count,
    char const* id_prefix)
{
    for (size_t i = 0; i < count; i++)
    {
        bool val = false;
        auto it = map.find(entries[i].id);
        if (it != map.end())
        {
            val = it->second;
        }
        std::string label =
            std::string(entries[i].display_name) + "##" + id_prefix + entries[i].id;
        if (ImGui::Checkbox(label.c_str(), &val))
        {
            map[entries[i].id] = val;
        }
    }
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

// ==================================================================================
// JSON helpers
// ==================================================================================

bool safe_bool(jt::Json& j, char const* key, bool def)
{
    if (!j.contains(key))
        return def;
    auto& v = j[key];
    switch (v.getType())
    {
        case jt::Json::Bool:   return v.getBool();
        case jt::Json::String: return v.getString() == "true";
        case jt::Json::Long:   return v.getLong() != 0;
        default:               return def;
    }
}

double safe_double(jt::Json& j, char const* key, double def)
{
    if (!j.contains(key))
        return def;
    auto& v = j[key];
    switch (v.getType())
    {
        case jt::Json::Double: return v.getDouble();
        case jt::Json::Long:   return static_cast<double>(v.getLong());
        case jt::Json::Float:  return static_cast<double>(v.getFloat());
        default:               return def;
    }
}

int32_t safe_int(jt::Json& j, char const* key, int32_t def)
{
    if (!j.contains(key))
        return def;
    auto& v = j[key];
    switch (v.getType())
    {
        case jt::Json::Long:   return static_cast<int32_t>(v.getLong());
        case jt::Json::Double: return static_cast<int32_t>(v.getDouble());
        case jt::Json::Float:  return static_cast<int32_t>(v.getFloat());
        default:               return def;
    }
}

std::string safe_string(jt::Json& j, char const* key, char const* def)
{
    if (!j.contains(key))
        return def;
    auto& v = j[key];
    if (v.getType() == jt::Json::String)
        return v.getString();
    return def;
}

jt::Json map_to_json(std::map<std::string, bool> const& m)
{
    jt::Json obj;
    for (auto const& kv : m)
    {
        obj[kv.first] = kv.second;
    }
    return obj;
}

std::map<std::string, bool> json_to_map(
    jt::Json& j,
    char const* key,
    std::map<std::string, bool> const& defaults)
{
    if (!j.contains(key))
        return defaults;
    auto& v = j[key];
    if (v.getType() != jt::Json::Object)
        return defaults;

    std::map<std::string, bool> result;
    auto& obj = v.getObject();
    for (auto const& kv : obj)
    {
        if (kv.second.getType() == jt::Json::Bool)
            result[kv.first] = kv.second.getBool();
        else
            result[kv.first] = false;
    }
    return result;
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

    // General
    s.mode           = safe_string(j, "mode", "trace");
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
    s.use_causal           = safe_bool(j, "use_causal", false);

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
    s.timemory_components = safe_string(j, "timemory_components", "wall_clock");

    // Instrument
    s.instr_include    = safe_string(j, "instr_include", "");
    s.instr_exclude    = safe_string(j, "instr_exclude", "");
    s.min_instructions = safe_int(j, "min_instructions", 0);

    return s;
}

jt::Json RocprofSysSettings::ToJson() const
{
    jt::Json p;

    // General
    p["mode"]           = mode;
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
    p["use_causal"]           = use_causal;

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
    p["timemory_components"] = timemory_components;

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
        {"run",        "Run (LD_PRELOAD)"},
        {"sample",     "Sample"},
        {"instrument", "Instrument (Dyninst)"},
        {"causal",     "Causal"},
    };
}

std::string RocprofSysBackend::GetDefaultBinary(std::string const& tool_id) const
{
    if (tool_id == "run")
        return "rocprof-sys-run";
    if (tool_id == "sample")
        return "rocprof-sys-sample";
    if (tool_id == "instrument")
        return "rocprof-sys-instrument";
    if (tool_id == "causal")
        return "rocprof-sys-causal";
    return "rocprof-sys-run";
}

std::vector<TabDescriptor> RocprofSysBackend::GetTabs(std::string const& tool_id) const
{
    std::vector<TabDescriptor> tabs;

    tabs.push_back({"general", "General", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderGeneralTab(); }});
    tabs.push_back({"backends", "Backends", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderBackendsTab(); }});
    tabs.push_back({"sampling", "Sampling", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderSamplingTab(); }});
    tabs.push_back({"rocm", "ROCm", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderRocmTab(); }});
    tabs.push_back({"perfetto", "Perfetto", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderPerfettoTab(); }});
    tabs.push_back({"process_sampling", "Process Sampling", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderProcessSamplingTab(); }});
    tabs.push_back({"parallelism", "Parallelism", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderParallelismTab(); }});

    if (tool_id == "instrument")
    {
        tabs.push_back({"instrument", "Instrument", [this]() {
            const_cast<RocprofSysBackend*>(this)->RenderInstrumentTab(); }});
    }

    tabs.push_back({"advanced", "Advanced", [this]() {
        const_cast<RocprofSysBackend*>(this)->RenderAdvancedTab(); }});

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

    // Causal mode conflicts
    if (m_settings.mode == "causal")
    {
        if (m_settings.trace_backend)
            return "Causal mode is incompatible with Perfetto tracing";
        if (m_settings.profile || m_settings.flat_profile)
            return "Causal mode is incompatible with timemory profiling";
        if (m_settings.use_sampling)
            return "Causal mode is incompatible with statistical sampling";
    }

    // Coverage mode conflicts
    if (m_settings.mode == "coverage")
    {
        if (m_settings.trace_backend)
            return "Coverage mode is incompatible with Perfetto tracing";
        if (m_settings.profile || m_settings.flat_profile)
            return "Coverage mode is incompatible with timemory profiling";
        if (m_settings.use_sampling)
            return "Coverage mode is incompatible with statistical sampling";
        if (m_settings.use_causal)
            return "Coverage mode is incompatible with causal profiling";
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
// Mode defaults
// ==================================================================================

void RocprofSysBackend::ApplyModeDefaults(std::string const& new_mode)
{
    if (new_mode == "trace")
    {
        m_settings.trace_backend = true;
        m_settings.profile       = false;
        m_settings.flat_profile  = false;
        m_settings.use_causal    = false;
    }
    else if (new_mode == "sampling")
    {
        m_settings.trace_backend = false;
        m_settings.profile       = false;
        m_settings.flat_profile  = false;
        m_settings.use_sampling  = true;
        m_settings.use_causal    = false;
    }
    else if (new_mode == "causal")
    {
        m_settings.trace_backend      = false;
        m_settings.profile            = false;
        m_settings.flat_profile       = false;
        m_settings.use_sampling       = false;
        m_settings.use_process_sampling = false;
        m_settings.use_causal         = true;
    }
    else if (new_mode == "coverage")
    {
        m_settings.trace_backend        = false;
        m_settings.profile              = false;
        m_settings.flat_profile         = false;
        m_settings.use_sampling         = false;
        m_settings.use_process_sampling = false;
        m_settings.use_causal           = false;
        m_settings.use_mpip             = false;
        m_settings.use_ucx              = false;
        m_settings.use_shmem            = false;
        m_settings.use_rcclp            = false;
        m_settings.use_ompt             = false;
        m_settings.use_kokkosp          = false;
        m_settings.use_amd_smi          = false;
    }
}

bool RocprofSysBackend::IsCausalOrCoverage() const
{
    return m_settings.mode == "causal" || m_settings.mode == "coverage";
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
    if (m_settings.use_mpip && config.tool_id == "instrument")
    {
        warnings.push_back({WarningMessage::kWarning,
            "Runtime instrumentation is incompatible with MPI spawn. "
            "Consider binary rewrite mode or use the Sample tool"});
    }

    // Tool routing: run + sampling mode
    if (config.tool_id == "run" &&
        m_settings.mode == "sampling" &&
        m_settings.use_sampling)
    {
        warnings.push_back({WarningMessage::kInfo,
            "Consider using the Sample tool for uninstrumented sampling"});
    }

    // Tool routing: run + causal mode
    if (config.tool_id == "run" && m_settings.mode == "causal")
    {
        warnings.push_back({WarningMessage::kWarning,
            "Causal CLI options are not available via rocprof-sys-run. "
            "Switch to the Causal tool"});
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

    // Output path (always emit if set)
    if (!config.target.output_directory.empty())
    {
        env_out.emplace_back("ROCPROFSYS_OUTPUT_PATH", config.target.output_directory);
    }

    // General
    emit_string("ROCPROFSYS_MODE", m_settings.mode, defaults.mode);
    emit_double("ROCPROFSYS_TRACE_DELAY", m_settings.trace_delay, defaults.trace_delay);
    emit_double("ROCPROFSYS_TRACE_DURATION",
                m_settings.trace_duration, defaults.trace_duration);
    emit_string("ROCPROFSYS_TRACE_REGION",
                m_settings.trace_region, defaults.trace_region);

    // Backends
    emit_bool("ROCPROFSYS_TRACE", m_settings.trace_backend, defaults.trace_backend);
    emit_bool("ROCPROFSYS_PROFILE", m_settings.profile, defaults.profile);
    emit_bool("ROCPROFSYS_FLAT_PROFILE", m_settings.flat_profile, defaults.flat_profile);
    emit_bool("ROCPROFSYS_USE_ROCPD", m_settings.use_rocpd, defaults.use_rocpd);
    emit_bool("ROCPROFSYS_USE_SAMPLING", m_settings.use_sampling, defaults.use_sampling);
    emit_bool("ROCPROFSYS_USE_PROCESS_SAMPLING",
              m_settings.use_process_sampling, defaults.use_process_sampling);
    emit_bool("ROCPROFSYS_USE_AMD_SMI", m_settings.use_amd_smi, defaults.use_amd_smi);
    emit_bool("ROCPROFSYS_USE_CAUSAL", m_settings.use_causal, defaults.use_causal);

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
    emit_bool("ROCPROFSYS_USE_PID", m_settings.use_pid, defaults.use_pid);
    emit_string("ROCPROFSYS_TIMEMORY_COMPONENTS",
                m_settings.timemory_components, defaults.timemory_components);

    // Instrument args
    if (config.tool_id == "instrument")
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

    emit_string("ROCPROFSYS_MODE", m_settings.mode);
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
    emit_bool("ROCPROFSYS_USE_CAUSAL", m_settings.use_causal);
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
    emit_bool("ROCPROFSYS_USE_PID", m_settings.use_pid);
    emit_string("ROCPROFSYS_TIMEMORY_COMPONENTS", m_settings.timemory_components);

    return cfg.str();
}

// ==================================================================================
// Tab render functions
// ==================================================================================

void RocprofSysBackend::RenderGeneralTab()
{
    const char* modes[] = {"trace", "sampling", "causal", "coverage"};
    int mode_idx = 0;
    for (int i = 0; i < 4; i++)
    {
        if (m_settings.mode == modes[i])
        {
            mode_idx = i;
            break;
        }
    }
    ImGui::Text("Mode:");
    ImGui::SameLine();
    if (ImGui::Combo("##Mode", &mode_idx, modes, 4))
    {
        std::string new_mode = modes[mode_idx];
        if (new_mode != m_settings.mode)
        {
            m_settings.mode = new_mode;
            ApplyModeDefaults(new_mode);
        }
    }
    HelpMarker("ROCPROFSYS_MODE", "Profiling mode: trace, sampling, causal, or coverage");

    if (m_settings.mode == "causal")
    {
        WarningText("Causal mode disables trace, profile, and sampling backends");
    }
    else if (m_settings.mode == "coverage")
    {
        WarningText("Coverage mode disables most collection backends");
    }

    ImGui::Text("Trace Delay (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##TraceDelay", &m_settings.trace_delay, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_TRACE_DELAY",
               "Seconds before collection starts (0 = immediate)");

    ImGui::Text("Trace Duration (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##TraceDuration", &m_settings.trace_duration, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_TRACE_DURATION",
               "Duration of collection in seconds (0 = unlimited)");

    ImGui::Text("Trace Region:");
    ImGui::SameLine();
    InputTextString("##TraceRegion", m_settings.trace_region);
    HelpMarker("ROCPROFSYS_TRACE_REGION",
               "Comma-separated ROCTX region names for selective tracing");
}

void RocprofSysBackend::RenderBackendsTab()
{
    bool locked = IsCausalOrCoverage();

    auto toggle = [&](char const* label, bool& val, char const* env,
                      char const* help, bool force_disabled = false)
    {
        if (force_disabled)
            ImGui::BeginDisabled();
        ImGui::Checkbox(label, &val);
        if (force_disabled)
            ImGui::EndDisabled();
        HelpMarker(env, help);
    };

    toggle("Perfetto Tracing", m_settings.trace_backend,
           "ROCPROFSYS_TRACE", "Enable Perfetto trace backend", locked);
    toggle("ROCpd Database", m_settings.use_rocpd,
           "ROCPROFSYS_USE_ROCPD", "Enable ROCpd SQLite output");

    // Profile type: radio group (Off / Hierarchical / Flat)
    ImGui::Separator();
    ImGui::Text("Timemory Profile:");
    HelpMarker("ROCPROFSYS_PROFILE / ROCPROFSYS_FLAT_PROFILE",
               "Profiling backend mode (hierarchical or flat)");

    int profile_mode = 0;
    if (m_settings.profile)       profile_mode = 1;
    if (m_settings.flat_profile)  profile_mode = 2;

    if (locked)
        ImGui::BeginDisabled();
    if (ImGui::RadioButton("Off##Profile", profile_mode == 0))
    {
        m_settings.profile      = false;
        m_settings.flat_profile = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Hierarchical", profile_mode == 1))
    {
        m_settings.profile      = true;
        m_settings.flat_profile = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Flat", profile_mode == 2))
    {
        m_settings.profile      = false;
        m_settings.flat_profile = true;
    }
    if (locked)
        ImGui::EndDisabled();

    ImGui::Separator();

    toggle("Statistical Sampling", m_settings.use_sampling,
           "ROCPROFSYS_USE_SAMPLING", "Enable call-stack sampling",
           locked);
    toggle("Process Sampling", m_settings.use_process_sampling,
           "ROCPROFSYS_USE_PROCESS_SAMPLING", "Background process/system metrics",
           m_settings.mode == "coverage");
    toggle("AMD SMI", m_settings.use_amd_smi,
           "ROCPROFSYS_USE_AMD_SMI", "GPU metrics via AMD SMI",
           m_settings.mode == "coverage");
    toggle("Causal Profiling", m_settings.use_causal,
           "ROCPROFSYS_USE_CAUSAL", "Enable causal profiling mode",
           m_settings.mode == "coverage");

    // Soft warning: trace + profile both on
    if (m_settings.trace_backend &&
        (m_settings.profile || m_settings.flat_profile))
    {
        ImGui::Spacing();
        WarningText("Both trace and profile backends are enabled -- "
                    "this increases overhead");
    }
}

void RocprofSysBackend::RenderSamplingTab()
{
    bool locked = IsCausalOrCoverage();
    if (locked)
        ImGui::BeginDisabled();

    ImGui::Text("Sampling Frequency (Hz):");
    ImGui::SameLine();
    ImGui::InputDouble("##SampFreq", &m_settings.sampling_freq, 10.0, 100.0, "%.0f");
    HelpMarker("ROCPROFSYS_SAMPLING_FREQ", "Software interrupts per second");

    ImGui::Separator();
    ImGui::Text("Timer Sources:");

    ImGui::Checkbox("CPU Time", &m_settings.sampling_cputime);
    HelpMarker("ROCPROFSYS_SAMPLING_CPUTIME",
               "Sample on CPU-time timer (ITIMER_PROF)");

    ImGui::Checkbox("Real Time", &m_settings.sampling_realtime);
    HelpMarker("ROCPROFSYS_SAMPLING_REALTIME",
               "Sample on real-time timer (ITIMER_REAL)");

    ImGui::Checkbox("Hardware Overflow", &m_settings.sampling_overflow);
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW",
               "Sample on hardware counter overflow");

    ImGui::Separator();

    ImGui::Text("Duration (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##SampDur", &m_settings.sampling_duration, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_SAMPLING_DURATION",
               "Stop sampling after N seconds (0 = unlimited)");

    int alloc_sz = m_settings.sampling_allocator_size;
    ImGui::Text("Allocator Size:");
    ImGui::SameLine();
    if (ImGui::InputInt("##AllocSz", &alloc_sz))
    {
        m_settings.sampling_allocator_size = alloc_sz;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE",
               "Threads per background allocator");

    ImGui::Text("Overflow Event:");
    ImGui::SameLine();
    InputTextString("##OverflowEvt", m_settings.sampling_overflow_event);
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT",
               "Linux perf metric name for overflow sampling");

    ImGui::Separator();

    ImGui::Checkbox("Keep Internal Frames", &m_settings.sampling_keep_internal);
    HelpMarker("ROCPROFSYS_SAMPLING_KEEP_INTERNAL",
               "Show rocprof-sys frames in call stacks");

    ImGui::Checkbox("Include Inline Entries", &m_settings.sampling_include_inlines);
    HelpMarker("ROCPROFSYS_SAMPLING_INCLUDE_INLINES",
               "Include inline function entries in stacks");

    if (locked)
        ImGui::EndDisabled();
}

void RocprofSysBackend::RenderRocmTab()
{
    ImGui::Text("ROCm Domains:");
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS",
               "ROCm SDK domains to trace (checked = enabled)");

    RenderCheckboxMap(m_settings.rocm_domains,
                      kRocmDomains, kRocmDomainsCount, "rd_");

    ImGui::Spacing();
    ImGui::Text("Custom Domains:");
    ImGui::SameLine();
    InputTextString("##RocmDomainsCustom", m_settings.rocm_domains_custom);
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS",
               "Additional comma-separated domain names not in the list above");

    ImGui::Separator();

    ImGui::Text("Hardware Counters:");
    InputTextString("##RocmEvents", m_settings.rocm_events);
    HelpMarker("ROCPROFSYS_ROCM_EVENTS",
               "HW counters (use :device=N syntax for specific GPU)");

    ImGui::Checkbox("Group by Queue", &m_settings.rocm_group_by_queue);
    HelpMarker("ROCPROFSYS_ROCM_GROUP_BY_QUEUE",
               "Group by HSA queue instead of HIP stream");
}

void RocprofSysBackend::RenderPerfettoTab()
{
    const char* backends[] = {"inprocess", "system", "all"};
    int backend_idx = 0;
    for (int i = 0; i < 3; i++)
    {
        if (m_settings.perfetto_backend == backends[i])
        {
            backend_idx = i;
            break;
        }
    }
    ImGui::Text("Backend:");
    ImGui::SameLine();
    if (ImGui::Combo("##PerfBackend", &backend_idx, backends, 3))
    {
        m_settings.perfetto_backend = backends[backend_idx];
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BACKEND", "Perfetto tracing backend mode");

    int buf_kb = m_settings.perfetto_buffer_size_kb;
    ImGui::Text("Buffer Size (KB):");
    ImGui::SameLine();
    if (ImGui::InputInt("##BufKB", &buf_kb, 1024, 10240))
    {
        m_settings.perfetto_buffer_size_kb = buf_kb;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB",
               "Perfetto shared memory buffer size");

    int flush_ms = m_settings.perfetto_flush_period_ms;
    ImGui::Text("Flush Period (ms):");
    ImGui::SameLine();
    if (ImGui::InputInt("##FlushMs", &flush_ms, 1000, 5000))
    {
        m_settings.perfetto_flush_period_ms = flush_ms;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS",
               "Flush interval in milliseconds");

    const char* policies[] = {"discard", "fill"};
    int policy_idx = (m_settings.perfetto_fill_policy == "fill") ? 1 : 0;
    ImGui::Text("Fill Policy:");
    ImGui::SameLine();
    if (ImGui::Combo("##FillPolicy", &policy_idx, policies, 2))
    {
        m_settings.perfetto_fill_policy = policies[policy_idx];
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILL_POLICY",
               "Buffer fill policy: discard old or stop writing");

    ImGui::Checkbox("Annotations", &m_settings.perfetto_annotations);
    HelpMarker("ROCPROFSYS_PERFETTO_ANNOTATIONS",
               "Function argument annotations (larger traces)");

    ImGui::Checkbox("Combine Traces", &m_settings.perfetto_combine_traces);
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
    RenderCheckboxMap(m_settings.enable_categories,
                      kPerfettoCategories, kPerfettoCategoriesCount, "encat_");
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
    RenderCheckboxMap(m_settings.disable_categories,
                      kPerfettoCategories, kPerfettoCategoriesCount, "discat_");
    if (has_enable)
        ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::Text("Output File:");
    ImGui::SameLine();
    InputTextString("##PerfFile", m_settings.perfetto_file);
    HelpMarker("ROCPROFSYS_PERFETTO_FILE", "Output filename for perfetto trace");
}

void RocprofSysBackend::RenderProcessSamplingTab()
{
    bool coverage_locked = (m_settings.mode == "coverage");
    if (coverage_locked)
        ImGui::BeginDisabled();

    ImGui::Checkbox("CPU Frequency / Mem / Context Switches",
                    &m_settings.cpu_freq_enabled);
    HelpMarker("ROCPROFSYS_CPU_FREQ_ENABLED",
               "Enable CPU frequency, memory, and context switch sampling");

    ImGui::Separator();

    ImGui::Text("AMD SMI Metrics:");
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS",
               "GPU metrics to collect (checked = enabled)");

    RenderCheckboxMap(m_settings.amd_smi_metrics,
                      kAmdSmiMetrics, kAmdSmiMetricsCount, "smi_");

    ImGui::Spacing();
    ImGui::Text("Custom Metrics:");
    ImGui::SameLine();
    InputTextString("##SmiMetricsCustom", m_settings.amd_smi_metrics_custom);
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS",
               "Additional comma-separated metric names not in the list above");

    ImGui::Separator();

    ImGui::Text("CPUs:");
    ImGui::SameLine();
    InputTextString("##SampCPUs", m_settings.sampling_cpus);
    HelpMarker("ROCPROFSYS_SAMPLING_CPUS",
               "CPU list for frequency sampling ('none', 'all', or index list)");

    ImGui::Text("GPUs:");
    ImGui::SameLine();
    InputTextString("##SampGPUs", m_settings.sampling_gpus);
    HelpMarker("ROCPROFSYS_SAMPLING_GPUS",
               "AMD SMI device indices for GPU sampling");

    ImGui::Checkbox("AI NIC Metrics", &m_settings.use_ainic);
    HelpMarker("ROCPROFSYS_USE_AINIC", "Enable AI NIC metrics collection");

    if (coverage_locked)
        ImGui::EndDisabled();
}

void RocprofSysBackend::RenderParallelismTab()
{
    bool coverage_locked = (m_settings.mode == "coverage");
    if (coverage_locked)
        ImGui::BeginDisabled();

    auto toggle = [&](char const* label, bool& val, char const* env,
                      char const* help)
    {
        ImGui::Checkbox(label, &val);
        HelpMarker(env, help);
    };

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

    if (coverage_locked)
        ImGui::EndDisabled();
}

void RocprofSysBackend::RenderInstrumentTab()
{
    ImGui::Text("Binary Instrumentation Options");
    ImGui::Separator();

    ImGui::Text("Include Regex:");
    InputTextString("##InstrInclude", m_settings.instr_include);
    HelpMarker("-I / --function-include",
               "Regex for functions to include in instrumentation");

    ImGui::Text("Exclude Regex:");
    InputTextString("##InstrExclude", m_settings.instr_exclude);
    HelpMarker("-E / --function-exclude",
               "Regex for functions to exclude from instrumentation");

    int min_instr = m_settings.min_instructions;
    ImGui::Text("Min Instructions:");
    ImGui::SameLine();
    if (ImGui::InputInt("##MinInstr", &min_instr))
    {
        if (min_instr < 0) min_instr = 0;
        m_settings.min_instructions = min_instr;
    }
    HelpMarker("--min-instructions",
               "Minimum instruction count for a function to be instrumented");
}

void RocprofSysBackend::RenderAdvancedTab()
{
    ImGui::Text("Config File:");
    ImGui::SameLine();
    InputTextString("##CfgFile", m_settings.config_file);
    HelpMarker("ROCPROFSYS_CONFIG_FILE",
               "Path to rocprof-sys configuration file");

    const char* levels[] = {"trace", "debug", "info", "warning", "error", "critical"};
    int level_idx = 2;
    for (int i = 0; i < 6; i++)
    {
        if (m_settings.log_level == levels[i])
        {
            level_idx = i;
            break;
        }
    }
    ImGui::Text("Log Level:");
    ImGui::SameLine();
    if (ImGui::Combo("##LogLevel", &level_idx, levels, 6))
    {
        m_settings.log_level = levels[level_idx];
    }
    HelpMarker("ROCPROFSYS_LOG_LEVEL", "Logging verbosity level");

    ImGui::Text("Log File:");
    ImGui::SameLine();
    InputTextString("##LogFile", m_settings.log_file);
    HelpMarker("ROCPROFSYS_LOG_FILE",
               "Log file name (empty disables file logging)");

    ImGui::Text("Temp Directory:");
    ImGui::SameLine();
    InputTextString("##TmpDir", m_settings.tmpdir);
    HelpMarker("ROCPROFSYS_TMPDIR",
               "Base directory for temporary/spill files");

    ImGui::Checkbox("Use PID in Output", &m_settings.use_pid);
    HelpMarker("ROCPROFSYS_USE_PID",
               "Suffix output filenames with process ID");

    ImGui::Text("Timemory Components:");
    InputTextString("##TimComponents", m_settings.timemory_components);
    HelpMarker("ROCPROFSYS_TIMEMORY_COMPONENTS",
               "Comma-separated timemory component list");
}

} // namespace View
} // namespace RocProfVis
