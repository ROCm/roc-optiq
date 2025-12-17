// =====================================================================
//                    !!! W A R N I N G !!!
// =====================================================================
// 
// This file should never be a part of the PUBLIC API, this contains "internal" persistent
// states of widgets and context management for nested widgets and popups. 
// In case of single-header version of this library, the contents here should go to an
// "internal" or anonymous namespace inside the "glimmer" namespace
// 
// =====================================================================
//                    WHAT IS THIS FILE???
// =====================================================================
//
// There are two kinds of data here:
// 1. *PersistentData structs which store data that has to persist across "frames"
// 2. *Builder structs, which store data necessary to create composite widgets
//
// *Builder structs are not persistent data, and hence created, used and destroyed every frame.
// As *PersistentData structs have to be persisted, WidgetContextData struct takes care of it.
// Such data is stored at a "per-widget" level, as the data is heterogenous in nature.
// 
// Layout stacks are also maintained by the same context data. As computing widget geometry inside
// layouts involve the knowledge of all descendants inside the layout, the actual geometry and hence
// the event handling is deferred until `EndLayout` is called. To capture defered event handling
// instead of capturing lambda's as `std::function`, which is both heavy-weight and incurs virtual
// function call, only the data required to handle events is recorded. It is replayed once the 
// geometry of widgets are computed and rendered.
// 
// It also maintains a style stack, which is shared across all contexts i.e. independent of
// stacked contexts. Style data is maintained per "predefined widget state". Refer to WidgetState
// enum in types.h to see the supported states. 
//
// TODO: Add the ability for custom widget states (required for robust cusotm widget support)

#pragma once

#include "types.h"
#include "style.h"

#include <bit>

#ifndef GLIMMER_MAX_SPLITTER_REGIONS
#define GLIMMER_MAX_SPLITTER_REGIONS 4
#endif

#ifndef GLIMMER_MAX_STYLE_STACKSZ
#define GLIMMER_MAX_STYLE_STACKSZ 16
#endif

#ifndef GLIMMER_MAX_WIDGET_SPECIFIC_STYLES
#define GLIMMER_MAX_WIDGET_SPECIFIC_STYLES 4
#endif

#ifndef GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL
#define GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL 4
#endif

#ifndef GLIMMER_MAX_LAYOUT_NESTING 
#define GLIMMER_MAX_LAYOUT_NESTING 8
#endif

#ifndef GLIMMER_MAX_REGION_NESTING 
#define GLIMMER_MAX_REGION_NESTING 8
#endif

namespace glimmer
{
    extern UIConfig Config;

    struct AnimationData
    {
        int32_t elements = 0;
        int32_t types = 0;
        ImGuiDir direction = ImGuiDir::ImGuiDir_Right;
        float offset = 0.f;
        float timestamp = 0;

        void moveByPixel(float amount, float max, float reset);
    };

    constexpr int AnimationsPreallocSz = 32;
    constexpr int ShadowsPreallocSz = 64;
    constexpr int FontStylePreallocSz = 64;
    constexpr int BorderPreallocSz = 64;
    constexpr int GradientPreallocSz = 64;

    inline int log2(auto i) { return i <= 0 ? 0 : 8 * sizeof(i) - std::countl_zero(i) - 1; }

    using StyleStackT = DynamicStack<StyleDescriptor, int16_t, GLIMMER_MAX_STYLE_STACKSZ>;
	using RegionStackT = DynamicStack<int32_t, int16_t, GLIMMER_MAX_REGION_NESTING>;

    struct RendererEventIndexRange
    {
        std::pair<int, int> primitives{ -1, -1 };
        std::pair<int, int> events{ -1, -1 };
    };

    struct WidgetIdClasses
    {
        std::string_view id;
        Vector<std::string_view, int16_t, 8> classes;
    };

#pragma region Widget Specific Persistent-States/Builders

    struct RegionBuilder
    {
        int32_t id = -1;
        ImVec2 origin;
        ImVec2 size;
        int32_t depth = 0;
		Layout layout = Layout::Invalid;
		StyleDescriptor style;
		bool fixedWidth = false;
		bool fixedHeight = false;
    };

