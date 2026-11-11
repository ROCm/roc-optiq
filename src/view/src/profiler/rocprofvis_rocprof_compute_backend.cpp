// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_rocprof_compute_backend.h"
#include "../widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_settings_manager.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

namespace
{
// An enum stored as its integer value. Anything outside [0, last] - a corrupted
// profile, or one written by a newer build - falls back to the default rather
// than being cast into a value the launch would act on.
int
EnumFromInt(int value, int last, int fallback, char const* key)
{
    int result = fallback;
    if(value >= 0 && value <= last)
    {
        result = value;
    }
    else
    {
        spdlog::warn("Ignoring unknown {} value {} in a saved launch profile", key,
                     value);
    }
    return result;
}
}  // namespace

// ==================================================================================
// RocprofComputeSettings JSON
// ==================================================================================

RocprofComputeSettings
RocprofComputeSettings::FromJson(jt::Json const& json)
{
    RocprofComputeSettings s;
    jt::Json&              j = const_cast<jt::Json&>(json);

    // Profile
    s.profile_overwrite = JsonUtils::GetBool(j, "profile_overwrite", false);
    s.profile_roofline  = static_cast<Roofline>(
        EnumFromInt(JsonUtils::GetInt(j, "profile_roofline", kRooflineEnabledInclusive),
                    kRooflineDisabled, kRooflineEnabledInclusive, "profile_roofline"));
    s.profile_pc_sampling = static_cast<PcSamplingMethod>(
        EnumFromInt(JsonUtils::GetInt(j, "profile_pc_sampling", kPcSamplingDisabled),
                    kPcSamplingHostTrap, kPcSamplingDisabled, "profile_pc_sampling"));
    if(j.contains("profile_target"))
    {
        s.profile_target = TargetSpec::FromJson(j["profile_target"]);
    }

    // Analyze
    s.analyze_output_format_db = JsonUtils::GetBool(j, "analyze_output_format_db", false);
    if(j.contains("analyze_target"))
    {
        s.analyze_target = TargetSpec::FromJson(j["analyze_target"]);
    }

    if(j.contains("profile_extra_arguments"))
    {
        s.profile_extra_arguments =
            ExtraArguments::FromJson(j["profile_extra_arguments"]);
    }
    if(j.contains("analyze_extra_arguments"))
    {
        s.analyze_extra_arguments =
            ExtraArguments::FromJson(j["analyze_extra_arguments"]);
    }

    return s;
}

jt::Json
RocprofComputeSettings::ToJson() const
{
    jt::Json p;

    // Profile
    p["profile_overwrite"]   = profile_overwrite;
    p["profile_roofline"]    = static_cast<int>(profile_roofline);
    p["profile_pc_sampling"] = static_cast<int>(profile_pc_sampling);
    p["profile_target"]      = profile_target.ToJson();

    // Analyze
    p["analyze_output_format_db"] = analyze_output_format_db;
    p["analyze_target"]           = analyze_target.ToJson();

    p["profile_extra_arguments"] = profile_extra_arguments.ToJson();
    p["analyze_extra_arguments"] = analyze_extra_arguments.ToJson();

    return p;
}

// ==================================================================================
// RocprofComputeBackend core
// ==================================================================================

RocprofComputeBackend::RocprofComputeBackend()
: m_settings()
{}

const char*
RocprofComputeBackend::Id() const
{
    return "rocprof-compute";
}

const char*
RocprofComputeBackend::DisplayName() const
{
    return "ROCm Compute Profiler";
}

std::vector<ToolOption>
RocprofComputeBackend::GetTools() const
{
    return {
        { kRPVProfilerToolRocprofCompute, kRPVProfilerOperationCapture, "Profile" },
        { kRPVProfilerToolRocprofCompute, kRPVProfilerOperationAnalyze, "Analyze" },
    };
}

TabDescriptor
RocprofComputeBackend::GetOptions(rocprofvis_profiler_tool_t      tool,
                                  rocprofvis_profiler_operation_t operation) const
{
    (void) tool;
    return { "profiling_options", "Profiling Options", [this, operation]() {
                return const_cast<RocprofComputeBackend*>(this)->RenderProfilingOptions(
                    operation);
            } };
}

