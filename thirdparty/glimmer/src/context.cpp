#include "context.h"
#include "im_font_manager.h"
#include "renderer.h"
#include "style.h"
#include "libs/inc/implot/implot.h"
#include <list>

#ifndef GLIMMER_MAX_OVERLAYS
#define GLIMMER_MAX_OVERLAYS 32
#endif

#ifndef GLIMMER_GLOBAL_ANIMATION_FRAMETIME
#define GLIMMER_GLOBAL_ANIMATION_FRAMETIME 18
#endif

#include "draw.h"

namespace glimmer
{
    static std::list<WidgetContextData> WidgetContexts;
    static WidgetContextData* CurrentContext = nullptr;
    static ImPlotContext* ChartsContext = nullptr;
    static bool StartedRendering = false;
    static bool HasImPlotContext = false;
    static bool RemovePopupAtFrameExit = false;
    static int32_t IgnoreStyleStackBits = -1;

    void CopyStyle(const StyleDescriptor& src, StyleDescriptor& dest);
    void HandleRegionEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result);
    void HandleLabelEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result);
    void HandleButtonEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text, IRenderer& renderer, const IODescriptor& io, WidgetDrawResult& result);
    void HandleRadioButtonEvent(int32_t id, const ImRect& extent, float maxrad, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleToggleButtonEvent(int32_t id, const ImRect& extent, ImVec2 center, IRenderer& renderer, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleCheckboxEvent(int32_t id, const ImRect& extent, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleSliderEvent(int32_t id, const ImRect& extent, const ImRect& thumb, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleRangeSliderEvent(int32_t id, const ImRect& extent, const ImRect& minthumb, const ImRect& maxthumb, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleTextInputEvent(int32_t id, const ImRect& content, const ImRect& clear, const IODescriptor& io,
        IRenderer& renderer, WidgetDrawResult& result);
    void HandleDropDownEvent(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);
    void HandleSpinnerEvent(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn, const IODescriptor& io,
        WidgetDrawResult& result);
    void HandleTabBarEvent(int32_t id, const ImRect& content, const IODescriptor& io, IRenderer& renderer, WidgetDrawResult& result);
    void HandleAccordionEvent(int32_t id, const ImRect& region, int ridx, const IODescriptor& io, WidgetDrawResult& result);
    void HandleMediaResourceEvent(int32_t id, const ImRect& padding, const ImRect& content, const IODescriptor& io, WidgetDrawResult& result);

    void AnimationData::moveByPixel(float amount, float max, float reset)
    {
        timestamp += Config.platform->desc.deltaTime;

        if (timestamp >= GLIMMER_GLOBAL_ANIMATION_FRAMETIME)
        {
            offset += amount;
            if (offset >= max) offset = reset;
            timestamp = 0.f;
        }
    }

    void LayoutBuilder::reset()
    {
        type = Layout::Invalid;
        id = 0;
        fill = FD_None;
        alignment = TextAlignLeading;
        from = -1, to = -1, itemidx = -1;
        currow = -1, currcol = -1;
        geometry = ImRect{ { FIT_SZ, FIT_SZ },{ FIT_SZ, FIT_SZ } };
        available = ImRect{ };
        nextpos = ImVec2{ 0.f, 0.f };
        prevpos = ImVec2{ 0.f, 0.f };
        spacing = ImVec2{ 0.f, 0.f };
        maxdim = ImVec2{ 0.f, 0.f };
        startpos = ImVec2{ 0.f, 0.f };
        cumulative = ImVec2{ 0.f, 0.f };
        size = ImVec2{ 0.f, 0.f };
        hofmode = OverflowMode::Scroll;
        vofmode = OverflowMode::Scroll;
        scroll = ScrollableRegion{};
        tabbar.reset();
        popSizingOnEnd = false;
        currspan.first = currspan.second = 1;

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            styleStartIdx[idx] = -1;
        }

        itemIndexes.clear(true);
        griditems.clear(true);
        containerStack.clear(true);
        rows.clear(true);
        cols.clear(true);
    }

    void TabBarBuilder::reset()
    {
        id = -1;
        geometry = 0;
        sizing = TabBarItemSizing::ResizeToFit;
        neighbors = NeighborWidgets{};
        items.clear(true);
        newTabButton = false;
    }

    void ItemGridBuilder::reset()
    {
        id = -1;
        origin = size = nextpos = maxHeaderExtent = maxCellExtent = totalsz = ImVec2{};
        geometry = 0;
        levels = currlevel = depth = 0;
        selectedCol = -1;
        movingCols = std::make_pair<int16_t, int16_t>(-1, -1);
        phase = ItemGridConstructPhase::None;
        perDepthRowCount.clear(true);
        rowYs.clear(true);
        clickedItem.row = clickedItem.col = clickedItem.depth = -1;
        resizecol = -1;

        for (auto idx = 0; idx < 5; ++idx)
        {
            for (auto hidx = 0; hidx < headers[idx].size(); ++hidx)
            {
                headers[idx][hidx].content = headers[idx][hidx].extent = ImRect{};
                headers[idx][hidx].range = RendererEventIndexRange{};
                headers[idx][hidx].alignment = TextAlignCenter;
                headers[idx][hidx].offset = ImVec2{};
                headers[idx][hidx].parent = -1;
                headers[idx][hidx].props = 0;
            }

            headers[idx].clear(false);
        }

        headerHeights[0] = headerHeights[1] = headerHeights[2] = headerHeights[3] = headerHeight = 0.f;
        currRow = currCol = 0;
        event = WidgetDrawResult{};
        contentInteracted = addedBounds = false;
    }

    void AccordionBuilder::reset()
    {
        id = geometry = 0;
        origin = size = totalsz = textsz = extent = ImVec2{};
        content = ImRect{};
        headerHeight = 0.f;
        totalRegions = 0;
        text = icon[0] = icon[1] = "";
        textType = TextType::PlainText;
        event = WidgetDrawResult{};
        regions.clear(true);
        border = FourSidedBorder{};
        bgcolor = 0;
        resflags[0] = resflags[1] = RT_INVALID;
        hscroll = vscroll = false;
    }

    ImVec2 WidgetContextData::NextAdHocPos() const
    {
        const auto& layout = adhocLayout.top();
        auto offset = ImVec2{};

        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto type = (WidgetType)(id >> WidgetTypeBits);

            if (type == WT_Accordion)
            {
                auto& accordion = accordions.top();
                const auto& state = AccordionState(accordion.id);
                offset = state.scrolls[accordion.totalRegions].state.pos;
                return state.scrolls[accordion.totalRegions].viewport.Min + offset;
            }
            else if (type == WT_Scrollable && !layout.addedOffset)
            {
                offset = states[type][index].state.scroll.state.pos;
            }
        }

        return layout.nextpos - offset;
    }

    void AddFontPtr(FontStyle& font)
    {
        if (font.font == nullptr && StartedRendering)
        {
            auto isbold = font.flags & FontStyleBold;
            auto isitalics = font.flags & FontStyleItalics;
            auto islight = font.flags & FontStyleLight;
            auto ft = isbold && isitalics ? FT_BoldItalics : isbold ? FT_Bold : isitalics ? FT_Italics : islight ? FT_Light : FT_Normal;
            font.font = GetFont(font.family, font.size, ft);
        }
    }

    void InitFrameData()
    {
        StartedRendering = true;
        RemovePopupAtFrameExit = false;

        if (!HasImPlotContext)
        {
            HasImPlotContext = true;
            ChartsContext = ImPlot::CreateContext();
            auto& style = ImPlot::GetStyle();
            style.PlotPadding = { 0.f, 0.f };
        }

        for (auto it = WidgetContexts.rbegin(); it != WidgetContexts.rend(); ++it)
        {
            auto& context = *it;
            context.InsideFrame = true;
            context.adhocLayout.push();
        }

        for (auto idx = 0; idx < WSI_Total; ++idx)
            AddFontPtr(WidgetContextData::StyleStack[idx].top().font);

        if (Config.platform->desc.isRightClicked())
            WidgetContextData::RightClickContext.pos = Config.platform->desc.mousepos;
    }

    void ResetFrameData()
    {
        for (auto& context : WidgetContexts)
        {
            context.InsideFrame = false;
            context.adhocLayout.clear(true);

            for (auto idx = 0; idx < WT_TotalTypes; ++idx)
            {
                context.itemGeometries[idx].reset(ImRect{ {0.f, 0.f}, {0.f, 0.f} });
                context.itemSizes[idx].reset(ImVec2{});
            }
            
            context.ResetLayoutData();
            context.maxids[WT_SplitterRegion] = 0;
            context.maxids[WT_Layout] = 0;
            context.maxids[WT_Charts] = 0;
            context.regions.clear(true);
            context.lastLayoutIdx = -1;

            for (auto& layout : context.layouts)
                layout.reset();
            context.layouts.clear(false);

            assert(context.layoutStack.empty());
        }

        CurrentContext = &(*(WidgetContexts.begin()));
        auto rtpos = WidgetContextData::RightClickContext.pos;
        WidgetContextData::RightClickContext = UIElementDescriptor{};
        WidgetContextData::RightClickContext.pos = rtpos;
        WidgetContextData::ContextMenuOptionParams.clear(true);

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto popsz = WidgetContextData::StyleStack[idx].size() - 1;
            if (popsz > 0) WidgetContextData::StyleStack[idx].pop(popsz, true);
        }

        if (RemovePopupAtFrameExit)
        {
            WidgetContextData::ActivePopUpRegion = ImRect{};
            WidgetContextData::RightClickContext.pos = ImVec2{};
        }
    }

    void WidgetContextData::RecordForReplay(int64_t data, LayoutOps ops)
    {
        replayContent.emplace_back(data, ops);
    }

    int32_t WidgetContextData::GetNextCount(WidgetType type)
    {
        auto count = maxids[type];

        if (count == states[type].size())
        {
            auto sz = Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount(type) :
                WidgetContextData::GetExpectedWidgetCount(type);
            states[type].reserve(states[type].size() + sz);

            auto oldsz = WidgetStyles[type].size();
            WidgetStyles[type].expand_and_create(sz, false);

            for (auto idx = 0; idx < sz; ++idx)
            {
                states[type].emplace_back(WidgetConfigData{ type });
                auto& styles = WidgetStyles[type][idx + oldsz];

                for (auto ws = 0; ws < WSI_Total; ++ws)
                    styles[ws] = StyleDescriptor{};
            }

            switch (type)
            {
            case WT_Checkbox: {
                checkboxStates.reserve(checkboxStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    checkboxStates.emplace_back();
                break;
            }
            case WT_RadioButton: {
                radioStates.reserve(radioStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    radioStates.emplace_back();
                break;
            }
            case WT_TextInput: {
                inputTextStates.reserve(inputTextStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    inputTextStates.emplace_back();
                break;
            }
            case WT_ToggleButton: {
                toggleStates.reserve(toggleStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    toggleStates.emplace_back();
                break;
            }
            case WT_Spinner: {
                spinnerStates.reserve(spinnerStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    spinnerStates.emplace_back();
                break;
            }
            case WT_DropDown: {
                dropDownOptions.reserve(dropDownOptions.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    dropDownOptions.emplace_back();
                break;
            }
            case WT_Accordion: {
                accordionStates.reserve(accordionStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    accordionStates.emplace_back();
                break;
			}
			case WT_TabBar: {
				tabBarStates.reserve(tabBarStates.size() + sz);
				for (auto idx = 0; idx < sz; ++idx)
					tabBarStates.emplace_back();
				break;
			}
			case WT_ItemGrid: {
				gridStates.reserve(gridStates.size() + sz);
				for (auto idx = 0; idx < sz; ++idx)
					gridStates.emplace_back();
				break;
			}
            case WT_Splitter: {
                splitterStates.reserve(splitterStates.size() + sz);
                for (auto idx = 0; idx < sz; ++idx)
                    splitterStates.emplace_back();
                break;
            }
            default: break;
            }
        }

        maxids[type]++;
        return count;
    }

    StyleDescriptor WidgetContextData::GetStyle(int32_t state)
    {
        auto style = log2((unsigned)state);
        auto res = StyleStack[style].top();
        AddFontPtr(res.font);
        if (style != WSI_Default) CopyStyle(StyleStack[WSI_Default].top(), res);
        return res;
    }

    void WidgetContextData::IgnoreStyleStack(int32_t wtypes)
    {
        IgnoreStyleStackBits = IgnoreStyleStackBits == -1 ? wtypes : IgnoreStyleStackBits | wtypes;
    }

    void WidgetContextData::RestoreStyleStack()
    {
        IgnoreStyleStackBits = -1;
    }

    StyleDescriptor WidgetContextData::GetStyle(int32_t state, int32_t id)
    {
        return glimmer::GetStyle(*this, id, StyleStack, state);
    }

    void WidgetContextData::RegisterWidgetIdClass(WidgetType wt, int32_t index, const WidgetIdClasses& idClasses)
    {
        if (!idClasses.classes.empty())
        {
            for (int i = 0; i < idClasses.classes.size(); ++i)
            {
                for (auto idx = 0; idx < WSI_Total; ++idx)
                {
                    auto& style = glimmer::GetStyle(idClasses.classes[i], (WidgetStateIndex)idx);
                    WidgetStyles[wt][index][idx].From(style);
                }
            }
        }

        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto& style = glimmer::GetStyle(idClasses.id, (WidgetStateIndex)idx);
            WidgetStyles[wt][index][idx].From(style);
        }
    }

    void WidgetContextData::RemovePopup()
    {
        RemovePopupAtFrameExit = true;
    }

    IRenderer& WidgetContextData::ToggleDeferedRendering(bool defer, bool reset)
    {
        usingDeferred = defer;

        if (defer)
        {
            auto renderer = CreateDeferredRenderer(&ImGuiMeasureText);
            if (reset) { renderer->Reset(); adhocLayout.push(); }
            deferedRenderer = renderer;
            return *deferedRenderer;
        }
        else if (reset)
        {
            if (adhocLayout.size() > 1) adhocLayout.pop(1, true);
            deferedRenderer->Reset();
            return *Config.renderer;
        }
    }

    IRenderer& WidgetContextData::GetRenderer()
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                return (accordion.geometry & FromRight) || (accordion.geometry & FromBottom) || usingDeferred ?
                    *deferedRenderer : *Config.renderer;
            }
        }

        return usingDeferred ? *deferedRenderer : *Config.renderer;
    }

    void WidgetContextData::PushContainer(int32_t parentId, int32_t id)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (wtype == WT_SplitterRegion)
            splitterScrollPaneParentIds[index] = parentId;

        containerStack.push() = id;
    }

    void WidgetContextData::PopContainer(int32_t id)
    {
        containerStack.pop(1, true);

        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (wtype == WT_SplitterRegion)
        {
            splitterScrollPaneParentIds[index] = -1;
        }
    }

    void WidgetContextData::RecordDeferRange(RendererEventIndexRange& range, bool start) const
    {
        if (start)
        {
            range.events.first = deferedEvents.size();
            range.primitives.first = deferedRenderer->TotalEnqueued();
        }
        else
        {
            range.events.second = deferedEvents.size();
            range.primitives.second = deferedRenderer->TotalEnqueued();
        }
    }
    
    void WidgetContextData::AddItemGeometry(int id, const ImRect& geometry, bool ignoreParent)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (index >= itemGeometries[wtype].size())
        {
            auto sz = std::max((int16_t)128, (int16_t)(index - itemGeometries[wtype].size() + 1));
            itemGeometries[wtype].expand_and_create(sz, true);
        }

        itemGeometries[wtype][index] = geometry;
        adhocLayout.top().lastItemId = id;

        if (!containerStack.empty() && !ignoreParent)
        {
            id = containerStack.top();
            wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_Scrollable)
            {
                index = id & WidgetIndexMask;
                auto& region = states[wtype][index].state.scroll;
                region.content.x = std::max(region.content.x, geometry.Max.x);
                region.content.y = std::max(region.content.y, geometry.Max.y);
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                accordion.extent.x = std::max(accordion.extent.x, geometry.Max.x);
                accordion.extent.y = std::max(accordion.extent.y, geometry.Max.y);
            }
        }

        /*if (currSpanDepth > 0 && spans[currSpanDepth].popWhenUsed)
        {
            spans[currSpanDepth] = ElementSpan{};
            --currSpanDepth;
        }*/
    }

    void WidgetContextData::AddItemSize(int id, ImVec2 sz)
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);

        if (index >= itemSizes[wtype].size())
        {
            auto sz = std::max((int16_t)128, (int16_t)(index - itemSizes[wtype].size() + 1));
            itemSizes[wtype].expand_and_create(sz, true);
        }

        itemSizes[wtype][index] = sz;
    }

    EventDeferInfo EventDeferInfo::ForRegion(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
		info.type = WT_Region;
        info.id = id;
        info.params.region.margin = margin;
        info.params.region.border = border;
        info.params.region.padding = padding;
        info.params.region.content = content;
		return info;
    }

    EventDeferInfo EventDeferInfo::ForLabel(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding,
        const ImRect& content, const ImRect& text)
    {
        EventDeferInfo info;
        info.type = WT_Label;
        info.id = id;
        info.params.label.margin = margin;
        info.params.label.border = border;
        info.params.label.padding = padding;
        info.params.label.content = content;
        info.params.label.text = text;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForButton(int32_t id, const ImRect& margin, const ImRect& border, const ImRect& padding, const ImRect& content, const ImRect& text)
    {
        EventDeferInfo info;
        info.type = WT_Button;
        info.id = id;
        info.params.button.margin = margin;
        info.params.button.border = border;
        info.params.button.padding = padding;
        info.params.button.content = content;
        info.params.button.text = text;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForCheckbox(int32_t id, const ImRect& extent)
    {
        EventDeferInfo info;
        info.type = WT_Checkbox;
        info.id = id;
        info.params.checkbox.extent = extent;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForRadioButton(int32_t id, const ImRect& extent, float maxrad)
    {
        EventDeferInfo info;
        info.type = WT_RadioButton;
        info.id = id;
        info.params.radio.extent = extent;
        info.params.radio.maxrad = maxrad;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForToggleButton(int32_t id, const ImRect& extent, ImVec2 center)
    {
        EventDeferInfo info;
        info.type = WT_ToggleButton;
        info.id = id;
        info.params.toggle.extent = extent;
        info.params.toggle.center = center;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForSpinner(int32_t id, const ImRect& extent, const ImRect& incbtn, const ImRect& decbtn)
    {
        EventDeferInfo info;
        info.type = WT_Spinner;
        info.id = id;
        info.params.spinner.extent = extent;
        info.params.spinner.incbtn = incbtn;
        info.params.spinner.decbtn = decbtn;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForSlider(int32_t id, const ImRect& extent, const ImRect& thumb)
    {
        EventDeferInfo info;
        info.type = WT_Slider;
        info.id = id;
        info.params.slider.extent = extent;
        info.params.slider.thumb = thumb;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForRangeSlider(int32_t id, const ImRect& extent, const ImRect& minthumb, const ImRect& maxthumb)
    {
        EventDeferInfo info;
        info.type = WT_RangeSlider;
        info.id = id;
        info.params.rangeslider.extent = extent;
        info.params.rangeslider.minThumb = minthumb;
        info.params.rangeslider.maxThumb = maxthumb;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForTextInput(int32_t id, const ImRect& extent, const ImRect& clear)
    {
        EventDeferInfo info;
        info.type = WT_TextInput;
        info.id = id;
        info.params.input.content = extent;
        info.params.input.clear = clear;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForDropDown(int32_t id, const ImRect& margin, const ImRect& border,
        const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_DropDown;
        info.id = id;
        info.params.dropdown.margin = margin;
        info.params.dropdown.border = border;
        info.params.dropdown.padding = padding;
        info.params.dropdown.content = content;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForTabBar(int32_t id, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_TabBar;
        info.id = id;
        info.params.tabbar.content = content;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForAccordion(int32_t id, const ImRect& region, int32_t ridx)
    {
        EventDeferInfo info;
        info.type = WT_Accordion;
        info.id = id;
        info.params.accordion.region = region;
        info.params.accordion.ridx = ridx;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForScrollRegion(int32_t id)
    {
        EventDeferInfo info;
        info.type = WT_Scrollable;
        info.id = id;
        return info;
    }

    EventDeferInfo EventDeferInfo::ForMediaResource(int32_t id, const ImRect& padding, const ImRect& content)
    {
        EventDeferInfo info;
        info.type = WT_MediaResource;
        info.id = id;
        info.params.media.content = content;
        info.params.media.padding = padding;
        return info;
    }

    WidgetDrawResult WidgetContextData::HandleEvents(ImVec2 origin, int from, int to)
    {
        auto io = Config.platform->CurrentIO();
        auto& renderer = usingDeferred ? *deferedRenderer : *Config.renderer;
        WidgetDrawResult result;
        to = to == -1 ? deferedEvents.size() : to;

        for (auto idx = from; idx < to; ++idx)
        {
            auto ev = deferedEvents[idx];

            switch (ev.type)
            {
            case WT_Region:
                ev.params.region.margin.Translate(origin);
                ev.params.region.border.Translate(origin);
                ev.params.region.padding.Translate(origin);
                ev.params.region.content.Translate(origin);
                HandleRegionEvent(ev.id, ev.params.label.margin, ev.params.label.border,
                    ev.params.label.padding, ev.params.label.content, renderer, io, result);
				break;
            case WT_Label: 
                ev.params.label.margin.Translate(origin);
                ev.params.label.border.Translate(origin);
                ev.params.label.padding.Translate(origin);
                ev.params.label.content.Translate(origin);
                ev.params.label.text.Translate(origin);
                HandleLabelEvent(ev.id, ev.params.label.margin, ev.params.label.border, 
                    ev.params.label.padding, ev.params.label.content, ev.params.label.text, 
                    renderer, io, result);
                break;
            case WT_Button:
                ev.params.button.margin.Translate(origin);
                ev.params.button.border.Translate(origin);
                ev.params.button.padding.Translate(origin);
                ev.params.button.content.Translate(origin);
                ev.params.button.text.Translate(origin);
                HandleButtonEvent(ev.id, ev.params.button.margin, ev.params.button.border, 
                    ev.params.button.padding, ev.params.button.content, ev.params.button.text, 
                    renderer, io, result);
                break;
            case WT_Checkbox:
                ev.params.checkbox.extent.Translate(origin);
                HandleCheckboxEvent(ev.id, ev.params.checkbox.extent, io, result);
                break;
            case WT_RadioButton:
                ev.params.radio.extent.Translate(origin);
                HandleRadioButtonEvent(ev.id, ev.params.radio.extent, ev.params.radio.maxrad, renderer, io, result);
                break;
            case WT_ToggleButton:
                ev.params.toggle.extent.Translate(origin);
                ev.params.toggle.center += origin;
                HandleToggleButtonEvent(ev.id, ev.params.toggle.extent, ev.params.toggle.center, renderer, io, result);
                break;
            case WT_Spinner:
                ev.params.spinner.extent.Translate(origin);
                ev.params.spinner.incbtn.Translate(origin);
                ev.params.spinner.decbtn.Translate(origin);
                HandleSpinnerEvent(ev.id, ev.params.spinner.extent, ev.params.spinner.incbtn, ev.params.spinner.decbtn, 
                    io, result);
                break;
            case WT_Slider:
                ev.params.slider.extent.Translate(origin);
                ev.params.slider.thumb.Translate(origin);
                HandleSliderEvent(ev.id, ev.params.slider.extent, ev.params.slider.thumb, io, result);
                break;
            case WT_RangeSlider:
                ev.params.rangeslider.extent.Translate(origin);
                ev.params.rangeslider.minThumb.Translate(origin);
                ev.params.rangeslider.maxThumb.Translate(origin);
                HandleRangeSliderEvent(ev.id, ev.params.rangeslider.extent, ev.params.rangeslider.minThumb, 
                    ev.params.rangeslider.maxThumb, io, result);
                break;
            case WT_TextInput:
                ev.params.input.content.Translate(origin);
                ev.params.input.clear.Translate(origin);
                HandleTextInputEvent(ev.id, ev.params.input.content, ev.params.input.clear, io, renderer, result);
                break;
            case WT_DropDown:
                ev.params.dropdown.margin.Translate(origin);
                ev.params.dropdown.border.Translate(origin);
                ev.params.dropdown.padding.Translate(origin);
                ev.params.dropdown.content.Translate(origin);
                HandleDropDownEvent(ev.id, ev.params.dropdown.margin, ev.params.dropdown.border, 
                    ev.params.dropdown.padding, ev.params.dropdown.content, io, renderer, result);
                break;
            case WT_TabBar:
                ev.params.tabbar.content.Translate(origin);
                HandleTabBarEvent(ev.id, ev.params.tabbar.content, io, renderer, result);
                break;
            case WT_Accordion:
                ev.params.accordion.region.Translate(origin);
                HandleAccordionEvent(ev.id, ev.params.accordion.region, ev.params.accordion.ridx, io, result);
                break;
            case WT_Scrollable:
				// TODO: Handle scrollable events if any in future
                break;
            case WT_MediaResource:
                ev.params.media.content.Translate(origin);
                ev.params.media.padding.Translate(origin);
                HandleMediaResourceEvent(ev.id, ev.params.media.padding, ev.params.media.content, io, result);
            default:
                break;
            }
        }

        if (to == -1) deferedEvents.clear(true);
        return result;
    }

    void WidgetContextData::ResetLayoutData()
    {
        for (auto idx = 0; idx < WSI_Total; ++idx)
            layoutStyles[idx].clear(true);

        layoutStack.clear(false);
        layoutItems.clear(true);
        replayContent.clear(true);
    }

    void WidgetContextData::ClearDeferredData()
    {
        deferedEvents.clear(true);
        deferedRenderer->Reset();
    }

    const ImRect& WidgetContextData::GetGeometry(int32_t id) const
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        return itemGeometries[wtype][index];
    }

    ImVec2 WidgetContextData::GetSize(int32_t id) const
    {
        auto index = id & WidgetIndexMask;
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        return itemSizes[wtype][index];
    }

    ImRect WidgetContextData::GetLayoutSize() const
    {
        return lastLayoutIdx == -1 ? ImRect{} : itemGeometries[WT_Layout][lastLayoutIdx];
    }

    ImVec2 WidgetContextData::MaximumSize() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_ItemGrid)
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                auto extent = grid.headers[grid.currlevel][grid.currCol].extent;
                return extent.GetSize();
            }
            else if (wtype == WT_Scrollable)
            {
                auto index = id & WidgetIndexMask;
                auto type = id >> WidgetTypeBits;
                auto& region = states[type][index].state.scroll;
                auto sz = ImVec2{ (region.type & ST_Horizontal) ? region.extent.x : region.viewport.GetWidth(),
                    (region.type & ST_Vertical) ? region.extent.y : region.viewport.GetHeight() };
                if (region.type & ST_Always_H) sz.y -= Config.scrollbarSz;
                if (region.type & ST_Always_V) sz.x -= Config.scrollbarSz;
                return sz;
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                return ImVec2{ accordion.hscroll ? accordion.extent.x - accordion.origin.x : accordion.size.x,
                    accordion.vscroll ? accordion.extent.y - accordion.origin.y : accordion.size.y };
            }
            else
            {
                return itemGeometries[wtype][index].GetSize();
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            auto currpos = adhocLayout.top().nextpos;
            return ImVec2{ popupSize.x == -1.f ? rect.GetWidth() - currpos.x : popupSize.x,
                popupSize.y == -1.f ? rect.GetHeight() - currpos.y : popupSize.y };
        }
    }

    ImVec2 WidgetContextData::MaximumExtent() const
    {
        if (!containerStack.empty())
        {
            auto id = containerStack.top();
            auto index = id & WidgetIndexMask;
            auto wtype = (WidgetType)(id >> WidgetTypeBits);

            if (wtype == WT_ItemGrid)
            {
                auto& grid = CurrentItemGridContext->itemGrids.top();
                const auto& config = CurrentItemGridContext->GetState(id).state.grid;
                auto max = grid.headers[grid.currlevel][grid.currCol].extent.Max - config.cellpadding;
                return max;
            }
            else if (wtype == WT_Scrollable)
            {
                auto index = id & WidgetIndexMask;
                auto type = id >> WidgetTypeBits;
                auto& region = states[type][index].state.scroll;
                auto sz = ImVec2{ (region.type & ST_Horizontal) ? region.extent.x + region.viewport.Min.x : region.viewport.Max.x,
                    (region.type & ST_Vertical) ? region.extent.y + region.viewport.Min.y : region.viewport.Max.y };
                if (region.type & ST_Always_H) sz.y -= Config.scrollbarSz;
                if (region.type & ST_Always_V) sz.x -= Config.scrollbarSz;
                return sz;
            }
            else if (wtype == WT_Accordion)
            {
                auto& accordion = accordions.top();
                const auto& state = AccordionState(accordion.id);
                const auto& region = state.scrolls[accordion.totalRegions];
                return ImVec2{ accordion.hscroll ? region.extent.x : region.viewport.Max.x,
                    accordion.vscroll ? region.extent.y : region.viewport.Max.y };
            }
            else
            {
                return itemGeometries[wtype][index].Max;
            }
        }
        else
        {
            auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
            auto ispopup = popupOrigin.x != -1.f;
            return ispopup ? popupOrigin + popupSize : rect.Max;
        }
    }

    ImVec2 WidgetContextData::WindowSize() const
    {
        auto rect = ImGui::GetCurrentWindow()->InnerClipRect;
        auto ispopup = popupOrigin.x != -1.f;
        return ispopup ? popupSize : rect.GetSize();
    }

    int WidgetContextData::GetExpectedWidgetCount(WidgetType type)
    {
        switch (type)
        {
            case WT_ItemGrid: return 4;
            case WT_TabBar: return 8;
            case WT_Splitter: return 4;
            case WT_Accordion: return 4;
            case WT_DropDown: return 16;
            default: return 32; // Default for other widget types
        }
    }
    
    WidgetContextData::WidgetContextData()
    {
        for (auto idx = 0; idx < WT_TotalTypes; ++idx)
        {
            maxids[idx] = tempids[idx] = 0;

            if (idx != WT_Layout && idx != WT_Sublayout && idx != WT_Scrollable &&
                idx != WT_SplitterRegion)
            {
                auto count = Config.GetTotalWidgetCount ? Config.GetTotalWidgetCount((WidgetType)idx) : 
                    GetExpectedWidgetCount((WidgetType)idx);
                states[idx].resize(count, WidgetConfigData{ (WidgetType)idx });
                WidgetStyles[idx].resize(count);

                switch (idx)
                {
                case WT_ItemGrid: gridStates.resize(count); break;
                case WT_TabBar: tabBarStates.resize(count); break;
                case WT_ToggleButton: toggleStates.resize(count); break;
                case WT_RadioButton: radioStates.resize(count); break;
                case WT_Checkbox: checkboxStates.resize(count); break;
                case WT_TextInput: inputTextStates.resize(count); break;
                case WT_Spinner: spinnerStates.resize(count); break;
                case WT_Splitter: 
                    splitterStates.resize(count); 
                    splitterScrollPaneParentIds.resize(count * GLIMMER_MAX_SPLITTER_REGIONS, -1);
                    break;
                case WT_Accordion: accordionStates.resize(count); break;
                case WT_DropDown: dropDownOptions.resize(count); break;
                default:
                    break;
                }
            }
        }
    }
    
    SplitterPersistentState::SplitterPersistentState()
    {
        for (auto idx = 0; idx < GLIMMER_MAX_SPLITTER_REGIONS; ++idx)
        {
            states[idx] = WS_Default;
            containers[idx] = -1;
            isdragged[idx] = false;
            dragstart[idx] = 0.f;
        }
    }

    WidgetContextData& GetContext()
    {
        return *CurrentContext;
    }

    static void InitContextStyles()
    {
        if (StyleDescriptor::GlobalThemeProvider != nullptr)
        {
            GlobalWidgetTheme theme;
            StyleDescriptor::GlobalThemeProvider(&theme);
            theme.toggle.fontsz *= Config.fontScaling;

            for (auto idx = 0; idx < WSI_Total; ++idx)
            {
                auto& style = WidgetContextData::StyleStack[idx].push();

                WidgetContextData::radioButtonStyles[idx].push() = theme.radio;
                WidgetContextData::sliderStyles[idx].push() = theme.slider;
                WidgetContextData::rangeSliderStyles[idx].push() = theme.rangeSlider;
                WidgetContextData::spinnerStyles[idx].push() = theme.spinner;
                WidgetContextData::toggleButtonStyles[idx].push() = theme.toggle;
                WidgetContextData::tabBarStyles[idx].push() = theme.tabbar;
            }
        }
        else
        {
            for (auto idx = 0; idx < WSI_Total; ++idx)
            {
                auto& style = WidgetContextData::StyleStack[idx].push();

                WidgetContextData::radioButtonStyles[idx].push();
                auto& slider = WidgetContextData::sliderStyles[idx].push();
                auto& rangeslider = WidgetContextData::rangeSliderStyles[idx].push();
                auto& spinner = WidgetContextData::spinnerStyles[idx].push();
                auto& toggle = WidgetContextData::toggleButtonStyles[idx].push();
                auto& tab = WidgetContextData::tabBarStyles[idx].push();
                toggle.fontsz *= Config.fontScaling;

                switch (idx)
                {
                case WSI_Hovered:
                    slider.thumbColor = ToRGBA(255, 255, 255);
                    rangeslider.minThumb.color = rangeslider.maxThumb.color = ToRGBA(255, 255, 255);
                    spinner.downbtnColor = spinner.upbtnColor = ToRGBA(240, 240, 240);
                    tab.closebgcolor = tab.pinbgcolor = ToRGBA(150, 150, 150);
                    tab.pincolor = ToRGBA(0, 0, 0);
                    tab.closecolor = ToRGBA(255, 0, 0);
                    [[fallthrough]];
                case WSI_Checked:
                    toggle.trackColor = ToRGBA(200, 200, 200);
                    toggle.indicatorTextColor = ToRGBA(100, 100, 100);
                    slider.trackColor = rangeslider.trackColor = ToRGBA(175, 175, 175);
                    slider.fillColor = rangeslider.fillColor = ToRGBA(100, 149, 237);
                    break;
                default:
                    toggle.trackColor = ToRGBA(152, 251, 152);
                    toggle.indicatorTextColor = ToRGBA(0, 100, 0);
                    slider.thumbColor = ToRGBA(240, 240, 240);
                    rangeslider.minThumb.color = rangeslider.maxThumb.color = ToRGBA(240, 240, 240);
                    spinner.downbtnColor = spinner.upbtnColor = ToRGBA(200, 200, 200);
                    tab.closebgcolor = tab.pinbgcolor = ToRGBA(0, 0, 0, 0);
                    tab.pincolor = ToRGBA(0, 0, 0);
                    tab.closecolor = ToRGBA(255, 0, 0);
                    break;
                }
            }
        }


    }

    WidgetContextData& PushContext(int32_t id)
    {
        if (id < 0)
        {
            if (CurrentContext == nullptr)
            {
                constexpr auto totalStyles = 16 * WSI_Total;
                CurrentContext = &(WidgetContexts.emplace_back());
                InitContextStyles();
            }
        }
        else
        {
            auto wtype = (id >> WidgetTypeBits);
            auto index = id & WidgetIndexMask;

            if ((int)CurrentContext->nestedContexts[wtype].size() <= index)
            {
                auto& ctx = WidgetContexts.emplace_back();
                ctx.parentContext = CurrentContext;
                ctx.InsideFrame = CurrentContext->InsideFrame;

                auto& children = CurrentContext->nestedContexts[wtype];
                if (children.empty()) children.resize(WT_TotalNestedContexts, nullptr);
                children[index] = &ctx;

                auto& layout = ctx.adhocLayout.push();
                layout.nextpos = CurrentContext->adhocLayout.top().nextpos;
            }

            CurrentContext = CurrentContext->nestedContexts[wtype][index];
            
            for (auto idx = 0; idx < WT_TotalTypes; ++idx) CurrentContext->maxids[idx] = 0;
        }

        return *CurrentContext;
    }

    void PopContext()
    {
        CurrentContext = GetContext().parentContext;
        assert(CurrentContext != nullptr);
    }

    void Cleanup()
    {
        ImPlot::DestroyContext(ChartsContext);
    }

    StyleDescriptor GetStyle(WidgetContextData& context, int32_t id, StyleStackT* StyleStack, int32_t state)
    {
        StyleDescriptor res;
        StyleDescriptor const* defstyle = nullptr;
        auto style = (WidgetStateIndex)log2((unsigned)state);
        auto wtype = (WidgetType)(id >> WidgetTypeBits);
        auto index = id & WidgetIndexMask;

        defstyle = &glimmer::GetWidgetStyle(wtype, WidgetStateIndex::WSI_Default);
        res.From(glimmer::GetWidgetStyle(wtype, style));

        if (defstyle->specified == 0) defstyle = &(context.WidgetStyles[wtype][index][WSI_Default]);
        res.From(context.WidgetStyles[wtype][index][style]);

        if ((IgnoreStyleStackBits == -1) || !(IgnoreStyleStackBits & (1 << wtype)))
        {
            res.From(StyleStack[style].top());
            defstyle = &(StyleStack[WSI_Default].top());
        }
        else if (defstyle->specified == 0)
            defstyle = StyleStack[WSI_Default].begin();

        if (style != WSI_Default) CopyStyle(*defstyle, res);
        AddFontPtr(res.font);
        return res;
    }

	// This is a global config which can only be set during initialization
    // Read-only access is provided once set for subsequent frames
    UIConfig Config{};

    NestedContextSource InvalidSource{};

    ImRect WidgetContextData::ActivePopUpRegion;
    UIElementDescriptor WidgetContextData::RightClickContext;
    Vector<ContextMenuItemDescriptor, int16_t, 16> WidgetContextData::ContextMenuOptions{ false };
    Vector<ContextMenuItemParams, int16_t, 16> WidgetContextData::ContextMenuOptionParams{ false };
    int32_t WidgetContextData::PopupTarget = -1;
    StyleStackT WidgetContextData::StyleStack[WSI_Total]{ false, false, false, false,
        false, false, false, false, false };
    WidgetContextData* WidgetContextData::CurrentItemGridContext = nullptr;
	int32_t WidgetContextData::CurrentWidgetId = -1;
    DynamicStack<ToggleButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::toggleButtonStyles[WSI_Total];
    DynamicStack<RadioButtonStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES>  WidgetContextData::radioButtonStyles[WSI_Total];
    DynamicStack<SliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::sliderStyles[WSI_Total];
    DynamicStack<RangeSliderStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::rangeSliderStyles[WSI_Total];
    DynamicStack<SpinnerStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::spinnerStyles[WSI_Total];
    DynamicStack<TabBarStyleDescriptor, int16_t, GLIMMER_MAX_WIDGET_SPECIFIC_STYLES> WidgetContextData::tabBarStyles[WSI_Total];
}