    struct ItemGridStyleDescriptor
    {
        uint32_t gridcolor = IM_COL32(100, 100, 100, 255);
    };

    enum class ItemGridCurrentState
    {
        Default, ResizingColumns, ReorderingColumns
    };

    struct ItemGridPersistentState
    {
        struct HeaderCellResizeState
        {
            ImVec2 lastPos; // Last mouse position while dragging
            float modified = 0.f; // Records already modified column width
            bool mouseDown = false; // If mouse is down on column boundary
        };

        struct HeaderCellDragState
        {
            ItemGridConfig::ColumnConfig config;
            ImVec2 lastPos;
            ImVec2 startPos;
            int16_t column = -1;
            int16_t level = -1;
            int16_t potentialColumn = -1;
            bool mouseDown = false;
        };

        struct BiDirMap
        {
            Vector<int16_t, int16_t> ltov{ 128, -1 };
            Vector<int16_t, int16_t> vtol{ 128, -1 };
        };

        Vector<HeaderCellResizeState, int16_t, 32> cols[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL];
        Vector<int32_t, int16_t, 32> headerStates[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL];
        BiDirMap colmap[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL];
        HeaderCellDragState drag;
        ScrollableRegion scroll;
        ScrollableRegion altscroll;
        ImVec2 totalsz;
        ItemGridCurrentState state = ItemGridCurrentState::Default;

        int16_t sortedCol = -1;
        int16_t sortedLevel = -1;
        bool sortedAscending = false;

        struct ItemId 
        {
            int32_t row = -1;
            int32_t col = -1;
            int32_t depth = -1;
        };

        Vector<ItemId, int16_t, 32> selections{ false };
        float lastSelection = -1.f;
        float currentSelection = -1.f;
        
        struct 
        {
            int32_t row = -1;
            int16_t col = -1;
            int16_t depth = 0;
            int32_t state = WS_Default;
        } cellstate;

        template <typename ContainerT>
        void swapColumns(int16_t from, int16_t to, Span<ContainerT> headers, int level)
        {
            auto lfrom = colmap[level].vtol[from];
            auto lto = colmap[level].vtol[to];
            colmap[level].vtol[from] = lto; colmap[level].ltov[lfrom] = to;
            colmap[level].vtol[to] = lfrom; colmap[level].ltov[lto] = from;

            std::pair<int16_t, int16_t> movingColRangeFrom = { lfrom, lfrom }, nextMovingRangeFrom = { INT16_MAX, -1 };
            std::pair<int16_t, int16_t> movingColRangeTo = { lto, lto }, nextMovingRangeTo = { INT16_MAX, -1 };

            for (auto l = level + 1; l < (int)headers.size(); ++l)
            {
                for (int16_t col = 0; col < (int16_t)headers[l].size(); ++col)
                {
                    auto& hdr = headers[l][col];
                    if (hdr.parent >= movingColRangeFrom.first && hdr.parent <= movingColRangeFrom.second)
                    {
                        nextMovingRangeFrom.first = std::min(nextMovingRangeFrom.first, col);
                        nextMovingRangeFrom.second = std::max(nextMovingRangeFrom.second, col);
                    }
                    else if (hdr.parent >= movingColRangeTo.first && hdr.parent <= movingColRangeTo.second)
                    {
                        nextMovingRangeTo.first = std::min(nextMovingRangeTo.first, col);
                        nextMovingRangeTo.second = std::max(nextMovingRangeTo.second, col);
                    }
                }

                auto startTo = colmap[l].ltov[nextMovingRangeFrom.first];
                auto startFrom = colmap[l].ltov[nextMovingRangeTo.first];

                for (auto col = nextMovingRangeTo.first, idx = startTo; col <= nextMovingRangeTo.second; ++col, ++idx)
                {
                    colmap[l].ltov[col] = idx;
                    colmap[l].vtol[idx] = col;
                }

                for (auto col = nextMovingRangeFrom.first, idx = startFrom; col <= nextMovingRangeFrom.second; ++col, ++idx)
                {
                    colmap[l].ltov[col] = idx;
                    colmap[l].vtol[idx] = col;
                }

                movingColRangeFrom = nextMovingRangeFrom;
                movingColRangeTo = nextMovingRangeTo;
                nextMovingRangeFrom = nextMovingRangeTo = { INT16_MAX, -1 };
            }
        }
    };

