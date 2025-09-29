// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_annotations.h"
namespace RocProfVis
{
namespace View
{
class AnnotationView : public RocWidget
{
public:
    AnnotationView(std::shared_ptr<AnnotationsManager> annotation_manager);
    ~AnnotationView();

    // Call this to render the annotation view using ImGui
    void Render() override;

private:
    std::shared_ptr<AnnotationsManager> m_annotations;
    int m_selected_note_id;
};
}  // namespace View
}  // namespace RocProfVis