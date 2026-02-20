// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_controller_process_executor.h"

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace RocProfVis
{
namespace View
{

enum class JobState
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled,
    Unknown
};

struct JobInfo
{
    std::string job_id;
    std::string job_name;
    std::string partition;
    std::string status;
    JobState state;
    std::string submit_time;
    std::string start_time;
    std::string elapsed_time;
    std::string nodes;
    std::string output_dir;
    bool monitored;  // Whether to actively monitor this job
    std::chrono::steady_clock::time_point last_update;
};

class JobQueueView : public RocWidget
{
public:
    JobQueueView();
    ~JobQueueView();

    void Render() override;
    void Show();
    void Hide();
    bool IsVisible() const;

    // Add job to monitoring
    void AddJob(const std::string& job_id, const std::string& job_name,
                const std::string& output_dir);

    // Refresh job list from SLURM
    void RefreshJobList();

private:
    void RenderJobTable();
    void RenderJobDetails();
    void UpdateJobStatus(JobInfo& job);
    JobState ParseJobState(const std::string& status);
    void CancelJob(const std::string& job_id);
    void ViewJobOutput(const JobInfo& job);
    void OpenJobResults(const JobInfo& job);

    bool m_visible;
    std::vector<JobInfo> m_jobs;
    int m_selected_job_index;
    bool m_auto_refresh;
    float m_refresh_interval;
    std::chrono::steady_clock::time_point m_last_refresh;
    std::shared_ptr<Controller::ProcessExecutor::AsyncContext> m_refresh_context;
    bool m_refreshing;
};

}  // namespace View
}  // namespace RocProfVis
