// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_compute_selection.h"
#include "widgets/rocprofvis_split_containers.h"
#include "model/compute/rocprofvis_compute_model_types.h"

#include <map>
#include <memory>
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
struct TabItem;
enum class PcSamplingLayer : uint32_t;

struct LineSelection
{
    static constexpr uint64_t UNSELECTED = 0;
    uint64_t hovered_line       = UNSELECTED;
    uint64_t selected_line      = UNSELECTED;
    uint64_t source_scroll_line = UNSELECTED;
    uint64_t source_scroll_file = UNSELECTED;
    uint64_t isa_scroll_line    = UNSELECTED;
    bool     hovered_this_frame = false;
};

struct FetchStateType
{
    bool     queued        = false;
    bool     in_flight     = false;
    bool     loaded        = false;
    bool     failed        = false;
    uint64_t request_token = 0;
};

struct IsaPane : FetchStateType
{
    uint64_t                       code_object_uuid = ComputeSelection::INVALID_SELECTION_ID;
    std::shared_ptr<IsaCodeWidget> widget;

    void ResetFetch()
    {
        static_cast<FetchStateType&>(*this) = {};
        code_object_uuid = ComputeSelection::INVALID_SELECTION_ID;
    }
};

struct SourcePane : FetchStateType
{
    uint64_t                             selected_uuid = 0;
    std::map<std::string, uint64_t>      file_uuid_by_path;
    std::set<uint64_t>                   loaded_uuids;
    std::shared_ptr<SourceCodeWidget>    widget;

    void ResetFetch()
    {
        static_cast<FetchStateType&>(*this) = {};
        selected_uuid = 0;
        file_uuid_by_path.clear();
        loaded_uuids.clear();
    }
};

class ComputeIsaView : public RocWidget
{
public:
    static constexpr const char* TAB_ID = "isa_view";

    static TabItem CreateTabItem(DataProvider& data_provider);
    static TabItem CreateTabItem(DataProvider& data_provider, bool has_isa_lines);

    explicit ComputeIsaView(DataProvider& data_provider);
    ~ComputeIsaView();

    void Update() override;
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
    void SelectSourceFile(uint64_t source_file_uuid);
    void SelectSourceFileForScroll();
    void QueuePcSamplingFetch(PcSamplingLayer layer);
    void            ClearPendingPcSamplingFetches();
    void            CancelInFlightFetches();
    FetchStateType& FetchStateFor(PcSamplingLayer layer);
    bool HasValidPcSamplingSelection() const;
    bool TryTakeNextPendingPcSamplingFetch(PcSamplingLayer& layer);
    void SubmitPcSamplingFetch(PcSamplingLayer layer);
    void FetchPendingPcSampling();
    void RefreshCodeWidgets();
    void OnPcSamplingReady(PcSamplingLayer layer, uint32_t kernel_id,
                           uint64_t source_file_uuid, uint32_t generation,
                           uint64_t request_token, rocprofvis_result_t result);

    SettingsManager&                  m_settings;
    DataProvider&                     m_data_provider;

    LayoutItem::Ptr                   m_source_layout_item;
    std::shared_ptr<HSplitContainer>  m_horizontal_split_container;

    uint32_t                          m_current_kernel_id;
    uint32_t                          m_current_workload_id;
    uint32_t                          m_fetch_generation = 0;
    uint64_t                          m_next_request_token = 0;
    IsaPane                           m_isa;
    SourcePane                        m_source;
    FetchStateType                    m_stalls;

    LineSelection                     m_line_selection;

    float m_control_panel_height;

    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_workload_selection_changed_token;
    bool m_show_metadata_enabled;
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
    void CalculateLineNumberWidth(size_t count);
    void PushStyles();

    bool m_show_stall = false;

    LineSelection& m_line_selection;
    float          m_line_num_width  = 0.0f;
    uint32_t       m_line_num_digits = 1;

    SettingsManager& m_settings;
    ImGuiTableFlags  m_table_flags;

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
    uint32_t GetScrollTarget(ImGuiListClipper& clipper);
    void RenderLine(uint32_t index);

    struct SourceRow
    {
        std::string content;
        uint64_t    id          = 0;
        uint64_t    line_number = 0;
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
    struct StallReason
    {
        std::string text;
        uint64_t    count = 0;
    };

    struct IsaRow
    {
        std::string              instruction;
        uint64_t                 id                         = 0;
        uint64_t                 code_object_offset         = 0;
        uint64_t                 source_line_id             = 0;
        uint64_t                 source_file_id             = 0;
        uint64_t                 stall_count                = 0;
        uint64_t                 total_count                = 0;
        uint64_t                 stall_reason_sample_count  = 0;
        std::vector<StallReason> stall_reasons;
    };

    // Intermediate lookup tables built once per Load and consumed while assembling rows.
    struct SourceLocation
    {
        uint64_t source_line_id = 0;
        uint64_t source_file_id = 0;
    };

    struct SampleCounts
    {
        uint64_t total_count = 0;
        uint64_t stall_count = 0;
    };

    struct SampleAggregation
    {
        std::unordered_map<uint64_t, SampleCounts> counts_by_instruction;
        std::unordered_map<uint64_t, uint64_t>     instruction_by_sample_state;
        uint64_t                                   kernel_total_samples = 0;
    };

    static const CodeObjectStore* FindCodeObject(const PcSamplingData& data,
                                                 uint64_t code_object_uuid);
    static std::unordered_map<uint64_t, SourceLocation>
        BuildSourceLocations(const PcSamplingData& data);
    static SampleAggregation AggregateSampleCounts(const PcSamplingData& data);
    static std::unordered_map<uint64_t, std::string>
        BuildStallReasonText(const PcSamplingData& data);
    static std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t>>
        BuildStallReasonCounts(
            const PcSamplingData&                         data,
            const std::unordered_map<uint64_t, uint64_t>& instruction_by_sample_state);
    static std::vector<StallReason>
        BuildStallReasons(const std::unordered_map<uint64_t, uint64_t>&    reason_counts,
                          const std::unordered_map<uint64_t, std::string>& reason_text,
                          uint64_t&                                        classified_sample_count);
    static IsaRow
        BuildRow(const InstructionLine&                              instruction_line,
                 const std::unordered_map<uint64_t, SourceLocation>& source_locations,
                 const SampleAggregation&                            sample_aggregation,
                 const std::unordered_map<
                     uint64_t, std::unordered_map<uint64_t, uint64_t>>& stall_reason_counts,
                 const std::unordered_map<uint64_t, std::string>&       stall_reason_text);
    static std::string
        ResolveStallReasonText(const std::unordered_map<uint64_t, std::string>& reason_text,
                               uint64_t                                         lookup_uuid);

    uint32_t GetScrollTarget(ImGuiListClipper& clipper);
    void RenderLine(uint32_t index);

    static double      CalculatePercentage(uint64_t value, uint64_t total);
    static ImU32       HeatmapColor(double percent);
    static std::string FormatSampleCount(uint64_t value);
    bool               RenderPercentBarCell(double percent);
    void               RenderSamplesCell(uint64_t sample_count);
    void               RenderStallReasonsTooltip(const IsaRow& row);
    void               RenderStallReasonTable(const IsaRow& row);

    std::vector<IsaRow> m_entries;
    uint64_t            m_kernel_total_samples        = 0;
    uint64_t            m_hottest_instruction_samples = 0;
    uint64_t            m_largest_code_object_offset  = 0;
};

}  // namespace View
}  // namespace RocProfVis
