<h1>Glimmer: An Immediate Mode GUI Library (Under Progress 🚧)</h1>
---------------------------------------

Glimmer is a C++20 library for creating graphical user interfaces using an immediate mode paradigm. This approach simplifies UI development by allowing developers to define the UI structure and behavior directly within their application logic.

- **Immediate Mode**: Unlike retained mode GUI libraries where UI elements are persistent objects, Glimmer processes UI elements on each frame. This means the UI is redrawn every frame based on the current
   application state. 

- **Widgets**: Glimmer provides a set of common UI widgets such as labels, buttons, checkboxes, radio buttons, text inputs, dropdowns, sliders, spinners, and more complex ones like item grids, accordion and tab bars. 

- **Layout System**: The library includes a flexible layout system to arrange widgets. It supports: 
   - **Ad-hoc Positioning**: Widgets can be placed at specific coordinates. 
   - **Flexbox Layouts**: Flexbox layout scheme (choose between built-in, [Yoga](https://github.com/facebook/yoga) and [Clay](https://github.com/nicbarker/clay)). 
   - **Grid Layouts**: Row and Column specified layout scheme with multi-span row and column support. 
   - **Splitter Layouts**: Dividing regions into resizable sub-regions. 
   - **Fill and Alignment**: Controls for how widgets fill available space and align within their
   parent. 
   - **Spanning and Moving**: Fine-grained control over widget placement relative to others. 

- **Styling**: Widgets can be styled using CSS-like string definitions. This includes properties for colors,
   fonts, borders, padding, margins, and shadows. Styles can be applied globally, per widget type, or for specific widget states (default, hover, pressed, focused, checked, disabled). 

- **SVG & Image Support**: Both SVG and PNG images can be rendered. [LunaSVG](https://github.com/sammycage/lunasvg) and [STB_Image](https://github.com/nothings/stb) is being used respectively.

- **Text Rendering**: Fonts are supported through the [freetype2](https://freetype.org/) library and basic support for extended ASCII exists. Complex scripts and bidirectional text is not yet supported.

- **Rich Text**: Rich text rendering is supported through [ImRichText](https://github.com/ajax-crypto/ImRichText)
   
- **Platform Abstraction**: The library includes a platform abstraction layer to handle window creation and event polling. 

- **Context Management**: A central `WidgetContextData` manages the state of all widgets, styles,
   layouts, and other UI-related data, minimizes allocations within frames.

- **Native File Dialogs**: Native file dialogs are supported by using [nfd-extended](https://github.com/btzy/nativefiledialog-extended) library or can be defined by the platform layer i.e. SDL3.

- **Plots**: [ImPlot](https://github.com/epezent/implot) library is used to create plots/graphs.

<h2> Styling & Events:</h2>

- PushStyle and PopStyle are used to apply and remove styles for default and specific widget states (e.g., `WS_Selected`, `WS_Checked`).
- CSS-like strings define properties like background-color, border, padding, alignment, width. SetNextStyle is used for one-time style application. 
- Event Handling: A polling loop processes UI interactions and redraws the UI. 

<h2> Platform & Rendering </h2>

Glimmer abstracts the rendering process through the `glimmer::IRenderer` interface. There are following renderers available:
  - **ImGui Renderer** which uses ImGui to generate geometry primitives and use some ImGui platform integration to render.
  - **SVG Renderer** which dumps each frame as SVG markup in some SVG file.
  - (Internal) **Deferred Renderer** which enqueues draw commands and can be dequed at any point.
  - (Planned) **Blend2D Renderer** which uses the [Blend2D](https://blend2d.com/) library to render which works without any GPU in the system.
  - (Planned) **Test Renderer** which will enable creating automated tests (along with a test platform interface to pump events).

The platform integration is abstracted by `glimmer::IPlatform` interface. Integration is tested with SDL3, SDL3-GPU and GLFW + OpenGL3 combinations in the test projects.

<h2>	Getting Started (Conceptual)</h2>

1.	Include Glimmer: Add glimmer.h to your project.
2.	Initialize Platform and Renderer:
    -	Get the platform instance: `glimmer::GetPlatform()`.
    -	Create a window using `platform->CreateWindow()`.
    -	Create a renderer instance (e.g., `glimmer::CreateImGuiRenderer()`).
    -	Set the renderer in the `WindowConfig`.
3.	Load Fonts: Use `glimmer::LoadDefaultFonts()` or other font management functions.
4.	Register widgets with specific IDs, using `glimmer::GetNextId` and `glimmer::GetWidgetState`
5.	Main Loop:
    -	Poll platform events using `platform->PollEvents()`. Inside the event callback:
    -	Define your UI structure by creating widgets (e.g., `glimmer::Label(id, ...)`), applying styles (`glimmer::PushStyle(...)`), and managing layouts (`glimmer::BeginLayout(...)`, `glimmer::Move(...)`, `glimmer::EndLayout()`).
6.	Widget Interaction:
    -	Obtain unique IDs for widgets using `glimmer::GetNextId(WidgetType)`.
    -	Access and modify widget-specific state using `glimmer::GetWidgetState(id).state.<widget_specific_state>`.
    -	Widget functions (like `glimmer::Button()`) return a `WidgetDrawResult` which will have event details. However if widget belongs in a layout hierarchy, the last `EndLayout()` call returns the event details with widget id.
Look at `GlimmerTest/test.cpp` file for an example.

<h2> Library configuration macros </h2>

| Macro | Definition/Value | Purpose |
|-------|------------------|---------|
| `GLIMMER_DISABLE_SVG` | (conditional) | When defined, disables SVG rendering support (excludes lunasvg) |
| `GLIMMER_DISABLE_IMAGES` | (conditional) | When defined, disables image loading support (excludes stb_image) |
| `GLIMMER_DISABLE_RICHTEXT` | (conditional) | When defined, disables rich text rendering | 
| `GLIMMER_DISABLE_PLOTS` | (conditional) | When defined, disables plotting/graph library integration | 
| `GLIMMER_ENABLE_NFDEXT` | (conditional) | When defined, enables nfd-extended library to enable file pickers | 
| `GLIMMER_TOTAL_ID_SIZE` | (1 << 16) (65536) | Maximum size for widget ID string backing store |
| `GLIMMER_MAX_ITEMGRID_COLUMN_CATEGORY_LEVEL` | (not shown, likely 8-16) | Maximum nesting level for item grid column categories |
| `GLIMMER_MAX_SPLITTER_REGIONS` | (8-16) | Maximum number of regions in a splitter widget |
| `GLIMMER_MAX_STYLE_STACKSZ` | (8-16) | Maximum nested CSS styles allowed |
| `GLIMMER_MAX_WIDGET_SPECIFIC_STYLES` | (8-16) | Maximum distinct per widget styles allowed |
| `GLIMMER_MAX_LAYOUT_NESTING` | (8-16) | Maximum nesting of layouts allowed |
| `GLIMMER_MAX_REGION_NESTING` | (8-16) | Maximum nesting of containers/regions allowed |
| `GLIMMER_DEFAULT_FONTFAMILY` | String value | Default font family name for the UI |
| `IM_COL32_A_MASK` | 0xFF000000 | Alpha channel mask for 32-bit colors |
| `IM_COL32_WHITE` | 0xFFFFFFFF | White color constant |
