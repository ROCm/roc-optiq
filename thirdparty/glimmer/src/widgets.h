#pragma once
#include "types.h"

namespace glimmer
{
    UIConfig& GetUIConfig(bool needRichText);
    int32_t GetNextId(WidgetType type);
    int16_t GetNextCount(WidgetType type);

    WidgetConfigData& GetWidgetConfig(WidgetType type, int16_t id);
    WidgetConfigData& GetWidgetConfig(int32_t id);
    void SetTooltip(int32_t id, std::string_view tooltip);
    void SetPrevTooltip(std::string_view tooltip);
    void SetNextTooltip(std::string_view tooltip);

#if !defined(GLIMMER_DISABLE_SVG) || !defined(GLIMMER_DISABLE_IMAGES)
    WidgetDrawResult Icon(int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Icon(int32_t id, int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Icon(std::string_view id, int32_t rtype, IconSizingType sztype, std::string_view resource, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
#endif
#ifdef GLIMMER_ENABLE_ICON_FONT
    WidgetDrawResult Icon(std::string_view resource, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
#endif
    WidgetDrawResult Icon(int32_t id, SymbolIcon icon, IconSizingType sztype, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Icon(std::string_view id, SymbolIcon icon, IconSizingType sztype, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    void BeginFlexRegion(int32_t id, Direction dir, ImVec2 spacing = { 0.f, 0.f }, bool wrap = true, int32_t events = 0, int32_t geometry = ToBottomRight, const NeighborWidgets & neighbors = NeighborWidgets{});
    void BeginFlexRegion(std::string_view id, Direction dir, ImVec2 spacing = { 0.f, 0.f }, bool wrap = true, int32_t events = 0, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void BeginGridRegion(int32_t id, int rows, int cols, ImVec2 spacing = { 0.f, 0.f }, int32_t events = 0, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void BeginGridRegion(std::string_view id, int rows, int cols, ImVec2 spacing = { 0.f, 0.f }, int32_t events = 0, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult EndRegion();

    WidgetDrawResult Label(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Label(std::string_view id, std::string_view content, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Label(std::string_view id, std::string_view content, std::string_view tooltip, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    
    WidgetDrawResult Button(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Button(std::string_view id, std::string_view content, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void BeginButton(std::string_view id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult EndButton();
    
    WidgetDrawResult ToggleButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ToggleButton(bool* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult ToggleButton(std::string_view id, bool* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult RadioButton(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RadioButton(bool* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RadioButton(std::string_view id, bool* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult Checkbox(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Checkbox(CheckState* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Checkbox(std::string_view id, CheckState* state, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult Spinner(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(int32_t* value, int32_t step, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(std::string_view id, int32_t* value, int32_t step, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(float* value, float step, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(std::string_view id, float* value, float step, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(double* value, float step, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Spinner(std::string_view id, double* value, float step, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult Slider(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(int32_t* value, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(std::string_view id, int32_t* value, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(float* value, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(std::string_view id, float* value, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(double* value, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult Slider(std::string_view id, double* value, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult RangeSlider(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(int32_t* min_val, int32_t* max_val, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(std::string_view id, int32_t* min_val, int32_t* max_val, std::pair<int32_t, int32_t> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(float* min_val, float* max_val, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(std::string_view id, float* min_val, float* max_val, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(double* min_val, double* max_val, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult RangeSlider(std::string_view id, double* min_val, double* max_val, std::pair<float, float> range, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult TextInput(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(char* out, int size, std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(std::string_view id, char* out, int size, std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(char* out, int size, int strlen, std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult TextInput(std::string_view id, char* out, int size, int strlen, std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    template <size_t sz>
    WidgetDrawResult TextInput(char (&out)[sz], std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{})
    {
        return TextInput(out, sz, placeholder, geometry, neighbors);
    }
    template <size_t sz>
    WidgetDrawResult TextInput(std::string_view id, char(&out)[sz], std::string_view placeholder, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{})
    {
        return TextInput(id, out, placeholder, geometry, neighbors);
    }

    WidgetDrawResult DropDown(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(int32_t* selection, std::string_view text, bool(*options)(int32_t), int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(std::string_view id, int32_t* selection, std::string_view text, bool(*options)(int32_t), int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(int32_t* selection, std::string_view text, const std::initializer_list<std::string_view>& options, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult DropDown(std::string_view id, int32_t* selection, std::string_view text, const std::initializer_list<std::string_view>& options, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    WidgetDrawResult StaticItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    WidgetDrawResult StaticItemGrid(std::string_view id, const std::initializer_list<std::string_view>& headers, std::pair<std::string_view, TextType>(*cell)(int32_t, int16_t), 
        int32_t totalRows, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});

    void StartSplitRegion(int32_t id, Direction dir, const std::initializer_list<SplitRegion>& splits,
        int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void NextSplitRegion();
    void EndSplitRegion();

    bool StartPopUp(int32_t id, ImVec2 origin, ImVec2 size = { FLT_MAX, FLT_MAX });
    void SetPopUpCallback(PopUpRenderPhase phase, PopUpCallbackT callback, void* data = nullptr);
    WidgetDrawResult EndPopUp();

    void StartScrollableRegion(int32_t id, int32_t flags, int32_t geometry = ToBottomRight, 
        const NeighborWidgets& neighbors = NeighborWidgets{}, ImVec2 maxsz = { FLT_MAX, FLT_MAX });
    ImRect EndScrollableRegion();

    bool StartContextMenu(ImVec2 fixedsz = { FLT_MAX, FLT_MAX });
    UIElementDescriptor GetContextMenuContext();
    void AddContextMenuEntry(std::string_view text, TextType type = TextType::PlainText, std::string_view prefix = "", ResourceType rt = RT_INVALID);
    void AddContextMenuEntry(SymbolIcon icon, std::string_view text, TextType type = TextType::PlainText);
    void AddContextMenuEntry(CheckState* state, std::string_view text, TextType type = TextType::PlainText);
    void AddContextMenuSeparator(uint32_t color, float thickness = 1.f);
    WidgetDrawResult EndContextMenu();

    bool StartTabBar(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    void AddTab(std::string_view name, std::string_view tooltip = "", int32_t flags = 0);
    void AddTab(int32_t resflags, std::string_view icon, TextType extype, std::string_view text, int32_t flags = 0, ImVec2 iconsz = {});
    WidgetDrawResult EndTabBar(std::optional<bool> canAddTab = std::nullopt);

    bool StartAccordion(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    bool StartAccordionHeader();
    void AddAccordionHeaderExpandedIcon(int32_t resflags, std::string_view res);
    void AddAccordionHeaderCollapsedIcon(int32_t resflags, std::string_view res);
    void AddAccordionHeaderText(std::string_view content, TextType textType = TextType::PlainText);
    void EndAccordionHeader(std::optional<bool> expanded = std::nullopt);
    bool StartAccordionContent(float height = FLT_MAX, int32_t scrollflags = 0, ImVec2 maxsz = { FLT_MAX, FLT_MAX });
    void EndAccordionContent();
    WidgetDrawResult EndAccordion();

    bool StartItemGrid(int32_t id, int32_t geometry = ToBottomRight, const NeighborWidgets& neighbors = NeighborWidgets{});
    bool StartItemGridHeader(int levels = 1);
    void AddHeaderColumn(const ItemGridConfig::ColumnConfig& config);
    void CategorizeColumns();
    void AddColumnCategory(const ItemGridConfig::ColumnConfig& config, int16_t from, int16_t to);
    WidgetDrawResult EndItemGridHeader();
    void PopulateItemGrid(int totalRows, ItemGridPopulateMethod method = ItemGridPopulateMethod::ByRows);
    WidgetDrawResult EndItemGrid();

#ifndef GLIMMER_DISABLE_PLOTS
    bool StartPlot(std::string_view id, ImVec2 size = { FLT_MAX, FLT_MAX }, int32_t flags = 0);
    WidgetDrawResult EndPlot();
#endif
}
