// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_root.h"
#include "rocprofvis_navigation_manager.h"
#include "rocprofvis_navigation_url.h"

namespace RocProfVis
{
namespace View
{

ComputeRoot::ComputeRoot(std::string owner_id)
: m_compute_data_provider(nullptr)
, m_tab_container(nullptr)
, m_owner_id(owner_id)
{
    m_compute_data_provider = std::make_shared<ComputeDataProvider>();

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->AddTab(TabItem{"Summary View", COMPUTE_SUMMARY_VIEW_URL, std::make_shared<ComputeSummaryView>(m_owner_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Roofline View", COMPUTE_ROOFLINE_VIEW_URL, std::make_shared<ComputeRooflineView>(m_owner_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Block View", COMPUTE_BLOCK_VIEW_URL, std::make_shared<ComputeBlockView>(m_owner_id, m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Table View", COMPUTE_TABLE_VIEW_URL, std::make_shared<ComputeTableView>(m_owner_id, m_compute_data_provider), false});

    NavigationManager::GetInstance()->RegisterContainer(m_tab_container, m_owner_id, m_owner_id);
}

ComputeRoot::~ComputeRoot() {
    NavigationManager::GetInstance()->UnregisterContainer(m_tab_container, m_owner_id, m_owner_id);
}

void ComputeRoot::Update()
{
    if (!m_compute_data_provider->ProfileLoaded())
    {
        m_compute_data_provider->LoadProfile();

        if (m_compute_data_provider->ProfileLoaded())
        {
            if (m_tab_container)
            {
                m_tab_container->Update();
            }
        }
    }
}

void ComputeRoot::SetProfilePath(std::filesystem::path path)
{
    m_compute_data_provider->SetProfilePath(path);
}

bool ComputeRoot::ProfileLoaded()
{
    return m_compute_data_provider->ProfileLoaded();
}

void ComputeRoot::Render()
{
    if (m_compute_data_provider->ProfileLoaded())
    {
        if (m_tab_container)
        {
            m_tab_container->Render();
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
