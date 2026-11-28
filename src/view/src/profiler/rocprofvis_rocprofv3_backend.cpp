// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_rocprofv3_backend.h"
#include "../widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <array>

namespace RocProfVis
{
namespace View
{

// ==================================================================================
// RocprofV3Settings JSON
// ==================================================================================
namespace
{
struct TracingProp
{
    const char* display_name;
    const char* tooltip;
    const char* argv;
    const char* json_key;
    bool        default_value;
};
struct WindowUnitProp
{
    const char* display_name;
    const char* argv;
};

constexpr std::array<TracingProp, RocprofV3Settings::Tracing::kTracingLast>
    TRACING_PROPS = {
        { { "Runtime Trace",
            "Collects tracing data for HIP runtime API, marker (ROCTx) API, RCCL API, "
            "memory operations (copies, scratch, and allocation), and kernel dispatches. "
            "Similar to --sys-trace without HIP compiler API and the underlying HSA API "
            "tracing.",
            "--runtime-trace", "collection_runtime_trace", true },
          { "System Trace",
            "Collects tracing data for HIP API, HSA API, marker (ROCTx) API, RCCL API, "
            "memory operations (copies, scratch, and allocations), and kernel "
            "dispatches.",
            "--sys-trace", "collection_system_trace", false },
          { "HIP Trace",
            "Combination of --hip-runtime-trace and --hip-compiler-trace. This option "
            "enables only the HIP API tracing. Unlike previous iterations of rocprofv3, "
            "this option doesn’t enable kernel tracing, memory copy tracing, and so on.",
            "--hip-trace", "collection_hip_trace", false },
          { "Marker Trace",
            "Collects marker (ROCTx) traces. Similar to --roctx-trace option in earlier "
            "rocprof versions, but with improved ROCTx library with more features.",
            "--marker-trace", "collection_marker_trace", false },
          { "Kernel Trace", "Collects kernel dispatch traces.", "--kernel-trace",
            "collection_kernel_trace", false },
          { "Memory Copy Trace", "Collects memory copy traces.", "--memory-copy-trace",
            "collection_memory_copy_trace", false },
          { "Memory Allocation Trace",
            "Collects memory allocation traces. Displays starting address, allocation "
            "size, and the agent where allocation occurs.",
            "--memory-allocation-trace", "collection_memory_allocation_trace", false },
          { "KFD Trace",
            "Collects --kfd-page-migration-trace, --kfd-page-mapping-trace, "
            "--kfd-queue-trace, and --kfd-dropped-events-trace. KFD (Kernel Fusion "
            "Driver) traces capture low-level driver routines including mapping, "
            "unmapping, and migration of data between GPU and system memories, as well "
            "as eviction or restoration of GPU queues to facilitate such routines.",
            "--kfd-trace", "kfd_trace", false },
          { "Scratch Memory Trace",
            "Collects scratch memory operations traces. Helps in determining scratch "
            "allocations and manage them efficiently.",
            "--scratch-memory-trace", "collection_scratch_memory_trace", false },
          { "HSA Trace",
            "Collects --hsa-core-trace, --hsa-amd-trace, --hsa-image-trace, and "
            "--hsa-finalizer-trace. This option only enables the HSA API tracing. Unlike "
            "previous iterations of rocprof, this doesn’t enable kernel tracing, memory "
            "copy tracing, and so on.",
            "--hsa-trace", "collection_hsa_trace", false },
          { "RCCL Trace",
            "Collects traces for RCCL (ROCm Communication Collectives Library), which is "
            "also pronounced as ‘Rickle’.",
            "--rccl-trace", "collection_rccl_trace", false },
          { "Kokkos Trace",
            "Enables builtin Kokkos tools support, which implies enabling --marker-trace "
            "collection and --kernel-rename.",
            "--kokkos-trace", "collection_kokkos_trace", false },
          { "RocDecode Trace", "Collects traces for rocDecode APIs.", "--rocdecode-trace",
            "collection_rocdecode_trace", false },
          { "HIP Event Trace",
            "Traces GPU-side HIP event barriers produced by hipEventRecord and "
            "hipStreamWaitEvent. Automatically enabled by --hip-trace and "
            "--hip-runtime-trace.",
            "--hip-event-trace", "collection_hip_event_trace", false },
          { "HIP FILE Trace", "Collects traces for hipFILE APIs.", "--hipfile-trace",
            "collection_hipfile_trace", false } }
    };
constexpr std::array<WindowUnitProp, RocprofV3Settings::WindowUnit::kWindowUnitLast>
    WINDOWUNIT_PROPS = { { { "Nanoseconds", "nsec" },
                           { "Microseconds", "usec" },
                           { "Milliseconds", "msec" },
                           { "Seconds", "sec" },
                           { "Minutes", "min" },
                           { "Hours", "hour" } } };
}  // namespace

RocprofV3Settings
RocprofV3Settings::FromJson(jt::Json const& json)
{
    RocprofV3Settings s;
    jt::Json&         j = const_cast<jt::Json&>(json);

    for(size_t i = 0; i < kTracingLast; ++i)
    {
        s.collection_tracing[i] = JsonUtils::GetBool(j, TRACING_PROPS[i].json_key,
                                                     TRACING_PROPS[i].default_value);
    }
    // A Runtime or System trace stands alone, as the Trace combo shows it.
    if(s.collection_tracing[kAggregateRuntimeTrace])
    {
        s.collection_tracing.reset();
        s.collection_tracing.set(kAggregateRuntimeTrace);
    }
    else if(s.collection_tracing[kAggregateSysTrace])
    {
        s.collection_tracing.reset();
        s.collection_tracing.set(kAggregateSysTrace);
    }
    s.collection_counters = JsonUtils::GetString(j, "collection_counters", "");

    s.window_enabled = JsonUtils::GetBool(j, "window_enabled", false);
    s.window_delay =
        static_cast<uint32_t>(std::max(JsonUtils::GetInt(j, "window_delay", 0), 0));
    s.window_duration =
        static_cast<uint32_t>(std::max(JsonUtils::GetInt(j, "window_duration", 0), 0));
    s.window_repeat =
        static_cast<uint32_t>(std::max(JsonUtils::GetInt(j, "window_repeat", 0), 0));
    s.window_unit = static_cast<WindowUnit>(
        JsonUtils::GetEnum(j, "window_unit", kSeconds, kWindowUnitLast));

    if(j.contains("target"))
    {
        s.target = TargetSpec::FromJson(j["target"]);
    }
    if(j.contains("extra_arguments"))
    {
        s.extra_arguments = ExtraArguments::FromJson(j["extra_arguments"]);
    }

    return s;
}

jt::Json
RocprofV3Settings::ToJson() const
{
    jt::Json p;

    for(size_t i = 0; i < kTracingLast; ++i)
    {
        p[TRACING_PROPS[i].json_key] = collection_tracing[i];
    }
    p["collection_counters"] = collection_counters;

    p["window_enabled"]  = window_enabled;
    p["window_delay"]    = window_delay;
    p["window_duration"] = window_duration;
    p["window_repeat"]   = window_repeat;
    p["window_unit"]     = static_cast<int>(window_unit);

    p["target"]          = target.ToJson();
    p["extra_arguments"] = extra_arguments.ToJson();

    return p;
}

// ==================================================================================
// RocprofV3Backend core
// ==================================================================================

RocprofV3Backend::RocprofV3Backend()
: m_settings()
{}

const char*
RocprofV3Backend::Id() const
{
    return "rocprofv3";
}

const char*
RocprofV3Backend::DisplayName() const
{
    return "ROCprofiler-SDK";
}

std::vector<ToolOption>
RocprofV3Backend::GetTools() const
{
    return {
        { kRPVProfilerToolRocprofV3, kRPVProfilerOperationDefault, "rocprofv3" },
    };
}

TabDescriptor
RocprofV3Backend::GetOptions(rocprofvis_profiler_tool_t      tool,
                             rocprofvis_profiler_operation_t operation) const
{
    (void) tool;
    return { "profiling_options", "Profiling Options", [this, operation]() {
                return const_cast<RocprofV3Backend*>(this)->RenderProfilingOptions(
                    operation);
            } };
}

std::vector<TabDescriptor>
RocprofV3Backend::GetAdvancedOptions(rocprofvis_profiler_tool_t      tool,
                                     rocprofvis_profiler_operation_t operation) const
{
    (void) tool;
    (void) operation;
    return {};
}

// ==================================================================================
// Validation
// ==================================================================================

std::string
RocprofV3Backend::Validate(LaunchConfig const& config) const
{
    std::string error;
    if(m_settings.target.path.empty())
    {
        error = "Target executable is required";
    }
    else if(m_settings.target.output_directory.empty())
    {
        error = "Output directory is required";
    }
    return error;
}

// ==================================================================================
// Target section
// ==================================================================================

TargetSpec const&
RocprofV3Backend::GetTarget(rocprofvis_profiler_operation_t operation) const
{
    (void) operation;
    return m_settings.target;
}

ExtraArguments&
RocprofV3Backend::GetExtraArguments(rocprofvis_profiler_operation_t operation)
{
    (void) operation;
    return m_settings.extra_arguments;
}

float
RocprofV3Backend::MinWidth() const
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    return ImGui::CalcTextSize("TraceMemory Allocation TraceDuration").x +
           ImGui::CalcTextSize(
               WINDOWUNIT_PROPS[static_cast<size_t>(RocprofV3Settings::kMicroseconds)]
                   .display_name)
               .x +
           2.0f * (ImGui::GetTextLineHeight() + ImGui::GetFrameHeight()) +
           4.0f * style.FramePadding.x + 5.0f * style.ItemSpacing.x + style.IndentSpacing;
}

bool
RocprofV3Backend::RenderTargetSection(LaunchConfig const&   config,
                                      TargetBrowseFn const& on_browse)
{
    const bool                is_remote = (config.connection == ConnectionType::kSsh);
    std::vector<std::string>& recents   = GetProfilerRecentTargets(config);

    bool        modified = false;
    const float label_w  = TargetLabelColumnWidth("Output folder");

    TargetPathRow program;
    program.label = "Program";
    program.id    = "TargetExe";
    program.hint =
        is_remote ? "/remote/path/to/app" : "/path/to/app  (or a name on $PATH)";
    program.browse_title    = "Choose Program";
    program.kind            = File;
    program.recents         = &recents;
    program.recents_tooltip = "Recent programs";

    TargetPathRow output;
    output.label        = "Output folder";
    output.id           = "OutputDir";
    output.hint         = is_remote ? "remote folder for results" : "folder for results";
    output.browse_title = "Choose Output Directory";
    output.kind         = Directory;

    modified |= RenderTargetPathRow(program, m_settings.target.path, on_browse, label_w);
    modified |= RenderTargetArgumentsRow(m_settings.target.arguments, label_w);
    modified |= RenderTargetPathRow(output, m_settings.target.output_directory, on_browse,
                                    label_w);
    modified |= RenderTargetAutoLoadRow(m_settings.target.auto_load_trace);
    return modified;
}

std::vector<std::string>&
RocprofV3Backend::GetProfilerRecentTargets(LaunchConfig const& config)
{
    return SettingsManager::Get().GetProfilerRecentTargets(
        config.connection, config.ssh_connection_ref, Id(), config.tool,
        config.operation);
}

void
RocprofV3Backend::LoadSettings(jt::Json const& payload)
{
    m_settings = RocprofV3Settings::FromJson(payload);
}

jt::Json
RocprofV3Backend::SaveSettings() const
{
    return m_settings.ToJson();
}

// ==================================================================================
// Warnings & hints
// ==================================================================================

std::vector<WarningMessage>
RocprofV3Backend::GetWarnings(LaunchConfig const& config) const
{
    std::vector<WarningMessage> warnings;
    if(m_settings.collection_tracing.none() && m_settings.collection_counters.empty() &&
       m_settings.extra_arguments.argv.empty())
    {
        warnings.push_back(
            { WarningMessage::kWarning, "No trace options or counters selected." });
    }
    return warnings;
}

void
RocprofV3Backend::FlattenToExecution(
    LaunchConfig const& config, std::vector<std::pair<std::string, std::string>>& env_out,
    std::vector<std::string>& argv_out) const
{
    (void) config;
    (void) env_out;
    if(!m_settings.target.output_directory.empty())
    {
        argv_out.push_back("--output-directory");
        argv_out.push_back(m_settings.target.output_directory);
    }
    for(size_t i = 0; i < RocprofV3Settings::kTracingLast; i++)
    {
        if(m_settings.collection_tracing[i])
        {
            argv_out.push_back(TRACING_PROPS[i].argv);
        }
    }
    if(m_settings.collection_counters.size())
    {
        argv_out.push_back("--pmc");
        argv_out.push_back(m_settings.collection_counters);
    }
    if(m_settings.window_enabled)
    {
        argv_out.push_back("--collection-period");
        argv_out.push_back(std::to_string(m_settings.window_delay) + ":" +
                           std::to_string(m_settings.window_duration) + ":" +
                           std::to_string(m_settings.window_repeat));
        argv_out.push_back("--collection-period-unit");
        argv_out.push_back(
            WINDOWUNIT_PROPS[static_cast<size_t>(m_settings.window_unit)].argv);
    }
    m_settings.extra_arguments.Argv(argv_out);
    m_settings.target.Argv(true, argv_out);
}

// ==================================================================================
// ExportCfg
// ==================================================================================

std::string
RocprofV3Backend::ExportCfg() const
{
    // Nothing calls this yet...
    return "";
}

bool
RocprofV3Backend::RenderProfilingOptions(rocprofvis_profiler_operation_t operation)
{
    (void) operation;
    bool              changed = false;
    const ImGuiStyle& style   = SettingsManager::GetInstance().GetDefaultStyle();
    ImFont*           icons =
        SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon);
    ImGui::BeginGroup();
    float trace_width = ImGui::CalcTextSize("Memory Allocation Trace").x +
                        2.0f * style.FramePadding.x + ImGui::GetFrameHeight();
    LaunchSubHeader("COLLECTION");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Trace");
    ImGui::SameLine();
    const bool custom =
        m_settings.collection_tracing.any() &&
        !(m_settings.collection_tracing[RocprofV3Settings::kAggregateRuntimeTrace] ||
          m_settings.collection_tracing[RocprofV3Settings::kAggregateSysTrace]);
    ImGui::SetNextItemWidth(trace_width);
    if(ImGui::BeginCombo(
           "##collection_tracing",
           m_settings.collection_tracing.none() ? "None"
           : m_settings.collection_tracing[RocprofV3Settings::kAggregateRuntimeTrace]
               ? TRACING_PROPS[RocprofV3Settings::kAggregateRuntimeTrace].display_name
           : m_settings.collection_tracing[RocprofV3Settings::kAggregateSysTrace]
               ? TRACING_PROPS[RocprofV3Settings::kAggregateSysTrace].display_name
               : "[Custom]"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, custom
                                                     ? ImGui::GetStyle().Alpha *
                                                           ImGui::GetStyle().DisabledAlpha
                                                     : ImGui::GetStyle().Alpha);
        if(ImGui::Selectable(
               TRACING_PROPS[RocprofV3Settings::kAggregateRuntimeTrace].display_name,
               m_settings.collection_tracing[RocprofV3Settings::kAggregateRuntimeTrace]))
        {
            m_settings.collection_tracing.reset();
            m_settings.collection_tracing.set(RocprofV3Settings::kAggregateRuntimeTrace);
            changed = true;
        }
        if(ImGui::Selectable(
               TRACING_PROPS[RocprofV3Settings::kAggregateSysTrace].display_name,
               m_settings.collection_tracing[RocprofV3Settings::kAggregateSysTrace]))
        {
            m_settings.collection_tracing.reset();
            m_settings.collection_tracing.set(RocprofV3Settings::kAggregateSysTrace);
            changed = true;
        }
        if(ImGui::Selectable("None", m_settings.collection_tracing.none()))
        {
            m_settings.collection_tracing.reset();
            changed = true;
        }
        ImGui::PopStyleVar();
        ImGui::SeparatorText("Custom");
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            custom ? ImGui::GetStyle().Alpha
                                   : ImGui::GetStyle().Alpha *
                                         ImGui::GetStyle().DisabledAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        for(size_t i = RocprofV3Settings::kAggregateTraceLast;
            i < RocprofV3Settings::kTracingLast; i++)
        {
            bool set = m_settings.collection_tracing[i];
            if(ImGui::Checkbox(TRACING_PROPS[i].display_name, &set))
            {
                m_settings.collection_tracing.reset(
                    RocprofV3Settings::kAggregateSysTrace);
                m_settings.collection_tracing.reset(
                    RocprofV3Settings::kAggregateRuntimeTrace);
                m_settings.collection_tracing.flip(i);
                changed = true;
            }
        }
        ImGui::PopStyleVar(2);
        ImGui::EndCombo();
    }
    HelpMarker([this]() {
        if(m_settings.collection_tracing.none())
        {
            ImGui::TextUnformatted("None selected.");
        }
        else
        {
            for(size_t i = 0; i < RocprofV3Settings::kTracingLast; i++)
            {
                if(m_settings.collection_tracing[i])
                {
                    ImGui::TextUnformatted(TRACING_PROPS[i].argv);
                    ImGui::Separator();
                    ImGui::TextUnformatted(TRACING_PROPS[i].tooltip);
                }
            }
        }
    });
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Counters");
    ImGui::SameLine();
    std::pair<bool, bool> counters = InputTextWithClear(
        "##Counters", "SQ_WAVES, TCC_MISS", m_settings.collection_counters, icons,
        SettingsManager::GetInstance().GetColor(Colors::kButton), style,
        ImGui::CalcTextSize("Trace").x + trace_width - ImGui::CalcTextSize("Counters").x);
    if(counters.first)
    {
        changed = true;
    }
    else if(counters.second)
    {
        m_settings.collection_counters.clear();
        changed = true;
    }
    HelpMarker("--pmc", "Specifies performance monitoring counters to be collected. Use "
                        "comma or space to specify more than one counter.");
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, style.ItemSpacing.x + style.IndentSpacing);
    ImGui::BeginGroup();
    float input_width =
        ImGui::CalcTextSize(
            WINDOWUNIT_PROPS[static_cast<size_t>(RocprofV3Settings::kMicroseconds)]
                .display_name)
            .x +
        ImGui::GetFrameHeight() + 2.0f * style.FramePadding.x;
    LaunchSubHeader("TRACE WINDOW");
    changed |= ToggleSwitch("Enabled", &m_settings.window_enabled);
    HelpMarker("--collection-period", "The collection period is the time interval during "
                                      "which the profiling data is collected.");
    ImGui::BeginDisabled(!m_settings.window_enabled);
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Delay");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Duration");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Repeat");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Unit");
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(input_width);
    int i_val = m_settings.window_delay;
    if(ImGui::InputInt("##Delay", &i_val, 0))
    {
        m_settings.window_delay = static_cast<uint32_t>(std::max(i_val, 0));
        changed                 = true;
    }
    HelpMarker("--collection-period", "Time before the data collection begins.");
    ImGui::SetNextItemWidth(input_width);
    i_val = m_settings.window_duration;
    if(ImGui::InputInt("##Duration", &i_val, 0))
    {
        m_settings.window_duration = static_cast<uint32_t>(std::max(i_val, 0));
        changed                    = true;
    }
    HelpMarker("--collection-period", "Duration of data collection.");
    ImGui::SetNextItemWidth(input_width);
    i_val = m_settings.window_repeat;
    if(ImGui::InputInt("##Repeat", &i_val, 0))
    {
        m_settings.window_repeat = static_cast<uint32_t>(std::max(i_val, 0));
        changed                  = true;
    }
    HelpMarker("--collection-period",
               "Number of times the data collection cycle runs. 0 repeats it until "
               "the application exits.");
    ImGui::SetNextItemWidth(input_width);
    if(ImGui::BeginCombo(
           "##Unit",
           WINDOWUNIT_PROPS[static_cast<size_t>(m_settings.window_unit)].display_name))
    {
        for(size_t i = 0; i < RocprofV3Settings::kWindowUnitLast; i++)
        {
            if(ImGui::Selectable(WINDOWUNIT_PROPS[i].display_name,
                                 static_cast<size_t>(m_settings.window_unit) == i))
            {
                m_settings.window_unit = static_cast<RocprofV3Settings::WindowUnit>(i);
                changed                = true;
            }
        }
        ImGui::EndCombo();
    }
    HelpMarker("--collection-period-unit",
               "Change the unit of time used in --collection-period");
    ImGui::EndGroup();
    ImGui::EndDisabled();
    ImGui::EndGroup();
    return changed;
}

}  // namespace View
}  // namespace RocProfVis
