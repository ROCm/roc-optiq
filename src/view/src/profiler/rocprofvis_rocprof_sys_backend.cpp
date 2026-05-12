// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_rocprof_sys_backend.h"
#include "rocprofvis_gui_helpers.h"
#include "imgui.h"
#include <sstream>
#include <cstdio>

namespace RocProfVis
{
namespace View
{
namespace
{

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

} // namespace

// ==================================================================================
// RocprofSysSettings JSON round-trip
// ==================================================================================

RocprofSysSettings RocprofSysSettings::FromJson(jt::Json const& json)
{
    RocprofSysSettings s;
    jt::Json& j = const_cast<jt::Json&>(json);

    // General
    s.mode           = safe_string(j, "mode", "trace");
    s.trace_delay    = safe_double(j, "trace_delay", 0.0);
    s.trace_duration = safe_double(j, "trace_duration", 0.0);
    s.trace_region   = safe_string(j, "trace_region", "");

    // Backends
    s.trace_backend        = safe_bool(j, "trace", false);
    s.profile              = safe_bool(j, "profile", false);
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
    s.sampling_overflow_event  = safe_string(j, "sampling_overflow_event", "perf::PERF_COUNT_HW_CACHE_REFERENCES");
    s.sampling_keep_internal   = safe_bool(j, "sampling_keep_internal", true);
    s.sampling_include_inlines = safe_bool(j, "sampling_include_inlines", false);

    // ROCm
    s.rocm_domains        = safe_string(j, "rocm_domains", "hip_runtime_api,marker_api,kernel_dispatch,memory_copy,scratch_memory");
    s.rocm_events         = safe_string(j, "rocm_events", "");
    s.rocm_group_by_queue = safe_bool(j, "rocm_group_by_queue", false);

    // Perfetto
    s.perfetto_backend        = safe_string(j, "perfetto_backend", "inprocess");
    s.perfetto_buffer_size_kb = safe_int(j, "perfetto_buffer_size_kb", 1024000);
    s.perfetto_flush_period_ms = safe_int(j, "perfetto_flush_period_ms", 10000);
    s.perfetto_fill_policy    = safe_string(j, "perfetto_fill_policy", "discard");
    s.perfetto_annotations    = safe_bool(j, "perfetto_annotations", true);
    s.perfetto_combine_traces = safe_bool(j, "perfetto_combine_traces", false);
    s.enable_categories       = safe_string(j, "enable_categories", "");
    s.disable_categories      = safe_string(j, "disable_categories", "");
    s.perfetto_file           = safe_string(j, "perfetto_file", "perfetto-trace.proto");

    // Process Sampling
    s.cpu_freq_enabled = safe_bool(j, "cpu_freq_enabled", false);
    s.amd_smi_metrics  = safe_string(j, "amd_smi_metrics", "busy,temp,power,mem_usage");
    s.sampling_cpus    = safe_string(j, "sampling_cpus", "none");
    s.sampling_gpus    = safe_string(j, "sampling_gpus", "all");
    s.use_ainic        = safe_bool(j, "use_ainic", false);

    // Parallelism
    s.use_mpip    = safe_bool(j, "use_mpip", true);
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
    p["rocm_domains"]        = rocm_domains;
    p["rocm_events"]         = rocm_events;
    p["rocm_group_by_queue"] = rocm_group_by_queue;

    // Perfetto
    p["perfetto_backend"]         = perfetto_backend;
    p["perfetto_buffer_size_kb"]  = static_cast<long>(perfetto_buffer_size_kb);
    p["perfetto_flush_period_ms"] = static_cast<long>(perfetto_flush_period_ms);
    p["perfetto_fill_policy"]     = perfetto_fill_policy;
    p["perfetto_annotations"]     = perfetto_annotations;
    p["perfetto_combine_traces"]  = perfetto_combine_traces;
    p["enable_categories"]        = enable_categories;
    p["disable_categories"]       = disable_categories;
    p["perfetto_file"]            = perfetto_file;

    // Process Sampling
    p["cpu_freq_enabled"] = cpu_freq_enabled;
    p["amd_smi_metrics"]  = amd_smi_metrics;
    p["sampling_cpus"]    = sampling_cpus;
    p["sampling_gpus"]    = sampling_gpus;
    p["use_ainic"]        = use_ainic;

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
// RocprofSysBackend
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
        {"run", "Run (LD_PRELOAD)"},
        {"sample", "Sample"},
        {"instrument", "Instrument (Dyninst)"}
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

void RocprofSysBackend::FlattenToExecution(
    LaunchConfig const& config,
    std::vector<std::pair<std::string, std::string>>& env_out,
    std::vector<std::string>& argv_out) const
{
    auto emit_bool = [&](char const* env_name, bool val)
    {
        env_out.emplace_back(env_name, val ? "true" : "false");
    };

    auto emit_string = [&](char const* env_name, std::string const& val)
    {
        if (!val.empty())
        {
            env_out.emplace_back(env_name, val);
        }
    };

    auto emit_double = [&](char const* env_name, double val)
    {
        std::ostringstream oss;
        oss << val;
        env_out.emplace_back(env_name, oss.str());
    };

    auto emit_int = [&](char const* env_name, int32_t val)
    {
        env_out.emplace_back(env_name, std::to_string(val));
    };

    // Output path
    if (!config.target.output_directory.empty())
    {
        env_out.emplace_back("ROCPROFSYS_OUTPUT_PATH", config.target.output_directory);
    }

    // General
    emit_string("ROCPROFSYS_MODE", m_settings.mode);
    emit_double("ROCPROFSYS_TRACE_DELAY", m_settings.trace_delay);
    emit_double("ROCPROFSYS_TRACE_DURATION", m_settings.trace_duration);
    emit_string("ROCPROFSYS_TRACE_REGION", m_settings.trace_region);

    // Backends
    emit_bool("ROCPROFSYS_TRACE", m_settings.trace_backend);
    emit_bool("ROCPROFSYS_PROFILE", m_settings.profile);
    emit_bool("ROCPROFSYS_USE_ROCPD", m_settings.use_rocpd);
    emit_bool("ROCPROFSYS_USE_SAMPLING", m_settings.use_sampling);
    emit_bool("ROCPROFSYS_USE_PROCESS_SAMPLING", m_settings.use_process_sampling);
    emit_bool("ROCPROFSYS_USE_AMD_SMI", m_settings.use_amd_smi);
    emit_bool("ROCPROFSYS_USE_CAUSAL", m_settings.use_causal);

    // Sampling
    emit_double("ROCPROFSYS_SAMPLING_FREQ", m_settings.sampling_freq);
    emit_bool("ROCPROFSYS_SAMPLING_CPUTIME", m_settings.sampling_cputime);
    emit_bool("ROCPROFSYS_SAMPLING_REALTIME", m_settings.sampling_realtime);
    emit_bool("ROCPROFSYS_SAMPLING_OVERFLOW", m_settings.sampling_overflow);
    emit_double("ROCPROFSYS_SAMPLING_DURATION", m_settings.sampling_duration);
    emit_int("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE", m_settings.sampling_allocator_size);
    emit_string("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT", m_settings.sampling_overflow_event);
    emit_bool("ROCPROFSYS_SAMPLING_KEEP_INTERNAL", m_settings.sampling_keep_internal);
    emit_bool("ROCPROFSYS_SAMPLING_INCLUDE_INLINES", m_settings.sampling_include_inlines);

    // ROCm
    emit_string("ROCPROFSYS_ROCM_DOMAINS", m_settings.rocm_domains);
    emit_string("ROCPROFSYS_ROCM_EVENTS", m_settings.rocm_events);
    emit_bool("ROCPROFSYS_ROCM_GROUP_BY_QUEUE", m_settings.rocm_group_by_queue);

    // Perfetto
    emit_string("ROCPROFSYS_PERFETTO_BACKEND", m_settings.perfetto_backend);
    emit_int("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB", m_settings.perfetto_buffer_size_kb);
    emit_int("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS", m_settings.perfetto_flush_period_ms);
    emit_string("ROCPROFSYS_PERFETTO_FILL_POLICY", m_settings.perfetto_fill_policy);
    emit_bool("ROCPROFSYS_PERFETTO_ANNOTATIONS", m_settings.perfetto_annotations);
    emit_bool("ROCPROFSYS_PERFETTO_COMBINE_TRACES", m_settings.perfetto_combine_traces);
    emit_string("ROCPROFSYS_ENABLE_CATEGORIES", m_settings.enable_categories);
    emit_string("ROCPROFSYS_DISABLE_CATEGORIES", m_settings.disable_categories);
    emit_string("ROCPROFSYS_PERFETTO_FILE", m_settings.perfetto_file);

    // Process Sampling
    emit_bool("ROCPROFSYS_CPU_FREQ_ENABLED", m_settings.cpu_freq_enabled);
    emit_string("ROCPROFSYS_AMD_SMI_METRICS", m_settings.amd_smi_metrics);
    emit_string("ROCPROFSYS_SAMPLING_CPUS", m_settings.sampling_cpus);
    emit_string("ROCPROFSYS_SAMPLING_GPUS", m_settings.sampling_gpus);
    emit_bool("ROCPROFSYS_USE_AINIC", m_settings.use_ainic);

    // Parallelism
    emit_bool("ROCPROFSYS_USE_MPIP", m_settings.use_mpip);
    emit_bool("ROCPROFSYS_USE_UCX", m_settings.use_ucx);
    emit_bool("ROCPROFSYS_USE_SHMEM", m_settings.use_shmem);
    emit_bool("ROCPROFSYS_USE_RCCLP", m_settings.use_rcclp);
    emit_bool("ROCPROFSYS_USE_OMPT", m_settings.use_ompt);
    emit_bool("ROCPROFSYS_USE_KOKKOSP", m_settings.use_kokkosp);

    // Advanced
    emit_string("ROCPROFSYS_CONFIG_FILE", m_settings.config_file);
    emit_string("ROCPROFSYS_LOG_LEVEL", m_settings.log_level);
    emit_string("ROCPROFSYS_LOG_FILE", m_settings.log_file);
    emit_string("ROCPROFSYS_TMPDIR", m_settings.tmpdir);
    emit_bool("ROCPROFSYS_USE_PID", m_settings.use_pid);
    emit_string("ROCPROFSYS_TIMEMORY_COMPONENTS", m_settings.timemory_components);

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
        {
            cfg << env_name << " = " << val << "\n";
        }
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
    emit_string("ROCPROFSYS_ROCM_DOMAINS", m_settings.rocm_domains);
    emit_string("ROCPROFSYS_ROCM_EVENTS", m_settings.rocm_events);
    emit_bool("ROCPROFSYS_ROCM_GROUP_BY_QUEUE", m_settings.rocm_group_by_queue);
    emit_string("ROCPROFSYS_PERFETTO_BACKEND", m_settings.perfetto_backend);
    emit_int("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB", m_settings.perfetto_buffer_size_kb);
    emit_int("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS", m_settings.perfetto_flush_period_ms);
    emit_string("ROCPROFSYS_PERFETTO_FILL_POLICY", m_settings.perfetto_fill_policy);
    emit_bool("ROCPROFSYS_PERFETTO_ANNOTATIONS", m_settings.perfetto_annotations);
    emit_bool("ROCPROFSYS_PERFETTO_COMBINE_TRACES", m_settings.perfetto_combine_traces);
    emit_string("ROCPROFSYS_ENABLE_CATEGORIES", m_settings.enable_categories);
    emit_string("ROCPROFSYS_DISABLE_CATEGORIES", m_settings.disable_categories);
    emit_string("ROCPROFSYS_PERFETTO_FILE", m_settings.perfetto_file);
    emit_bool("ROCPROFSYS_CPU_FREQ_ENABLED", m_settings.cpu_freq_enabled);
    emit_string("ROCPROFSYS_AMD_SMI_METRICS", m_settings.amd_smi_metrics);
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
        m_settings.mode = modes[mode_idx];
    }
    HelpMarker("ROCPROFSYS_MODE", "Profiling mode: trace, sampling, causal, or coverage");

    ImGui::Text("Trace Delay (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##TraceDelay", &m_settings.trace_delay, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_TRACE_DELAY", "Seconds before collection starts (0 = immediate)");

    ImGui::Text("Trace Duration (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##TraceDuration", &m_settings.trace_duration, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_TRACE_DURATION", "Duration of collection in seconds (0 = unlimited)");

    char region_buf[512];
    std::snprintf(region_buf, sizeof(region_buf), "%s", m_settings.trace_region.c_str());
    ImGui::Text("Trace Region:");
    ImGui::SameLine();
    if (ImGui::InputText("##TraceRegion", region_buf, sizeof(region_buf)))
    {
        m_settings.trace_region = region_buf;
    }
    HelpMarker("ROCPROFSYS_TRACE_REGION", "Comma-separated ROCTX region names for selective tracing");
}

void RocprofSysBackend::RenderBackendsTab()
{
    auto toggle = [&](char const* label, bool& val, char const* env, char const* help)
    {
        ImGui::Checkbox(label, &val);
        HelpMarker(env, help);
    };

    toggle("Perfetto Tracing", m_settings.trace_backend, "ROCPROFSYS_TRACE", "Enable Perfetto trace backend");
    toggle("Timemory Profile", m_settings.profile, "ROCPROFSYS_PROFILE", "Enable timemory profiling backend");
    toggle("ROCpd Database", m_settings.use_rocpd, "ROCPROFSYS_USE_ROCPD", "Enable ROCpd SQLite output");
    toggle("Statistical Sampling", m_settings.use_sampling, "ROCPROFSYS_USE_SAMPLING", "Enable call-stack sampling");
    toggle("Process Sampling", m_settings.use_process_sampling, "ROCPROFSYS_USE_PROCESS_SAMPLING", "Background process/system metrics");
    toggle("AMD SMI", m_settings.use_amd_smi, "ROCPROFSYS_USE_AMD_SMI", "GPU metrics via AMD SMI");
    toggle("Causal Profiling", m_settings.use_causal, "ROCPROFSYS_USE_CAUSAL", "Enable causal profiling mode");
}

void RocprofSysBackend::RenderSamplingTab()
{
    ImGui::Text("Sampling Frequency (Hz):");
    ImGui::SameLine();
    ImGui::InputDouble("##SampFreq", &m_settings.sampling_freq, 10.0, 100.0, "%.0f");
    HelpMarker("ROCPROFSYS_SAMPLING_FREQ", "Software interrupts per second");

    ImGui::Separator();
    ImGui::Text("Timer Sources:");

    ImGui::Checkbox("CPU Time", &m_settings.sampling_cputime);
    HelpMarker("ROCPROFSYS_SAMPLING_CPUTIME", "Sample on CPU-time timer (ITIMER_PROF)");

    ImGui::Checkbox("Real Time", &m_settings.sampling_realtime);
    HelpMarker("ROCPROFSYS_SAMPLING_REALTIME", "Sample on real-time timer (ITIMER_REAL)");

    ImGui::Checkbox("Hardware Overflow", &m_settings.sampling_overflow);
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW", "Sample on hardware counter overflow");

    ImGui::Separator();

    ImGui::Text("Duration (s):");
    ImGui::SameLine();
    ImGui::InputDouble("##SampDur", &m_settings.sampling_duration, 0.0, 0.0, "%.2f");
    HelpMarker("ROCPROFSYS_SAMPLING_DURATION", "Stop sampling after N seconds (0 = unlimited)");

    int alloc_sz = m_settings.sampling_allocator_size;
    ImGui::Text("Allocator Size:");
    ImGui::SameLine();
    if (ImGui::InputInt("##AllocSz", &alloc_sz))
    {
        m_settings.sampling_allocator_size = alloc_sz;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE", "Threads per background allocator");

    char oe_buf[256];
    std::snprintf(oe_buf, sizeof(oe_buf), "%s", m_settings.sampling_overflow_event.c_str());
    ImGui::Text("Overflow Event:");
    ImGui::SameLine();
    if (ImGui::InputText("##OverflowEvt", oe_buf, sizeof(oe_buf)))
    {
        m_settings.sampling_overflow_event = oe_buf;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT", "Linux perf metric name for overflow sampling");

    ImGui::Separator();

    ImGui::Checkbox("Keep Internal Frames", &m_settings.sampling_keep_internal);
    HelpMarker("ROCPROFSYS_SAMPLING_KEEP_INTERNAL", "Show rocprof-sys frames in call stacks");

    ImGui::Checkbox("Include Inline Entries", &m_settings.sampling_include_inlines);
    HelpMarker("ROCPROFSYS_SAMPLING_INCLUDE_INLINES", "Include inline function entries in stacks");
}

void RocprofSysBackend::RenderRocmTab()
{
    ImGui::Text("ROCm Domains (comma-separated):");
    char domains_buf[1024];
    std::snprintf(domains_buf, sizeof(domains_buf), "%s", m_settings.rocm_domains.c_str());
    if (ImGui::InputText("##RocmDomains", domains_buf, sizeof(domains_buf)))
    {
        m_settings.rocm_domains = domains_buf;
    }
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS", "Comma-separated list of ROCm SDK domains to trace");

    ImGui::TextWrapped("Available: hip_runtime_api, marker_api, kernel_dispatch, memory_copy, "
                       "scratch_memory, page_migration, hsa_core_api, hsa_amd_ext_api, "
                       "hsa_image_ext_api, rccl_api");

    ImGui::Separator();

    char events_buf[1024];
    std::snprintf(events_buf, sizeof(events_buf), "%s", m_settings.rocm_events.c_str());
    ImGui::Text("Hardware Counters:");
    if (ImGui::InputText("##RocmEvents", events_buf, sizeof(events_buf)))
    {
        m_settings.rocm_events = events_buf;
    }
    HelpMarker("ROCPROFSYS_ROCM_EVENTS", "HW counters (use :device=N syntax for specific GPU)");

    ImGui::Checkbox("Group by Queue", &m_settings.rocm_group_by_queue);
    HelpMarker("ROCPROFSYS_ROCM_GROUP_BY_QUEUE", "Group by HSA queue instead of HIP stream");
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
    HelpMarker("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB", "Perfetto shared memory buffer size");

    int flush_ms = m_settings.perfetto_flush_period_ms;
    ImGui::Text("Flush Period (ms):");
    ImGui::SameLine();
    if (ImGui::InputInt("##FlushMs", &flush_ms, 1000, 5000))
    {
        m_settings.perfetto_flush_period_ms = flush_ms;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS", "Flush interval in milliseconds");

    const char* policies[] = {"discard", "fill"};
    int policy_idx = (m_settings.perfetto_fill_policy == "fill") ? 1 : 0;
    ImGui::Text("Fill Policy:");
    ImGui::SameLine();
    if (ImGui::Combo("##FillPolicy", &policy_idx, policies, 2))
    {
        m_settings.perfetto_fill_policy = policies[policy_idx];
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILL_POLICY", "Buffer fill policy: discard old or stop writing");

    ImGui::Checkbox("Annotations", &m_settings.perfetto_annotations);
    HelpMarker("ROCPROFSYS_PERFETTO_ANNOTATIONS", "Function argument annotations (larger traces)");

    ImGui::Checkbox("Combine Traces", &m_settings.perfetto_combine_traces);
    HelpMarker("ROCPROFSYS_PERFETTO_COMBINE_TRACES", "Combine per-process traces into one file");

    ImGui::Separator();

    char enable_buf[512];
    std::snprintf(enable_buf, sizeof(enable_buf), "%s", m_settings.enable_categories.c_str());
    ImGui::Text("Enable Categories:");
    if (ImGui::InputText("##EnableCat", enable_buf, sizeof(enable_buf)))
    {
        m_settings.enable_categories = enable_buf;
    }
    HelpMarker("ROCPROFSYS_ENABLE_CATEGORIES", "Comma-separated perfetto categories to enable");

    char disable_buf[512];
    std::snprintf(disable_buf, sizeof(disable_buf), "%s", m_settings.disable_categories.c_str());
    ImGui::Text("Disable Categories:");
    if (ImGui::InputText("##DisableCat", disable_buf, sizeof(disable_buf)))
    {
        m_settings.disable_categories = disable_buf;
    }
    HelpMarker("ROCPROFSYS_DISABLE_CATEGORIES", "Comma-separated perfetto categories to disable");

    ImGui::Separator();

    char file_buf[256];
    std::snprintf(file_buf, sizeof(file_buf), "%s", m_settings.perfetto_file.c_str());
    ImGui::Text("Output File:");
    ImGui::SameLine();
    if (ImGui::InputText("##PerfFile", file_buf, sizeof(file_buf)))
    {
        m_settings.perfetto_file = file_buf;
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILE", "Output filename for perfetto trace");
}

void RocprofSysBackend::RenderProcessSamplingTab()
{
    ImGui::Checkbox("CPU Frequency / Mem / Context Switches", &m_settings.cpu_freq_enabled);
    HelpMarker("ROCPROFSYS_CPU_FREQ_ENABLED", "Enable CPU frequency, memory, and context switch sampling");

    ImGui::Separator();

    char metrics_buf[512];
    std::snprintf(metrics_buf, sizeof(metrics_buf), "%s", m_settings.amd_smi_metrics.c_str());
    ImGui::Text("AMD SMI Metrics:");
    if (ImGui::InputText("##SmiMetrics", metrics_buf, sizeof(metrics_buf)))
    {
        m_settings.amd_smi_metrics = metrics_buf;
    }
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS", "Comma-separated GPU metrics (empty=all, 'none'=disable)");
    ImGui::TextWrapped("Available: busy, temp, power, mem_usage, vcn_activity, jpeg_activity, xgmi, pcie, sdma_usage");

    ImGui::Separator();

    char cpus_buf[128];
    std::snprintf(cpus_buf, sizeof(cpus_buf), "%s", m_settings.sampling_cpus.c_str());
    ImGui::Text("CPUs:");
    ImGui::SameLine();
    if (ImGui::InputText("##SampCPUs", cpus_buf, sizeof(cpus_buf)))
    {
        m_settings.sampling_cpus = cpus_buf;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_CPUS", "CPU list for frequency sampling ('none', 'all', or index list)");

    char gpus_buf[128];
    std::snprintf(gpus_buf, sizeof(gpus_buf), "%s", m_settings.sampling_gpus.c_str());
    ImGui::Text("GPUs:");
    ImGui::SameLine();
    if (ImGui::InputText("##SampGPUs", gpus_buf, sizeof(gpus_buf)))
    {
        m_settings.sampling_gpus = gpus_buf;
    }
    HelpMarker("ROCPROFSYS_SAMPLING_GPUS", "AMD SMI device indices for GPU sampling");

    ImGui::Checkbox("AI NIC Metrics", &m_settings.use_ainic);
    HelpMarker("ROCPROFSYS_USE_AINIC", "Enable AI NIC metrics collection");
}

void RocprofSysBackend::RenderParallelismTab()
{
    auto toggle = [&](char const* label, bool& val, char const* env, char const* help)
    {
        ImGui::Checkbox(label, &val);
        HelpMarker(env, help);
    };

    toggle("MPI (MPIP)", m_settings.use_mpip, "ROCPROFSYS_USE_MPIP", "Profile MPI functions");
    toggle("UCX", m_settings.use_ucx, "ROCPROFSYS_USE_UCX", "UCX communication wrappers");
    toggle("OpenSHMEM", m_settings.use_shmem, "ROCPROFSYS_USE_SHMEM", "OpenSHMEM profiling");
    toggle("RCCL", m_settings.use_rcclp, "ROCPROFSYS_USE_RCCLP", "RCCL collective communication profiling");
    toggle("OpenMP (OMPT)", m_settings.use_ompt, "ROCPROFSYS_USE_OMPT", "OpenMP Tools interface");
    toggle("Kokkos", m_settings.use_kokkosp, "ROCPROFSYS_USE_KOKKOSP", "Kokkos Tools callback interface");
}

void RocprofSysBackend::RenderInstrumentTab()
{
    ImGui::Text("Binary Instrumentation Options");
    ImGui::Separator();

    char include_buf[512];
    std::snprintf(include_buf, sizeof(include_buf), "%s", m_settings.instr_include.c_str());
    ImGui::Text("Include Regex:");
    if (ImGui::InputText("##InstrInclude", include_buf, sizeof(include_buf)))
    {
        m_settings.instr_include = include_buf;
    }
    HelpMarker("-I / --function-include", "Regex for functions to include in instrumentation");

    char exclude_buf[512];
    std::snprintf(exclude_buf, sizeof(exclude_buf), "%s", m_settings.instr_exclude.c_str());
    ImGui::Text("Exclude Regex:");
    if (ImGui::InputText("##InstrExclude", exclude_buf, sizeof(exclude_buf)))
    {
        m_settings.instr_exclude = exclude_buf;
    }
    HelpMarker("-E / --function-exclude", "Regex for functions to exclude from instrumentation");

    int min_instr = m_settings.min_instructions;
    ImGui::Text("Min Instructions:");
    ImGui::SameLine();
    if (ImGui::InputInt("##MinInstr", &min_instr))
    {
        if (min_instr < 0) min_instr = 0;
        m_settings.min_instructions = min_instr;
    }
    HelpMarker("--min-instructions", "Minimum instruction count for a function to be instrumented");
}

void RocprofSysBackend::RenderAdvancedTab()
{
    char cfg_buf[512];
    std::snprintf(cfg_buf, sizeof(cfg_buf), "%s", m_settings.config_file.c_str());
    ImGui::Text("Config File:");
    ImGui::SameLine();
    if (ImGui::InputText("##CfgFile", cfg_buf, sizeof(cfg_buf)))
    {
        m_settings.config_file = cfg_buf;
    }
    HelpMarker("ROCPROFSYS_CONFIG_FILE", "Path to rocprof-sys configuration file");

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

    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf), "%s", m_settings.log_file.c_str());
    ImGui::Text("Log File:");
    ImGui::SameLine();
    if (ImGui::InputText("##LogFile", log_buf, sizeof(log_buf)))
    {
        m_settings.log_file = log_buf;
    }
    HelpMarker("ROCPROFSYS_LOG_FILE", "Log file name (empty disables file logging)");

    char tmp_buf[256];
    std::snprintf(tmp_buf, sizeof(tmp_buf), "%s", m_settings.tmpdir.c_str());
    ImGui::Text("Temp Directory:");
    ImGui::SameLine();
    if (ImGui::InputText("##TmpDir", tmp_buf, sizeof(tmp_buf)))
    {
        m_settings.tmpdir = tmp_buf;
    }
    HelpMarker("ROCPROFSYS_TMPDIR", "Base directory for temporary/spill files");

    ImGui::Checkbox("Use PID in Output", &m_settings.use_pid);
    HelpMarker("ROCPROFSYS_USE_PID", "Suffix output filenames with process ID");

    char comp_buf[256];
    std::snprintf(comp_buf, sizeof(comp_buf), "%s", m_settings.timemory_components.c_str());
    ImGui::Text("Timemory Components:");
    if (ImGui::InputText("##TimComponents", comp_buf, sizeof(comp_buf)))
    {
        m_settings.timemory_components = comp_buf;
    }
    HelpMarker("ROCPROFSYS_TIMEMORY_COMPONENTS", "Comma-separated timemory component list");
}

} // namespace View
} // namespace RocProfVis
