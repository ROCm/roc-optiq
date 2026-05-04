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

bool JsonBool(jt::Json& payload, char const* key, bool default_val)
{
    if (!payload.contains(key))
    {
        payload[key] = default_val;
    }
    return payload[key].getBool();
}

void SetJsonBool(jt::Json& payload, char const* key, bool val)
{
    payload[key] = val;
}

std::string JsonString(jt::Json& payload, char const* key, char const* default_val = "")
{
    if (!payload.contains(key))
    {
        payload[key] = std::string(default_val);
    }
    return payload[key].getString();
}

void SetJsonString(jt::Json& payload, char const* key, std::string const& val)
{
    payload[key] = val;
}

double JsonDouble(jt::Json& payload, char const* key, double default_val = 0.0)
{
    if (!payload.contains(key))
    {
        payload[key] = default_val;
    }
    return payload[key].getDouble();
}

void SetJsonDouble(jt::Json& payload, char const* key, double val)
{
    payload[key] = val;
}

long JsonLong(jt::Json& payload, char const* key, long default_val = 0)
{
    if (!payload.contains(key))
    {
        payload[key] = default_val;
    }
    return payload[key].getLong();
}

void SetJsonLong(jt::Json& payload, char const* key, long val)
{
    payload[key] = val;
}

} // namespace

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

    tabs.push_back({"general", "General", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderGeneralTab(p); }});
    tabs.push_back({"backends", "Backends", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderBackendsTab(p); }});
    tabs.push_back({"sampling", "Sampling", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderSamplingTab(p); }});
    tabs.push_back({"rocm", "ROCm", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderRocmTab(p); }});
    tabs.push_back({"perfetto", "Perfetto", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderPerfettoTab(p); }});
    tabs.push_back({"process_sampling", "Process Sampling", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderProcessSamplingTab(p); }});
    tabs.push_back({"parallelism", "Parallelism", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderParallelismTab(p); }});

    if (tool_id == "instrument")
    {
        tabs.push_back({"instrument", "Instrument", [this](jt::Json& p) {
            const_cast<RocprofSysBackend*>(this)->RenderInstrumentTab(p); }});
    }

    tabs.push_back({"advanced", "Advanced", [this](jt::Json& p) {
        const_cast<RocprofSysBackend*>(this)->RenderAdvancedTab(p); }});

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