    struct ColumnProps : public ItemGridConfig::ColumnConfig
    {
        ImVec2 offset;
        RendererEventIndexRange range;
        RendererEventIndexRange sortIndicatorRange;
        int32_t alignment = TextAlignCenter;
        uint32_t bgcolor = 0;
        uint32_t fgcolor = 0;
        bool selected = false;
        bool highlighted = false;
    };

    enum class ItemGridConstructPhase
    {
        None, Headers, HeaderCells, HeaderPlacement, Rows, Columns
    };

    struct WidgetContextData;

    struct ItemGridBuilder
    {
        int32_t id = -1;
        ImVec2 origin;
        ImVec2 size;
        WidgetContextData* context = nullptr;
        int32_t geometry = 0; 
        int16_t levels = 0;
        int16_t currlevel = 0;
        int16_t selectedCol = -1;
        int16_t depth = 0;
        std::pair<int16_t, int16_t> movingCols{ -1, -1 };
        ImVec2 nextpos;
        ImVec2 maxHeaderExtent;
        ImVec2 maxCellExtent;
        ImVec2 totalsz;
        float cellIndent = 0.f;
        float headerHeight = 0.f;
        float maxColWidth = 0.f;
        float btnsz = 0.f;
        int32_t rowcount = 0;
        int16_t resizecol = -1;
        NeighborWidgets neighbors;
        ItemGridConstructPhase phase = ItemGridConstructPhase::None;
        Vector<ColumnProps, int16_t, 32> headers[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL + 1] = {
            Vector<ColumnProps, int16_t, 32>{ true },
            Vector<ColumnProps, int16_t, 32>{ false },
            Vector<ColumnProps, int16_t, 32>{ false },
            Vector<ColumnProps, int16_t, 32>{ false },
            Vector<ColumnProps, int16_t, 32>{ false },
        };
        Vector<int32_t, int16_t> perDepthRowCount{ false };

        struct RowYToIndexMapping
        {
            float from, to = 0.f;
            int32_t depth = 0; 
            int32_t row = 0;
        };
        float currentY = 0.f, startY = 0.f;
        Vector<RowYToIndexMapping, int32_t> rowYs{ false };
        ItemGridPersistentState::ItemId clickedItem;

        std::pair<ItemDescendentVisualState, int16_t> childState;
        float headerHeights[GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL] = { 0.f, 0.f, 0.f, 0.f };
        int32_t currRow = 0, currCol = 0;
        WidgetDrawResult event;
        ItemGridPopulateMethod method = ItemGridPopulateMethod::ByRows;
        bool contentInteracted = false;
        bool addedBounds = false;

        ColumnProps& currentHeader() { return headers[currlevel][currCol]; }
        void reset();
    };

    struct ToggleButtonPersistentState
    {
        float btnpos = -1.f;
        float progress = 0.f;
        bool animate = false;
    };

    struct RadioButtonPersistentState
    {
        float radius = -1.f;
        float progress = 0.f;
        bool animate = false;
    };

    struct CheckboxPersistentState
    {
        float progress = -1.f;
        bool animate = false;
    };

    enum class TextOpType { Addition, Deletion, Replacement };

    struct TextInputOperation
    {
        std::pair<int32_t, int32_t> range{ -1, -1 };
        int32_t caretpos = 0;
        char opmem[128] = { 0 };
        TextOpType type;
    };

    struct InputTextPersistentState
    {
        int caretpos = 0;
        int32_t clearState = WS_Default;
        bool caretVisible = true;
        bool isSelecting = false;
        float lastCaretShowTime = 0.f;
        float selectionStart = -1.f;
        float lastClickTime = -1.f;
        ScrollableRegion scroll;
        Vector<float, int16_t> pixelpos; // Cumulative pixel position of characters
        UndoRedoStack<TextInputOperation> ops; // Text operations for redo/undo stack
        TextInputOperation currops;

