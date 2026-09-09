// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include "app_tests.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_minimap.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_summary_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include <string>
#include <filesystem>
#include <fstream>
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_data_provider.h"
#include "model/rocprofvis_trace_data_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "widgets/rocprofvis_tab_container.h"
#include "rocprofvis_view_test_access.h"
using namespace RocProfVis::View;

namespace
{
    TraceView* GetTraceViewOrSkip(ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        // A null project means the db never opened (a real regression); fail hard.
        // A non-null project of the wrong view type is an expected wrong-db skip.
        IM_CHECK_RETV(project != nullptr, nullptr);
        TraceView* tv = dynamic_cast<TraceView*>(project->GetView().get());
        if (tv == nullptr)
        {
            ctx->LogWarning("SKIP: no trace view loaded (open a system/trace profile to exercise this)");
            return nullptr;
        }
        return tv;
    }

    ComputeView* GetComputeViewOrSkip(ImGuiTestContext* ctx)
    {
        AppWindow* app = AppWindow::GetInstance();
        Project* project = app->GetCurrentProject();
        IM_CHECK_RETV(project != nullptr, nullptr);
        ComputeView* cv = dynamic_cast<ComputeView*>(project->GetView().get());
        if (cv == nullptr)
        {
            ctx->LogWarning("SKIP: no compute view loaded (open a compute profile to exercise this)");
            return nullptr;
        }
        return cv;
    }

// Flame-graph event bars are raw draw_list rects registered with the Test
// Engine via IMGUI_TEST_ENGINE_ITEM_ADD under the track's "FV" child window.
// These helpers gather that window's bars and pick reliably clickable targets
// by width: the widest two avoid 1px slivers a click would miss, and give
// multi-select tests two distinct targets on the same lane.

// Finds the two widest bars under `flame_window_id`. out_second may stay empty
// if fewer than two bars exist. Returns the number found (0, 1, or 2).
int WidestFlameBars(ImGuiTestContext* ctx, unsigned int flame_window_id,
                    ImGuiTestItemInfo& out_first, ImGuiTestItemInfo& out_second)
{
    if(flame_window_id == 0) return 0;
    ImGuiTestItemList bars;
    ctx->GatherItems(&bars, ImGuiTestRef(flame_window_id));

    int   found        = 0;
    float first_width  = -1.0f;
    float second_width = -1.0f;
    for(const ImGuiTestItemInfo& bar : bars)
    {
        const float width = bar.RectFull.GetWidth();
        if(width > first_width)
        {
            out_second   = out_first;
            second_width = first_width;
            out_first    = bar;
            first_width  = width;
        }
        else if(width > second_width)
        {
            out_second   = bar;
            second_width = width;
        }
        found = found < 2 ? found + 1 : 2;
    }
    return found;
}

bool FirstEventScreenCenter(ImGuiTestContext* ctx, unsigned int flame_window_id,
                            ImVec2& out_center)
{
    ImGuiTestItemInfo first, second;
    if(WidestFlameBars(ctx, flame_window_id, first, second) < 1) return false;
    out_center = first.RectFull.GetCenter();
    return true;
}

bool TwoEventScreenCenters(ImGuiTestContext* ctx, unsigned int flame_window_id,
                           ImVec2& out_first, ImVec2& out_second)
{
    ImGuiTestItemInfo first, second;
    if(WidestFlameBars(ctx, flame_window_id, first, second) < 2) return false;
    out_first  = first.RectFull.GetCenter();
    out_second = second.RectFull.GetCenter();
    return true;
}

// Finds the first pair of bars that overlap in time but sit on different
// rows, returning their centers. Only overlapping bars meaningfully test
// selection -- well-separated bars have disjoint hit-regions and resolve
// correctly regardless, so clicking them would prove nothing.
bool TwoStackedEventScreenCenters(ImGuiTestContext* ctx, unsigned int flame_window_id,
                                  ImVec2& out_first, ImVec2& out_second)
{
    if(flame_window_id == 0) return false;
    ImGuiTestItemList bars;
    ctx->GatherItems(&bars, ImGuiTestRef(flame_window_id));

    const int count = bars.GetSize();
    // Skip bars too thin to reliably click.
    const float kMinClickWidth = 4.0f;
    for(int i = 0; i < count; i++)
    {
        const ImGuiTestItemInfo* a = bars.GetByIndex(i);
        if(a->RectFull.GetWidth() < kMinClickWidth) continue;
        // Two bars are on different rows if their centers are more than half a
        // bar-height apart.
        const float row_threshold = a->RectFull.GetHeight() * 0.5f;
        for(int j = i + 1; j < count; j++)
        {
            const ImGuiTestItemInfo* b = bars.GetByIndex(j);
            if(b->RectFull.GetWidth() < kMinClickWidth) continue;
            const bool overlap_x = a->RectFull.Min.x < b->RectFull.Max.x &&
                                   b->RectFull.Min.x < a->RectFull.Max.x;
            const bool diff_row =
                fabsf(a->RectFull.GetCenter().y - b->RectFull.GetCenter().y) > row_threshold;
            if(overlap_x && diff_row)
            {
                out_first  = a->RectFull.GetCenter();
                out_second = b->RectFull.GetCenter();
                return true;
            }
        }
    }
    return false;
}

// Opens the "Track Options" gear menu for the flame track whose FV child window
// is fv_id and returns the submenu window (nullptr on failure). Pass the fv_id
// of the track the caller asserts on, so the menu and the assertion target the
// same track. Menu entries are matched by DebugLabel substring: their labels
// carry icon-padding spaces and their ids change every run. Caller must
// PopupCloseAll().
ImGuiWindow* OpenTrackGearMenu(ImGuiTestContext* ctx, unsigned int fv_id)
{
    if(fv_id == 0)
    {
        ctx->LogWarning("SKIP: no rendered flame track to open a gear menu on");
        return nullptr;
    }
    ImGuiWindow* fv = ImGui::FindWindowByID(fv_id);
    if(fv == nullptr || fv->ParentWindow == nullptr) return nullptr;

    // A track's FV and MetaData Area windows are the two children of one
    // per-track container, so the sibling of this FV is the right meta window.
    ImGuiWindow* container = fv->ParentWindow;
    ImGuiWindow* meta      = nullptr;
    for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
    {
        if(w->WasActive && w->ParentWindow == container &&
           strstr(w->Name, "MetaData Area"))
        {
            meta = w;
            break;
        }
    }
    if(meta == nullptr)
    {
        ctx->LogWarning("SKIP: flame track has no MetaData Area window");
        return nullptr;
    }

    ctx->MouseMoveToPos(ImVec2(meta->Pos.x + meta->Size.x * 0.5f,
                               meta->Pos.y + meta->Size.y * 0.5f));
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->Yield(3);

    ImGuiTestItemList items;
    ctx->GatherItems(&items, "//$FOCUSED");
    ImGuiID gear_id = 0;
    for(int i = 0; i < items.GetSize(); i++)
        if(strstr(items[i]->DebugLabel, "Track Options")) { gear_id = items[i]->ID; break; }
    if(gear_id == 0) return nullptr;

    ctx->ItemClick(gear_id);
    ctx->Yield(3);

    for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
        if(w->WasActive && strstr(w->Name, "Track Options###Menu")) return w;
    return nullptr;
}

// Clicks a control in an open gear submenu whose label contains `label`.
// Returns false if no gathered item matches.
bool ClickGearMenuItem(ImGuiTestContext* ctx, ImGuiWindow* menu, const char* label)
{
    ctx->SetRef(menu);
    ImGuiTestItemList items;
    ctx->GatherItems(&items, "");
    for(int i = 0; i < items.GetSize(); i++)
        if(strstr(items[i]->DebugLabel, label))
        {
            ctx->ItemClick(items[i]->ID);
            return true;
        }
    return false;
}

// The track sidebar renders into a child window whose name embeds the split
// container's address, so there is no stable ref to it. Scan the active windows.
// Other views build left/right splits too, so a match only counts once the
// sidebar's own "Project" tree node is found inside it.
ImGuiWindow* FindSidebarWindow(ImGuiTestContext* ctx)
{
    for(ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
    {
        if(!w->WasActive || strstr(w->Name, "LeftColumn") == nullptr) continue;
        if(ctx->ItemExists(ImHashStr("Project", 0, w->ID))) return w;
    }
    return nullptr;
}

// Resolve a track's sidebar row button by id, not by DebugLabel. ImGui only
// records a label for items that pass clipping, and the sidebar scrolls, so a row
// below the fold gathers with an empty label. The button's id is the track name
// hashed over the row's id scope, stable at any scroll position. ItemClick scrolls
// the row into view on its own.
ImGuiID TrackButtonId(ImGuiTestItemList& items, const std::string& name)
{
    for(int i = 0; i < items.GetSize(); i++)
        if(items[i]->ID == ImHashStr(name.c_str(), 0, items[i]->ParentID))
            return items[i]->ID;
    return 0;
}

// Restores show_summary when it goes out of scope. The Summary tests set it
// true to drive their load path; without this, that state would leak into
// later tests and cover the timeline.
struct ShowSummaryGuard
{
    bool prev;
    ShowSummaryGuard()
        : prev(SettingsManager::GetInstance().GetAppWindowSettings().show_summary)
    {
    }
    ~ShowSummaryGuard()
    {
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = prev;
    }
};

// Restores the tab set and active tab that existed when constructed, so a test
// that opens extra dbs (sys_shared_db_open_dedups_and_switches) doesn't leave
// stray tabs and a changed current project for the next test. The kTabClosed
// event that frees the project is queued, so the destructor yields to drain it.
struct TabStateGuard
{
    ImGuiTestContext* ctx;
    TabContainer*     tc;
    std::vector<std::string> start_ids;
    std::string              start_active_id;

    TabStateGuard(ImGuiTestContext* c, TabContainer* t) : ctx(c), tc(t)
    {
        if(!tc) return;
        for(const TabItem* tab : tc->GetTabs()) start_ids.push_back(tab->m_id);
        const TabItem* active = tc->GetActiveTab();
        if(active) start_active_id = active->m_id;
    }

    ~TabStateGuard()
    {
        if(!tc) return;
        std::vector<std::string> to_close;
        for(const TabItem* tab : tc->GetTabs())
        {
            bool was_present = false;
            for(const std::string& id : start_ids)
                if(id == tab->m_id) { was_present = true; break; }
            if(!was_present) to_close.push_back(tab->m_id);
        }
        for(const std::string& id : to_close) tc->RemoveTab(id);
        if(!start_active_id.empty()) tc->SetActiveTab(start_active_id);
        if(ctx) ctx->Yield(3);
    }
};
}  // namespace

void RegisterAppTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    t = IM_REGISTER_TEST(e, "app", "common_file_menu_exists");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        IM_CHECK(ctx->ItemExists("##MenuBar/File"));
    
    };

    t = IM_REGISTER_TEST(e, "app", "common_file_menu_opens");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ctx->SetRef("Main Window");
        ctx->ItemClick("##MenuBar/File");
        IM_CHECK(ctx->ItemExists("//Menu_00/Open"));
        // Close the menu so its popup doesn't intercept clicks in later tests.
        ctx->PopupCloseAll();
    };
    t = IM_REGISTER_TEST(e, "app", "sys_events_view_populates");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        AnalysisView* av = TraceViewTestPeer{*tv}.AnalysisViewPtr();
        IM_CHECK(av != nullptr);
        if (av == nullptr) return;
        EventsView* ev = AnalysisViewTestPeer{*av}.EventsViewPtr();
        IM_CHECK(ev != nullptr);
        if (ev == nullptr) return;

        // A prior run may have left an event selected; the clear is dispatched
        // through EventManager, so yield before asserting the empty baseline.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        IM_CHECK(EventsViewTestPeer{*ev}.EventItemCount() == 0);

        // Timeline events are canvas-drawn (no widget IDs). Click the screen
        // center of the first rendered event box, captured by the renderer with
        // the geometry it draws/hit-tests with, so the click is window- and
        // trace-independent (no hard-coded sidebar/track offsets).
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);

        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        // Selection is deferred a frame, so move/release with the mouse parked.
        ctx->MouseMoveToPos(event_center);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(3);

        IM_CHECK(EventsViewTestPeer{*ev}.EventItemCount() > 0);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_zoom_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // The W/S zoom hotkeys only fire while the timeline has pseudo-focus,
        // which a mouse-down inside the graph sets. Park the cursor on the first
        // rendered event (its captured center is a point known to be in-graph)
        // and press the mouse to acquire focus.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        const float zoom_before = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;

        // Zoom in: hotkey is consumed in the timeline's per-frame input handler,
        // so keep the cursor parked in-graph while pressing.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const float zoom_in = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;
        IM_CHECK(zoom_in > zoom_before);

        // Zoom out returns toward the starting zoom.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_S);
        ctx->Yield(3);
        const float zoom_out = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords().z;
        IM_CHECK(zoom_out < zoom_in);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_bookmark_save_restore_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Ctrl+1 always writes bookmark slot 1, and m_bookmarks is a slot-keyed
        // map, so a re-save overwrites rather than grows it. Clear first so the
        // 0->1 assertion holds on every invocation in a persistent process (the
        // interactive harness reuses one process; headless is fresh each run).
        TraceViewTestPeer{*tv}.ClearBookmarks();
        ctx->Yield(1);
        IM_CHECK(TraceViewTestPeer{*tv}.BookmarkCount() == 0);

        // HandleHotKeys is gated on IsWindowFocused(RootAndChildWindows) for
        // "Main Window". Headless that focus is implicit, but interactively the
        // Test Engine window holds it and the chord is dropped. Focus explicitly,
        // then click an event to land the cursor in-graph.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->WindowFocus("Main Window");
        ctx->MouseMoveToPos(event_center);
        ctx->MouseClick(0);
        ctx->Yield(2);

        // Save the current view, then record the coords the bookmark captured.
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_1);
        ctx->Yield(3);
        IM_CHECK(TraceViewTestPeer{*tv}.BookmarkCount() == 1);
        const ViewCoords saved = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        const double saved_span = saved.v_max_x - saved.v_min_x;
        IM_CHECK(saved_span > 0.0);
        if (saved_span <= 0.0) return;

        // Zoom in so the view range changes away from the saved one.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);
        const ViewCoords moved = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        IM_CHECK((moved.v_max_x - moved.v_min_x) < saved_span);

        // Restore: bare "1" moves the view back to the saved range. Restore
        // reconstructs zoom/offset from the saved span through a float, so assert
        // within a small relative tolerance rather than exact equality.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_1);
        ctx->Yield(3);
        const ViewCoords restored = TraceViewTestPeer{*tv}.TimelineViewPtr()->GetViewCoords();
        const double tol = saved_span * 0.01;
        IM_CHECK(fabs(restored.v_min_x - saved.v_min_x) < tol);
        IM_CHECK(fabs(restored.v_max_x - saved.v_max_x) < tol);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_multi_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Start from a clean selection.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Need two distinct, clickable events in one flame track.
        ctx->Yield(3);
        ImVec2 first(0.0f, 0.0f), second(0.0f, 0.0f);
        bool have_two = TwoEventScreenCenters(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), first, second);
        if (!have_two)
        {
            ctx->LogWarning("SKIP: track lacks two distinct events to multi-select");
            return;
        }

        // Plain click selects the first event.
        ctx->MouseMoveToPos(first);
        ctx->MouseClick(0);
        ctx->Yield(3);
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 1);

        // Ctrl+click the second event adds to the selection (multi-select)
        // rather than replacing it. KeyDown holds the modifier the flame track
        // reads via HotkeyManager during click handling.
        ctx->KeyDown(ImGuiMod_Ctrl);
        ctx->MouseMoveToPos(second);
        ctx->MouseClick(0);
        ctx->Yield(3);
        ctx->KeyUp(ImGuiMod_Ctrl);
        ctx->Yield(2);

        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 2);

        // Leave a clean selection for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_minimap_toggle_drives_click");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        Minimap* mm = TraceViewTestPeer{*tv}.MinimapPtr();
        IM_CHECK(mm != nullptr);
        if (mm == nullptr) return;

        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == true);
        // Counter overlay state before touching events, to confirm the two
        // layers toggle independently (checklist: "Layers independent").
        const bool counters_before = MinimapTestPeer{*mm}.ShowCounters();

        // The ICON_COMPASS toolbar button and the ##events checkbox are both real
        // ImGui widgets nested under unknown layout child-windows. The "**/"
        // wildcard ref locates them past those intermediates by trailing label.
        // The Minimap is a plain Begin("Minimap") window, not a popup, so ref it
        // by title rather than $FOCUSED (which the window does not reliably own).
        ctx->Yield(3);
        ctx->ItemClick("//Main Window/**/\uF273");  // \uF273 = ICON_COMPASS
        ctx->Yield(3);

        ctx->SetRef("//Minimap");
        IM_CHECK(ctx->ItemExists("**/##events"));
        ctx->ItemClick("**/##events");
        ctx->Yield(2);
        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == false);
        // Toggling events must not disturb the counter overlay.
        IM_CHECK(MinimapTestPeer{*mm}.ShowCounters() == counters_before);

        // Toggle back to confirm the click is bidirectional.
        ctx->ItemClick("**/##events");
        ctx->Yield(2);
        IM_CHECK(MinimapTestPeer{*mm}.ShowEvents() == true);

        // Close the Minimap popup so it doesn't cover later tests' widgets.
        ctx->ItemClick("//Main Window/**/\uF273");  // \uF273 = ICON_COMPASS
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_view_tab_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        if (tc == nullptr)
        {
            ctx->LogWarning("SKIP: compute view has no tab container");
            return;
        }

        IM_CHECK(TabContainerTestPeer{*tc}.TabCount() >= 2);
        if (TabContainerTestPeer{*tc}.TabCount() < 2) return;

        ctx->Yield(3);
        const int start_idx = TabContainerTestPeer{*tc}.ActiveTabIndex();
        IM_CHECK(start_idx >= 0);

        // Each tab is submitted as BeginTabItem(label) under PushID(id), nested in
        // unknown layout child-windows. The "**/" wildcard locates the tab header
        // by its trailing label past those intermediates and the id seed.
        const int target_idx = (start_idx == 0) ? 1 : 0;
        const std::vector<const TabItem*> tabs = tc->GetTabs();
        IM_CHECK(target_idx < static_cast<int>(tabs.size()));
        if (target_idx >= static_cast<int>(tabs.size())) return;
        const std::string target_label = tabs[target_idx]->m_label;

        ctx->ItemClick(("//Main Window/**/" + target_label).c_str());
        ctx->Yield(3);

        IM_CHECK(TabContainerTestPeer{*tc}.ActiveTabIndex() == target_idx);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_workload_details_populates");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        if (tc == nullptr)
        {
            ctx->LogWarning("SKIP: compute view has no tab container");
            return;
        }

        const std::vector<const TabItem*> tabs = tc->GetTabs();
        ComputeWorkloadView* wv = nullptr;
        std::string          wv_label;
        for (const TabItem* tab : tabs)
        {
            if (tab->m_id == "compute_workload_view")
            {
                wv       = dynamic_cast<ComputeWorkloadView*>(tab->m_widget.get());
                wv_label = tab->m_label;
                break;
            }
        }
        if (wv == nullptr)
        {
            ctx->LogWarning("SKIP: no Workload Details tab in this build");
            return;
        }

        // m_workload_info populates in Render(), so the tab must be active first.
        ctx->ItemClick(("//Main Window/**/" + wv_label).c_str());
        ctx->Yield(3);

        ComputeWorkloadViewTestPeer peer{*wv};
        IM_CHECK(peer.WorkloadInfoPtr() != nullptr);
        if (peer.WorkloadInfoPtr() == nullptr) return;

        // Both panels fill only when the render gate passes: 2 cols, non-empty.
        IM_CHECK(peer.SystemInfoCols() == 2);
        IM_CHECK(peer.SystemInfoRows() > 0);
        IM_CHECK(peer.ProfilingConfigCols() == 2);
        IM_CHECK(peer.ProfilingConfigRows() > 0);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_workload_auto_selected");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Loading a compute profile auto-selects the first workload, which
        // cascades to selecting that workload's first kernel. Both selection
        // ids must therefore be valid (not the INVALID sentinel) once loaded.
        ctx->Yield(3);
        IM_CHECK(sel->GetSelectedWorkload() != ComputeSelection::INVALID_SELECTION_ID);
        IM_CHECK(sel->GetSelectedKernel() != ComputeSelection::INVALID_SELECTION_ID);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_comparison_target_kernel_computes_delta");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        if (tc == nullptr)
        {
            ctx->LogWarning("SKIP: compute view has no tab container");
            return;
        }

        const std::vector<const TabItem*> tabs = tc->GetTabs();
        ComputeComparisonView* comp = nullptr;
        std::string            comp_label;
        for (const TabItem* tab : tabs)
        {
            if (tab->m_id == "compute_comparison_view")
            {
                comp       = dynamic_cast<ComputeComparisonView*>(tab->m_widget.get());
                comp_label = tab->m_label;
                break;
            }
        }
        if (comp == nullptr)
        {
            ctx->LogWarning("SKIP: no Baseline Comparison tab in this build");
            return;
        }

        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        const uint32_t workload = sel->GetSelectedWorkload();
        const uint32_t baseline_kernel = sel->GetSelectedKernel();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        IM_CHECK(baseline_kernel != ComputeSelection::INVALID_SELECTION_ID);

        ctx->ItemClick(("//Main Window/**/" + comp_label).c_str());
        ctx->Yield(3);

        ComputeComparisonViewTestPeer peer{*comp};

        // The toolbar combos live in a nested child window the "//Main Window/**/"
        // wildcard can't reach. Find it by name fragment, click relative to it, and
        // re-find after each Yield (window pointers don't survive a rebuild).
        auto set_ref_to_toolbar = [&]() -> bool
        {
            ImGuiContext* g = ImGui::GetCurrentContext();
            for (ImGuiWindow* w : g->Windows)
            {
                if (w->WasActive && strstr(w->Name, "TabContainer") &&
                    strstr(w->Name, "/toolbar_"))
                {
                    ctx->SetRef(w);
                    return true;
                }
            }
            return false;
        };

        // Pick the target workload first; it enables the kernel combo.
        IM_CHECK(set_ref_to_toolbar());
        ctx->ItemClick("##TargetWorkloads");
        ctx->Yield(1);
        {
            ImGuiTestItemList items;
            ctx->GatherItems(&items, "//$FOCUSED");
            IM_CHECK(items.GetSize() >= 1);
            if (items.GetSize() < 1) return;
            ctx->ItemClick(items[0]->ID);
        }
        ctx->Yield(2);

        // Index into the target workload's kernels (not baseline's): that combo's
        // order is the gathered-item order we click by index below.
        const uint32_t target_workload = peer.TargetWorkloadId();
        std::vector<const KernelInfo*> kernels =
            cv->GetDataProvider()->ComputeModel().GetKernelInfoList(target_workload);
        int target_idx = -1;
        for (int i = 0; i < static_cast<int>(kernels.size()); i++)
        {
            if (kernels[i] != nullptr && kernels[i]->id != baseline_kernel)
            {
                target_idx = i;
                break;
            }
        }
        if (target_idx < 0)
        {
            ctx->LogWarning("SKIP: target workload has no kernel distinct from the baseline");
            return;
        }

        IM_CHECK(set_ref_to_toolbar());
        ctx->ItemClick("##target_kernels");
        ctx->Yield(1);
        {
            ImGuiTestItemList items;
            ctx->GatherItems(&items, "//$FOCUSED");
            IM_CHECK(target_idx < items.GetSize());
            if (target_idx >= items.GetSize()) return;
            ctx->ItemClick(items[target_idx]->ID);
        }
        ctx->Yield(2);
        ctx->SetRef("//Main Window");

        // Baseline and target fetch sequentially, so a "while pending" drain can
        // slip through the gap between them. Poll the final end state instead.
        const uint32_t want_kernel = kernels[target_idx]->id;
        for (int i = 0; i < 300; i++)
        {
            if (peer.TargetKernelId() == want_kernel && !peer.RequestsPending() &&
                peer.CategoryCount() > 0 && peer.HasDifferenceColumn())
                break;
            ctx->Yield(2);
        }

        IM_CHECK(peer.TargetKernelId() == want_kernel);
        IM_CHECK(peer.CategoryCount() > 0);
        IM_CHECK(peer.HasDifferenceColumn());
    };

    t = IM_REGISTER_TEST(e, "app", "compute_table_view_pin_persists_across_kernel_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        IM_CHECK(tc != nullptr);
        if (tc == nullptr) return;

        const std::vector<const TabItem*> tabs = tc->GetTabs();
        ComputeTableView* tbl = nullptr;
        for (const TabItem* tab : tabs)
        {
            if (tab->m_id == "compute_table_view")
            {
                tbl = dynamic_cast<ComputeTableView*>(tab->m_widget.get());
                break;
            }
        }
        if (tbl == nullptr)
        {
            ctx->LogWarning("SKIP: no Table View tab in this build");
            return;
        }

        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        const uint32_t workload = sel->GetSelectedWorkload();
        const uint32_t baseline_kernel = sel->GetSelectedKernel();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        IM_CHECK(baseline_kernel != ComputeSelection::INVALID_SELECTION_ID);

        std::vector<const KernelInfo*> kernels =
            cv->GetDataProvider()->ComputeModel().GetKernelInfoList(workload);
        if (kernels.size() < 2)
        {
            ctx->LogWarning("SKIP: workload has fewer than two kernels to switch between");
            return;
        }

        tc->SetActiveTab("compute_table_view");
        ctx->Yield(3);
        ComputeTableViewTestPeer peer{*tbl};
        for (int i = 0; i < 200 && (peer.FetchPending() || peer.TableWidgetCount() == 0); i++)
            ctx->Yield(2);
        IM_CHECK(peer.TableWidgetCount() > 0);
        if (peer.TableWidgetCount() == 0) return;

        // Metric tables sit in a nested child window the "//Main Window/**/"
        // wildcard can't reach; grab the innermost "_table" one.
        auto find_table_window = [&]() -> ImGuiWindow*
        {
            ImGuiWindow* found = nullptr;
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if (w->WasActive && strstr(w->Name, "_table") &&
                    strstr(w->Name, "TabContainer"))
                    found = w;  // keep last = deepest
            return found;
        };
        ImGuiWindow* table_win = find_table_window();
        IM_CHECK(table_win != nullptr);
        if (table_win == nullptr) return;

        // Each metric row starts with an empty-label pin Checkbox("") in column 0,
        // followed by the metric-id cell (label like "0.1.3:Duration"). So the pin
        // control is the empty-label item just before a cell whose label starts with
        // a digit and contains a dot.
        ctx->SetRef(table_win);
        ImGuiTestItemList items;
        ctx->GatherItems(&items, "");
        ImGuiID pin_checkbox = 0;
        for (int i = 1; i < items.GetSize(); i++)
        {
            const char* lbl = items[i]->DebugLabel;
            const bool looks_like_id =
                lbl[0] >= '0' && lbl[0] <= '9' && strchr(lbl, '.') != nullptr;
            if (looks_like_id && items[i - 1]->DebugLabel[0] == '\0')
            {
                pin_checkbox = items[i - 1]->ID;
                break;
            }
        }
        IM_CHECK(pin_checkbox != 0);
        if (pin_checkbox == 0) { ctx->SetRef("//Main Window"); return; }

        IM_CHECK(peer.PinnedCount() == 0);
        ctx->ItemClick(pin_checkbox);
        ctx->Yield(3);
        ctx->SetRef("//Main Window");

        // Remember what got pinned so we can check it survives the kernel switch.
        IM_CHECK(peer.PinnedCount() == 1);
        if (peer.PinnedCount() != 1) return;
        const MetricId pinned = peer.FirstPinned();

        // Switch kernels: the table refetches, but pins should persist
        // (ComputeTableView::RestoreMetricPining).
        uint32_t other_kernel = ComputeSelection::INVALID_SELECTION_ID;
        for (const KernelInfo* k : kernels)
            if (k != nullptr && k->id != baseline_kernel) { other_kernel = k->id; break; }
        IM_CHECK(other_kernel != ComputeSelection::INVALID_SELECTION_ID);

        // Wait for the refetch to START before draining: SelectKernel's event fires
        // a frame later, so an immediate drain would see no pending fetch and exit.
        sel->SelectKernel(other_kernel);
        for (int i = 0; i < 20 && !peer.FetchPending(); i++) ctx->Yield(1);
        for (int i = 0; i < 300 && (peer.FetchPending() || peer.TableWidgetCount() == 0); i++)
            ctx->Yield(2);

        const bool still_pinned = peer.IsPinned(pinned);

        // Restore before asserting: IM_CHECK aborts on failure, and pins + kernel
        // selection are shared across compute tests.
        sel->SelectKernel(baseline_kernel);
        ctx->Yield(3);
        for (int i = 0; i < 200 && peer.FetchPending(); i++) ctx->Yield(2);
        if (peer.IsPinned(pinned)) peer.Unpin(pinned);

        IM_CHECK(still_pinned);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_pan_hotkey");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        // Pan (A/D) shares the timeline's m_pseudo_focus gate, set by a mouse-down
        // in the graph; park on the first event to acquire it.
        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        // At zoom 1 the view spans the full range, so pan is clamped with no
        // headroom; zoom in first to make room to move.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);

        // Pan left first so the subsequent D pan always has right-headroom
        // regardless of where the zoom centered the view.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_A);
        ctx->KeyPress(ImGuiKey_A);
        ctx->Yield(3);

        const double v_min_before = tlv->GetViewCoords().v_min_x;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_D);
        ctx->Yield(3);
        const double v_min_right = tlv->GetViewCoords().v_min_x;
        IM_CHECK(v_min_right > v_min_before);

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_A);
        ctx->Yield(3);
        const double v_min_left = tlv->GetViewCoords().v_min_x;
        IM_CHECK(v_min_left < v_min_right);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_vscroll");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);

        // Scroll step is a fraction of the max scroll offset, which is 0 when all
        // tracks fit the viewport; Up/Down then no-op and the assertion would
        // false-fail. Skip rather than assert on a trace with no headroom.
        if (TimelineViewTestPeer{*tlv}.MaxYScroll() <= 0.0f)
        {
            ctx->LogWarning("SKIP: all tracks fit the viewport, no vertical scroll headroom");
            return;
        }

        const double y_before = tlv->GetViewCoords().y;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->Yield(3);
        const double y_down = tlv->GetViewCoords().y;

        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_UpArrow);
        ctx->Yield(3);
        const double y_up = tlv->GetViewCoords().y;

        IM_CHECK(y_down > y_before);
        IM_CHECK(y_up < y_down);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_reset_view_button");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        // Dirty the view via zoom alone; vertical scroll can no-op when all
        // tracks fit the viewport, so it is not a reliable way off default.
        ctx->MouseMoveToPos(event_center);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_W);
        ctx->KeyPress(ImGuiKey_W);
        ctx->Yield(3);

        const float zoom_dirty = tlv->GetViewCoords().z;
        IM_CHECK(zoom_dirty > 1.0f);

        // "Reset View" lives inside BeginChild("Toolbar") (no stable top-level
        // path), so reach it with a recursive wildcard search.
        ctx->SetRef("Main Window");
        ctx->ItemClick("**/Reset View");
        ctx->Yield(3);

        // Reset restores the full range (zoom ~1.0), not merely a smaller zoom;
        // pin to the default so a partial reset (e.g. still 2x) fails.
        const float  zoom_after = tlv->GetViewCoords().z;
        const double y_after    = tlv->GetViewCoords().y;
        IM_CHECK(zoom_after <= 1.0f + 0.01f);
        IM_CHECK(y_after == 0.0);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_compact_mode_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        FlameTrackItem* flame = TimelineViewTestPeer{*tlv}.FirstFlameTrack();
        IM_CHECK(flame != nullptr);
        if (flame == nullptr) return;

        // Click the real "Compact Mode" checkbox so the test covers the menu
        // wiring, not just the field. Turning it on shrinks the level height.
        const unsigned int fv_id = FlameTrackItemTestPeer{*flame}.FlameWindowId();
        const bool  orig_compact = flame->IsCompactMode();
        const float orig_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        ImGuiWindow* menu = OpenTrackGearMenu(ctx, fv_id);
        if (menu == nullptr) return;  // logged skip inside the helper
        const bool clicked_on = ClickGearMenuItem(ctx, menu, "Compact Mode");
        ctx->PopupCloseAll();
        ctx->Yield(2);
        IM_CHECK(clicked_on);
        if (!clicked_on) return;

        const bool  on_compact = flame->IsCompactMode();
        const float on_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        // Toggle back through the checkbox to restore state for later tests. A
        // peer restore backstops it in case the second click fails to register.
        ImGuiWindow* menu2       = OpenTrackGearMenu(ctx, fv_id);
        bool         clicked_off = false;
        if (menu2 != nullptr) clicked_off = ClickGearMenuItem(ctx, menu2, "Compact Mode");
        ctx->PopupCloseAll();
        ctx->Yield(2);
        if (flame->IsCompactMode() != orig_compact)
            FlameTrackItemTestPeer{*flame}.SetCompactMode(orig_compact);
        ctx->Yield(2);
        const bool  back_compact = flame->IsCompactMode();
        const float back_height  = FlameTrackItemTestPeer{*flame}.LevelHeight();

        IM_CHECK(on_compact != orig_compact);
        IM_CHECK(on_height != orig_height);
        IM_CHECK(clicked_off);
        IM_CHECK(back_compact == orig_compact);
        IM_CHECK(back_height == orig_height);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_mark_time_range");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Start from no event selection and no marked range.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        sel->ClearTimeRange();
        ctx->Yield(3);
        IM_CHECK(!sel->HasValidTimeRangeSelection());

        // The M (Toggle Mark) hotkey builds a time-range from the selected
        // events, so select one first. The hotkey shares the timeline's
        // pseudo-focus gate (set by a mouse-down in the graph), same as W/S/A/D.
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = FirstEventScreenCenter(
            ctx, TimelineViewTestPeer{*tlv}.FirstFlameWindowId(), event_center);
        IM_CHECK(have_center);
        if (!have_center) return;

        ctx->MouseMoveToPos(event_center);
        ctx->MouseClick(0);
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() >= 1);

        // M marks the selected events' time range.
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_M);
        ctx->Yield(3);
        IM_CHECK(sel->HasValidTimeRangeSelection());

        // M again clears the marked range (toggle).
        ctx->MouseMoveToPos(event_center);
        ctx->KeyPress(ImGuiKey_M);
        ctx->Yield(3);
        IM_CHECK(!sel->HasValidTimeRangeSelection());

        // Leave a clean selection state for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_event_color_mode");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);
        FlameTrackItem* flame = TimelineViewTestPeer{*tlv}.FirstFlameTrack();
        IM_CHECK(flame != nullptr);
        if (flame == nullptr) return;

        using ColorMode = EventTrackOptions::EventColorMode;
        auto label_for = [](ColorMode m) -> const char* {
            switch (m)
            {
                case ColorMode::kByEventName: return "Color by Name";
                case ColorMode::kByTimeLevel: return "Color by Time Level";
                case ColorMode::kNone:        return "No Color";
                default:                      return nullptr;  // kMixed has no radio
            }
        };

        // Click the real color-mode radio so the test covers the menu wiring,
        // not just the field. Switch to a different mode, then restore the
        // original by clicking its radio.
        const ColorMode orig  = FlameTrackItemTestPeer{*flame}.GetEventColorMode();
        const ColorMode other = (orig == ColorMode::kByTimeLevel)
                                    ? ColorMode::kByEventName
                                    : ColorMode::kByTimeLevel;
        const char* orig_label = label_for(orig);
        if (orig_label == nullptr)
        {
            ctx->LogWarning("SKIP: track color mode has no radio to restore to (kMixed)");
            return;
        }
        const unsigned int fv_id = FlameTrackItemTestPeer{*flame}.FlameWindowId();

        ImGuiWindow* menu = OpenTrackGearMenu(ctx, fv_id);
        if (menu == nullptr) return;  // logged skip inside the helper
        const bool clicked_other = ClickGearMenuItem(ctx, menu, label_for(other));
        ctx->PopupCloseAll();
        ctx->Yield(2);
        IM_CHECK(clicked_other);
        if (!clicked_other) return;
        const ColorMode changed = FlameTrackItemTestPeer{*flame}.GetEventColorMode();

        // Restore the original mode through its radio. A peer restore backstops
        // it in case the click fails to register.
        ImGuiWindow* menu2       = OpenTrackGearMenu(ctx, fv_id);
        bool         clicked_orig = false;
        if (menu2 != nullptr) clicked_orig = ClickGearMenuItem(ctx, menu2, orig_label);
        ctx->PopupCloseAll();
        ctx->Yield(2);
        if (FlameTrackItemTestPeer{*flame}.GetEventColorMode() != orig)
            FlameTrackItemTestPeer{*flame}.SetEventColorMode(orig);
        ctx->Yield(2);
        const ColorMode restored = FlameTrackItemTestPeer{*flame}.GetEventColorMode();

        IM_CHECK(changed == other);
        IM_CHECK(changed != orig);
        IM_CHECK(clicked_orig);
        IM_CHECK(restored == orig);
    };

    t = IM_REGISTER_TEST(e, "app", "common_settings_theme_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        SettingsManager& sm = SettingsManager::GetInstance();

        // Flip the theme the same way the Preferences combo does: mutate the live
        // user settings then ApplyUserSettings(old, save_json=false). The false
        // keeps it off the on-disk settings json. The applied palette (GetColor)
        // must follow, not just the bool, so assert a theme color changes too.
        // Restore at the end so later tests / a 2nd run see the original theme.
        const bool   orig_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32  orig_bg   = sm.GetColor(Colors::kBgMain);

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // SettingsManager is a process-global singleton, so asserting before the
        // restore would leak a flipped theme into every later test and the next run.
        UserSettings before = sm.GetUserSettings();
        sm.GetUserSettings().display_settings.use_dark_mode = !orig_dark;
        sm.ApplyUserSettings(before, false);
        ctx->Yield(2);
        const bool  flipped_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32 flipped_bg   = sm.GetColor(Colors::kBgMain);

        UserSettings flipped = sm.GetUserSettings();
        sm.GetUserSettings().display_settings.use_dark_mode = orig_dark;
        sm.ApplyUserSettings(flipped, false);
        ctx->Yield(2);
        const bool  back_dark = sm.GetUserSettings().display_settings.use_dark_mode;
        const ImU32 back_bg   = sm.GetColor(Colors::kBgMain);

        IM_CHECK(flipped_dark != orig_dark);
        IM_CHECK(flipped_bg != orig_bg);
        IM_CHECK(back_dark == orig_dark);
        IM_CHECK(back_bg == orig_bg);
    };

    t = IM_REGISTER_TEST(e, "app", "common_settings_time_unit_change");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        SettingsManager& sm = SettingsManager::GetInstance();

        // Change the timeline time unit via the same path the Units combo uses,
        // then restore the original so later tests / a 2nd run see the default.
        const TimeFormat orig = sm.GetUserSettings().unit_settings.time_format;
        const TimeFormat other =
            (orig == TimeFormat::kNanoseconds) ? TimeFormat::kSeconds : TimeFormat::kNanoseconds;

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // SettingsManager is a process-global singleton, so asserting before the
        // restore would leak the changed time unit into later tests / the next run.
        UserSettings before = sm.GetUserSettings();
        sm.GetUserSettings().unit_settings.time_format = other;
        sm.ApplyUserSettings(before, false);
        ctx->Yield(2);
        const TimeFormat changed_fmt = sm.GetUserSettings().unit_settings.time_format;

        UserSettings changed = sm.GetUserSettings();
        sm.GetUserSettings().unit_settings.time_format = orig;
        sm.ApplyUserSettings(changed, false);
        ctx->Yield(2);
        const TimeFormat back_fmt = sm.GetUserSettings().unit_settings.time_format;

        IM_CHECK(changed_fmt == other);
        IM_CHECK(back_fmt == orig);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_histogram_normalization_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineModel& tl = tv->GetDataProvider()->DataModel().GetTimeline();

        // The All Tracks / Visible Tracks normalization toggle lives in the
        // histogram right-click popup (no stable path), so drive it through the
        // same model calls the menu items make. Assert the flag flips, then
        // restore the original so later tests / a 2nd run see the default.
        const bool orig_global = tl.IsNormalizeGlobal();

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and the
        // TimelineModel is shared across every timeline test in the process, so
        // asserting before the restore would leak the flipped mode into later tests.
        tl.ToggleNormalization();
        tl.UpdateHistogram({});
        ctx->Yield(2);
        const bool toggled_global = tl.IsNormalizeGlobal();

        tl.ToggleNormalization();
        tl.UpdateHistogram({});
        ctx->Yield(2);
        const bool back_global = tl.IsNormalizeGlobal();

        IM_CHECK(toggled_global != orig_global);
        IM_CHECK(back_global == orig_global);
    };

    t = IM_REGISTER_TEST(e, "app", "compute_kernel_select_changes");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        ctx->Yield(3);
        const uint32_t workload = sel->GetSelectedWorkload();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        const uint32_t auto_kernel = sel->GetSelectedKernel();
        IM_CHECK(auto_kernel != ComputeSelection::INVALID_SELECTION_ID);

        std::vector<const KernelInfo*> kernels =
            cv->GetDataProvider()->ComputeModel().GetKernelInfoList(workload);
        if (kernels.size() < 2)
        {
            ctx->LogWarning("SKIP: workload has fewer than two kernels to switch between");
            return;
        }

        uint32_t other = ComputeSelection::INVALID_SELECTION_ID;
        for (const KernelInfo* k : kernels)
        {
            if (k != nullptr && k->id != auto_kernel) { other = k->id; break; }
        }
        IM_CHECK(other != ComputeSelection::INVALID_SELECTION_ID);

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, and
        // ComputeSelection is shared across compute tests, so asserting before the
        // restore would leak the non-auto kernel into later tests / the next run.
        sel->SelectKernel(other);
        ctx->Yield(3);
        const uint32_t selected = sel->GetSelectedKernel();

        sel->SelectKernel(auto_kernel);
        ctx->Yield(2);

        IM_CHECK(selected == other);
        IM_CHECK(selected != auto_kernel);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_search_finds_results");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        EventSearch* es = TraceViewTestPeer{*tv}.EventSearchPtr();
        IM_CHECK(es != nullptr);
        if (es == nullptr) return;

        // Clear so the searched flag starts from a known baseline (the harness
        // reuses one process interactively).
        es->Clear();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == false);

        // The search term is coupled to the CI sample db (sample/rocpd-transpose.db):
        // it must name an event that exists AND is a searchable op type
        // (Launch/Dispatch/MemoryCopy/MemoryAllocate/LaunchSample -- see
        // EventSearch::Search). If the sample db changes, update it to a term the
        // new db contains.
        es->TextInput() = "hipLaunchKernel";
        es->Search();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == true);

        // The fetch is deferred; let it drain (Update re-runs Search when the
        // request completes) before reading the result count.
        for (int i = 0; i < 60 && EventSearchTestPeer{*es}.RequestPending(); i++) ctx->Yield(2);
        ctx->Yield(5);
        IM_CHECK(EventSearchTestPeer{*es}.ResultCount() > 0);

        es->Clear();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_search_zero_result_and_clear");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        EventSearch* es = TraceViewTestPeer{*tv}.EventSearchPtr();
        IM_CHECK(es != nullptr);
        if (es == nullptr) return;

        // Clear so the searched flag starts from a known baseline (the harness
        // reuses one process interactively).
        es->Clear();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == false);

        // A nonsense token no event name can contain, so the empty-result path is
        // exercised deterministically regardless of which db the harness was given.
        // Type into the real search field. RenderEventSearch runs the search on the
        // frame the focused field sees Enter, so Enter is what issues the query.
        ctx->SetRef("Main Window");
        ctx->ItemInput("**/search_bar/##input_text_with_clear");
        ctx->KeyCharsReplaceEnter("zzq_no_such_event_zzq");
        ctx->Yield(2);
        IM_CHECK(es->Searched() == true);

        // The fetch is deferred. Let it drain (Update re-runs Search when the
        // request completes) before reading the result count.
        for (int i = 0; i < 60 && EventSearchTestPeer{*es}.RequestPending(); i++) ctx->Yield(2);
        ctx->Yield(5);
        IM_CHECK(EventSearchTestPeer{*es}.ResultCount() == 0);

        // The X button is the clear path. IconButton pushes the glyph as an id and
        // draws it as the button, so the ref ends in the glyph twice.
        const std::string clear_ref =
            std::string("**/search_bar/") + ICON_X_CIRCLED + "/" + ICON_X_CIRCLED;
        ctx->ItemClick(clear_ref.c_str());
        ctx->Yield(2);
        IM_CHECK(es->Searched() == false);
        IM_CHECK(EventSearchTestPeer{*es}.ResultCount() == 0);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_pie_kernel_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // FetchSummary + TopKernels::Update only run while the Summary window is
        // shown; headless with no saved layout it may be closed, leaving the kernel
        // list null forever. Force it open ourselves rather than depending on another
        // test having flipped this shared setting earlier in the process.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;

        // Summary data loads asynchronously; let Update() populate the kernel list.
        // KernelCount() is 0 while m_kernels is still null.
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);

        // Pick a real kernel index, skipping the synthetic "Others" bucket (which
        // ToggleSelectKernel treats as a deselect). Bail if there's nothing else.
        size_t target = 0;
        if (peer.PaddedIdx() == target) target = 1;
        if (peer.KernelCount() <= target)
        {
            ctx->LogWarning("SKIP: summary has no selectable top kernel for this trace");
            return;
        }

        // This drives the model (ToggleSelectKernel) and asserts selection state.
        // The pie is ImPlot-canvas drawn with no widget ID, so the click-to-index
        // hit-test is out of scope here; only the selection wiring is covered.

        // Clear to a known baseline (the harness reuses one process interactively).
        peer.ClearSelection();
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx() == std::nullopt);

        peer.Select(target);
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx().has_value());
        IM_CHECK(peer.SelectedIdx().value() == target);

        peer.ClearSelection();
        ctx->Yield(2);
        IM_CHECK(peer.SelectedIdx() == std::nullopt);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_top_kernel_name");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // The Summary fetch (FetchSummary) and TopKernels::Update both run only when
        // the Summary window is shown; headless with no saved layout it may be closed,
        // leaving the kernel list null forever. Force it open before draining the load.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;

        // Summary data loads asynchronously; let Update() populate the kernel list.
        // KernelCount() is 0 while m_kernels is still null.
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);

        IM_CHECK(peer.KernelCount() > 0);
        IM_CHECK(!peer.KernelName(0).empty());
        for (size_t i=1; i < peer.KernelCount();i++){
            IM_CHECK(peer.ExecTimeSum(0) >= peer.ExecTimeSum(i));
        }

    };

    t = IM_REGISTER_TEST(e, "app", "sys_summary_display_mode_switch");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        SummaryView* sv = TraceViewTestPeer{*tv}.SummaryViewPtr();
        IM_CHECK(sv != nullptr);
        if (sv == nullptr) return;
        TopKernels* tk = SummaryViewTestPeer{*sv}.TopKernelsPtr();
        IM_CHECK(tk != nullptr);
        if (tk == nullptr) return;

        // The Pie/Bar/Table switcher renders only once the Summary window is
        // shown and its kernel list has loaded; force it open and drain the
        // async fetch first (mirrors sys_summary_top_kernel_name).
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = true;
        TopKernelsTestPeer peer{*tk};
        for (int i = 0; i < 60 && peer.KernelCount() == 0; i++) ctx->Yield(2);
        IM_CHECK(peer.KernelCount() > 0);

        // Reaching this point means the load path ran (not the skip), so the
        // mode assertions below genuinely exercise the switcher. Default is Pie.
        ctx->Yield(2);
        IM_CHECK(peer.IsDisplayPie());

        // The switcher is three IconButtons in the "Summary" window. IconButton
        // does PushID(glyph)+Button(glyph), so each button's ref ends in the icon
        // glyph twice (rocprovfis_icon_defines.h). The "**/" hops the dynamic
        // layout ids between the window and the button (clamped_view/RightColumn/
        // TopRow hashes that vary per build). Poll the mode after each click rather
        // than a fixed yield: the click settles over a variable number of frames.
        std::string bar_ref = std::string("//Summary/**/") + ICON_CHART_BAR + "/" + ICON_CHART_BAR;
        ctx->ItemClick(bar_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayBar(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayBar());

        std::string table_ref = std::string("//Summary/**/") + ICON_LIST + "/" + ICON_LIST;
        ctx->ItemClick(table_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayTable(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayTable());

        std::string pie_ref = std::string("//Summary/**/") + ICON_CHART_PIE + "/" + ICON_CHART_PIE;
        ctx->ItemClick(pie_ref.c_str());
        for (int i = 0; i < 30 && !peer.IsDisplayPie(); i++) ctx->Yield(2);
        IM_CHECK(peer.IsDisplayPie());
    };

    t = IM_REGISTER_TEST(e, "app", "compute_kernel_table_loads_sorted");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ComputeView* cv = GetComputeViewOrSkip(ctx);
        if (!cv) return;
        TabContainer* tc = ComputeViewTestPeer{*cv}.TabContainerPtr();
        IM_CHECK(tc != nullptr);
        if (tc == nullptr) return;

        // The kernel metric table renders only while the "Kernel Details" tab is
        // active (TabContainer renders just the active tab's content).
        tc->SetActiveTab("compute_kernel_details_view");
        ctx->Yield(3);
        const TabItem* tab = tc->GetActiveTab();
        IM_CHECK(tab != nullptr);
        if (tab == nullptr) return;
        ComputeKernelDetailsView* kd =
            dynamic_cast<ComputeKernelDetailsView*>(tab->m_widget.get());
        IM_CHECK(kd != nullptr);
        if (kd == nullptr) return;
        KernelMetricTable* kt = ComputeKernelDetailsViewTestPeer{*kd}.KernelMetricTablePtr();
        IM_CHECK(kt != nullptr);
        if (kt == nullptr) return;

        // The table fetches its rows in response to a workload-selection event.
        // The initial auto-select (ComputeView::CreateView) fires before this view
        // subscribes, so the table starts empty; drive the fetch ourselves for the
        // already-selected workload.
        ComputeSelection* sel = ComputeViewTestPeer{*cv}.ComputeSelectionPtr();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        const uint32_t workload = sel->GetSelectedWorkload();
        IM_CHECK(workload != ComputeSelection::INVALID_SELECTION_ID);
        kt->FetchData(workload);

        // Wait for the rows to arrive: the table registers its own ImGui window
        // (name contains "kernel_selection_table") only once BeginTable runs, which
        // requires non-empty data. Its presence proves the table rendered populated.
        ImGuiWindow* table_win = nullptr;
        for (int i = 0; i < 120 && table_win == nullptr; i++)
        {
            ctx->Yield(2);
            for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows)
                if (strstr(w->Name, "kernel_selection_table")) { table_win = w; break; }
        }
        IM_CHECK(table_win != nullptr);

        // On load the table sorts by the Duration column (index 2), descending
        // (KernelMetricTable ctor: DURATION_COLUMN_INDEX + kRPVControllerSortOrderDescending).
        KernelMetricTableTestPeer peer{*kt};
        IM_CHECK(peer.SortColumnIndex() == 2);

        // Prove the rows actually loaded in that order rather than just trusting the
        // ctor default (which would pass even on an empty/unsorted fetch): the model's
        // Duration cells are plain ns integers, so assert the column is monotonically
        // non-increasing. Header col 2 is "Duration (ns)".
        const std::vector<std::vector<std::string>>& rows =
            cv->GetDataProvider()->ComputeModel().GetKernelSelectionTable().GetTableData();
        if (rows.size() < 2)
        {
            ctx->LogWarning("SKIP: fewer than two kernel rows to verify sort order");
            return;
        }
        for (size_t r = 1; r < rows.size(); r++)
        {
            IM_CHECK(rows[r - 1].size() > 2 && rows[r].size() > 2);
            if (rows[r - 1].size() <= 2 || rows[r].size() <= 2) return;
            const double prev = std::strtod(rows[r - 1][2].c_str(), nullptr);
            const double cur  = std::strtod(rows[r][2].c_str(), nullptr);
            IM_CHECK(prev >= cur);
        }
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_select_named_track_event");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Event chart items populate after the track's data fetch drains, so poll
        // for a flame track that has at least one event before reaching in.
        FlameTrackItem* flame    = nullptr;
        uint64_t        track_id = 0;
        for (int i = 0; i < 60 && flame == nullptr; i++)
        {
            for (FlameTrackItem* candidate :
                 TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
            {
                if (FlameTrackItemTestPeer{*candidate}.ChartItemCount() > 0)
                {
                    flame    = candidate;
                    track_id = candidate->GetID();
                    break;
                }
            }
            if (flame == nullptr) ctx->Yield(2);
        }
        if (flame == nullptr)
        {
            ctx->LogWarning("SKIP: no flame track has events to select by identity");
            return;
        }

        // Target the earliest event by timestamp (chart-item order is not stable,
        // so identity comes from the (name, start_ts) pair, not an index).
        uint64_t    uuid     = 0;
        std::string name;
        double      start_ts = 0.0;
        bool        have_event =
            FlameTrackItemTestPeer{*flame}.EarliestEvent(uuid, name, start_ts);
        IM_CHECK(have_event);
        if (!have_event) return;

        // Selection is dispatched through EventManager, so clear then yield before
        // asserting the empty baseline.
        sel->UnselectAllEvents();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Select that exact event by its (track, uuid) identity, the same call the
        // flame track makes on a bar click.
        sel->SelectTrackEvent(track_id, uuid);
        ctx->Yield(3);
        IM_CHECK(sel->EventSelected(uuid));
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.size() == 1 && ids[0] == uuid);

        // Leave a clean selection for following tests.
        sel->UnselectAllEvents();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_track_details_populates_on_select");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        AnalysisView* av = TraceViewTestPeer{*tv}.AnalysisViewPtr();
        IM_CHECK(av != nullptr);
        if (av == nullptr) return;
        TrackDetails* td = AnalysisViewTestPeer{*av}.TrackDetailsPtr();
        IM_CHECK(td != nullptr);
        if (td == nullptr) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Tracks appear once the timeline's data fetch drains, so poll for a
        // displayed flame track before reaching in for one to select.
        FlameTrackItem* track = nullptr;
        for (int i = 0; i < 60 && track == nullptr; i++)
        {
            std::vector<FlameTrackItem*> flames =
                TimelineViewTestPeer{*tlv}.DisplayedFlameTracks();
            if (!flames.empty()) { track = flames.front(); break; }
            ctx->Yield(2);
        }
        if (track == nullptr)
        {
            ctx->LogWarning("SKIP: no displayed flame track to select");
            return;
        }
        const uint64_t track_id = track->GetID();

        // Selection dispatches async through EventManager, so poll after every drive.
        // The reused process may carry a prior test's selection, so reset first.
        sel->UnselectAllTracks();
        for (int i = 0; i < 60 && TrackDetailsTestPeer{*td}.DetailCount() != 0; i++) ctx->Yield(2);
        IM_CHECK(TrackDetailsTestPeer{*td}.DetailCount() == 0);

        // Select that exact track by identity, the same call a track-header click makes.
        sel->SelectTrack(*track);
        for (int i = 0; i < 60 && TrackDetailsTestPeer{*td}.DetailCount() == 0; i++) ctx->Yield(2);
        IM_CHECK(TrackDetailsTestPeer{*td}.DetailCount() == 1);
        IM_CHECK(TrackDetailsTestPeer{*td}.HasTrack(track_id));

        sel->UnselectTrack(*track);
        for (int i = 0; i < 60 && TrackDetailsTestPeer{*td}.DetailCount() != 0; i++) ctx->Yield(2);
        IM_CHECK(TrackDetailsTestPeer{*td}.DetailCount() == 0);

        // Leave a clean selection for following tests.
        sel->UnselectAllTracks();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_measure_tool");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        MeasurementController* mc = TraceViewTestPeer{*tv}.MeasurementControllerPtr();
        IM_CHECK(mc != nullptr);
        if (mc == nullptr) return;

        // Two distinct timestamps inside the trace's visible range; the measured
        // span between them must be > 0.
        const ViewCoords coords = tlv->GetViewCoords();
        const double     span   = coords.v_max_x - coords.v_min_x;
        IM_CHECK(span > 0.0);
        if (span <= 0.0) return;
        const double t0 = coords.v_min_x + span * 0.25;
        const double t1 = coords.v_min_x + span * 0.75;

        // Clear to a known baseline: measurement state persists on the TraceView
        // across tests in the reused process.
        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        IM_CHECK(mc->IsMeasurementMode() == false);

        // Enter mode and place both points via the same controller calls the menu
        // item and freehand click handler drive.
        mc->EnterMeasurementMode();
        mc->SetFreehandMeasurementPoint(t0);
        mc->SetFreehandMeasurementPoint(t1);
        ctx->Yield(2);

        // Capture observation, restore, THEN assert: IM_CHECK early-returns on
        // failure, so leaving measurement mode active would leak into later tests.
        const MeasurementState state = mc->GetMeasurementState();
        const double duration =
            std::fabs(mc->GetEffectiveTimestamp(1) - mc->GetEffectiveTimestamp(0));

        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        const bool inactive_after = (mc->IsMeasurementMode() == false);

        IM_CHECK(state == MeasurementState::kComplete);
        IM_CHECK(duration > 0.0);
        IM_CHECK(inactive_after);
    };

    // AIPROFVIS-333: dragging the Description-column splitter must
    // resize the sidebar without panning the timeline.
    t = IM_REGISTER_TEST(e, "app", "sys_splitter_resize_no_pan");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->SetRef("Main Window");
        ctx->Yield(3);

        auto tpt = tlv->GetTransform();
        IM_CHECK(tpt != nullptr);
        if (tpt == nullptr) return;
        const double range = tpt->GetRangeX();
        IM_CHECK(range > 0.0);
        if (range <= 0.0) return;

        // Restore before the asserts. IM_CHECK returns on failure, so a trailing
        // restore would leak state into later tests on abort.
        const float  orig_zoom    = tpt->GetZoom();
        const double orig_offset  = tpt->GetViewTimeOffsetNs();
        const float  orig_sidebar = TimelineViewTestPeer{*tlv}.SidebarSize();

        auto restore = [&]()
        {
            tpt->SetZoom(orig_zoom);
            tpt->SetViewTimeOffsetNs(orig_offset);
            TimelineViewTestPeer{*tlv}.SetSidebarSize(orig_sidebar);
        };

        // At zoom 1 the view spans the full range and any pan offset clamps to 0,
        // so the bug is invisible; zoom in to create pan headroom.
        //
        // SetZoom applies only the min bound; the max clamp (range/graph_size_x)
        // runs at render in ComputePixelMapping. So read the effective zoom back
        // after a Yield, center the offset on that, Yield again, then read the
        // achieved offset: if the clamp left no headroom, skip instead of false-green.
        tpt->SetZoom(4.0f);
        ctx->Yield(3);
        const float  zoom    = tpt->GetZoom();
        const double max_off = range - range / static_cast<double>(zoom);
        tpt->SetViewTimeOffsetNs(max_off * 0.5);
        ctx->Yield(3);
        const double achieved_off = tpt->GetViewTimeOffsetNs();
        if (achieved_off <= range * 1e-6)
        {
            restore();
            ctx->LogWarning("SKIP: zoom clamped, no pan headroom to detect the bug");
            return;
        }

        const char* splitter_ref = "**/##MovePositionLineVert";
        if (!ctx->ItemExists(splitter_ref))
        {
            restore();
            ctx->LogWarning("SKIP: description-column splitter not present");
            return;
        }

        const float  sidebar_before = TimelineViewTestPeer{*tlv}.SidebarSize();
        const double v_min_before   = tlv->GetViewCoords().v_min_x;

        ctx->ItemDragWithDelta(splitter_ref, ImVec2(40.0f, 0.0f));
        ctx->Yield(3);

        const float  sidebar_after = TimelineViewTestPeer{*tlv}.SidebarSize();
        const double v_min_after   = tlv->GetViewCoords().v_min_x;
        const double eps           = range * 1e-4;

        restore();
        ctx->Yield(2);

        // Guard: the drag must have resized the sidebar, else the pan check is vacuous.
        IM_CHECK(sidebar_after > sidebar_before);

        // The defect: resizing shifts the timeline min-x; correct behavior holds it.
        IM_CHECK(std::fabs(v_min_after - v_min_before) <= eps);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_timeline_track_expand_collapse");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;

        ctx->Yield(3);

        // Expanding only changes height when the track has enough levels for its
        // expanded height to exceed the default; find a flame track for which that
        // holds so the height assertion is meaningful.
        FlameTrackItem* flame = nullptr;
        for (FlameTrackItem* candidate : TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
        {
            FlameTrackItemTestPeer peer{*candidate};
            const float level_h = peer.LevelHeight();
            if (level_h > 0.0f &&
                peer.ExpandedTrackHeight() > peer.DefaultTrackHeight())
            {
                flame = candidate;
                break;
            }
        }
        if (flame == nullptr)
        {
            ctx->LogWarning("SKIP: no flame track deep enough for expand to change height");
            return;
        }

        FlameTrackItemTestPeer peer{*flame};
        const bool  orig_expanded = peer.IsExpanded();
        const float orig_height   = flame->GetTrackHeight();

        // The expand/collapse arrow sits in a meta area with no stable ref, so
        // drive the same side effect the button triggers. Force a collapsed
        // baseline, then expand, capturing both states.
        peer.SetExpanded(false);
        ctx->Yield(2);
        const bool  collapsed_state  = peer.IsExpanded();
        const float collapsed_height = flame->GetTrackHeight();

        peer.SetExpanded(true);
        ctx->Yield(2);
        const bool  expanded_state  = peer.IsExpanded();
        const float expanded_height = flame->GetTrackHeight();

        // Restore original state + exact height so later tests see the same layout.
        peer.SetExpanded(orig_expanded);
        peer.SetTrackHeight(orig_height);
        ctx->Yield(2);
        const bool  restored_state  = peer.IsExpanded();
        const float restored_height = flame->GetTrackHeight();

        IM_CHECK(collapsed_state == false);
        IM_CHECK(expanded_state == true);
        IM_CHECK(expanded_height != collapsed_height);
        IM_CHECK(restored_state == orig_expanded);
        IM_CHECK(restored_height == orig_height);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_stacked_event_select_distinct");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // Hide the Summary window so it doesn't sit over the timeline and catch
        // the bar clicks below.
        ShowSummaryGuard summary_guard;
        SettingsManager::GetInstance().GetAppWindowSettings().show_summary = false;
        ctx->Yield(2);

        // Start from an empty selection. Selection is async, so yield first.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        std::vector<uint64_t> ids;
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());

        // Look for a stacked, overlapping pair of bars to click. Scan every
        // flame track, since the first one may not contain such a pair.
        ctx->Yield(3);
        ImVec2 a(0.0f, 0.0f), b(0.0f, 0.0f);
        bool   have_targets = false;
        for (FlameTrackItem* flame : TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
        {
            if (flame == nullptr) continue;
            const unsigned int flame_window_id = FlameTrackItemTestPeer{*flame}.FlameWindowId();
            if (TwoStackedEventScreenCenters(ctx, flame_window_id, a, b))
            {
                have_targets = true;
                break;
            }
        }
        if (!have_targets)
        {
            ctx->LogWarning("SKIP: no lane with overlapping stacked events to disambiguate");
            return;
        }

        // Click the first bar and wait for the selection to settle on one event.
        ctx->MouseMoveToPos(a);
        ctx->MouseClick(0);
        for (int i = 0; i < 60 && ids.size() != 1; i++)
        {
            ctx->Yield(2);
            ids.clear();
            sel->GetSelectedEvents(ids);
        }
        IM_CHECK(ids.size() == 1);
        if (ids.size() != 1) return;
        const uint64_t id_a = ids.front();

        // Click the second bar. A plain click replaces the selection, so wait
        // for it to land on a single event other than the first. The loop is
        // bounded so that if the selection never changes off the first event,
        // it falls through to the assertion below instead of spinning forever.
        ctx->MouseMoveToPos(b);
        ctx->MouseClick(0);
        ids.clear();
        sel->GetSelectedEvents(ids);
        for (int i = 0; i < 60 && (ids.size() != 1 || ids.front() == id_a); i++)
        {
            ctx->Yield(2);
            ids.clear();
            sel->GetSelectedEvents(ids);
        }
        IM_CHECK(ids.size() == 1);
        if (ids.size() != 1) return;
        const uint64_t id_b = ids.front();

        // Clicking two different stacked bars must select two different events.
        IM_CHECK(id_a != id_b);

        // Leave a clean selection for following tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);
        ids.clear();
        sel->GetSelectedEvents(ids);
        IM_CHECK(ids.empty());
    };

    t = IM_REGISTER_TEST(e, "app", "sys_shared_db_open_dedups_and_switches");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        namespace fs = std::filesystem;

        AppWindow* app = AppWindow::GetInstance();
        IM_CHECK(app != nullptr);
        if (app == nullptr) return;

        // Construct before opening anything so the guard captures the startup
        // tab set as the state to restore.
        TabStateGuard tab_guard(ctx, AppWindowTestPeer{*app}.TabContainerPtr());

        // Sample dbs resolve relative to the working directory (the repo root).
        // Skip if either is missing.
        auto resolve_sample = [](const char* rel) -> std::string {
            fs::path p(rel);
            if (!fs::exists(p)) return std::string();
            return fs::weakly_canonical(p).string();
        };
        const std::string db_a = resolve_sample("sample/rocpd-transpose.db");
        const std::string db_b = resolve_sample("sample/rocprof_compute_23ed6f36.db");
        if (db_a.empty() || db_b.empty())
        {
            ctx->LogWarning("SKIP: sample dbs (rocpd-transpose.db / rocprof_compute_23ed6f36.db) not found");
            return;
        }

        // A project's id is its source-db path, so a .db and a .rpv that point
        // at that same .db share one id.
        const std::string id_a = db_a;
        const std::string id_b = db_b;

        // Open DB_A; afterward its project must exist.
        app->OpenFile(db_a);
        ctx->Yield(3);
        IM_CHECK(app->GetProject(id_a) != nullptr);

        // Open DB_B as a second, active tab so the later switch back to DB_A is
        // actually observable.
        app->OpenFile(db_b);
        ctx->Yield(3);
        IM_CHECK(app->GetProject(id_b) != nullptr);
        IM_CHECK(app->GetCurrentProject() != nullptr);
        if (app->GetCurrentProject() == nullptr) return;
        IM_CHECK(app->GetCurrentProject()->GetID() == id_b);

        // Write a temp .rpv pointing at DB_A by absolute path, so it resolves
        // back to DB_A's id no matter where the .rpv lives. Escape the path so
        // the JSON stays valid.
        const fs::path rpv_path = fs::temp_directory_path() / "rocprofvis_shared_db_dedup.rpv";
        std::string escaped;
        for (char c : db_a)
        {
            if (c == '\\' || c == '"') escaped.push_back('\\');
            escaped.push_back(c);
        }
        {
            std::ofstream out(rpv_path);
            IM_CHECK(out.is_open());
            if (!out.is_open()) return;
            out << "{\"general\": {\"version\": \"1.0\", \"trace_path\": \""
                << escaped << "\"}}";
        }

        app->OpenFile(rpv_path.string());
        ctx->Yield(3);

        // Opening the .rpv must switch back to the existing DB_A tab instead of
        // opening a duplicate.
        IM_CHECK(app->GetCurrentProject() != nullptr);
        if (app->GetCurrentProject() == nullptr) return;
        IM_CHECK(app->GetCurrentProject()->GetID() == id_a);

        // No project should be keyed at the .rpv path itself.
        IM_CHECK(app->GetProject(rpv_path.string()) == nullptr);

        // Remove the temp .rpv and dismiss the dedup popup so it can't cover
        // later tests. tab_guard restores the tab set on scope exit.
        std::error_code ec;
        fs::remove(rpv_path, ec);
        ctx->PopupCloseAll();
        ctx->Yield(2);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_measurement_clear_button");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        MeasurementController* mc = TraceViewTestPeer{*tv}.MeasurementControllerPtr();
        IM_CHECK(mc != nullptr);
        if (mc == nullptr) return;

        // Two distinct timestamps inside the visible range to form a measurement.
        const ViewCoords coords = tlv->GetViewCoords();
        const double     span   = coords.v_max_x - coords.v_min_x;
        IM_CHECK(span > 0.0);
        if (span <= 0.0) return;
        const double t0 = coords.v_min_x + span * 0.25;
        const double t1 = coords.v_min_x + span * 0.75;

        // Baseline: measurement state persists on the TraceView across tests in the
        // reused process. Reset to inactive with no points; this also guarantees the
        // "Measure" entry button is the one rendered (Exit/Clear render only in mode).
        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        IM_CHECK(mc->IsMeasurementMode() == false);

        ctx->SetRef("Main Window");

        // Enter measurement mode with a real click on the toolbar "Measure" button
        // (PushID("measure_start") + Button("Measure")).
        ctx->ItemClick("**/measure_start/Measure");
        ctx->Yield(2);
        IM_CHECK(mc->IsMeasurementMode() == true);
        if (mc->IsMeasurementMode() == false) return;

        // Place two points via the same controller call the freehand click handler
        // drives; headless bar clicks don't reach the flame track's deferred-click
        // measurement path reliably. The button under test (Clear) is a real click.
        mc->SetFreehandMeasurementPoint(t0);
        mc->SetFreehandMeasurementPoint(t1);
        ctx->Yield(2);
        const MeasurementState placed_state = mc->GetMeasurementState();

        // Clear with a real click on the toolbar "Clear" button (renders only once a
        // point exists). ClearMeasurement keeps mode active but drops both points.
        ctx->ItemClick("**/Clear");
        ctx->Yield(2);

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, so leaving
        // measurement mode active would leak into later tests.
        const MeasurementState cleared_state = mc->GetMeasurementState();
        const bool no_points = !mc->GetPoint(0).valid && !mc->GetPoint(1).valid;

        mc->ExitMeasurementMode();
        mc->ClearMeasurement();
        ctx->Yield(2);
        const bool inactive_after = (mc->IsMeasurementMode() == false);

        IM_CHECK(placed_state == MeasurementState::kComplete);
        IM_CHECK(cleared_state == MeasurementState::kWaitingForFirst);
        IM_CHECK(no_points);
        IM_CHECK(inactive_after);
    };

    // AIPROFVIS-117: deselecting one of several selected tracks left the Event
    // Table showing the old row set.
    t = IM_REGISTER_TEST(e, "app", "sys_event_table_updates_on_track_deselect");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        DataProvider* dp = tv->GetDataProvider();
        IM_CHECK(dp != nullptr);
        if (dp == nullptr) return;

        AppWindowSettings& settings = SettingsManager::GetInstance().GetAppWindowSettings();
        const bool prev_details_panel = settings.show_details_panel;
        settings.show_details_panel   = true;
        tv->SetAnalysisViewVisibility(true);

        // Selection is driven by clicking sidebar rows, so the panel has to stay up.
        const bool prev_sidebar = settings.show_sidebar;
        settings.show_sidebar   = true;
        tv->SetSidebarViewVisibility(true);

        // A prior test may have left a time range. The table fetch is bounded by it,
        // so clear it for the measurement and put it back in restore().
        double     prev_range_start = 0.0;
        double     prev_range_end   = 0.0;
        const bool had_range = sel->GetSelectedTimeRange(prev_range_start, prev_range_end);

        // Track selection dispatches through EventManager on a later frame, and the
        // table refetch it triggers is async, so every drive is followed by this.
        auto drain = [&]() {
            ctx->Yield(3);
            for (int polls = 0; polls < 120 &&
                 dp->IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID); polls++)
                ctx->Yield(2);
            ctx->Yield(5);
        };
        auto row_count = [&]() -> uint64_t {
            return dp->DataModel().GetTables().GetTableTotalRowCount(TableType::kEventTable);
        };
        auto restore = [&]() {
            sel->UnselectAllTracks();
            if (had_range) sel->SelectTimeRange(prev_range_start, prev_range_end);
            else           sel->ClearTimeRange();
            ctx->Yield(3);
            settings.show_details_panel = prev_details_panel;
            tv->SetAnalysisViewVisibility(prev_details_panel);
            settings.show_sidebar = prev_sidebar;
            tv->SetSidebarViewVisibility(prev_sidebar);
            ctx->Yield(2);
        };

        // Tracks appear once the timeline's data fetch drains.
        std::vector<FlameTrackItem*> flames;
        for (int i = 0; i < 60 && flames.empty(); i++)
        {
            flames = TimelineViewTestPeer{*tlv}.DisplayedFlameTracks();
            if (flames.empty()) ctx->Yield(2);
        }
        if (flames.empty())
        {
            restore();
            ctx->LogWarning("SKIP: no displayed flame track to select");
            return;
        }

        sel->UnselectAllTracks();
        sel->ClearTimeRange();
        drain();

        // Every select and deselect below is a click on the track's sidebar row,
        // whose button runs ToggleSelectTrack, so the same click selects an
        // unselected track and deselects a selected one.
        ImGuiWindow* sidebar = FindSidebarWindow(ctx);
        if (sidebar == nullptr) restore();
        IM_CHECK(sidebar != nullptr);
        ctx->SetRef(sidebar);
        ImGuiTestItemList sidebar_items;
        ctx->GatherItems(&sidebar_items, "");

        // Resolve every candidate's row up front. If none resolves, the sidebar
        // could not be driven at all, a broken click path rather than a thin trace,
        // so fail here instead of falling through to the data-shortage SKIP below.
        std::vector<ImGuiID> buttons(flames.size(), 0);
        size_t               resolved = 0;
        for (size_t i = 0; i < flames.size(); i++)
        {
            buttons[i] = TrackButtonId(sidebar_items, flames[i]->GetName());
            if (buttons[i] != 0) resolved++;
        }
        if (resolved == 0) restore();
        IM_CHECK(resolved > 0);

        // Only tracks the Event Table actually draws rows from can produce a
        // deselect delta. The table unions per-track row sets that are disjoint by
        // construction, so dropping a track whose own contribution is non-empty must
        // change the total. Measuring each candidate alone is what rules out a false
        // pass from picking a B that contributes nothing (a non-event track, or an
        // event track with no events in range), where count_A == count_AB and the
        // assertion below would hold even with the bug present.
        FlameTrackItem* track_a  = nullptr;
        FlameTrackItem* track_b  = nullptr;
        ImGuiID         button_a = 0;
        ImGuiID         button_b = 0;
        const size_t    kMaxCandidates = 12;
        for (size_t i = 0; i < flames.size() && i < kMaxCandidates && track_b == nullptr; i++)
        {
            if (buttons[i] == 0) continue;
            ctx->ItemClick(buttons[i]);
            drain();
            // Two same-named tracks resolve to one row, so a click lands on only one
            // of them. The selection model says which, and only then is the count
            // that track's own total.
            const uint64_t rows = sel->IsTrackSelected(*flames[i]) ? row_count() : 0;
            ctx->ItemClick(buttons[i]);  // same button toggles the track back off
            drain();
            if (rows == 0) continue;
            if (track_a == nullptr) { track_a = flames[i]; button_a = buttons[i]; }
            else                    { track_b = flames[i]; button_b = buttons[i]; }
        }
        if (track_b == nullptr)
        {
            restore();
            ctx->LogWarning("SKIP: need two populated tracks to observe a deselect delta");
            return;
        }

        sel->UnselectAllTracks();
        drain();
        ctx->ItemClick(button_a);
        drain();
        ctx->ItemClick(button_b);
        drain();
        const uint64_t count_ab = row_count();

        ctx->ItemClick(button_b);  // toggles B back off, leaving only A selected
        drain();
        const uint64_t count_a = row_count();

        // Capture, restore, THEN assert: IM_CHECK early-returns on failure, so a
        // leaked selection or open panel would follow into later tests.
        restore();

        IM_CHECK(count_ab > 0);
        IM_CHECK(count_a > 0);
        IM_CHECK(count_a != count_ab);
    };

    // Advanced Details "Aggregate": picking a group-by column and clicking Submit
    // re-runs the event-table query with a GROUP BY, collapsing raw rows into
    // grouped ones, so the total row count changes.
    t = IM_REGISTER_TEST(e, "app", "sys_event_table_aggregate_changes_rows");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;
        DataProvider* dp = tv->GetDataProvider();
        IM_CHECK(dp != nullptr);
        if (dp == nullptr) return;

        // The Event Table and its Aggregate controls only register with the Test
        // Engine while the Advanced Details panel renders, so force it open.
        const bool details_visible =
            SettingsManager::GetInstance().GetAppWindowSettings().show_details_panel;
        tv->SetAnalysisViewVisibility(true);

        // The table is filled by clicking sidebar rows, so that panel has to stay up.
        const bool sidebar_visible =
            SettingsManager::GetInstance().GetAppWindowSettings().show_sidebar;
        tv->SetSidebarViewVisibility(true);
        ctx->Yield(3);

        // Every drive re-queries the table asynchronously. Wait for the request to
        // be issued, drain it, then let the model settle before reading a count.
        auto drain_event_table = [&]()
        {
            ctx->Yield(3);
            for (int i = 0; i < 200 &&
                 dp->IsRequestPending(DataProvider::EVENT_TABLE_REQUEST_ID); i++)
                ctx->Yield(2);
            ctx->Yield(5);
        };
        auto restore = [&]()
        {
            sel->UnselectAllTracks();
            tv->SetAnalysisViewVisibility(details_visible);
            tv->SetSidebarViewVisibility(sidebar_visible);
            ctx->Yield(3);
        };

        // Only event ("flame") tracks feed the Event Table. Select them all so it
        // holds raw rows to aggregate.
        std::vector<FlameTrackItem*> flames;
        for (int i = 0; i < 60; i++)
        {
            flames = TimelineViewTestPeer{*tlv}.DisplayedFlameTracks();
            if (!flames.empty()) break;
            ctx->Yield(2);
        }
        if (flames.empty())
        {
            ctx->LogWarning("SKIP: no displayed flame track to fill the event table");
            restore();
            return;
        }
        sel->UnselectAllTracks();
        drain_event_table();

        // A missing sidebar is a broken click path, not a thin trace, so fail rather
        // than SKIP (the flame-track shortage above is the legitimate data SKIP).
        ImGuiWindow* sidebar = FindSidebarWindow(ctx);
        if (sidebar == nullptr) restore();
        IM_CHECK(sidebar != nullptr);
        ctx->SetRef(sidebar);
        ImGuiTestItemList sidebar_items;
        ctx->GatherItems(&sidebar_items, "");

        // Fill the table by clicking each flame track's sidebar row. Same-named rows
        // share one button id and the button toggles, so click the distinct ids once
        // each. A duplicate-named pair fills from one of the two, which still holds
        // raw rows to aggregate.
        std::vector<ImGuiID> track_buttons;
        for (FlameTrackItem* flame : flames)
        {
            const ImGuiID id = TrackButtonId(sidebar_items, flame->GetName());
            if (id == 0) continue;
            bool queued = false;
            for (ImGuiID q : track_buttons) queued = queued || (q == id);
            if (!queued) track_buttons.push_back(id);
        }
        if (track_buttons.empty())
        {
            ctx->LogWarning("SKIP: no flame track row button found in the sidebar");
            restore();
            return;
        }
        for (ImGuiID id : track_buttons) ctx->ItemClick(id);
        drain_event_table();

        const TablesModel& tables = dp->DataModel().GetTables();
        const uint64_t rows_before = tables.GetTableTotalRowCount(TableType::kEventTable);
        if (rows_before == 0)
        {
            ctx->LogWarning("SKIP: event table is empty, nothing to aggregate");
            restore();
            return;
        }

        ctx->SetRef("Main Window");
        // ImGui::Combo() never reports its label to the Test Engine, so the
        // "**/##group_by" wildcard cannot resolve it. The combo and the Submit
        // button are added under the same table id stack, so hash the combo's
        // label over Submit's parent id to reach it.
        const ImGuiTestItemInfo submit =
            ctx->ItemInfo("**/Submit", ImGuiTestOpFlags_NoError);
        const ImGuiID group_by_id = ImHashStr("##group_by", 0, submit.ParentID);
        if (submit.ID == 0 || !ctx->ItemExists(group_by_id))
        {
            ctx->LogWarning("SKIP: Aggregate controls are not registered with the "
                            "Test Engine");
            restore();
            return;
        }

        // The options are Selectables in the combo popup, which DO carry labels.
        // "-- None --" is the ungrouped state we restore to; any other entry is a
        // groupable column, so take the first one.
        const char* none_label = "-- None --";
        ctx->ItemClick(group_by_id);
        ctx->Yield(3);
        ImGuiTestItemList options;
        ctx->GatherItems(&options, "//$FOCUSED");
        ImGuiID none_id   = 0;
        ImGuiID column_id = 0;
        for (int i = 0; i < options.GetSize(); i++)
        {
            if (strcmp(options[i]->DebugLabel, none_label) == 0)
                none_id = options[i]->ID;
            else if (column_id == 0)
                column_id = options[i]->ID;
        }
        if (none_id == 0 || column_id == 0)
        {
            ctx->LogWarning("SKIP: group-by combo popup offers no column to group on");
            ctx->PopupCloseAll();
            restore();
            return;
        }

        ctx->ItemClick(column_id);
        ctx->Yield(3);
        ctx->ItemClick("**/Submit");
        drain_event_table();

        const uint64_t rows_after = tables.GetTableTotalRowCount(TableType::kEventTable);

        // Restore BEFORE asserting: IM_CHECK early-returns on failure, and a live
        // group-by would leak into every later event-table test.
        ctx->ItemClick(group_by_id);
        ctx->Yield(3);
        ctx->ItemClick(none_id);
        ctx->Yield(3);
        ctx->ItemClick("**/Submit");
        drain_event_table();
        restore();

        IM_CHECK(rows_after != rows_before);
    };

    t = IM_REGISTER_TEST(e, "app", "sys_event_search_multi_substring");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        EventSearch* es = TraceViewTestPeer{*tv}.EventSearchPtr();
        IM_CHECK(es != nullptr);
        if (es == nullptr) return;

        // Clear so the searched flag starts from a known baseline (the harness
        // reuses one process interactively).
        es->Clear();
        ctx->Yield(2);
        IM_CHECK(es->Searched() == false);

        // Quote-delimited segments make EventSearch::Search split the input into
        // several terms instead of one, which is the multi-term parse this test
        // covers. "hip" and "Launch" are both substrings of hipLaunchKernel (proven
        // searchable in this db by sys_event_search_finds_results), so the query is
        // non-empty even though the default options AND-combine the terms
        // (m_partial_matching defaults to false).
        // Type into the real search field. RenderEventSearch runs the search on the
        // frame the focused field sees Enter, so Enter is what issues the query.
        ctx->SetRef("Main Window");
        ctx->ItemInput("**/search_bar/##input_text_with_clear");
        ctx->KeyCharsReplaceEnter("\"hip\"\"Launch\"");
        ctx->Yield(2);
        IM_CHECK(es->Searched() == true);

        // The fetch is deferred. Let it drain (Update re-runs Search when the
        // request completes) before reading the result count.
        for (int i = 0; i < 60 && EventSearchTestPeer{*es}.RequestPending(); i++) ctx->Yield(2);
        ctx->Yield(5);
        IM_CHECK(EventSearchTestPeer{*es}.ResultCount() > 0);

        // The X button is the clear path. IconButton pushes the glyph as an id and
        // draws it as the button, so the ref ends in the glyph twice.
        const std::string clear_ref =
            std::string("**/search_bar/") + ICON_X_CIRCLED + "/" + ICON_X_CIRCLED;
        ctx->ItemClick(clear_ref.c_str());
        ctx->Yield(2);
    };

    // AIPROFVIS-297: opening a .rpv whose referenced trace is gone must fail with a
    // message naming the missing trace, and must not create a file at that path.
    // AppWindow::OpenFile discards the Project it built on a failed open, so the test
    // owns the Project and calls Open() directly to read the error off it.
    t = IM_REGISTER_TEST(e, "app", "sys_project_missing_source_db_error");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        namespace fs = std::filesystem;

        // Open() reports the failure through the app's message dialog.
        AppWindow* app = AppWindow::GetInstance();
        IM_CHECK(app != nullptr);
        if (app == nullptr) return;

        std::error_code ec;
        const fs::path missing_db = fs::temp_directory_path() / "rocprofvis_missing_source_zzq.db";
        fs::remove(missing_db, ec);
        IM_CHECK(!fs::exists(missing_db));

        // Write a temp .rpv referencing the missing db by absolute path, escaped so
        // the JSON stays valid.
        const fs::path rpv_path = fs::temp_directory_path() / "rocprofvis_missing_source.rpv";
        std::string escaped;
        for (char c : missing_db.string())
        {
            if (c == '\\' || c == '"') escaped.push_back('\\');
            escaped.push_back(c);
        }
        {
            std::ofstream out(rpv_path);
            IM_CHECK(out.is_open());
            if (!out.is_open()) return;
            out << "{\"general\": {\"version\": \"1.0\", \"trace_path\": \""
                << escaped << "\"}}";
        }

        Project     proj;
        std::string path   = rpv_path.string();
        const Project::OpenResult result = proj.Open(path);
        const std::string message = ProjectTestPeer{proj}.OpenErrorMessage();

        // OpenProject resolves the trace through weakly_canonical, so match the
        // message against the same form rather than the raw path.
        const std::string expected_path = fs::weakly_canonical(missing_db).string();

        // The guard that replaced the old open attempt: no empty db is left behind.
        const bool still_missing = !fs::exists(missing_db) && !fs::exists(expected_path);

        fs::remove(rpv_path, ec);

        // The failed Open queued an error dialog, which does not actually open until
        // the next Render. Yield so it opens, then close it, otherwise it leaks into
        // later tests and blocks their input.
        ctx->Yield(2);
        ctx->PopupCloseAll();
        ctx->Yield(2);

        IM_CHECK(result == Project::OpenResult::Failed);
        IM_CHECK(message.find(expected_path) != std::string::npos);
        IM_CHECK(still_missing);
    };

    // AIPROFVIS-81: Event Details dropped the argument list for HIP API events.
    // Presence/shape only -- the args come back asynchronously through the
    // controller, with no single table to build a value oracle from.
    t = IM_REGISTER_TEST(e, "app", "sys_event_details_shows_hip_args");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        TraceView* tv = GetTraceViewOrSkip(ctx);
        if (!tv) return;
        AnalysisView* av = TraceViewTestPeer{*tv}.AnalysisViewPtr();
        IM_CHECK(av != nullptr);
        if (av == nullptr) return;
        EventsView* ev = AnalysisViewTestPeer{*av}.EventsViewPtr();
        IM_CHECK(ev != nullptr);
        if (ev == nullptr) return;
        TimelineView* tlv = TraceViewTestPeer{*tv}.TimelineViewPtr();
        IM_CHECK(tlv != nullptr);
        if (tlv == nullptr) return;
        std::shared_ptr<TimelineSelection> sel = tv->GetTimelineSelection();
        IM_CHECK(sel != nullptr);
        if (sel == nullptr) return;

        // A stray modal swallows hover for the windows beneath it, which would make
        // the canvas click a no-op. Close any open popup before clicking the timeline.
        ctx->PopupCloseAll();
        ctx->Yield(2);

        // A prior test may have left an event selected. The clear is dispatched
        // through EventManager, so yield before asserting the empty baseline.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(3);
        IM_CHECK(EventsViewTestPeer{*ev}.EventItemCount() == 0);

        // Only HIP API events carry the call's argument list, so restrict the
        // search to Launch-type tracks. Chart items populate after the track's
        // data fetch drains, so poll for one that has events.
        FlameTrackItem* flame = nullptr;
        for (int i = 0; i < 60 && flame == nullptr; i++)
        {
            for (FlameTrackItem* candidate :
                 TimelineViewTestPeer{*tlv}.DisplayedFlameTracks())
            {
                const TrackInfo* info = candidate->GetTrackInfo();
                if (info == nullptr ||
                    info->operation_types.count(kRocProfVisDmOperationLaunch) == 0)
                    continue;
                if (FlameTrackItemTestPeer{*candidate}.ChartItemCount() > 0)
                {
                    flame = candidate;
                    break;
                }
            }
            if (flame == nullptr) ctx->Yield(2);
        }
        if (flame == nullptr)
        {
            ctx->LogWarning("SKIP: no HIP-API event with args to inspect in this trace");
            return;
        }

        // Gather bars from the HIP track's own FV window, not the first track's, so
        // the clicked bar belongs to the Launch-type track asserted on above. The
        // window id is 0 until that track has rendered, so poll for it.
        ImVec2 event_center(0.0f, 0.0f);
        bool   have_center = false;
        for (int i = 0; i < 60 && !have_center; i++)
        {
            ctx->Yield(2);
            have_center = FirstEventScreenCenter(
                ctx, FlameTrackItemTestPeer{*flame}.FlameWindowId(), event_center);
        }
        IM_CHECK(have_center);
        if (!have_center) return;

        // Selection is deferred a frame, so move/release with the mouse parked.
        ctx->MouseMoveToPos(event_center);
        ctx->Yield(2);
        ctx->MouseDown(0);
        ctx->Yield(1);
        ctx->MouseUp(0);
        ctx->Yield(3);

        // The click lands on the widest bar, so there is no pre-chosen event. Assert
        // the selected event carries named args, not that a specific event was
        // selected. The event details, and with them the args, arrive
        // asynchronously. New items are emplace_front'ed, but scan every cached item
        // rather than relying on that ordering.
        size_t arg_item  = 0;
        size_t arg_count = 0;
        for (int i = 0; i < 60 && arg_count == 0; i++)
        {
            ctx->Yield(2);
            EventsViewTestPeer peer{*ev};
            for (size_t idx = 0; idx < peer.EventItemCount(); idx++)
            {
                if (peer.ArgCount(idx) > 0)
                {
                    arg_item  = idx;
                    arg_count = peer.ArgCount(idx);
                    break;
                }
            }
        }
        bool have_named_arg = false;
        for (size_t a = 0; a < arg_count; a++)
        {
            if (!EventsViewTestPeer{*ev}.ArgName(arg_item, a).empty())
            {
                have_named_arg = true;
                break;
            }
        }

        // Restore before asserting: IM_CHECK early-returns on failure, which
        // would otherwise leak a selected event into later tests.
        TraceViewTestPeer{*tv}.ClearEventSelection();
        ctx->Yield(2);

        IM_CHECK(arg_count > 0);
        IM_CHECK(have_named_arg);
    };
}