void RocprofSysBackend::FlattenToExecution(
    LaunchConfig const& config,
    std::vector<std::pair<std::string, std::string>>& env_out,
    std::vector<std::string>& argv_out) const
{
    jt::Json& p = const_cast<jt::Json&>(config.backend_payload);

    auto add_env_if = [&](char const* key, char const* env_name)
    {
        if (p.contains(key))
        {
            auto& val = p[key];
            if (val.getType() == jt::Json::Bool)
            {
                env_out.emplace_back(env_name, val.getBool() ? "true" : "false");
            }
            else if (val.getType() == jt::Json::String)
            {
                std::string s = val.getString();
                if (!s.empty())
                {
                    env_out.emplace_back(env_name, s);
                }
            }
            else if (val.getType() == jt::Json::Double)
            {
                std::ostringstream oss;
                oss << val.getDouble();
                env_out.emplace_back(env_name, oss.str());
            }
            else if (val.getType() == jt::Json::Long)
            {
                env_out.emplace_back(env_name, std::to_string(val.getLong()));
            }
        }
    };

    // Output path (set explicitly so artifacts land in the chosen dir)
    if (!config.target.output_directory.empty())
    {
        env_out.emplace_back("ROCPROFSYS_OUTPUT_PATH", config.target.output_directory);
    }

    // General
    add_env_if("mode", "ROCPROFSYS_MODE");
    add_env_if("trace_delay", "ROCPROFSYS_TRACE_DELAY");
    add_env_if("trace_duration", "ROCPROFSYS_TRACE_DURATION");
    add_env_if("trace_region", "ROCPROFSYS_TRACE_REGION");

    // Backends
    add_env_if("trace", "ROCPROFSYS_TRACE");
    add_env_if("profile", "ROCPROFSYS_PROFILE");
    add_env_if("use_rocpd", "ROCPROFSYS_USE_ROCPD");
    add_env_if("use_sampling", "ROCPROFSYS_USE_SAMPLING");
    add_env_if("use_process_sampling", "ROCPROFSYS_USE_PROCESS_SAMPLING");
    add_env_if("use_amd_smi", "ROCPROFSYS_USE_AMD_SMI");
    add_env_if("use_causal", "ROCPROFSYS_USE_CAUSAL");

    // Sampling
    add_env_if("sampling_freq", "ROCPROFSYS_SAMPLING_FREQ");
    add_env_if("sampling_cputime", "ROCPROFSYS_SAMPLING_CPUTIME");
    add_env_if("sampling_realtime", "ROCPROFSYS_SAMPLING_REALTIME");
    add_env_if("sampling_overflow", "ROCPROFSYS_SAMPLING_OVERFLOW");
    add_env_if("sampling_duration", "ROCPROFSYS_SAMPLING_DURATION");
    add_env_if("sampling_allocator_size", "ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE");
    add_env_if("sampling_overflow_event", "ROCPROFSYS_SAMPLING_OVERFLOW_EVENT");
    add_env_if("sampling_keep_internal", "ROCPROFSYS_SAMPLING_KEEP_INTERNAL");
    add_env_if("sampling_include_inlines", "ROCPROFSYS_SAMPLING_INCLUDE_INLINES");

    // ROCm
    add_env_if("rocm_domains", "ROCPROFSYS_ROCM_DOMAINS");
    add_env_if("rocm_events", "ROCPROFSYS_ROCM_EVENTS");
    add_env_if("rocm_group_by_queue", "ROCPROFSYS_ROCM_GROUP_BY_QUEUE");

    // Perfetto
    add_env_if("perfetto_backend", "ROCPROFSYS_PERFETTO_BACKEND");
    add_env_if("perfetto_buffer_size_kb", "ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB");
    add_env_if("perfetto_flush_period_ms", "ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS");
    add_env_if("perfetto_fill_policy", "ROCPROFSYS_PERFETTO_FILL_POLICY");
    add_env_if("perfetto_annotations", "ROCPROFSYS_PERFETTO_ANNOTATIONS");
    add_env_if("perfetto_combine_traces", "ROCPROFSYS_PERFETTO_COMBINE_TRACES");
    add_env_if("enable_categories", "ROCPROFSYS_ENABLE_CATEGORIES");
    add_env_if("disable_categories", "ROCPROFSYS_DISABLE_CATEGORIES");
    add_env_if("perfetto_file", "ROCPROFSYS_PERFETTO_FILE");

    // Process Sampling
    add_env_if("cpu_freq_enabled", "ROCPROFSYS_CPU_FREQ_ENABLED");
    add_env_if("amd_smi_metrics", "ROCPROFSYS_AMD_SMI_METRICS");
    add_env_if("sampling_cpus", "ROCPROFSYS_SAMPLING_CPUS");
    add_env_if("sampling_gpus", "ROCPROFSYS_SAMPLING_GPUS");
    add_env_if("use_ainic", "ROCPROFSYS_USE_AINIC");

    // Parallelism
    add_env_if("use_mpip", "ROCPROFSYS_USE_MPIP");
    add_env_if("use_ucx", "ROCPROFSYS_USE_UCX");
    add_env_if("use_shmem", "ROCPROFSYS_USE_SHMEM");
    add_env_if("use_rcclp", "ROCPROFSYS_USE_RCCLP");
    add_env_if("use_ompt", "ROCPROFSYS_USE_OMPT");
    add_env_if("use_kokkosp", "ROCPROFSYS_USE_KOKKOSP");

    // Advanced
    add_env_if("config_file", "ROCPROFSYS_CONFIG_FILE");
    add_env_if("log_level", "ROCPROFSYS_LOG_LEVEL");
    add_env_if("log_file", "ROCPROFSYS_LOG_FILE");
    add_env_if("tmpdir", "ROCPROFSYS_TMPDIR");
    add_env_if("use_pid", "ROCPROFSYS_USE_PID");
    add_env_if("timemory_components", "ROCPROFSYS_TIMEMORY_COMPONENTS");

    if (config.tool_id == "instrument")
    {
        if (p.contains("instr_include"))
        {
            std::string inc = p["instr_include"].getString();
            if (!inc.empty())
            {
                argv_out.push_back("-I");
                argv_out.push_back(inc);
            }
        }
        if (p.contains("instr_exclude"))
        {
            std::string exc = p["instr_exclude"].getString();
            if (!exc.empty())
            {
                argv_out.push_back("-E");
                argv_out.push_back(exc);
            }
        }
        if (p.contains("min_instructions"))
        {
            long long val = p["min_instructions"].getLong();
            if (val > 0)
            {
                argv_out.push_back("--min-instructions");
                argv_out.push_back(std::to_string(val));
            }
        }
    }
}