std::vector<TabDescriptor>
RocprofComputeBackend::GetAdvancedOptions(rocprofvis_profiler_tool_t      tool,
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
RocprofComputeBackend::Validate(LaunchConfig const& config) const
{
    std::string error;
    if(config.operation == kRPVProfilerOperationAnalyze)
    {
        if(m_settings.analyze_target.path.empty())
        {
            error = "Workload folder is required";
        }
    }
    else if(m_settings.profile_target.path.empty())
    {
        error = "Target executable is required";
    }
    else if(m_settings.profile_target.output_directory.empty())
    {
        error = "Output directory is required";
    }
    return error;
}

// ==================================================================================
// Target section
// ==================================================================================

TargetSpec const&
RocprofComputeBackend::GetTarget(rocprofvis_profiler_operation_t operation) const
{
    return operation == kRPVProfilerOperationAnalyze ? m_settings.analyze_target
                                                     : m_settings.profile_target;
}

ExtraArguments&
RocprofComputeBackend::GetExtraArguments(rocprofvis_profiler_operation_t operation)
{
    return operation == kRPVProfilerOperationAnalyze ? m_settings.analyze_extra_arguments
                                                     : m_settings.profile_extra_arguments;
}

bool
RocprofComputeBackend::RenderTargetSection(LaunchConfig const&   config,
                                           TargetBrowseFn const& on_browse)
{
    const bool                is_remote = (config.connection == ConnectionType::kSsh);
    std::vector<std::string>& recents   = GetRecentTargets(config.tool, config.operation);

    bool modified = false;
    if(config.operation == kRPVProfilerOperationAnalyze)
    {
        // Analyze reads the workload folder Profile wrote and writes its
        // database to the working directory, so there is no program, argument
        // or output folder to set.
        TargetPathRow workload;
        workload.label = "Workload";
        workload.id    = "WorkloadDir";
        workload.hint  = is_remote ? "remote folder for results" : "folder for results";
        workload.browse_title    = "Choose Workload";
        workload.kind            = Directory;
        workload.recents         = &recents;
        workload.recents_tooltip = "Recent workloads";

        modified |= RenderTargetPathRow(workload, m_settings.analyze_target.path,
                                        config.connection, on_browse,
                                        TargetLabelColumnWidth("Workload"));
        modified |= RenderTargetAutoLoadRow(m_settings.analyze_target.auto_load_trace);
    }
    else
    {
        const float label_w = TargetLabelColumnWidth("Output folder");

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
        output.label = "Output folder";
        output.id    = "OutputDir";
        output.hint  = is_remote ? "remote folder for results" : "folder for results";
        output.browse_title = "Choose Output Directory";
        output.kind         = Directory;

        modified |= RenderTargetPathRow(program, m_settings.profile_target.path,
                                        config.connection, on_browse, label_w);
        modified |=
            RenderTargetArgumentsRow(m_settings.profile_target.arguments, label_w);
        modified |=
            RenderTargetPathRow(output, m_settings.profile_target.output_directory,
                                config.connection, on_browse, label_w);
    }
    return modified;
}

// Profile (programs) and Analyze (workload folders) keep separate lists.
std::vector<std::string>&
RocprofComputeBackend::GetRecentTargets(rocprofvis_profiler_tool_t      tool,
                                        rocprofvis_profiler_operation_t operation)
{
    return SettingsManager::Get().GetRecentTargets(Id(), tool, operation);
}

void
RocprofComputeBackend::LoadSettings(jt::Json const& payload)
{
    m_settings = RocprofComputeSettings::FromJson(payload);
}

jt::Json
RocprofComputeBackend::SaveSettings() const
{
    return m_settings.ToJson();
}

// ==================================================================================
// Warnings & hints
// ==================================================================================

std::vector<WarningMessage>
RocprofComputeBackend::GetWarnings(LaunchConfig const& config) const
{
    std::vector<WarningMessage> warnings;
    if(m_settings.profile_pc_sampling)
    {
        warnings.push_back({WarningMessage::kWarning,
            "Performance counter collection is skipped when using PC sampling."});
    }
    return warnings;
}

void
RocprofComputeBackend::FlattenToExecution(
    LaunchConfig const& config, std::vector<std::pair<std::string, std::string>>& env_out,
    std::vector<std::string>& argv_out) const
{
    (void) env_out;
    if(config.operation == kRPVProfilerOperationCapture)
    {
        argv_out.push_back("profile");
        if(!m_settings.profile_target.output_directory.empty())
        {
            argv_out.push_back("--output-directory");
            argv_out.push_back(m_settings.profile_target.output_directory);
        }
        if(m_settings.profile_overwrite)
        {
            argv_out.push_back("--overwrite");
        }
        if(m_settings.profile_roofline)
        {
            argv_out.push_back(
                m_settings.profile_roofline ==
                        RocprofComputeSettings::Roofline::kRooflineEnabledExclusive
                    ? "--roof-only"
                    : "--no-roof");
        }
        if(m_settings.profile_pc_sampling)
        {
            argv_out.push_back("--experimental");
            argv_out.push_back("--pc-sampling");
            argv_out.push_back("--pc-sampling-method");
            argv_out.push_back(
                m_settings.profile_pc_sampling ==
                        RocprofComputeSettings::PcSamplingMethod::kPcSamplingStochastic
                    ? "stochastic"
                    : "host_trap");
        }
        m_settings.profile_extra_arguments.Argv(argv_out);
        m_settings.profile_target.Argv(true, argv_out);
    }
    else if(config.operation == kRPVProfilerOperationAnalyze)
    {
        argv_out.push_back("analyze");
        if(m_settings.analyze_output_format_db)
        {
            argv_out.push_back("--output-format");
            argv_out.push_back("db");
        }
        m_settings.analyze_extra_arguments.Argv(argv_out);
        if(m_settings.analyze_target.path.size())
        {
            argv_out.push_back("--path");
            m_settings.analyze_target.Argv(false, argv_out);
        }
    }
}

// ==================================================================================
// ExportCfg
// ==================================================================================

std::string
RocprofComputeBackend::ExportCfg() const
{
    // Nothing calls this yet...
    return "";
}

bool
RocprofComputeBackend::RenderProfilingOptions(rocprofvis_profiler_operation_t operation)
{
    bool changed = false;
    if(operation == kRPVProfilerOperationCapture)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::BeginGroup();
        LaunchSubHeader("OUTPUT");
        changed |= ToggleSwitch("Overwrite", &m_settings.profile_overwrite);
        HelpMarker("--overwrite", "Allow outputting into a non-empty path.");
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, style.ItemSpacing.x + style.IndentSpacing);
        ImGui::BeginGroup();
        LaunchSubHeader("COLLECTION");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Roofline");
        ImGui::SameLine();
        int roofline = static_cast<int>(m_settings.profile_roofline);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Roofline Only").x +
                                2.0f * style.FramePadding.x + ImGui::GetFrameHeight());
        if(ImGui::Combo("##roofline", &roofline, "Enabled\0Roofline Only\0Disabled\0\0"))
        {
            m_settings.profile_roofline =
                static_cast<RocprofComputeSettings::Roofline>(roofline);
            changed = true;
        }
        HelpMarker("Roofline", "Include roofline during profiling, exclusively profile "
                               "for roofline, or disable roofline during profiling.");
        LaunchSubHeader("EXPERIMENTAL");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("PC sampling");
        ImGui::SameLine();
        int pcsampling = static_cast<int>(m_settings.profile_pc_sampling);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Stochastic").x +
                                2.0f * style.FramePadding.x + ImGui::GetFrameHeight());
        if(ImGui::Combo("##pcsampling", &pcsampling,
                        "Disabled\0Stochastic\0Host trap\0\0"))
        {
            m_settings.profile_pc_sampling =
                static_cast<RocprofComputeSettings::PcSamplingMethod>(pcsampling);
            changed = true;
        }
        HelpMarker("Pc sampling",
                   "Enable/Disable PC sampling and specify the collection method.");
        ImGui::EndGroup();
    }
    else if(operation == kRPVProfilerOperationAnalyze)
    {
        ImGui::BeginGroup();
        LaunchSubHeader("OUTPUT_FORMAT");
        changed |= ToggleSwitch("ROCpd database", &m_settings.analyze_output_format_db);
        HelpMarker("--output-format db", "Enable ROCpd SQLite output.");
        ImGui::EndGroup();
    }
    return changed;
}

}  // namespace View
}  // namespace RocProfVis