        void moveLeft(float amount)
        {
            scroll.state.pos.x = std::max(0.f, scroll.state.pos.x - amount);
        }

        void moveRight(float amount)
        {
            scroll.state.pos.x = std::min(scroll.state.pos.x + amount, pixelpos.back());
        }
    };

    struct AdHocLayoutState
    {
        ImVec2 nextpos{ 0.f, 0.f }; // position of next widget
        int32_t lastItemId = -1; // last inserted item's ID
        bool insideContainer = false;
        bool addedOffset = false;
    };

    struct SplitterContainerState
    {
        ImRect extent;
        int32_t id;
        Direction dir = DIR_Vertical;
        bool isScrollable = false;
    };

    struct SplitterPersistentState
    {
        struct SplitRange
        {
            float min, max;
            float curr = -1.f;
        };

        int current = 0;
        SplitRange spacing[GLIMMER_MAX_SPLITTER_REGIONS]; // spacing from (i-1)th to ith splitter
        int32_t states[GLIMMER_MAX_SPLITTER_REGIONS]; // ith splitter's state
        int32_t containers[GLIMMER_MAX_SPLITTER_REGIONS]; // id of ith container
        ImRect viewport[GLIMMER_MAX_SPLITTER_REGIONS]; // ith non-scroll region geometry
        bool isdragged[GLIMMER_MAX_SPLITTER_REGIONS]; // ith drag state
        float dragstart[GLIMMER_MAX_SPLITTER_REGIONS]; // ith drag state

        SplitterPersistentState();
    };

    struct SpinnerPersistentState
    {
        float lastChangeTime = 0.f;
        float repeatRate = 0.f;
    };

    struct TabItemDescriptor
    {
        std::string_view name, tooltip, icon;
        TextType nameType = TextType::PlainText;
        int32_t iconType = ResourceType::RT_SVG;
        int32_t itemflags = 0;
        ImVec2 iconsz{};
    };

    struct TabBarBuilder
    {
        int32_t id;
        int32_t geometry;
        TabBarItemSizing sizing = TabBarItemSizing::ResizeToFit;
        NeighborWidgets neighbors;
        Vector<TabItemDescriptor, int16_t, 16> items{ false };
        std::string_view expand = "Expand";
        TextType expandType = TextType::PlainText;
        bool newTabButton = false;

        TabBarBuilder() {}

        void reset();
    };

    constexpr int16_t NewTabIndex = -1;
    constexpr int16_t DropDownTabIndex = -2;
    constexpr int16_t ExpandTabsIndex = -3;
    constexpr int16_t InvalidTabIndex = -4;
    constexpr int16_t MoveBackwardIndex = -4;
    constexpr int16_t MoveForwardIndex = -4;

    struct TabBarPersistentState
    {
        struct ItemDescriptor
        {
            int16_t state = 0;
            int16_t pos = -1;
            ImRect extent, close, pin, text, icon;
            float tabHoverDuration = 0.f, pinHoverDuration = 0.f, closeHoverDuration = 0.f;
            TabItemDescriptor descriptor;
            bool pinned = false;
        };

        int16_t current = InvalidTabIndex;
        int16_t hovered = InvalidTabIndex;
        Vector<ItemDescriptor, int16_t, 16> tabs;
        std::string_view expandContent = "Expand";
        TextType expandType = TextType::PlainText;
        ImRect create;
        ImRect dropdown;
        ImRect expand;
        ImRect moveForward, moveBackward;
        float createHoverDuration = 0.f;
		float lastRowStarty = 0.f;
        int32_t tabBeingDragged = -1;
        ImVec2 dragPosition{};
        ImVec2 dragStart{};
        ScrollableRegion scroll;
        bool expanded = false;
    };

    struct AccordionBuilder
    {
        struct RegionDescriptor
        {
            RendererEventIndexRange hrange;
            RendererEventIndexRange crange;
            ImVec2 header;
            ImVec2 content;
        };