jt::Json RocprofSysBackend::DefaultPayload() const
{
    jt::Json p;

    // General
    p["mode"] = std::string("trace");
    p["trace_delay"] = 0.0;
    p["trace_duration"] = 0.0;
    p["trace_region"] = std::string("");

    // Backends
    p["trace"] = true;
    p["profile"] = false;
    p["use_rocpd"] = true;
    p["use_sampling"] = false;
    p["use_process_sampling"] = true;
    p["use_amd_smi"] = true;
    p["use_causal"] = false;

    // Sampling
    p["sampling_freq"] = 300.0;
    p["sampling_cputime"] = false;
    p["sampling_realtime"] = false;
    p["sampling_overflow"] = false;
    p["sampling_duration"] = 0.0;
    p["sampling_allocator_size"] = 8L;
    p["sampling_overflow_event"] = std::string("perf::PERF_COUNT_HW_CACHE_REFERENCES");
    p["sampling_keep_internal"] = true;
    p["sampling_include_inlines"] = false;

    // ROCm
    p["rocm_domains"] = std::string("hip_runtime_api,marker_api,kernel_dispatch,memory_copy,scratch_memory");
    p["rocm_events"] = std::string("");
    p["rocm_group_by_queue"] = false;

    // Perfetto
    p["perfetto_backend"] = std::string("inprocess");
    p["perfetto_buffer_size_kb"] = 1024000L;
    p["perfetto_flush_period_ms"] = 10000L;
    p["perfetto_fill_policy"] = std::string("discard");
    p["perfetto_annotations"] = true;
    p["perfetto_combine_traces"] = false;
    p["enable_categories"] = std::string("");
    p["disable_categories"] = std::string("");
    p["perfetto_file"] = std::string("perfetto-trace.proto");

    // Process Sampling
    p["cpu_freq_enabled"] = false;
    p["amd_smi_metrics"] = std::string("busy,temp,power,mem_usage");
    p["sampling_cpus"] = std::string("none");
    p["sampling_gpus"] = std::string("all");
    p["use_ainic"] = false;

    // Parallelism
    p["use_mpip"] = true;
    p["use_ucx"] = false;
    p["use_shmem"] = false;
    p["use_rcclp"] = false;
    p["use_ompt"] = false;
    p["use_kokkosp"] = false;

    // Advanced
    p["config_file"] = std::string("");
    p["log_level"] = std::string("info");
    p["log_file"] = std::string("rocprof-sys-log.txt");
    p["tmpdir"] = std::string("");
    p["use_pid"] = true;
    p["timemory_components"] = std::string("wall_clock");

    // Instrument
    p["instr_include"] = std::string("");
    p["instr_exclude"] = std::string("");
    p["min_instructions"] = 0L;

    return p;
}

std::string RocprofSysBackend::ExportCfg(jt::Json const& payload) const
{
    jt::Json& p = const_cast<jt::Json&>(payload);
    std::ostringstream cfg;
    cfg << "# ROCm Systems Profiler configuration\n";
    cfg << "# Generated by ROCm Optiq\n\n";

    auto emit = [&](char const* key, char const* env_name)
    {
        if (!p.contains(key))
            return;
        auto& val = p[key];
        if (val.getType() == jt::Json::Bool)
        {
            cfg << env_name << " = " << (val.getBool() ? "true" : "false") << "\n";
        }
        else if (val.getType() == jt::Json::String)
        {
            std::string s = val.getString();
            if (!s.empty())
            {
                cfg << env_name << " = " << s << "\n";
            }
        }
        else if (val.getType() == jt::Json::Double)
        {
            cfg << env_name << " = " << val.getDouble() << "\n";
        }
        else if (val.getType() == jt::Json::Long)
        {
            cfg << env_name << " = " << val.getLong() << "\n";
        }
    };

    emit("mode", "ROCPROFSYS_MODE");
    emit("trace_delay", "ROCPROFSYS_TRACE_DELAY");
    emit("trace_duration", "ROCPROFSYS_TRACE_DURATION");
    emit("trace_region", "ROCPROFSYS_TRACE_REGION");
    emit("trace", "ROCPROFSYS_TRACE");
    emit("profile", "ROCPROFSYS_PROFILE");
    emit("use_rocpd", "ROCPROFSYS_USE_ROCPD");
    emit("use_sampling", "ROCPROFSYS_USE_SAMPLING");
    emit("use_process_sampling", "ROCPROFSYS_USE_PROCESS_SAMPLING");
    emit("use_amd_smi", "ROCPROFSYS_USE_AMD_SMI");
    emit("use_causal", "ROCPROFSYS_USE_CAUSAL");
    emit("sampling_freq", "ROCPROFSYS_SAMPLING_FREQ");
    emit("sampling_cputime", "ROCPROFSYS_SAMPLING_CPUTIME");
    emit("sampling_realtime", "ROCPROFSYS_SAMPLING_REALTIME");
    emit("sampling_overflow", "ROCPROFSYS_SAMPLING_OVERFLOW");
    emit("sampling_duration", "ROCPROFSYS_SAMPLING_DURATION");
    emit("sampling_allocator_size", "ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE");
    emit("sampling_overflow_event", "ROCPROFSYS_SAMPLING_OVERFLOW_EVENT");
    emit("sampling_keep_internal", "ROCPROFSYS_SAMPLING_KEEP_INTERNAL");
    emit("sampling_include_inlines", "ROCPROFSYS_SAMPLING_INCLUDE_INLINES");
    emit("rocm_domains", "ROCPROFSYS_ROCM_DOMAINS");
    emit("rocm_events", "ROCPROFSYS_ROCM_EVENTS");
    emit("rocm_group_by_queue", "ROCPROFSYS_ROCM_GROUP_BY_QUEUE");
    emit("perfetto_backend", "ROCPROFSYS_PERFETTO_BACKEND");
    emit("perfetto_buffer_size_kb", "ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB");
    emit("perfetto_flush_period_ms", "ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS");
    emit("perfetto_fill_policy", "ROCPROFSYS_PERFETTO_FILL_POLICY");
    emit("perfetto_annotations", "ROCPROFSYS_PERFETTO_ANNOTATIONS");
    emit("perfetto_combine_traces", "ROCPROFSYS_PERFETTO_COMBINE_TRACES");
    emit("enable_categories", "ROCPROFSYS_ENABLE_CATEGORIES");
    emit("disable_categories", "ROCPROFSYS_DISABLE_CATEGORIES");
    emit("perfetto_file", "ROCPROFSYS_PERFETTO_FILE");
    emit("cpu_freq_enabled", "ROCPROFSYS_CPU_FREQ_ENABLED");
    emit("amd_smi_metrics", "ROCPROFSYS_AMD_SMI_METRICS");
    emit("sampling_cpus", "ROCPROFSYS_SAMPLING_CPUS");
    emit("sampling_gpus", "ROCPROFSYS_SAMPLING_GPUS");
    emit("use_ainic", "ROCPROFSYS_USE_AINIC");
    emit("use_mpip", "ROCPROFSYS_USE_MPIP");
    emit("use_ucx", "ROCPROFSYS_USE_UCX");
    emit("use_shmem", "ROCPROFSYS_USE_SHMEM");
    emit("use_rcclp", "ROCPROFSYS_USE_RCCLP");
    emit("use_ompt", "ROCPROFSYS_USE_OMPT");
    emit("use_kokkosp", "ROCPROFSYS_USE_KOKKOSP");
    emit("log_level", "ROCPROFSYS_LOG_LEVEL");
    emit("log_file", "ROCPROFSYS_LOG_FILE");
    emit("tmpdir", "ROCPROFSYS_TMPDIR");
    emit("use_pid", "ROCPROFSYS_USE_PID");
    emit("timemory_components", "ROCPROFSYS_TIMEMORY_COMPONENTS");

    return cfg.str();
}

