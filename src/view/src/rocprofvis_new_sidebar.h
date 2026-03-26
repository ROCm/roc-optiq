// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_track_topology.h"
#include "widgets/rocprofvis_widget.h"
#include "icons/rocprovfis_icon_defines.h"

#include <optional>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackTopology;
class TimelineSelection;
class DataProvider;

class SideBarButton
{
public:
    SideBarButton(const char* pressed_icon, const char* unpressed_icon) 
    : m_pressed_icon(pressed_icon)
    , m_unpressed_icon(unpressed_icon)
    {};
    ~SideBarButton() = default;
    void Draw() ;
private:
    const char* m_pressed_icon;
    const char* m_unpressed_icon;
    bool  m_is_button_pressed;
};

class SidebarNode
{
public:
    SidebarNode()
    : m_eye_button(ICON_EYE_SLASH, ICON_EYE)
    {};
    virtual ~SidebarNode() = default;
    virtual void Draw() = 0;

private:
    virtual void DrawButton() = 0;
    virtual void DrawHeader() = 0;
    SideBarButton m_eye_button;
};

class CommonNode : public SidebarNode
{
public:
    CommonNode()          = default;
    virtual ~CommonNode() = default;

    void Draw() override;

private:
    SideBarButton m_collapce_button;
};

class LeafNode : public SidebarNode
{
public:
    LeafNode()          = default;
    virtual ~LeafNode() = default;

    void Draw() override;

private:
};


class NewSideBar : public RocWidget
{
public:
    NewSideBar(std::shared_ptr<TrackTopology>                   topology,
            std::shared_ptr<TimelineSelection>               timeline_selection,
            std::shared_ptr<std::vector<TrackGraph>> graphs, DataProvider& dp);
    ~NewSideBar();
    virtual void Render() override;
    virtual void Update() override;

private:

    SettingsManager&                         m_settings;
    std::shared_ptr<TrackTopology>           m_track_topology;
    std::shared_ptr<TimelineSelection>       m_timeline_selection;
    std::shared_ptr<std::vector<TrackGraph>> m_graphs;
    DataProvider&                            m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis