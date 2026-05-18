// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_split_containers.h"
#include "model/compute/rocprofvis_compute_model_types.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SourceCodeWidget;
class IsaCodeWidget;
class DataProvider;

struct LineSelection
{
    uint32_t hovered_line  = 0;
    uint32_t selected_line = 0;
};

class ComputeCodeView : public RocWidget
{
public:
    explicit ComputeCodeView(DataProvider& data_provider);
    ~ComputeCodeView();

    void Render() override;
private:
    void RenderControlPanel();
    void RenderSourceFileDropdown();
    void SubscribeToEvents();
    void LoadData(uint32_t kernel_id);
    void LoadSourceFileList(const PcSamplingData& data);

    SettingsManager&                  m_settings;
    DataProvider&                     m_data_provider;

    std::shared_ptr<SourceCodeWidget> m_source_code;
    std::shared_ptr<IsaCodeWidget>    m_isa_code;
    LayoutItem::Ptr                   m_isa_layout_item;
    std::shared_ptr<HSplitContainer>  m_horizontal_split_container;

    uint32_t                        m_current_source_file_id = 0;
    uint32_t                        m_current_code_object_id = 0;
    uint32_t                        m_current_kernel_id      = 0;

    std::map<std::string /*file_path*/, uint32_t /*file_id*/> m_source_files;
    LineSelection                   m_line_selection;

    float m_control_panel_height;

    EventManager::SubscriptionToken m_kernel_selection_changed_token;
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
    void CalcutlateLineNumberWidth(uint32_t count);
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
    ImVec4 m_comment_color;
};

class SourceCodeWidget : public BaseCodeWidget
{
public:
    SourceCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint32_t source_file_id);

    uint32_t GetSelectedLine() const { return m_line_selection.selected_line; }
    uint32_t GetHoveredLine()  const { return m_line_selection.hovered_line; }

private:
    void RenderLine(uint32_t index, uint32_t column_count);

    struct SourceRow
    {
        uint32_t    id;
        std::string content;
        float       metadata;
    };

    std::vector<SourceRow> m_lines;
};

class IsaCodeWidget : public BaseCodeWidget
{
public:
    IsaCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint32_t code_object_id);
    void ShowComments(bool show) { m_show_comments = show; };

private:
    void RenderLine(uint32_t index, uint32_t column_count);

    struct IsaRow
    {
        uint32_t    id;
        uint32_t    source_line_id;
        std::string instruction;
        std::string comment;
        float       stall;
    };

    bool                m_show_comments = false;
    std::vector<IsaRow> m_entries;
};

}  // namespace View
}  // namespace RocProfVis