// ==================================================================================
// Tab render functions
// ==================================================================================

void RocprofSysBackend::RenderGeneralTab(jt::Json& payload)
{
    // Mode combo
    const char* modes[] = {"trace", "sampling", "causal", "coverage"};
    std::string current_mode = JsonString(payload, "mode", "trace");
    int mode_idx = 0;
    for (int i = 0; i < 4; i++)
    {
        if (current_mode == modes[i])
        {
            mode_idx = i;
            break;
        }
    }
    ImGui::Text("Mode:");
    ImGui::SameLine();
    if (ImGui::Combo("##Mode", &mode_idx, modes, 4))
    {
        SetJsonString(payload, "mode", modes[mode_idx]);
    }
    HelpMarker("ROCPROFSYS_MODE", "Profiling mode: trace, sampling, causal, or coverage");

    // Trace delay
    double delay = JsonDouble(payload, "trace_delay", 0.0);
    ImGui::Text("Trace Delay (s):");
    ImGui::SameLine();
    if (ImGui::InputDouble("##TraceDelay", &delay, 0.0, 0.0, "%.2f"))
    {
        SetJsonDouble(payload, "trace_delay", delay);
    }
    HelpMarker("ROCPROFSYS_TRACE_DELAY", "Seconds before collection starts (0 = immediate)");

    // Trace duration
    double duration = JsonDouble(payload, "trace_duration", 0.0);
    ImGui::Text("Trace Duration (s):");
    ImGui::SameLine();
    if (ImGui::InputDouble("##TraceDuration", &duration, 0.0, 0.0, "%.2f"))
    {
        SetJsonDouble(payload, "trace_duration", duration);
    }
    HelpMarker("ROCPROFSYS_TRACE_DURATION", "Duration of collection in seconds (0 = unlimited)");

    // Trace region
    std::string region = JsonString(payload, "trace_region", "");
    char region_buf[512];
    std::snprintf(region_buf, sizeof(region_buf), "%s", region.c_str());
    ImGui::Text("Trace Region:");
    ImGui::SameLine();
    if (ImGui::InputText("##TraceRegion", region_buf, sizeof(region_buf)))
    {
        SetJsonString(payload, "trace_region", region_buf);
    }
    HelpMarker("ROCPROFSYS_TRACE_REGION", "Comma-separated ROCTX region names for selective tracing");
}

void RocprofSysBackend::RenderBackendsTab(jt::Json& payload)
{
    auto toggle = [&](char const* label, char const* key, char const* env, char const* help, bool def)
    {
        bool val = JsonBool(payload, key, def);
        if (ImGui::Checkbox(label, &val))
        {
            SetJsonBool(payload, key, val);
        }
        HelpMarker(env, help);
    };

    toggle("Perfetto Tracing", "trace", "ROCPROFSYS_TRACE", "Enable Perfetto trace backend", true);
    toggle("Timemory Profile", "profile", "ROCPROFSYS_PROFILE", "Enable timemory profiling backend", false);
    toggle("ROCpd Database", "use_rocpd", "ROCPROFSYS_USE_ROCPD", "Enable ROCpd SQLite output", false);
    toggle("Statistical Sampling", "use_sampling", "ROCPROFSYS_USE_SAMPLING", "Enable call-stack sampling", false);
    toggle("Process Sampling", "use_process_sampling", "ROCPROFSYS_USE_PROCESS_SAMPLING", "Background process/system metrics", true);
    toggle("AMD SMI", "use_amd_smi", "ROCPROFSYS_USE_AMD_SMI", "GPU metrics via AMD SMI", true);
    toggle("Causal Profiling", "use_causal", "ROCPROFSYS_USE_CAUSAL", "Enable causal profiling mode", false);
}

