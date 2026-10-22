// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_project.h"
#include <array>
#include <bitset>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
class TrackItem;
class TimelineSelection;
class TimelineTrackOptions;

class TrackOptions
{
public:
    // Types of TrackOptions, derived types must be listed after base
    enum Type
    {
        kTrack = 0,
        kCounter,
        kEvent,
        kQueue,
        kNumTypes,
    };

    TrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                 const std::string& project_id);
    TrackOptions(const TrackOptions& other);
    // Derived options are owned through unique_ptr<TrackOptions>
    virtual ~TrackOptions() = default;

    // Part of aggregation, types dictate how to combine themselves
    virtual TrackOptions& operator&=(const TrackOptions& other);

    // ProjectSetting interface...
    virtual void ToJson();
    virtual bool Valid() const;
    virtual void FromJson();

    // Render controls unique to this type
    virtual void Render();

    // Signal parent track item of changes (Expect single consumer so is self
    // resetting)
    bool Updated();

    // Inheritance hierarchy
    const std::bitset<kNumTypes>& TypeMask() const;

    // The track this option set belongs to.
    const TrackItem& GetTrackItem() const { return m_track_item; }

    // Option members...
    bool  m_display;
    float m_height;
#ifdef IMGUI_ENABLE_TEST_ENGINE
    friend struct FlameTrackItemTestPeer;
#endif
protected:
    class TrackProjectSetting : public ProjectSetting
    {
    public:
        TrackProjectSetting(const std::string& project_id, TrackOptions& options);
        void      ToJson() override final;
        bool      Valid() const override final;
        jt::Json& GetJson();

    private:
        TrackOptions& m_options;
    };

    std::bitset<kNumTypes> m_type_mask;
    bool                   m_updated;

    const TrackItem&                     m_track_item;
    TimelineTrackOptions&                m_ctx;
    const SettingsManager&               m_settings;
    std::unique_ptr<TrackProjectSetting> m_project_settings;
};

class CounterTrackOptions : public TrackOptions
{
public:
    struct Boxplot
    {
        bool enabled;
        bool stripes;
    };
    struct Highlight
    {
        bool  enabled;
        float range_min;
        float range_max;
    };

    CounterTrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                        const std::string& project_id);
    CounterTrackOptions(const CounterTrackOptions& other);

    CounterTrackOptions& operator&=(const TrackOptions& other) override final;

    void ToJson() override final;
    bool Valid() const override final;
    void FromJson() override final;
    void Render() override final;

    std::array<bool, AnalysisTrackStatistics::Counter::kCounterCount> m_show_analysis;
    Boxplot                                                           m_boxplot;
    Highlight                                                         m_highlight;
};

class EventTrackOptions : public TrackOptions
{
public:
    enum class EventColorMode
    {
        kNone,
        kByEventName,
        kByTimeLevel,
        kMixed,
        __kCount
    };

    EventTrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                      const std::string& project_id);
    EventTrackOptions(const EventTrackOptions& other);

    EventTrackOptions& operator&=(const TrackOptions& other) override;

    void ToJson() override;
    bool Valid() const override;
    void FromJson() override;
    void Render() override;

    EventColorMode m_color_mode;
    bool           m_compact;
    bool           m_expand;
};

class QueueTrackOptions : public EventTrackOptions
{
public:
    QueueTrackOptions(const TrackItem& track, TimelineTrackOptions& ctx,
                      const std::string& project_id);
    QueueTrackOptions(const QueueTrackOptions& other);

    QueueTrackOptions& operator&=(const TrackOptions& other) override final;

    void ToJson() override final;
    bool Valid() const override final;
    void FromJson() override final;
    void Render() override final;

    bool m_show_queue_utilization;
};

class TimelineTrackOptions
{
public:
    friend class TrackOptions;
    friend class CounterTrackOptions;
    friend class EventTrackOptions;
    friend class QueueTrackOptions;

    TimelineTrackOptions(const TimelineSelection& selection);

    // Construct options, loads from project, registers with context menu system, returns
    // option to caller
    std::unique_ptr<TrackOptions> InitTrack(const TrackItem& track);
    void                          Update();
    // Given a target track, snapshot required info and setup aggregated options
    void InitTrackOptionsSubmenu(const TrackItem& target);
    // Display the options menu for the target track passed in InitTrackOptionsSubmenu()
    void RenderTrackOptionsSubmenu();
    // Whether any known track is currently hidden.
    bool ShowHiddenTracksSubmenu() const;
    // Render the "Show Hidden Tracks" submenu contents. Call within an open menu
    // or popup.
    void RenderHiddenTracksSubmenu();

    void SetTrackSortSubmenu(std::function<void()> renderer);

    bool ShowTrackSortSubmenu() const;

    void RenderTrackSortSubmenu() const;

private:
    enum Propagate
    {
        kNone,
        kSelected,  // Apply to selected tracks
        kSiblings,  // Apply to all like tracks
    };

    // Reveal every track in the list (sets display + fires a single
    // visibility-changed event). Ignores tracks that are already displayed.
    void ShowTracks(const std::vector<TrackOptions*>& options);
    // Reveal every currently hidden track.
    void ShowAllHiddenTracks();

    // Construct options that is aggregate representation of compoents
    std::unique_ptr<TrackOptions> CreateAggregate(
        const std::bitset<TrackOptions::kNumTypes>& type_mask,
        const std::vector<TrackOptions*>&           components);
    // "Siblings" = propagation targets
    std::vector<TrackOptions*> Siblings(
        const TrackInfo::TrackType&               topology_type,
        const rocprofvis_controller_track_type_t& data_type);
    void RenderPropagateControl();

    // Public properties...
    Propagate        m_propagate;
    const TrackItem* m_context_menu_target;

    // Internal state...
    bool                                        m_init_context_menu;
    bool                                        m_update_aggregates;
    std::unordered_map<uint64_t, TrackOptions*> m_options_map;
    std::array<std::vector<TrackOptions*>, TrackInfo::TrackType::Count>
        m_siblings_by_topology_type;  // Queue/Thread(S)/Thread(I)/Streams/Counters
    std::array<std::vector<TrackOptions*>, 2> m_siblings_by_data_type;  // Event/Samples
    std::vector<TrackOptions*>                m_siblings_by_selection;

    // Aggregate options; selected tracks + like tracks...
    std::unique_ptr<TrackOptions> m_options_aggregate_selected;
    std::unique_ptr<TrackOptions> m_options_aggregate_type;

    const TimelineSelection& m_selection;
    const SettingsManager&   m_settings;

    std::function<void()> m_render_sort_menu;
};

}  // namespace View
}  // namespace RocProfVis
