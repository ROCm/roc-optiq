// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_job_queue_view.h"
#include "rocprofvis_appwindow.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace RocProfVis
{
namespace View
{

JobQueueView::JobQueueView()
: m_visible(false)
, m_selected_job_index(-1)
, m_auto_refresh(true)
, m_refresh_interval(5.0f)  // 5 seconds
, m_refreshing(false)
{
    m_widget_name = GenUniqueName("JobQueueView");
    m_last_refresh = std::chrono::steady_clock::now();
}

JobQueueView::~JobQueueView()
{
}

void JobQueueView::Show()
{
    m_visible = true;
    RefreshJobList();
}

void JobQueueView::Hide()
{
    m_visible = false;
}

bool JobQueueView::IsVisible() const
{
    return m_visible;
}

void JobQueueView::Render()
{
    if(!m_visible)
    {
        return;
    }

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

    if(ImGui::Begin("SLURM Job Queue Monitor", &m_visible, ImGuiWindowFlags_NoCollapse))
    {
        // Toolbar
        if(ImGui::Button("Refresh"))
        {
            RefreshJobList();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-refresh", &m_auto_refresh);

        if(m_auto_refresh)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderFloat("##refresh_interval", &m_refresh_interval, 1.0f, 60.0f, "%.1f sec");

            // Auto-refresh logic
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_refresh).count();

            if(elapsed >= m_refresh_interval && !m_refreshing)
            {
                RefreshJobList();
            }
        }

        ImGui::SameLine();
        if(m_refreshing)
        {
            ImGui::TextDisabled("Refreshing...");
        }
        else
        {
            ImGui::TextDisabled("(%zu jobs)", m_jobs.size());
        }

        ImGui::Separator();

        // Job table
        RenderJobTable();

        ImGui::Separator();

        // Job details panel
        if(m_selected_job_index >= 0 && m_selected_job_index < static_cast<int>(m_jobs.size()))
        {
            RenderJobDetails();
        }
    }

    ImGui::End();
    popup_style.PopStyles();
}

void JobQueueView::RenderJobTable()
{
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if(ImGui::BeginTable("JobTable", 7, flags, ImVec2(0, 300)))
    {
        ImGui::TableSetupColumn("Job ID", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Partition", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Nodes", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Elapsed", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for(int i = 0; i < static_cast<int>(m_jobs.size()); ++i)
        {
            const auto& job = m_jobs[i];

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            // Selectable row
            bool is_selected = (m_selected_job_index == i);
            if(ImGui::Selectable(job.job_id.c_str(), is_selected,
                                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
            {
                m_selected_job_index = i;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(job.job_name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(job.partition.c_str());

            ImGui::TableSetColumnIndex(3);
            // Color-code status
            ImVec4 status_color;
            switch(job.state)
            {
                case JobState::Running:
                    status_color = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);  // Green
                    break;
                case JobState::Pending:
                    status_color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);  // Yellow
                    break;
                case JobState::Completed:
                    status_color = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);  // Blue
                    break;
                case JobState::Failed:
                case JobState::Cancelled:
                    status_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
                    break;
                default:
                    status_color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);  // Gray
                    break;
            }
            ImGui::TextColored(status_color, "%s", job.status.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(job.nodes.c_str());

            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(job.elapsed_time.c_str());

            ImGui::TableSetColumnIndex(6);
            ImGui::PushID(i);

            if(job.state == JobState::Running || job.state == JobState::Pending)
            {
                if(ImGui::SmallButton("Cancel"))
                {
                    CancelJob(job.job_id);
                }
            }

            if(job.state == JobState::Completed)
            {
                ImGui::SameLine();
                if(ImGui::SmallButton("View Results"))
                {
                    OpenJobResults(job);
                }
            }

            if(ImGui::SmallButton("Output"))
            {
                ViewJobOutput(job);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void JobQueueView::RenderJobDetails()
{
    const auto& job = m_jobs[m_selected_job_index];

    ImGui::TextUnformatted("Job Details:");
    ImGui::Separator();

    ImGui::Text("Job ID:       %s", job.job_id.c_str());
    ImGui::Text("Name:         %s", job.job_name.c_str());
    ImGui::Text("Partition:    %s", job.partition.c_str());
    ImGui::Text("Status:       %s", job.status.c_str());
    ImGui::Text("Submit Time:  %s", job.submit_time.c_str());
    ImGui::Text("Start Time:   %s", job.start_time.c_str());
    ImGui::Text("Elapsed Time: %s", job.elapsed_time.c_str());
    ImGui::Text("Nodes:        %s", job.nodes.c_str());
    ImGui::Text("Output Dir:   %s", job.output_dir.c_str());
}

void JobQueueView::AddJob(const std::string& job_id, const std::string& job_name,
                          const std::string& output_dir)
{
    JobInfo job;
    job.job_id = job_id;
    job.job_name = job_name;
    job.output_dir = output_dir;
    job.monitored = true;
    job.state = JobState::Pending;
    job.status = "PENDING";
    job.last_update = std::chrono::steady_clock::now();

    m_jobs.push_back(job);
    spdlog::info("Added job {} to monitoring", job_id);
}

void JobQueueView::RefreshJobList()
{
    if(m_refreshing)
    {
        return;
    }

    m_refreshing = true;
    m_last_refresh = std::chrono::steady_clock::now();

    // Query SLURM for job status
    std::string command = "squeue --me --format=\"%i|%j|%P|%T|%M|%D|%S\" --noheader";

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;
    options.timeout_seconds = 10;

    m_refresh_context = Controller::ProcessExecutor::ExecuteAsync(
        command,
        options,
        [this](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            m_refreshing = false;

            if(result.exit_code == 0)
            {
                // Parse squeue output
                std::vector<JobInfo> updated_jobs;
                std::istringstream stream(result.stdout_output);
                std::string line;

                while(std::getline(stream, line))
                {
                    if(line.empty())
                        continue;

                    std::istringstream line_stream(line);
                    std::string job_id, job_name, partition, status, elapsed, nodes, start_time;

                    std::getline(line_stream, job_id, '|');
                    std::getline(line_stream, job_name, '|');
                    std::getline(line_stream, partition, '|');
                    std::getline(line_stream, status, '|');
                    std::getline(line_stream, elapsed, '|');
                    std::getline(line_stream, nodes, '|');
                    std::getline(line_stream, start_time, '|');

                    JobInfo job;
                    job.job_id = job_id;
                    job.job_name = job_name;
                    job.partition = partition;
                    job.status = status;
                    job.state = ParseJobState(status);
                    job.elapsed_time = elapsed;
                    job.nodes = nodes;
                    job.start_time = start_time;
                    job.last_update = std::chrono::steady_clock::now();

                    // Find if this job was already being monitored
                    for(const auto& existing_job : m_jobs)
                    {
                        if(existing_job.job_id == job_id)
                        {
                            job.output_dir = existing_job.output_dir;
                            job.monitored = existing_job.monitored;
                            job.submit_time = existing_job.submit_time;
                            break;
                        }
                    }

                    updated_jobs.push_back(job);
                }

                // Keep completed/failed jobs from previous list
                for(const auto& old_job : m_jobs)
                {
                    if(old_job.state == JobState::Completed || old_job.state == JobState::Failed ||
                       old_job.state == JobState::Cancelled)
                    {
                        // Check if not already in updated list
                        bool found = false;
                        for(const auto& new_job : updated_jobs)
                        {
                            if(new_job.job_id == old_job.job_id)
                            {
                                found = true;
                                break;
                            }
                        }

                        if(!found)
                        {
                            updated_jobs.push_back(old_job);
                        }
                    }
                }

                m_jobs = updated_jobs;
                spdlog::debug("Refreshed job list: {} jobs", m_jobs.size());
            }
            else
            {
                spdlog::warn("Failed to refresh job list: {}", result.error_message);
            }
        }
    );
}

JobState JobQueueView::ParseJobState(const std::string& status)
{
    if(status == "RUNNING" || status == "R")
        return JobState::Running;
    if(status == "PENDING" || status == "PD")
        return JobState::Pending;
    if(status == "COMPLETED" || status == "CD")
        return JobState::Completed;
    if(status == "FAILED" || status == "F")
        return JobState::Failed;
    if(status == "CANCELLED" || status == "CA")
        return JobState::Cancelled;

    return JobState::Unknown;
}

void JobQueueView::CancelJob(const std::string& job_id)
{
    std::string command = "scancel " + job_id;

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = true;

    Controller::ProcessExecutor::ExecuteAsync(
        command,
        options,
        [this, job_id](const Controller::ProcessExecutor::ExecutionResult& result)
        {
            if(result.exit_code == 0)
            {
                spdlog::info("Cancelled job {}", job_id);
                RefreshJobList();
            }
            else
            {
                spdlog::error("Failed to cancel job {}: {}", job_id, result.error_message);
            }
        }
    );
}

void JobQueueView::ViewJobOutput(const JobInfo& job)
{
    // Open job output file in system default editor
    std::string output_file = job.output_dir + "/" + job.job_name + "_" + job.job_id + ".out";

    spdlog::info("Opening job output: {}", output_file);

    // Use system command to open file
#ifdef _WIN32
    std::string command = "start " + output_file;
#else
    std::string command = "xdg-open " + output_file + " 2>/dev/null || cat " + output_file;
#endif

    Controller::ProcessExecutor::ExecutionOptions options;
    options.capture_output = false;

    Controller::ProcessExecutor::ExecuteAsync(command, options, nullptr);
}

void JobQueueView::OpenJobResults(const JobInfo& job)
{
    // Look for merged trace file in output directory
    std::string trace_file = job.output_dir + "/merged_trace.db";
    std::string ai_json = job.output_dir + "/ai_analysis.json";

    spdlog::info("Opening job results from: {}", job.output_dir);

    // Open in ROC-Optiq
    AppWindow::GetInstance()->OpenFile(trace_file);

    // Try to load AI analysis if it exists
    if(std::filesystem::exists(ai_json))
    {
        AppWindow::GetInstance()->LoadAiAnalysis(ai_json);
    }
}

}  // namespace View
}  // namespace RocProfVis
