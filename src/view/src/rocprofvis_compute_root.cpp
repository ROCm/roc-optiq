// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_root.h"
#include "rocprofvis_compute_data_provider.h"
#include "rocprofvis_compute_block.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_table.h"
#include "rocprofvis_navigation_manager.h"
#include "rocprofvis_navigation_url.h"

namespace RocProfVis
{
namespace View
{

ComputeRoot::ComputeRoot()
: m_compute_data_provider(nullptr)
, m_tab_container(nullptr)
, m_data_dirty_event_token(-1)
, m_data_dirty(false)
, m_trace_opened(false)
{
    m_id = GenUniqueName("");
    
    m_compute_data_provider = std::make_shared<ComputeDataProvider>();

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->AddTab(TabItem{"Summary View", COMPUTE_SUMMARY_VIEW_URL, std::make_shared<ComputeSummaryView>(m_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Roofline View", COMPUTE_ROOFLINE_VIEW_URL, std::make_shared<ComputeRooflineView>(m_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Block View", COMPUTE_BLOCK_VIEW_URL, std::make_shared<ComputeBlockView>(m_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Table View", COMPUTE_TABLE_VIEW_URL, std::make_shared<ComputeTableView>(m_id, m_compute_data_provider), false});

    NavigationManager::GetInstance()->RegisterContainer(m_tab_container, m_id, m_id);

    auto data_dirty_event_handler = [this](std::shared_ptr<RocEvent> event) 
    {
        m_data_dirty = true;
    };
    m_data_dirty_event_token = EventManager::GetInstance()->Subscribe(static_cast<int>(RocEvents::kComputeDataDirty), data_dirty_event_handler);
}

ComputeRoot::~ComputeRoot() 
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kComputeDataDirty), m_data_dirty_event_token);
    NavigationManager::GetInstance()->UnregisterContainer(m_tab_container, m_id, m_id);
}

void ComputeRoot::Update()
{
    if (m_data_dirty && m_trace_opened)
    {
        m_data_dirty = false;
        if (m_tab_container)
        {
            m_tab_container->Update();
        }
    }
}

void ComputeRoot::OpenTrace(const std::string& path)
{
    m_trace_opened = (kRocProfVisResultSuccess == m_compute_data_provider->LoadTrace(path));
}

void ComputeRoot::Render()
{
    if (m_trace_opened)
    {
        if (m_tab_container)
        {
            m_tab_container->Render();
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
