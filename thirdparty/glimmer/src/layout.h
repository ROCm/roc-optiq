#pragma once

#include "types.h"

namespace glimmer
{
    struct StyleDescriptor;

    // Ad-hoc Layout
    void PushSpan(int32_t direction); // Combination of FillDirection
    void SetSpan(int32_t direction); // Combination of FillDirection
    //void PushDir(int32_t direction); // Combination of WidgetGeometry
    //void SetDir(int32_t direction); // Combination of WidgetGeometry
    void PopSpan(int depth = 1);

    void Move(int32_t direction); // Combination of FillDirection
    void Move(int32_t id, int32_t direction); // Combination of FillDirection
    void Move(int32_t hid, int32_t vid, bool toRight, bool toBottom); // Combination of WidgetGeometry flags
    void Move(ImVec2 amount, int32_t direction); // Combination of WidgetGeometry
    void Move(ImVec2 pos);
    void AddSpacing(ImVec2 spacing);

    // Structured Layout inside container
    ImRect BeginFlexLayout(Direction dir, int32_t geometry, bool wrap = false,
        ImVec2 spacing = { 0.f, 0.f }, ImVec2 size = { 0.f, 0.f }, const NeighborWidgets& neighbors = NeighborWidgets{});
    ImRect BeginGridLayout(int rows, int cols, GridLayoutDirection dir, int32_t geometry, const std::initializer_list<float>& rowExtents = {},
        const std::initializer_list<float>& colExtents = {}, ImVec2 spacing = { 0.f, 0.f }, ImVec2 size = { 0.f, 0.f },
        const NeighborWidgets& neighbors = NeighborWidgets{});
    ImRect BeginLayout(std::string_view desc, const NeighborWidgets& neighbors = NeighborWidgets{});
    void NextRow();
    void NextColumn();
    void PushSizing(float width, float height, bool relativew = false, bool relativeh = false);
    void PopSizing(int depth = -1);
    WidgetDrawResult EndLayout(int depth = 1);
}