        int32_t id = 0;
        int32_t geometry = 0;
        ImVec2 origin;
        ImVec2 size, totalsz;
        ImRect content;
        ImVec2 textsz, extent;
        float headerHeight = 0.f;
        int16_t totalRegions = 0;
        std::string_view icon[2];
        std::string_view text;
        WidgetDrawResult event;
        Vector<RegionDescriptor, int16_t, 8> regions;
        FourSidedBorder border;
        FourSidedMeasure spacing;
        uint32_t bgcolor;
        TextType textType = TextType::PlainText;
        bool resflags[2] = { RT_SYMBOL, RT_SYMBOL };
        bool hscroll = false, vscroll = false;

        AccordionBuilder() {}
        void reset();
    };

    struct AccordionPersistentState
    {
        int16_t opened = -1;
        Vector<ScrollableRegion, int16_t, 8> scrolls;
        Vector<int32_t, int16_t, 8> hstates;
    };

#pragma endregion

#pragma region Layout Types

    struct GridLayoutItem
    {
        ImVec2 maxdim;
        ImRect bbox;
        int16_t row = -1, col = -1;
        int16_t rowspan = 1, colspan = 1;
        int32_t alignment = TextAlignLeading;
        int16_t index = -1;
    };

    struct LayoutItemDescriptor
    {
        WidgetType wtype = WidgetType::WT_Invalid;
        int32_t id = -1;
        int32_t scrollid = -1;
        int16_t layoutIdx = -1;
        ImRect margin, border, padding, content, text;
        ImRect prefix, suffix;
        ImVec2 relative;
        ImVec2 extent;
        int32_t sizing = 0;
        int16_t row = 0, col = 0;
        int16_t from = -1, to = -1;
        void* implData = nullptr;
    };

    enum class LayoutOps 
    { 
        PushStyle, PopStyle, SetStyle, IgnoreStyleStack, RestoreStyleStack, 
		PushTextType, PopTextType,
		PushRegion, PopRegion,
		PushScrollRegion, PopScrollRegion,
        AddWidget, AddLayout 
    };

    struct LayoutBuilder
    {
        Layout type = Layout::Invalid;
        int32_t id = 0;
        int32_t fill = FD_None;
        int32_t alignment = TextAlignLeading;
        int16_t from = -1, to = -1, itemidx = -1;
        int16_t styleStartIdx[WSI_Total];
        int16_t currow = -1, currcol = -1;
        ImRect geometry{ { FIT_SZ, FIT_SZ }, { FIT_SZ, FIT_SZ } };
        ImRect available; // Available space in the direction of layout in current window
        ImVec2 nextpos{ 0.f, 0.f }, prevpos{ 0.f, 0.f }, startpos{};
        ImVec2 spacing{ 0.f, 0.f };
        ImVec2 maxdim{ 0.f, 0.f }; // max dimension of widget in curren row/col
        ImVec2 cumulative{ 0.f, 0.f }, size{};
        ImRect extent{}; // max coords of widgets inside layout
        Vector<ImVec2, int16_t> rows{ false };
        Vector<ImVec2, int16_t> cols{ false };
        Vector<int16_t, int16_t> griditems{ false };
        std::pair<int, int> gridsz;
        std::pair<int16_t, int16_t> currspan{ 1, 1 };
        ItemGridPopulateMethod gpmethod = ItemGridPopulateMethod::ByRows;
        OverflowMode hofmode = OverflowMode::Scroll;
        OverflowMode vofmode = OverflowMode::Scroll;
        ScrollableRegion scroll;
        int32_t regionIdx = -1;
		void* implData = nullptr;
        bool popSizingOnEnd = false;

        Vector<std::pair<int32_t, LayoutOps>, int16_t> itemIndexes{ false };
        FixedSizeStack<int32_t, 16> containerStack;
        TabBarBuilder tabbar;

        LayoutBuilder()
        {
            for (auto idx = 0; idx < WSI_Total; ++idx) styleStartIdx[idx] = -1;
        }

        void reset();
    };

    void AddItemToLayout(LayoutBuilder& layout, LayoutItemDescriptor& item, const StyleDescriptor& style);

    // Determine extent of layout/splitter/other containers
    void AddExtent(LayoutItemDescriptor& wdesc, const StyleDescriptor& style, const NeighborWidgets& neighbors,
        float width = 0.f, float height = 0.f);

#pragma endregion

#pragma region Defered Handling

