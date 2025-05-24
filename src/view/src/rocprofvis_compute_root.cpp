// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_root.h"

namespace RocProfVis
{
namespace View
{

ComputeRoot::ComputeRoot()
: m_compute_data_provider(nullptr)
, m_tab_container(nullptr)
{
    m_compute_data_provider = std::make_shared<ComputeDataProvider>();

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->AddTab(TabItem{"Summary View", "compute_summary_view", std::make_shared<ComputeSummaryView>(m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Roofline View", "compute_roofline_view", std::make_shared<ComputeRooflineView>(m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Block View", "compute_block_view", std::make_shared<ComputeBlockView>(m_compute_data_provider), false});
    m_tab_container->AddTab(TabItem{"Table View", "compute_table_view", std::make_shared<ComputeTableView>(m_compute_data_provider), false});
}

ComputeRoot::~ComputeRoot() {}

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
