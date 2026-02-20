// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_process_executor.h"
#include "widgets/rocprofvis_widget.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RocProfVis
{
namespace View
{

class ProfilingDialog : public RocWidget
{
public:
    enum class ExecutionMode
    {
        Local,
        Remote,
        SLURM  // Submit to SLURM job scheduler
    };

    enum class JobScheduler
    {
        None,
        SLURM,
        PBS,
        LSF
    };

    enum class ProfilingTool
    {
        RocProfV3,
        RocProfCompute,
        RocProfSys
    };

    enum class LLMProvider
    {
        Local,      // No LLM, just local analysis
        Anthropic,  // Claude
        OpenAI      // GPT
    };

    enum class ContainerRuntime
    {
        None,
        Docker,
        Podman,
        Singularity
    };

    struct ContainerInfo
    {
        std::string id;
        std::string name;
        std::string image;
        std::string status;
        ContainerRuntime runtime;
    };

    enum class DialogScreen
    {
        ModeSelection,
        LocalConfig,
        RemoteConfig,
        SLURMConfig,
        BatchJobSubmission,
        Progress,
        Complete
    };

    enum class ProgressStage
    {
        None,
        Profiling,
        Merging,
        AIAnalysis,
        CopyingFiles,
        Complete
    };

    struct ProfilingConfig
    {
        ExecutionMode mode = ExecutionMode::Local;
        ProfilingTool tool = ProfilingTool::RocProfV3;

        // Profiling tool arguments (e.g., --sys-trace, --hip-trace)
        std::string tool_args;

        // Local execution
        std::string application_path;
        std::string application_args;
        std::string output_directory;

        // Remote execution
        std::string ssh_host;
        std::string ssh_user;
        int ssh_port = 22;
        std::string ssh_key_path;        // Optional SSH key file path
        std::string ssh_proxy_jump;      // ProxyJump/bastion host (e.g., user@bastion:port)
        std::string ssh_options;         // Custom SSH options (additional)
        std::string remote_app_path;
        std::string remote_args;
        std::string remote_output_path;
        bool copy_results_back = true;

        // Advanced options
        std::map<std::string, std::string> environment_vars;
        std::string pre_commands;  // Custom commands to run before profiling

        // Container options
        bool use_container = false;
        ContainerRuntime container_runtime = ContainerRuntime::None;
        std::string container_id;           // Container ID or name
        std::string container_image;        // Image name (for display)
        std::string container_exec_options; // Additional exec options

        // SLURM options
        JobScheduler scheduler = JobScheduler::None;
        std::string slurm_partition;        // Partition name
        std::string slurm_account;          // Account for billing
        int slurm_nodes = 1;
        int slurm_ntasks = 1;
        int slurm_cpus_per_task = 1;
        int slurm_gpus = 1;
        std::string slurm_time = "01:00:00"; // Wall time
        std::string slurm_job_name = "rocprof_job";
        std::string slurm_output_file;      // Job output file
        std::string slurm_extra_args;       // Additional sbatch arguments
        std::string slurm_job_id;           // Job ID after submission (for tracking)

        // AI Analysis options
        bool run_ai_analysis = true;  // Whether to run AI analysis for this profiling session
        LLMProvider llm_provider = LLMProvider::Local;
        std::string llm_api_key;
    };

    ProfilingDialog();
    ~ProfilingDialog();

    void Show();
    void Hide();
    bool IsVisible() const;

    void Render() override;

    // Callback when profiling completes successfully
    void SetCompletionCallback(std::function<void(const std::string& trace_path, const std::string& ai_json_path)> callback);

    // Show dialog pre-populated with an existing config (for re-running with different tool args)
    void ShowWithConfig(const ProfilingConfig& config);

    // Get the current profiling config (for storing after successful profiling)
    const ProfilingConfig& GetConfig() const { return m_config; }

private:
    void RenderModeSelection();
    void RenderLocalConfig();
    void RenderRemoteConfig();
    void RenderSLURMConfig();
    void RenderBatchJobSubmission();
    void RenderProgress();
    void RenderComplete();
    void RenderProgressStages();  // Render the stage visualization

    void ExecuteProfiling();
    void OnProfilingComplete(const Controller::ProcessExecutor::ExecutionResult& result);
    void MergeTraces();
    void OnMergeComplete(const Controller::ProcessExecutor::ExecutionResult& result);
    void TestSSHConnection();
    void RunAIAnalysis();
    void FindAndCopyRemoteTraceFile();
    void CopyRemoteFileBack();

    // Container detection and management
    void DetectContainers();
    void DetectDockerContainers();
    void DetectPodmanContainers();
    void DetectSingularityContainers();
    std::string WrapCommandForContainer(const std::string& command) const;

    // SLURM job scheduler support
    void DetectJobScheduler();
    std::string BuildSLURMBatchScript() const;
    void SubmitSLURMJob();
    void MonitorSLURMJob();
    void CancelSLURMJob();
    void RetrieveSLURMResults();

    // Template and batch job support
    void SaveConfigAsTemplate(const std::string& template_name);
    void LoadTemplate(const std::string& template_name);
    std::vector<std::string> GetSavedTemplates();
    void DeleteTemplate(const std::string& template_name);
    void SubmitBatchJobs();

    std::string GetToolCommand() const;
    std::string GetToolName() const;
    std::string FindTraceFile(const std::string& output) const;

    bool m_visible;
    DialogScreen m_current_screen;
    ProfilingConfig m_config;

    // UI state
    char m_app_path_buffer[512];
    char m_app_args_buffer[1024];
    char m_tool_args_buffer[512];
    char m_output_dir_buffer[512];
    char m_ssh_host_buffer[256];
    char m_ssh_user_buffer[128];
    int m_ssh_port_input;
    char m_remote_app_buffer[512];
    char m_remote_args_buffer[1024];
    char m_remote_output_buffer[512];
    char m_ssh_key_path_buffer[512];
    char m_ssh_proxy_jump_buffer[256];
    char m_ssh_options_buffer[512];
    char m_llm_api_key_buffer[256];
    char m_connection_name_buffer[128];
    char m_pre_commands_buffer[2048];  // For custom pre-profiling commands

    // Environment variables UI
    char m_env_var_key_buffer[128];
    char m_env_var_value_buffer[512];

    // Recent connections
    int m_selected_recent_connection;

    // Container detection
    std::vector<ContainerInfo> m_detected_containers;
    int m_selected_container_index;
    bool m_containers_loaded;
    bool m_container_detection_running;
    char m_container_exec_options_buffer[512];

    // SLURM UI state
    char m_slurm_partition_buffer[128];
    char m_slurm_account_buffer[128];
    int m_slurm_nodes_input;
    int m_slurm_ntasks_input;
    int m_slurm_cpus_per_task_input;
    int m_slurm_gpus_input;
    char m_slurm_time_buffer[32];
    char m_slurm_job_name_buffer[128];
    char m_slurm_extra_args_buffer[512];
    bool m_slurm_available;
    bool m_job_monitoring_active;
    std::string m_job_status;

    // Batch job submission
    std::vector<ProfilingConfig> m_batch_jobs;
    char m_template_name_buffer[128];
    int m_selected_template_index;
    std::vector<std::string> m_available_templates;
    int m_batch_job_count;

    // Progress tracking
    std::shared_ptr<Controller::ProcessExecutor::AsyncContext> m_execution_context;
    std::string m_progress_output;
    bool m_execution_running;
    bool m_ssh_test_running;
    std::string m_ssh_test_result;
    std::string m_ssh_test_details;  // Detailed diagnostic information
    ProgressStage m_current_stage;   // Current stage in the workflow

    // Results
    std::string m_result_trace_path;
    std::string m_ai_analysis_json_path;
    bool m_execution_success;
    std::string m_error_message;
    std::string m_current_run_id;  // Unique ID for current profiling run

    std::function<void(const std::string&, const std::string&)> m_completion_callback;
};

}  // namespace View
}  // namespace RocProfVis
