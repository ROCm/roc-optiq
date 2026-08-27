// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_split_containers.h"
#include "model/compute/rocprofvis_compute_model_types.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SourceCodeWidget;
class IsaCodeWidget;
class DataProvider;
enum class PcSamplingRequestKind : uint32_t;

struct LineSelection
{
    uint64_t hovered_line  = 0;
    uint64_t selected_line = 0;
};

class ComputeIsaView : public RocWidget
{
public:
    explicit ComputeIsaView(DataProvider& data_provider);
    ~ComputeIsaView();

    void Render() override;
private:
    void RenderControlPanel();
    void RenderSourceFileDropdown();
    void SubscribeToEvents();
    void SelectWorkload(uint32_t workload_id);
    void LoadData(uint32_t kernel_id);
    void ClearCodeData();
    void ClearSelectionData();
    void LoadSourceFileList(const PcSamplingData& data);
    void QueuePcSamplingFetch(PcSamplingRequestKind kind);
    void FetchPendingPcSampling();
    void RefreshCodeWidgets();
    void OnPcSamplingReady(PcSamplingRequestKind kind, uint32_t kernel_id,
                           uint64_t source_file_uuid, uint32_t generation, bool success);

    SettingsManager&                  m_settings;
    DataProvider&                     m_data_provider;

    std::shared_ptr<SourceCodeWidget> m_source_code;
    std::shared_ptr<IsaCodeWidget>    m_isa_code;
    LayoutItem::Ptr                   m_source_layout_item;
    std::shared_ptr<HSplitContainer>  m_horizontal_split_container;

    uint64_t                          m_current_source_file_uuid;
    uint64_t                          m_current_code_object_uuid;
    uint32_t                          m_current_kernel_id;
    uint32_t                          m_current_workload_id;
    uint32_t                          m_fetch_generation = 0;
    uint64_t                          m_active_request_id = 0;
    PcSamplingRequestKind             m_active_request_kind;
    bool                              m_fetch_in_progress = false;
    bool                              m_pending_isa_fetch    = false;
    bool                              m_pending_source_fetch = false;
    bool                              m_pending_stall_fetch  = false;
    bool                              m_isa_data_loaded      = false;
    bool                              m_stall_data_loaded    = false;

    std::map<std::string /*file_path*/, uint64_t /*file_id*/> m_source_files;
    std::set<uint64_t>                                        m_loaded_source_files;
    LineSelection                   m_line_selection;

    float m_control_panel_height;

    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_workload_selection_changed_token;
    bool m_show_metadata_enabled;
};

struct CodeLine
{
    std::string content;
    float       stall;
};

class BaseCodeWidget : public RocWidget
{
public:
    BaseCodeWidget(LineSelection& selection);
    ~BaseCodeWidget() = default;

    virtual void Render() override = 0;

    bool IsStallShown() { return m_show_stall; };
    void ChangeStallVisibility(bool show) { m_show_stall = show; };

protected:
    void CalculateLineNumberWidth(uint32_t count);
    void PushStyles();

    bool m_show_stall = false;

    LineSelection& m_line_selection;
    uint32_t       m_line_num_width  = 0;
    uint32_t       m_line_num_digits = 1;

    SettingsManager& m_settings;
    ImGuiTableFlags  m_table_flags;

    ImU32  m_selected_colour;
    ImU32  m_hovered_colour;

    ImVec4 m_line_num_color;
};

class SourceCodeWidget : public BaseCodeWidget
{
public:
    SourceCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t source_file_uuid);

    uint64_t GetSelectedLine() const { return m_line_selection.selected_line; }
    uint64_t GetHoveredLine()  const { return m_line_selection.hovered_line; }

private:
    void RenderLine(uint32_t index, uint32_t column_count);

    struct SourceRow
    {
        std::string content;
        uint64_t    id                = 0;
        uint64_t    line_number       = 0;
        float       summarised_stalls = 0.0f;
    };

    std::vector<SourceRow> m_lines;
};

class IsaCodeWidget : public BaseCodeWidget
{
public:
    IsaCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t code_object_uuid);

private:
    void RenderLine(uint32_t index, uint32_t column_count);

    struct IsaRow
    {
        std::string instruction;
        uint64_t    id                  = 0;
        uint64_t    source_line_id      = 0;
        uint64_t    issue_count         = 0;
        uint64_t    stall_count         = 0;
        uint64_t    total_count         = 0;
    };

    std::vector<IsaRow> m_entries;
};

}  // namespace View
}  // namespace RocProfVis
