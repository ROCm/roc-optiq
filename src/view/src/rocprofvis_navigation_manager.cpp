// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_navigation_manager.h"
#include <sstream>
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

NavigationManager* NavigationManager::s_instance = nullptr;

NavigationManager* NavigationManager::GetInstance()
{
    if (!s_instance)
    {
        s_instance = new NavigationManager();
    }
    return s_instance;
}

void NavigationManager::DestroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void NavigationManager::Go(const std::string& url)
{
    navigation_node_t* node = m_root_node.get();
    std::stringstream stream(url);
    std::string chunk;
    while (std::getline(stream, chunk, '/')) 
    {
        if (node && node->m_children.count(chunk) > 0)
        {
            node->m_children[chunk]->m_container->SetActiveTab(chunk);
            node = node->m_children[chunk];
        }
        else
        {
            break;
        }
    }
}

void NavigationManager::RegisterRootContainer(std::shared_ptr<TabContainer> container)
{
    m_root_node->m_container = container;
}

void NavigationManager::RegisterContainer(std::shared_ptr<TabContainer> container, const std::string& parent_id, const std::string& owner_id)
{
    m_containers.push_back(container_entry_t{container, parent_id, owner_id});
}

void NavigationManager::UnregisterContainer(std::shared_ptr<TabContainer> container, const std::string& parent_id, const std::string& owner_id)
{
    for (auto it = m_containers.begin(); it != m_containers.end(); it ++)
    {
        if (it->m_container == container && it->m_parent_id == parent_id && it->m_owner_id == owner_id)
        {
            m_containers.erase(it);
            break;
        }
    }
}

void NavigationManager::RefreshNavigationTree()
{
    m_node_map.clear();
    m_root_node->m_children.clear();

    for (auto entry : m_containers)
    {        
        if (m_node_map[entry.m_owner_id].count(entry.m_parent_id) == 0)
        {
            // Child created before parent. Create parent node but leave container and children empty to be filled later.
            std::unique_ptr<navigation_node_t> parent_node = std::make_unique<navigation_node_t>();
            parent_node->m_id = entry.m_parent_id;
            parent_node->m_container = nullptr;
            m_node_map[entry.m_owner_id][entry.m_parent_id] = std::move(parent_node);
        }
        for (const TabItem* tab : entry.m_container->GetTabs())
        {           
            if (m_node_map[entry.m_owner_id].count(tab->m_id) == 0)
            {
                // New node. Create it and add it to the parent's children.
                std::unique_ptr<navigation_node_t> node = std::make_unique<navigation_node_t>();
                node->m_id = tab->m_id;
                node->m_container = entry.m_container;
                node->m_children = {};
                m_node_map[entry.m_owner_id][entry.m_parent_id]->m_children[tab->m_id] = node.get();
                m_node_map[entry.m_owner_id][tab->m_id] = std::move(node);
            }
            else if (!m_node_map[entry.m_owner_id][tab->m_id]->m_container)
            {
                // Existing node with unpopulated container and children from first case. Populate container and add it to parent's children.
                m_node_map[entry.m_owner_id][tab->m_id]->m_container = entry.m_container;
                m_node_map[entry.m_owner_id][entry.m_parent_id]->m_children[tab->m_id] = m_node_map[entry.m_owner_id][tab->m_id].get();
            }
            else
            {
                spdlog::error("NavigationManager::RefreshNavigationTree - Duplicate tab id {} found for {}.", tab->m_id, entry.m_owner_id);
            }
        }
    }

    for (auto& it : m_node_map)
    {
        // Update the root node's children. These are the first level nodes that can be identfied by their owner_id being same as their parent_id;
        const std::string& owner_id = it.first;
        it.second[owner_id]->m_container = m_root_node->m_container;
        m_root_node->m_children[owner_id] = it.second[owner_id].get();
    }
}

NavigationManager::NavigationManager()
: m_root_node(nullptr)
{
    m_root_node = std::make_unique<navigation_node_t>();
    m_root_node->m_id = "root";
}

NavigationManager::~NavigationManager()
{}

}  // namespace View
}  // namespace RocProfVis