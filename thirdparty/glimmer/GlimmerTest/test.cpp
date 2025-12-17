#include "../src/glimmer.h"

#if !defined(_DEBUG) && defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef CreateWindow
#endif

auto svg = R"SVG(
<svg viewBox='0 0 108 95' xmlns='http://www.w3.org/2000/svg'>
<g transform='scale(0.1)'>
<path d='M552,598c-5,10-11,20-12,32c8,11,16,7,25,0c-2-12-7-22-13-32'/>
<path d='M528,7c-17,3-32,13-41,28l-473,820c-10,18-10,39,0,56c10,18,29,29,49,29h947c20,0,39-11,49-29c10-17,10-38,0-56l-473-820c-12-21-35-31-58-28' fill='#fff000'/>
<path d='M532,30c-9,2-17,7-21,15l-477,826c-5,9-5,21,0,30c5,9,15,15,26,15h954c10,0,20-6,26-15c5-9,5-21,0-30l-477-826c-7-11-18-16-31-15M537,126l405,713h-811z'/>
<path d='M582,378l-5,1l1,4l-129,31h-1c-5,3-9,7-10,12c-2,4-2,10,0,14c3,9,12,17,22,16h1l128-28l1,5l5-1zM580,389l8,34l-24,5h-121c1-4,3-7,8-9z'/>
<path d='M383,485l5,2l-1,4l132,19c6,2,10,6,12,11c2,4,2,10,1,14c-2,10-10,18-21,18l-131-17v5h-5zM387,496l-6,35l25,3l120-10v-1c-1-3-4-6-8-8z'/>
<path d='M249,699v43h211v-43h-64l-2,3l-2,4l-4,3c0,0-1,2-2,2h-4c-2,0-3,0-4,1c-1,1-3,1-3,2l-3,4c0,1-1,2-2,2h-4c0,0-2,1-3,0l-3-1c-1,0-3-1-3-2c-1-1,0-2-1-3l-1-3c-1-1-2-1-3-1c-1,0-4,0-4-1c0-2,0-3-1-4v-3v-3z'/>
<path d='M385,593c0,9-6,15-13,15c-7,0-13-6-13-15c0-8,12-39,14-39c1,0,12,31,12,39'/>
<path d='M382,671c0,8-6,15-13,15c-7,0-13-7-13-15c0-8,12-39,13-39c1,0,13,31,13,39'/>
<path d='M614,493c0,9-6,15-13,15c-7,0-13-6-13-15c0-8,12-39,13-39c2,0,13,31,13,39'/>
<path d='M613,591c0,8-6,15-13,15c-7,0-13-7-13-15c0-8,12-39,13-39c1,0,13,31,13,39'/>
<path d='M627,609c-7,0-8,3-7,8c1,6,4,9,7,13c12,10,25,13,37,18v3h-47c-2,6-8,10-15,10c-7,0-13-4-15-10h-4c0,4-4,7-9,7c-4,0-8-3-9-7h-42c-8,0-14,5-14,13c0,8,6,14,14,14h118c1,2,2,4,0,6h-140c-7,0-13,6-13,13c0,8,6,13,13,13h125c2,3,2,6,0,8h-112c-7,0-13,5-13,12c0,7,6,12,13,12h136c1,1,2,4,0,6h-94c-6,0-11,5-11,12c0,6,5,12,11,12h95v1c59,10,129,11,173-11v-105c-49-22-102-34-155-44c-6,1-14-2-19,1c-7-6-15-5-23-5'/>
</g>
</svg>
)SVG";