void RocprofSysBackend::RenderSamplingTab(jt::Json& payload)
{
    double freq = JsonDouble(payload, "sampling_freq", 300.0);
    ImGui::Text("Sampling Frequency (Hz):");
    ImGui::SameLine();
    if (ImGui::InputDouble("##SampFreq", &freq, 10.0, 100.0, "%.0f"))
    {
        SetJsonDouble(payload, "sampling_freq", freq);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_FREQ", "Software interrupts per second");

    ImGui::Separator();
    ImGui::Text("Timer Sources:");

    bool cputime = JsonBool(payload, "sampling_cputime", false);
    if (ImGui::Checkbox("CPU Time", &cputime))
    {
        SetJsonBool(payload, "sampling_cputime", cputime);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_CPUTIME", "Sample on CPU-time timer (ITIMER_PROF)");

    bool realtime = JsonBool(payload, "sampling_realtime", false);
    if (ImGui::Checkbox("Real Time", &realtime))
    {
        SetJsonBool(payload, "sampling_realtime", realtime);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_REALTIME", "Sample on real-time timer (ITIMER_REAL)");

    bool overflow = JsonBool(payload, "sampling_overflow", false);
    if (ImGui::Checkbox("Hardware Overflow", &overflow))
    {
        SetJsonBool(payload, "sampling_overflow", overflow);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW", "Sample on hardware counter overflow");

    ImGui::Separator();

    double samp_dur = JsonDouble(payload, "sampling_duration", 0.0);
    ImGui::Text("Duration (s):");
    ImGui::SameLine();
    if (ImGui::InputDouble("##SampDur", &samp_dur, 0.0, 0.0, "%.2f"))
    {
        SetJsonDouble(payload, "sampling_duration", samp_dur);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_DURATION", "Stop sampling after N seconds (0 = unlimited)");

    long alloc_sz = JsonLong(payload, "sampling_allocator_size", 8);
    int alloc_sz_int = static_cast<int>(alloc_sz);
    ImGui::Text("Allocator Size:");
    ImGui::SameLine();
    if (ImGui::InputInt("##AllocSz", &alloc_sz_int))
    {
        SetJsonLong(payload, "sampling_allocator_size", alloc_sz_int);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_ALLOCATOR_SIZE", "Threads per background allocator");

    std::string overflow_event = JsonString(payload, "sampling_overflow_event", "perf::PERF_COUNT_HW_CACHE_REFERENCES");
    char oe_buf[256];
    std::snprintf(oe_buf, sizeof(oe_buf), "%s", overflow_event.c_str());
    ImGui::Text("Overflow Event:");
    ImGui::SameLine();
    if (ImGui::InputText("##OverflowEvt", oe_buf, sizeof(oe_buf)))
    {
        SetJsonString(payload, "sampling_overflow_event", oe_buf);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_OVERFLOW_EVENT", "Linux perf metric name for overflow sampling");

    ImGui::Separator();

    bool keep_internal = JsonBool(payload, "sampling_keep_internal", true);
    if (ImGui::Checkbox("Keep Internal Frames", &keep_internal))
    {
        SetJsonBool(payload, "sampling_keep_internal", keep_internal);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_KEEP_INTERNAL", "Show rocprof-sys frames in call stacks");

    bool include_inlines = JsonBool(payload, "sampling_include_inlines", false);
    if (ImGui::Checkbox("Include Inline Entries", &include_inlines))
    {
        SetJsonBool(payload, "sampling_include_inlines", include_inlines);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_INCLUDE_INLINES", "Include inline function entries in stacks");
}

void RocprofSysBackend::RenderRocmTab(jt::Json& payload)
{
    ImGui::Text("ROCm Domains (comma-separated):");
    std::string domains = JsonString(payload, "rocm_domains",
        "hip_runtime_api,marker_api,kernel_dispatch,memory_copy,scratch_memory");
    char domains_buf[1024];
    std::snprintf(domains_buf, sizeof(domains_buf), "%s", domains.c_str());
    if (ImGui::InputText("##RocmDomains", domains_buf, sizeof(domains_buf)))
    {
        SetJsonString(payload, "rocm_domains", domains_buf);
    }
    HelpMarker("ROCPROFSYS_ROCM_DOMAINS", "Comma-separated list of ROCm SDK domains to trace");

    ImGui::TextWrapped("Available: hip_runtime_api, marker_api, kernel_dispatch, memory_copy, "
                       "scratch_memory, page_migration, hsa_core_api, hsa_amd_ext_api, "
                       "hsa_image_ext_api, rccl_api");

    ImGui::Separator();

    std::string events = JsonString(payload, "rocm_events", "");
    char events_buf[1024];
    std::snprintf(events_buf, sizeof(events_buf), "%s", events.c_str());
    ImGui::Text("Hardware Counters:");
    if (ImGui::InputText("##RocmEvents", events_buf, sizeof(events_buf)))
    {
        SetJsonString(payload, "rocm_events", events_buf);
    }
    HelpMarker("ROCPROFSYS_ROCM_EVENTS", "HW counters (use :device=N syntax for specific GPU)");

    bool group_by_queue = JsonBool(payload, "rocm_group_by_queue", false);
    if (ImGui::Checkbox("Group by Queue", &group_by_queue))
    {
        SetJsonBool(payload, "rocm_group_by_queue", group_by_queue);
    }
    HelpMarker("ROCPROFSYS_ROCM_GROUP_BY_QUEUE", "Group by HSA queue instead of HIP stream");
}

void RocprofSysBackend::RenderPerfettoTab(jt::Json& payload)
{
    // Backend
    const char* backends[] = {"inprocess", "system", "all"};
    std::string current_backend = JsonString(payload, "perfetto_backend", "inprocess");
    int backend_idx = 0;
    for (int i = 0; i < 3; i++)
    {
        if (current_backend == backends[i])
        {
            backend_idx = i;
            break;
        }
    }
    ImGui::Text("Backend:");
    ImGui::SameLine();
    if (ImGui::Combo("##PerfBackend", &backend_idx, backends, 3))
    {
        SetJsonString(payload, "perfetto_backend", backends[backend_idx]);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BACKEND", "Perfetto tracing backend mode");

    // Buffer size
    long buf_kb = JsonLong(payload, "perfetto_buffer_size_kb", 1024000);
    int buf_kb_int = static_cast<int>(buf_kb);
    ImGui::Text("Buffer Size (KB):");
    ImGui::SameLine();
    if (ImGui::InputInt("##BufKB", &buf_kb_int, 1024, 10240))
    {
        SetJsonLong(payload, "perfetto_buffer_size_kb", buf_kb_int);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_BUFFER_SIZE_KB", "Perfetto shared memory buffer size");

    // Flush period
    long flush_ms = JsonLong(payload, "perfetto_flush_period_ms", 10000);
    int flush_ms_int = static_cast<int>(flush_ms);
    ImGui::Text("Flush Period (ms):");
    ImGui::SameLine();
    if (ImGui::InputInt("##FlushMs", &flush_ms_int, 1000, 5000))
    {
        SetJsonLong(payload, "perfetto_flush_period_ms", flush_ms_int);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FLUSH_PERIOD_MS", "Flush interval in milliseconds");

    // Fill policy
    const char* policies[] = {"discard", "fill"};
    std::string current_policy = JsonString(payload, "perfetto_fill_policy", "discard");
    int policy_idx = (current_policy == "fill") ? 1 : 0;
    ImGui::Text("Fill Policy:");
    ImGui::SameLine();
    if (ImGui::Combo("##FillPolicy", &policy_idx, policies, 2))
    {
        SetJsonString(payload, "perfetto_fill_policy", policies[policy_idx]);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILL_POLICY", "Buffer fill policy: discard old or stop writing");

    // Annotations
    bool annotations = JsonBool(payload, "perfetto_annotations", true);
    if (ImGui::Checkbox("Annotations", &annotations))
    {
        SetJsonBool(payload, "perfetto_annotations", annotations);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_ANNOTATIONS", "Function argument annotations (larger traces)");

    // Combine traces
    bool combine = JsonBool(payload, "perfetto_combine_traces", false);
    if (ImGui::Checkbox("Combine Traces", &combine))
    {
        SetJsonBool(payload, "perfetto_combine_traces", combine);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_COMBINE_TRACES", "Combine per-process traces into one file");

    ImGui::Separator();

    // Categories enable/disable
    std::string enable_cat = JsonString(payload, "enable_categories", "");
    char enable_buf[512];
    std::snprintf(enable_buf, sizeof(enable_buf), "%s", enable_cat.c_str());
    ImGui::Text("Enable Categories:");
    if (ImGui::InputText("##EnableCat", enable_buf, sizeof(enable_buf)))
    {
        SetJsonString(payload, "enable_categories", enable_buf);
    }
    HelpMarker("ROCPROFSYS_ENABLE_CATEGORIES", "Comma-separated perfetto categories to enable");

    std::string disable_cat = JsonString(payload, "disable_categories", "");
    char disable_buf[512];
    std::snprintf(disable_buf, sizeof(disable_buf), "%s", disable_cat.c_str());
    ImGui::Text("Disable Categories:");
    if (ImGui::InputText("##DisableCat", disable_buf, sizeof(disable_buf)))
    {
        SetJsonString(payload, "disable_categories", disable_buf);
    }
    HelpMarker("ROCPROFSYS_DISABLE_CATEGORIES", "Comma-separated perfetto categories to disable");

    ImGui::Separator();

    // Perfetto file
    std::string perf_file = JsonString(payload, "perfetto_file", "perfetto-trace.proto");
    char file_buf[256];
    std::snprintf(file_buf, sizeof(file_buf), "%s", perf_file.c_str());
    ImGui::Text("Output File:");
    ImGui::SameLine();
    if (ImGui::InputText("##PerfFile", file_buf, sizeof(file_buf)))
    {
        SetJsonString(payload, "perfetto_file", file_buf);
    }
    HelpMarker("ROCPROFSYS_PERFETTO_FILE", "Output filename for perfetto trace");
}

void RocprofSysBackend::RenderProcessSamplingTab(jt::Json& payload)
{
    bool cpu_freq = JsonBool(payload, "cpu_freq_enabled", false);
    if (ImGui::Checkbox("CPU Frequency / Mem / Context Switches", &cpu_freq))
    {
        SetJsonBool(payload, "cpu_freq_enabled", cpu_freq);
    }
    HelpMarker("ROCPROFSYS_CPU_FREQ_ENABLED", "Enable CPU frequency, memory, and context switch sampling");

    ImGui::Separator();

    std::string metrics = JsonString(payload, "amd_smi_metrics", "busy,temp,power,mem_usage");
    char metrics_buf[512];
    std::snprintf(metrics_buf, sizeof(metrics_buf), "%s", metrics.c_str());
    ImGui::Text("AMD SMI Metrics:");
    if (ImGui::InputText("##SmiMetrics", metrics_buf, sizeof(metrics_buf)))
    {
        SetJsonString(payload, "amd_smi_metrics", metrics_buf);
    }
    HelpMarker("ROCPROFSYS_AMD_SMI_METRICS", "Comma-separated GPU metrics (empty=all, 'none'=disable)");
    ImGui::TextWrapped("Available: busy, temp, power, mem_usage, vcn_activity, jpeg_activity, xgmi, pcie, sdma_usage");

    ImGui::Separator();

    std::string cpus = JsonString(payload, "sampling_cpus", "none");
    char cpus_buf[128];
    std::snprintf(cpus_buf, sizeof(cpus_buf), "%s", cpus.c_str());
    ImGui::Text("CPUs:");
    ImGui::SameLine();
    if (ImGui::InputText("##SampCPUs", cpus_buf, sizeof(cpus_buf)))
    {
        SetJsonString(payload, "sampling_cpus", cpus_buf);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_CPUS", "CPU list for frequency sampling ('none', 'all', or index list)");

    std::string gpus = JsonString(payload, "sampling_gpus", "all");
    char gpus_buf[128];
    std::snprintf(gpus_buf, sizeof(gpus_buf), "%s", gpus.c_str());
    ImGui::Text("GPUs:");
    ImGui::SameLine();
    if (ImGui::InputText("##SampGPUs", gpus_buf, sizeof(gpus_buf)))
    {
        SetJsonString(payload, "sampling_gpus", gpus_buf);
    }
    HelpMarker("ROCPROFSYS_SAMPLING_GPUS", "AMD SMI device indices for GPU sampling");

    bool ainic = JsonBool(payload, "use_ainic", false);
    if (ImGui::Checkbox("AI NIC Metrics", &ainic))
    {
        SetJsonBool(payload, "use_ainic", ainic);
    }
    HelpMarker("ROCPROFSYS_USE_AINIC", "Enable AI NIC metrics collection");
}

void RocprofSysBackend::RenderParallelismTab(jt::Json& payload)
{
    auto toggle = [&](char const* label, char const* key, char const* env, char const* help, bool def)
    {
        bool val = JsonBool(payload, key, def);
        if (ImGui::Checkbox(label, &val))
        {
            SetJsonBool(payload, key, val);
        }
        HelpMarker(env, help);
    };

    toggle("MPI (MPIP)", "use_mpip", "ROCPROFSYS_USE_MPIP", "Profile MPI functions", true);
    toggle("UCX", "use_ucx", "ROCPROFSYS_USE_UCX", "UCX communication wrappers", false);
    toggle("OpenSHMEM", "use_shmem", "ROCPROFSYS_USE_SHMEM", "OpenSHMEM profiling", false);
    toggle("RCCL", "use_rcclp", "ROCPROFSYS_USE_RCCLP", "RCCL collective communication profiling", false);
    toggle("OpenMP (OMPT)", "use_ompt", "ROCPROFSYS_USE_OMPT", "OpenMP Tools interface", false);
    toggle("Kokkos", "use_kokkosp", "ROCPROFSYS_USE_KOKKOSP", "Kokkos Tools callback interface", false);
}

void RocprofSysBackend::RenderInstrumentTab(jt::Json& payload)
{
    ImGui::Text("Binary Instrumentation Options");
    ImGui::Separator();

    std::string include = JsonString(payload, "instr_include", "");
    char include_buf[512];
    std::snprintf(include_buf, sizeof(include_buf), "%s", include.c_str());
    ImGui::Text("Include Regex:");
    if (ImGui::InputText("##InstrInclude", include_buf, sizeof(include_buf)))
    {
        SetJsonString(payload, "instr_include", include_buf);
    }
    HelpMarker("-I / --function-include", "Regex for functions to include in instrumentation");

    std::string exclude = JsonString(payload, "instr_exclude", "");
    char exclude_buf[512];
    std::snprintf(exclude_buf, sizeof(exclude_buf), "%s", exclude.c_str());
    ImGui::Text("Exclude Regex:");
    if (ImGui::InputText("##InstrExclude", exclude_buf, sizeof(exclude_buf)))
    {
        SetJsonString(payload, "instr_exclude", exclude_buf);
    }
    HelpMarker("-E / --function-exclude", "Regex for functions to exclude from instrumentation");

    long min_instr = JsonLong(payload, "min_instructions", 0);
    int min_instr_int = static_cast<int>(min_instr);
    ImGui::Text("Min Instructions:");
    ImGui::SameLine();
    if (ImGui::InputInt("##MinInstr", &min_instr_int))
    {
        if (min_instr_int < 0) min_instr_int = 0;
        SetJsonLong(payload, "min_instructions", min_instr_int);
    }
    HelpMarker("--min-instructions", "Minimum instruction count for a function to be instrumented");
}

void RocprofSysBackend::RenderAdvancedTab(jt::Json& payload)
{
    // Config file
    std::string cfg_file = JsonString(payload, "config_file", "");
    char cfg_buf[512];
    std::snprintf(cfg_buf, sizeof(cfg_buf), "%s", cfg_file.c_str());
    ImGui::Text("Config File:");
    ImGui::SameLine();
    if (ImGui::InputText("##CfgFile", cfg_buf, sizeof(cfg_buf)))
    {
        SetJsonString(payload, "config_file", cfg_buf);
    }
    HelpMarker("ROCPROFSYS_CONFIG_FILE", "Path to rocprof-sys configuration file");

    // Log level
    const char* levels[] = {"trace", "debug", "info", "warning", "error", "critical"};
    std::string current_level = JsonString(payload, "log_level", "info");
    int level_idx = 2;
    for (int i = 0; i < 6; i++)
    {
        if (current_level == levels[i])
        {
            level_idx = i;
            break;
        }
    }
    ImGui::Text("Log Level:");
    ImGui::SameLine();
    if (ImGui::Combo("##LogLevel", &level_idx, levels, 6))
    {
        SetJsonString(payload, "log_level", levels[level_idx]);
    }
    HelpMarker("ROCPROFSYS_LOG_LEVEL", "Logging verbosity level");

    // Log file
    std::string log_file = JsonString(payload, "log_file", "rocprof-sys-log.txt");
    char log_buf[256];
    std::snprintf(log_buf, sizeof(log_buf), "%s", log_file.c_str());
    ImGui::Text("Log File:");
    ImGui::SameLine();
    if (ImGui::InputText("##LogFile", log_buf, sizeof(log_buf)))
    {
        SetJsonString(payload, "log_file", log_buf);
    }
    HelpMarker("ROCPROFSYS_LOG_FILE", "Log file name (empty disables file logging)");

    // Tmpdir
    std::string tmpdir = JsonString(payload, "tmpdir", "");
    char tmp_buf[256];
    std::snprintf(tmp_buf, sizeof(tmp_buf), "%s", tmpdir.c_str());
    ImGui::Text("Temp Directory:");
    ImGui::SameLine();
    if (ImGui::InputText("##TmpDir", tmp_buf, sizeof(tmp_buf)))
    {
        SetJsonString(payload, "tmpdir", tmp_buf);
    }
    HelpMarker("ROCPROFSYS_TMPDIR", "Base directory for temporary/spill files");

    // Use PID
    bool use_pid = JsonBool(payload, "use_pid", true);
    if (ImGui::Checkbox("Use PID in Output", &use_pid))
    {
        SetJsonBool(payload, "use_pid", use_pid);
    }
    HelpMarker("ROCPROFSYS_USE_PID", "Suffix output filenames with process ID");

    // Timemory components
    std::string components = JsonString(payload, "timemory_components", "wall_clock");
    char comp_buf[256];
    std::snprintf(comp_buf, sizeof(comp_buf), "%s", components.c_str());
    ImGui::Text("Timemory Components:");
    if (ImGui::InputText("##TimComponents", comp_buf, sizeof(comp_buf)))
    {
        SetJsonString(payload, "timemory_components", comp_buf);
    }
    HelpMarker("ROCPROFSYS_TIMEMORY_COMPONENTS", "Comma-separated timemory component list");
}

} // namespace View
} // namespace RocProfVis