    struct EventDeferInfo
    {
        WidgetType type; 
        int32_t id;

        union ParamsT {
            struct {
                ImRect margin, border, padding, content;
            } region;

            struct {
                ImRect margin, border, padding, content, text;
            } label;

            struct {
                ImRect margin, border, padding, content, text;
            } button;

            struct {
                ImRect extent;
                float maxrad = 0.f;
            } radio;

            struct {
                ImRect extent;
                ImVec2 center;
            } toggle;

            struct {
                ImRect extent;
            } checkbox;

            struct {
                ImRect extent;
                ImRect thumb;
            } slider;

            struct {
                ImRect extent;
                ImRect minThumb, maxThumb;
            } rangeslider;

            struct {
                ImRect content;
                ImRect clear;
            } input;

            struct {
                ImRect margin, border, padding, content;
            } dropdown;

            struct {
                ImRect extent;
                ImRect incbtn, decbtn;
            } spinner;

            struct {
                ImRect content;
            } tabbar;

            struct {
                ImRect region;
                int ridx = 0;
            } accordion;

            struct {
                ImRect padding, content;
            } media;

            ParamsT() {}
        } params;

        EventDeferInfo() : type{ WT_Invalid }, id{ -1 } {}

        static EventDeferInfo ForRegion(int32_t id, const ImRect& margin, const ImRect& border,
			const ImRect& padding, const ImRect& content);
        static EventDeferInfo ForLabel(int32_t id, const ImRect& margin, const ImRect& border, 
            const ImRect& padding, const ImRect& content, const ImRect& text);
        static EventDeferInfo ForButton(int32_t id, const ImRect& margin, const ImRect& border,
            const ImRect& padding, const ImRect& content, const ImRect& text);
        static EventDeferInfo ForCheckbox(int32_t id, const ImRect& extent);
        static EventDeferInfo ForRadioButton(int32_t id, const ImRect& extent, float maxrad);
        static EventDeferInfo ForToggleButton(int32_t id, const ImRect& extent, ImVec2 center);
        static EventDeferInfo ForSpinner(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn);
        static EventDeferInfo ForSlider(int32_t id, const ImRect& extent, const ImRect& thumb);
        static EventDeferInfo ForRangeSlider(int32_t id, const ImRect& extent, const ImRect& minthumb, const ImRect& maxthumb);
        static EventDeferInfo ForTextInput(int32_t id, const ImRect& extent, const ImRect& clear);
        static EventDeferInfo ForDropDown(int32_t id, const ImRect& margin, const ImRect& border,
            const ImRect& padding, const ImRect& content);
        static EventDeferInfo ForTabBar(int32_t id, const ImRect& content);
        static EventDeferInfo ForAccordion(int32_t id, const ImRect& region, int32_t ridx);
        static EventDeferInfo ForScrollRegion(int32_t id);
        static EventDeferInfo ForMediaResource(int32_t id, const ImRect& padding, const ImRect& content);
    };

    enum class NestedContextSourceType
    {
        None, Region, Layout, ItemGrid, // add others...
    };

    struct NestedContextSource
    {
        WidgetContextData* base = nullptr;
        NestedContextSourceType source = NestedContextSourceType::None;
    };

#pragma endregion

    struct ContextMenuItemParams
    {
        std::string_view text, prefix;
        TextType type;
        ResourceType rt;
        CheckState* check = nullptr;
        StyleDescriptor style;
        uint32_t color = 0;
        SymbolIcon icon = SymbolIcon::None;
        float thickness = 0.f;
    };

    struct ContextMenuItemDescriptor
    {
        int32_t state = WS_Default;
        int32_t prefixId = -1;
        ImRect content, textrect, prefix;
    };

#pragma region Widget Context Data

    constexpr int32_t WidgetIndexMask = 0xffff;
    constexpr int32_t WidgetTypeBits = 16;

