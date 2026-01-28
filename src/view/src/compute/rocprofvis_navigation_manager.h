// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

typedef struct navigation_node_t
{
    std::string m_id;
    std::shared_ptr<TabContainer> m_container;
    std::unordered_map<std::string, navigation_node_t*> m_children;
} navigation_node_t;

typedef struct container_entry_t
{
    std::shared_ptr<TabContainer> m_container;
    std::string m_parent_id;
    std::string m_owner_id;
} container_entry_t;

class NavigationManager
{
public:
    static NavigationManager* GetInstance();
    static void DestroyInstance();

    /*
    * Navigates to the specified location.
    * @param url Path of TabItem IDs to specified location seperated by '/'. Must begin with the root tab but does not have to include a leaf tab.
    */
    void Go(const std::string& url);
    /*
    * Registers the specified container as the root container.
    * @param container The container which is always present and is parent of all other containers while not having a parent of its own.
    */
    void RegisterRootContainer(std::shared_ptr<TabContainer> container);
    /*
    * Registers the specified container. 
    * @param container The container to register.
    * @param parent_id The id of the tab that the container belongs to.
    * @param owner_id The id of the root tab that the container belongs to.
    */
    void RegisterContainer(std::shared_ptr<TabContainer> container, const std::string& parent_id, const std::string& owner_id);
    /*
    * Unregisters the specified container.
    * @param container The container to register.
    * @param parent_id The id of the tab that the container belongs to.
    * @param owner_id The id of the root tab that the container belongs to.
    */
    void UnregisterContainer(std::shared_ptr<TabContainer> container, const std::string& parent_id, const std::string& owner_id);
    /*
     * Builds the navigation tree from the registered containers. Should be called if containers have been registered/unregistered.
    */
    void RefreshNavigationTree();

private:
    NavigationManager();
    ~NavigationManager();

    static NavigationManager* s_instance;

    std::vector<container_entry_t> m_containers;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<navigation_node_t>>> m_node_map;
    std::unique_ptr<navigation_node_t> m_root_node;
};

}  // namespace View
}  // namespace RocProfVis
