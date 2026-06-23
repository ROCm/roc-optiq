// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_project.h"
#include "rocprofvis_stickynote.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr double INVALID_TIME_NS = std::numeric_limits<double>::lowest();

class AnnotationsManager;

class AnnotationsManagerProjectSettings : public ProjectSetting
{
public:
    AnnotationsManagerProjectSettings(const std::string&  project_id,
                                      AnnotationsManager& annotations_view);
    ~AnnotationsManagerProjectSettings() override;

    void ToJson() override;
    void FromJson();
    bool Valid() const override;

private:
    AnnotationsManager& m_annotations_manager;
};

class AnnotationsManager
{
public:
    AnnotationsManager(const std::string& project_id);

    bool IsVisibile();
    void SetVisible(bool visible);
    void Clear();
    void AddSticky(double time_ns, float y_offset, const ImVec2& size,
                   const std::string& text, const std::string& title, double v_min,
                   double v_max, uint64_t track_id = INVALID_TRACK_ID,
                   bool is_minimized = true);

    void                     RemoveNotesPendingDelete();
    std::vector<StickyNote>& GetStickyNotes();

    // Drops a new note at the location, expanded and focused for inline typing.
    void CreateStickyNote(double time_ns, float y_offset, double v_min, double v_max,
                          ImVec2 graph_size, uint64_t track_id = INVALID_TRACK_ID);

private:
    std::vector<StickyNote>           m_sticky_notes;
    bool                              m_show_annotations;
    AnnotationsManagerProjectSettings m_project_settings;
};

}  // namespace View
}  // namespace RocProfVis