    // Captures widget states, is stored as a linked-list, each context representing
    // a window or overlay, this enables serialized Id's for nested overlays as well
    struct WidgetContextData
    {
        // This is quasi-persistent
        std::vector<WidgetConfigData> states[WT_TotalTypes];
        std::vector<ItemGridPersistentState> gridStates;
        std::vector<ToggleButtonPersistentState> toggleStates;
        std::vector<RadioButtonPersistentState> radioStates;
        std::vector<CheckboxPersistentState> checkboxStates;
        std::vector<InputTextPersistentState> inputTextStates;
        std::vector<SplitterPersistentState> splitterStates;
        std::vector<SpinnerPersistentState> spinnerStates;
        std::vector<TabBarPersistentState> tabBarStates;
        std::vector<AccordionPersistentState> accordionStates;
        std::vector<int32_t> splitterScrollPaneParentIds;
        std::vector<std::vector<std::pair<int32_t, int32_t>>> dropDownOptions;
        
        // Regions stack
        DynamicStack<int32_t, int16_t, GLIMMER_MAX_REGION_NESTING> regionBuilders{ false };
        Vector<RegionBuilder, int16_t, GLIMMER_MAX_REGION_NESTING> regions{ false };

        // Tab bars are not nested
        TabBarBuilder currentTab;

        // Stack of current item grids
        DynamicStack<ItemGridBuilder, int16_t, 4> itemGrids{ false };
        DynamicStack<NestedContextSource, int16_t, 16> nestedContextStack{ false };
        static WidgetContextData* CurrentItemGridContext;

        std::vector<WidgetContextData*> nestedContexts[WT_TotalNestedContexts];
        WidgetContextData* parentContext = nullptr;

        // Styling data is static as it is persisted across contexts
        static StyleStackT StyleStack[WSI_Total];

        // Per widget specific style objects
        static DynamicStack<ToggleButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> toggleButtonStyles[WSI_Total];
        static DynamicStack<RadioButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> radioButtonStyles[WSI_Total];
        static DynamicStack<SliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> sliderStyles[WSI_Total];
        static DynamicStack<RangeSliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> rangeSliderStyles[WSI_Total];
        static DynamicStack<SpinnerStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> spinnerStyles[WSI_Total];
        static DynamicStack<TabBarStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> tabBarStyles[WSI_Total];

        // Resolved styles, after applying widget, class(es) and id specific styles
        Vector<StyleDescriptor[WSI_Total], int16_t, 32> WidgetStyles[WT_TotalTypes];

        // This has to persistent
        std::vector<AnimationData> animations{ AnimationsPreallocSz, AnimationData{} };

