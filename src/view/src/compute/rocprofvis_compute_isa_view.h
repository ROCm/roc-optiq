// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>

#include "rocprofvis_compute_code_widgets.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_split_containers.h"

namespace RocProfVis
{
namespace View
{

class DataProvider;
struct TabItem;
enum class PcSamplingLayer : uint32_t;

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
    void RefreshCodeWidgets(PcSamplingLayer layer);
    void RefreshSourceWidget();
    void UpdateSamplingVisibility();
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
    bool m_show_sampling_details;
};

}  // namespace View
}  // namespace RocProfVis