void TestWindow(glimmer::UIConfig& config)
{
    enum Labels {
        UPPER, LEFT, LEFT2, CONTENT, BOTTOM, TOGGLE, RADIO, INPUT,
        DROPDOWN, CHECKBOX, SPLIT1, SPLIT2, GRID, SLIDER, SPINNER, TAB, ACCORDION, TOTAL
    };
    int32_t widgets[TOTAL];

    auto lid = glimmer::GetNextId(glimmer::WT_Label);
    auto& st = glimmer::GetWidgetConfig(lid).state.label;
    st.text = svg;
    st.type = glimmer::TextType::SVG;

    //st.text = "Hello";
    widgets[UPPER] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Label);
    glimmer::GetWidgetConfig(lid).state.label.text = "Left";
    widgets[LEFT] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Label);
    glimmer::GetWidgetConfig(lid).state.label.text = "Left2";
    widgets[LEFT2] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Splitter);
    glimmer::GetWidgetConfig(lid);
    widgets[SPLIT1] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Splitter);
    glimmer::GetWidgetConfig(lid);
    widgets[SPLIT2] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Label);
    glimmer::GetWidgetConfig(lid).state.label.text = "Content";
    widgets[CONTENT] = lid;

    lid = glimmer::GetNextId(glimmer::WT_Label);
    glimmer::GetWidgetConfig(lid).state.label.text = "Bottom";
    widgets[BOTTOM] = lid;

    auto tid = glimmer::GetNextId(glimmer::WT_ToggleButton);
    glimmer::GetWidgetConfig(tid).state.toggle.checked = false;
    widgets[TOGGLE] = tid;

    tid = glimmer::GetNextId(glimmer::WT_RadioButton);
    glimmer::GetWidgetConfig(tid).state.radio.checked = false;
    widgets[RADIO] = tid;

    tid = glimmer::GetNextId(glimmer::WT_Checkbox);
    glimmer::GetWidgetConfig(tid);
    widgets[CHECKBOX] = tid;

    tid = glimmer::GetNextId(glimmer::WT_Spinner);
    glimmer::GetWidgetConfig(tid).state.spinner.max = 100;
    widgets[SPINNER] = tid;

    tid = glimmer::GetNextId(glimmer::WT_TextInput);
    auto& model = glimmer::GetWidgetConfig(tid).state.input;
    model.placeholder = "This will be removed!";
    model.text.reserve(256);
    widgets[INPUT] = tid;

    tid = glimmer::GetNextId(glimmer::WT_Slider);
    auto& slider = glimmer::GetWidgetConfig(tid).state.slider;
    slider.max = 100.f;
    widgets[SLIDER] = tid;

    tid = glimmer::GetNextId(glimmer::WT_TabBar);
    auto& tab = glimmer::GetWidgetConfig(tid).state.tab;
    tab.sizing = glimmer::TabBarItemSizing::ResizeToFit;
    tab.expandTabs = true;
    widgets[TAB] = tid;

    tid = glimmer::GetNextId(glimmer::WT_DropDown);
    auto& dd = glimmer::GetWidgetConfig(tid).state.dropdown;
    dd.text = "DropDown";
    std::vector<std::pair<glimmer::WidgetType, std::string_view>> options;
    options.emplace_back(glimmer::WT_Checkbox, "Option 1");
    options.emplace_back(glimmer::WT_Checkbox, "Option 2");
    dd.options = options;
    widgets[DROPDOWN] = tid;

    widgets[ACCORDION] = glimmer::GetNextId(glimmer::WT_Accordion);

    auto gridid = glimmer::GetNextId(glimmer::WT_ItemGrid);
    auto& grid = glimmer::GetWidgetConfig(gridid).state.grid;
    grid.cellwidget = [](std::pair<float, float>, int32_t row, int16_t col, int16_t) {
        static char buffer[128];
        auto sz = std::snprintf(buffer, 127, "Test-%d-%d", row, col);

        auto id = glimmer::GetNextId(glimmer::WT_Label);
        glimmer::GetWidgetConfig(id).state.label.text = std::string_view{ buffer, (size_t)sz };
        auto result = glimmer::Label(id);

        glimmer::Move(glimmer::FD_Horizontal);
        id = glimmer::GetNextId(glimmer::WT_Checkbox);
        glimmer::GetWidgetConfig(id);
        result = glimmer::Checkbox(id);

        PopStyle(1, glimmer::WS_Default);
        result;
        };
    grid.cellprops = [](int32_t row, int16_t, int16_t, int32_t, int32_t) {
        glimmer::ItemGridItemProps props;
        row % 2 ? glimmer::PushStyle("background-color: white") :
            glimmer::PushStyle("background-color: rgb(200, 200, 200)");
        return props;
        };
    grid.header = [](ImVec2, float, int16_t level, int16_t col, int16_t) {
        auto id = glimmer::GetNextId(glimmer::WT_Label);
        if (level == 0)
        {
            if (col == 0) glimmer::GetWidgetConfig(id).state.label.text = "Header#1";
            else glimmer::GetWidgetConfig(id).state.label.text = "Header#2";
        }
        else
        {
            switch (col)
            {
            case 0: glimmer::GetWidgetConfig(id).state.label.text = "Header#1.1"; break;
            case 1: glimmer::GetWidgetConfig(id).state.label.text = "Header#1.2"; break;
            case 2: glimmer::GetWidgetConfig(id).state.label.text = "Header#2.1"; break;
            case 3: glimmer::GetWidgetConfig(id).state.label.text = "Header#2.2"; break;
            }
        }
        glimmer::Label(id);
    };
    widgets[GRID] = gridid;

    /*auto& root = grid.config.headers.emplace_back();
    root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Headerdede#1" });
    root.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Headerfefe#2" });

    auto& leaves = grid.config.headers.emplace_back();
    leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.1", .parent = 0 });
    leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#1.2", .parent = 0 });
    leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.1", .parent = 1 });
    leaves.push_back(glimmer::ItemGridState::ColumnConfig{ .name = "Header#2.2", .parent = 1 });

    grid.config.rows = 32;
    grid.uniformRowHeights = true;
    grid.setColumnResizable(-1, true);
    grid.setColumnProps(-1, glimmer::COL_Moveable, true);*/
    void* data = widgets;

    config.platform->PollEvents([](ImVec2, glimmer::IPlatform&, void* data) {
        int32_t* widgets = (int32_t*)data;
        glimmer::PushStyle("background-color: rgb(200, 200, 200); padding: 5px; alignment: center; margin: 5px;", 
            "border: 1px solid black");

        using namespace glimmer;

        Label(widgets[UPPER], ExpandH);
        Move(FD_Vertical);

        StartTabBar(widgets[TAB]);
        AddTab("Tab 1", "", TI_Closeable);
        AddTab("Tab 2", "", TI_Closeable);
        AddTab("Tab 3", "", TI_Closeable);
        EndTabBar(true);
        PopStyle(1, WS_Default);
        Move(FD_Vertical);

        StartSplitRegion(widgets[SPLIT1], DIR_Horizontal, { SplitRegion{.min = 0.2f, .max = 0.3f, .initial = 0.2f },
            SplitRegion{.min = 0.7f, .max = 0.8f, .initial = 0.8f } }, ExpandH);

        {
            StartSplitRegion(widgets[SPLIT2], DIR_Vertical, { SplitRegion{.min = 0.2f, .max = 0.8f },
                SplitRegion{.min = 0.2f, .max = 0.8f } }, ExpandV);
            {
                Label(widgets[LEFT], ExpandAll);
                
                NextSplitRegion();

                PushStyle("padding: 5px; background-color: gray;");
                BeginFlexRegion("test-region", DIR_Vertical, ImVec2{}, false, ETP_Hovered, ExpandAll);
                {
                    PushStyle("padding: 5px; background-color: lightblue;");
                    Label(widgets[CONTENT], ExpandAll);

                    PushStyle("padding: 5px; background-color: lightgreen;");
                    Label(widgets[LEFT2], ExpandV);
				}
                EndRegion();
                PopStyle(3, WS_Default);
            }
            EndSplitRegion();

            NextSplitRegion();

            StartAccordion(widgets[ACCORDION], ExpandH | ToTop | ToRight, NeighborWidgets{ .top = widgets[TAB] });
            PushStyle("background-color: rgb(100, 0, 255); color: rgb(200, 200, 200);", "color: white;");
            StartAccordionHeader();
            AddAccordionHeaderText("Test Accordion");
            EndAccordionHeader(FramesRendered() == 0 ? std::optional<bool>{ true } : std::nullopt);
            PopStyle(1, WS_Default);

            if (StartAccordionContent())
            {
                BeginFlexLayout(DIR_Horizontal, AlignHCenter | ExpandAll, false, { 5.f, 5.f });
                {
                    PushStyle(WS_Hovered, "background-color: crimson; color: white;");
                    Button("bottom", "Bottom");
					PopStyle(1, WS_Hovered);
                    Move(FD_Horizontal);

                    ToggleButton(widgets[TOGGLE], ToRight);

                    Move(FD_Horizontal);
                    RadioButton(widgets[RADIO], ToRight);
                    Spinner(widgets[SPINNER], ToRight);

                    Move(FD_Horizontal);
                    PushStyle("border: 1px solid gray; width: 200px; background-color: white", "border: 2px solid black;");
                    PushStyle(WS_Focused, "border: 2px solid black;");
                    TextInput(widgets[INPUT], ToRight);
                    PopStyle(1, WS_Default | WS_Focused);

                    Move(FD_Horizontal);
                    DropDown(widgets[DROPDOWN], ToRight);

                    PushStyle(WS_Default, "width: 200px; border-radius: 3px;");
                    PushStyle(WS_Hovered, "background-color: blue; color: white;");
                    Move(FD_Horizontal);
                    Slider(widgets[SLIDER], ToRight);
                    PopStyle(1, WS_Default);
                    PopStyle(1, WS_Hovered);
                }

                EndLayout();
            }

            EndAccordionContent();
            EndAccordion();

            Move(widgets[LEFT], widgets[TAB], true, true);
            PushStyle("background-color: white; padding: 5px; border: none;");

            StartItemGrid(widgets[GRID], ExpandAll, { .bottom = widgets[ACCORDION] });
            StartItemGridHeader(2);
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable, .parent = 0 });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable, .parent = 0 });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable, .parent = 0 });
            AddHeaderColumn(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable, .parent = 0 });

            CategorizeColumns();
            AddColumnCategory(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable }, 0, 1);
            AddColumnCategory(ItemGridConfig::ColumnConfig{ .props = COL_Resizable | COL_Sortable }, 2, 3);

            EndItemGridHeader();
            PopulateItemGrid(100, ItemGridPopulateMethod::ByColumns);
            EndItemGrid();
        }

        EndSplitRegion();
        PopStyle(5);
        return true;
    }, data);
}

#if !defined(_DEBUG) && defined(_WIN32)
int CALLBACK WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    int         nCmdShow
)
#else
int main(int argc, char** argv)
#endif
{
    auto& config = glimmer::GetUIConfig(true);
    config.renderer = glimmer::CreateImGuiRenderer();
    config.defaultFontSz = 24.f;
    config.platform = glimmer::GetPlatform();
    
    if (config.platform->CreateWindow({ .title = "Glimmer Demo" }))
    {
        glimmer::FontFileNames names;
        names.BasePath = "C:\\Work\\Sessions\\fonts\\";
        names.Monospace.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        //names.Proportional.Files[glimmer::FT_Normal] = "IosevkaFixed-Regular.ttf";
        glimmer::FontDescriptor desc;
        desc.flags = glimmer::FLT_Proportional | glimmer::FLT_Antialias | glimmer::FLT_Hinting;
        //desc.names = names;
        desc.sizes.push_back(16.f);
        desc.sizes.push_back(24.f);
        desc.sizes.push_back(32.f);
        desc.sizes.push_back(48.f);
        glimmer::LoadDefaultFonts(&desc, 1, true);

        TestWindow(config);
    }

    return 0;
}