        // Layout related members
        Vector<LayoutItemDescriptor, int16_t> layoutItems{ 128 };
        Vector<ImRect, int16_t> itemGeometries[WT_TotalTypes]{
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ false },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ false },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true },
            Vector<ImRect, int16_t>{ true }
        };
        Vector<ImVec2, int16_t> itemSizes[WT_TotalTypes]{
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ false },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ false },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true },
            Vector<ImVec2, int16_t>{ true }
        };
        DynamicStack<int32_t, int16_t> containerStack{ 16 };
        FixedSizeStack<SplitterContainerState, 16> splitterStack;
        FixedSizeStack<int32_t, GLIMMER_MAX_LAYOUT_NESTING> layoutStack;
        Vector<LayoutBuilder, int16_t> layouts;

        FixedSizeStack<AccordionBuilder, 4> accordions;
        FixedSizeStack<Sizing, GLIMMER_MAX_LAYOUT_NESTING> sizing;
        FixedSizeStack<int32_t, GLIMMER_MAX_LAYOUT_NESTING> spans;
        DynamicStack<AdHocLayoutState, int16_t, 4> adhocLayout;
        Vector<std::pair<int64_t, LayoutOps>, int16_t> replayContent;
        StyleStackT layoutStyles[WSI_Total]{ false, false, false, false,
            false, false, false, false, false };

        // Keep track of widget IDs
        int maxids[WT_TotalTypes];
        int tempids[WT_TotalTypes];
        int32_t lastLayoutIdx = -1;

        // Whether we are in a frame being rendered + current renderer
        bool InsideFrame = false;
        bool usingDeferred = false;
        bool deferEvents = false;
        Vector<EventDeferInfo, int16_t> deferedEvents;
        IRenderer* deferedRenderer = nullptr;

        ImVec2 popupOrigin{ -1.f, -1.f }, popupSize{ -1.f, -1.f };
        RendererEventIndexRange popupRange;
        PopUpCallbackT popupCallbacks[PRP_Total] = { nullptr, nullptr, nullptr };
        void* popupCallbackData[PRP_Total] = { nullptr, nullptr, nullptr };

        static ImRect ActivePopUpRegion;
        static int32_t PopupTarget;
        static UIElementDescriptor RightClickContext;
        static Vector<ContextMenuItemDescriptor, int16_t, 16> ContextMenuOptions;
        static Vector<ContextMenuItemParams, int16_t, 16> ContextMenuOptionParams;
        static int32_t CurrentWidgetId;

        int32_t GetNextCount(WidgetType type);

        WidgetConfigData& GetState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);
            return states[wtype][index];
        }

        WidgetConfigData const& GetState(int32_t id) const
        {
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);
            return states[wtype][index];
        }

        ItemGridPersistentState& GridState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return gridStates[index];
        }

        ToggleButtonPersistentState& ToggleState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return toggleStates[index];
        }

        RadioButtonPersistentState& RadioState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return radioStates[index];
        }

        CheckboxPersistentState& CheckboxState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return checkboxStates[index];
        }

        InputTextPersistentState& InputTextState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return inputTextStates[index];
        }

        SplitterPersistentState& SplitterState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return splitterStates[index];
        }

        SpinnerPersistentState& SpinnerState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return spinnerStates[index];
        }

        TabBarPersistentState& TabBarState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return tabBarStates[index];
        }

        AccordionPersistentState& AccordionState(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            return accordionStates[index];
        }

        const AccordionPersistentState& AccordionState(int32_t id) const
        {
            auto index = id & WidgetIndexMask;
            return accordionStates[index];
        }

        ScrollableRegion& ScrollRegion(int32_t id)
        {
            auto index = id & WidgetIndexMask;
            auto type = id >> WidgetTypeBits;
            return states[type][index].state.scroll;
        }

        ScrollableRegion const& ScrollRegion(int32_t id) const
        {
            auto index = id & WidgetIndexMask;
            auto type = id >> WidgetTypeBits;
            return states[type][index].state.scroll;
        }

        static StyleDescriptor GetStyle(int32_t state);
        static void IgnoreStyleStack(int32_t wtypes);
        static void RestoreStyleStack();
        static void RemovePopup();
        static int GetExpectedWidgetCount(WidgetType type);

        IRenderer& ToggleDeferedRendering(bool defer, bool reset = true);
        IRenderer& GetRenderer();
        void PushContainer(int32_t parentId, int32_t id);
        void PopContainer(int32_t id);
        void AddItemGeometry(int id, const ImRect& geometry, bool ignoreParent = false);
        void AddItemSize(int id, ImVec2 sz);
        WidgetDrawResult HandleEvents(ImVec2 origin, int from = 0, int to = -1);

        void RegisterWidgetIdClass(WidgetType wt, int32_t index, const WidgetIdClasses& idClasses);
        StyleDescriptor GetStyle(int32_t state, int32_t id);
        
        void RecordForReplay(int64_t data, LayoutOps ops);
        void ResetLayoutData();
        void ClearDeferredData();

        const ImRect& GetGeometry(int32_t id) const;
        ImVec2 GetSize(int32_t id) const;
        ImRect GetLayoutSize() const;
        void RecordDeferRange(RendererEventIndexRange& range, bool start) const;
        ImVec2 MaximumSize() const;
        ImVec2 MaximumExtent() const;
        ImVec2 WindowSize() const;
        ImVec2 NextAdHocPos() const;

        WidgetContextData();
    };

    void AddFontPtr(FontStyle& font);
    void InitFrameData();
    void ResetFrameData();
    WidgetContextData& GetContext();
    WidgetContextData& PushContext(int32_t id);
    void PopContext();
    void Cleanup();

    StyleDescriptor GetStyle(WidgetContextData& context, int32_t id, StyleStackT* StyleStack, int32_t state);

    extern NestedContextSource InvalidSource;

#pragma endregion
}
