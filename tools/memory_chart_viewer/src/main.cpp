// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// memory_chart_viewer - a tiny standalone previewer for the ROCm Optiq compute
// "Memory Chart". It renders a relational memory-chart layout (the same JSON
// schema stored in compute_workload.memory_chart_extdata) and, when given a
// rocprof-compute trace database, populates the metric values for a selected
// kernel. It exists so layouts can be iterated on without launching the full
// application.
//
// Usage:
//   memory_chart_viewer <layout.json> [trace.db] [options]
//   memory_chart_viewer <trace.db> --from-db            (use layout stored in DB)
//
// Options:
//   --json <path>       Layout JSON file (also inferred from a *.json argument).
//   --db <path>         Trace database  (also inferred from a *.db argument).
//   --from-db           Load the layout from compute_workload.memory_chart_extdata.
//   -w, --workload <id> Initial workload id.
//   -k, --kernel <uuid> Initial kernel uuid.
//   -h, --help          Show this help.
//
// In-app: pick the workload/kernel from the toolbar; press R (or the Reload
// button) to re-read the JSON file after editing it.

#if defined(_WIN32)
#    include <windows.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Let GLFW pull in the platform GL headers so glViewport/glClear are declared.
#include <GLFW/glfw3.h>

#include "memchart_renderer.h"
#include "metric_store.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct AppState
{
    std::string layout_path;
    std::string db_path;
    bool        use_db_layout = false;

    mcv::MetricStore    store;
    mcv::MemChartRenderer renderer;

    int      workload_idx = -1;
    uint32_t category_id  = 3;

    std::vector<mcv::KernelRow> kernels;
    int                         kernel_idx  = -1;
    int64_t                     kernel_uuid = -1;  // Preserved across reloads.

    // Initial selection requested on the command line.
    bool     have_workload_arg = false;
    uint32_t workload_arg      = 0;
    bool     have_kernel_arg   = false;
    int64_t  kernel_arg        = 0;

    std::string layout_status;   // Human-readable load result.
    bool        layout_ok = false;
    std::string layout_source;
    std::string metrics_status;
};

bool
EndsWith(const std::string& s, const char* suffix)
{
    const std::string suf(suffix);
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool
ReadFileToString(const std::string& path, std::string& out)
{
    std::ifstream file(path, std::ios::binary);
    if(!file.good()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

std::string
Truncate(const std::string& s, size_t n)
{
    if(s.size() <= n) return s;
    return s.substr(0, n > 3 ? n - 3 : 0) + "...";
}

uint32_t
CurrentWorkloadId(const AppState& app)
{
    if(app.workload_idx < 0 || app.workload_idx >= (int) app.store.Workloads().size())
        return 0;
    return app.store.Workloads()[app.workload_idx].id;
}

// (Re)choose the layout JSON text and parse it into the renderer.
void
ApplyLayout(AppState& app)
{
    std::string text;
    std::string source;

    bool db_ready = app.store.IsOpen() && app.workload_idx >= 0;

    // Explicit --from-db wins; otherwise a file wins; otherwise fall back to a
    // DB blob when no file was provided.
    if(app.use_db_layout && db_ready)
    {
        text = app.store.ReadLayoutBlob(CurrentWorkloadId(app));
        if(!text.empty()) source = "database blob (workload " +
                                    std::to_string(CurrentWorkloadId(app)) + ")";
    }
    if(text.empty() && !app.layout_path.empty())
    {
        if(ReadFileToString(app.layout_path, text)) source = "file: " + app.layout_path;
        else
        {
            app.layout_ok     = false;
            app.layout_status = "Cannot read file: " + app.layout_path;
            app.layout_source.clear();
            return;
        }
    }
    if(text.empty() && !app.use_db_layout && db_ready)
    {
        text = app.store.ReadLayoutBlob(CurrentWorkloadId(app));
        if(!text.empty()) source = "database blob (workload " +
                                    std::to_string(CurrentWorkloadId(app)) + ")";
    }

    if(text.empty())
    {
        app.layout_ok = false;
        app.layout_status =
            "No layout available. Pass a .json file, or a .db that has a "
            "memory_chart_extdata blob (with --from-db).";
        app.layout_source.clear();
        return;
    }

    std::string error;
    if(app.renderer.SetLayoutJson(text, &error))
    {
        app.layout_ok      = true;
        app.layout_status  = "Loaded OK";
        app.layout_source  = source;
        app.category_id    = app.renderer.Layout().metric_category_id;
    }
    else
    {
        app.layout_ok     = false;
        app.layout_status = "JSON error: " + error;
        app.layout_source = source;
    }
}

// Rebuild the kernel list for the current workload/category, keeping the current
// kernel selection when it still exists.
void
ReloadKernels(AppState& app)
{
    app.kernels.clear();
    app.kernel_idx = -1;
    if(!app.store.IsOpen() || app.workload_idx < 0) return;

    app.kernels =
        app.store.KernelsForWorkload(CurrentWorkloadId(app), app.category_id);
    if(app.kernels.empty()) return;

    // Prefer a command-line kernel, then the previously-selected uuid, else 0.
    int64_t wanted = app.kernel_uuid;
    if(app.have_kernel_arg)
    {
        wanted            = app.kernel_arg;
        app.have_kernel_arg = false;  // Only honor once.
    }
    for(size_t i = 0; i < app.kernels.size(); ++i)
    {
        if(app.kernels[i].uuid == wanted)
        {
            app.kernel_idx = (int) i;
            break;
        }
    }
    if(app.kernel_idx < 0) app.kernel_idx = 0;
    app.kernel_uuid = app.kernels[app.kernel_idx].uuid;
}

void
ReloadMetrics(AppState& app)
{
    if(!app.store.IsOpen() || app.kernel_idx < 0)
    {
        app.renderer.SetMetricStore(app.store.IsOpen() ? &app.store : nullptr);
        app.metrics_status = app.store.IsOpen() ? "No kernel selected" : "No trace DB";
        return;
    }

    std::string error;
    if(app.store.LoadKernelMetrics(CurrentWorkloadId(app), app.kernel_uuid,
                                   app.category_id, &error))
    {
        app.renderer.SetMetricStore(&app.store);
        app.metrics_status = std::to_string(app.store.LoadedMetricCount()) +
                             " metrics (category " + std::to_string(app.category_id) + ")";
    }
    else
    {
        app.renderer.SetMetricStore(&app.store);
        app.metrics_status = "Metric load failed: " + error;
    }
}

void
ReloadAll(AppState& app)
{
    ApplyLayout(app);
    ReloadKernels(app);
    ReloadMetrics(app);
}

void
DrawToolbar(AppState& app)
{
    // Reload.
    bool reload = ImGui::Button("Reload (R)");
    if(!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_R)) reload = true;

    ImGui::SameLine();
    ImVec4 status_col = app.layout_ok ? ImVec4(0.45f, 0.85f, 0.45f, 1.0f)
                                      : ImVec4(0.95f, 0.45f, 0.45f, 1.0f);
    ImGui::TextColored(status_col, "%s", app.layout_status.c_str());
    if(!app.layout_source.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", app.layout_source.c_str());
    }

    // Trace DB selectors.
    if(app.store.IsOpen())
    {
        const std::vector<mcv::WorkloadRow>& workloads = app.store.Workloads();

        ImGui::SetNextItemWidth(320.0f);
        std::string wl_preview = "(none)";
        if(app.workload_idx >= 0 && app.workload_idx < (int) workloads.size())
        {
            const mcv::WorkloadRow& w = workloads[app.workload_idx];
            wl_preview = w.name + (w.sub_name.empty() ? "" : " / " + w.sub_name) +
                         " (id " + std::to_string(w.id) + ")";
        }
        if(ImGui::BeginCombo("Workload", wl_preview.c_str()))
        {
            for(int i = 0; i < (int) workloads.size(); ++i)
            {
                const mcv::WorkloadRow& w        = workloads[i];
                std::string             label    = w.name +
                                    (w.sub_name.empty() ? "" : " / " + w.sub_name) +
                                    " (id " + std::to_string(w.id) + ")";
                bool selected = (i == app.workload_idx);
                if(ImGui::Selectable(label.c_str(), selected))
                {
                    if(i != app.workload_idx)
                    {
                        app.workload_idx = i;
                        app.kernel_uuid  = -1;  // Reset kernel on workload change.
                        ReloadAll(app);
                    }
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(520.0f);
        std::string k_preview = "(none)";
        if(app.kernel_idx >= 0 && app.kernel_idx < (int) app.kernels.size())
        {
            const mcv::KernelRow& k = app.kernels[app.kernel_idx];
            k_preview = Truncate(k.name, 70) + "  (uuid " + std::to_string(k.uuid) + ")";
        }
        if(ImGui::BeginCombo("Kernel", k_preview.c_str()))
        {
            for(int i = 0; i < (int) app.kernels.size(); ++i)
            {
                const mcv::KernelRow& k     = app.kernels[i];
                std::string label = Truncate(k.name, 90) + "  (uuid " +
                                    std::to_string(k.uuid) + ")";
                bool selected = (i == app.kernel_idx);
                if(ImGui::Selectable(label.c_str(), selected))
                {
                    if(i != app.kernel_idx)
                    {
                        app.kernel_idx  = i;
                        app.kernel_uuid = k.uuid;
                        ReloadMetrics(app);
                    }
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if(app.store.HasLayoutColumn())
        {
            ImGui::SameLine();
            if(ImGui::Checkbox("Layout from DB", &app.use_db_layout)) ReloadAll(app);
        }

        ImGui::TextDisabled("DB: %s  |  schema %s  |  view %s  |  %s",
                            app.db_path.c_str(),
                            app.store.SchemaVersion().empty()
                                ? "?"
                                : app.store.SchemaVersion().c_str(),
                            app.store.MetricViewName().empty()
                                ? "(none)"
                                : app.store.MetricViewName().c_str(),
                            app.metrics_status.c_str());
    }
    else
    {
        ImGui::TextDisabled(
            "No trace database loaded - metric values show N/A. Pass a .db to populate.");
    }

    if(reload) ReloadAll(app);
}

void
PrintUsage(const char* exe)
{
    std::printf(
        "memory_chart_viewer - standalone ROCm Optiq Memory Chart previewer\n\n"
        "Usage:\n"
        "  %s <layout.json> [trace.db] [options]\n"
        "  %s <trace.db> --from-db\n\n"
        "Options:\n"
        "  --json <path>        Layout JSON file (also inferred from a *.json arg)\n"
        "  --db <path>          Trace database (also inferred from a *.db arg)\n"
        "  --from-db            Use compute_workload.memory_chart_extdata as the layout\n"
        "  -w, --workload <id>  Initial workload id\n"
        "  -k, --kernel <uuid>  Initial kernel uuid\n"
        "  -h, --help           Show this help\n\n"
        "In-app: choose workload/kernel from the toolbar; press R to re-read the\n"
        "JSON file after editing it.\n",
        exe, exe);
}

void
GlfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

}  // namespace

int
main(int argc, char** argv)
{
    AppState app;

    for(int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto        next = [&](const char* name) -> std::string {
            if(i + 1 >= argc)
            {
                std::fprintf(stderr, "Missing value for %s\n", name);
                return "";
            }
            return argv[++i];
        };

        if(a == "-h" || a == "--help")
        {
            PrintUsage(argv[0]);
            return 0;
        }
        else if(a == "--json")
            app.layout_path = next("--json");
        else if(a == "--db")
            app.db_path = next("--db");
        else if(a == "--from-db")
            app.use_db_layout = true;
        else if(a == "-w" || a == "--workload")
        {
            app.workload_arg      = (uint32_t) std::stoul(next("--workload"));
            app.have_workload_arg = true;
        }
        else if(a == "-k" || a == "--kernel")
        {
            app.kernel_arg      = (int64_t) std::stoll(next("--kernel"));
            app.have_kernel_arg = true;
        }
        else if(EndsWith(a, ".json"))
            app.layout_path = a;
        else if(EndsWith(a, ".db") || EndsWith(a, ".sqlite") || EndsWith(a, ".rocpd"))
            app.db_path = a;
        else
            std::fprintf(stderr, "Ignoring unrecognized argument: %s\n", a.c_str());
    }

    if(app.layout_path.empty() && app.db_path.empty())
    {
        PrintUsage(argv[0]);
        std::fprintf(stderr, "\nNothing to show: provide a layout .json and/or a trace .db.\n");
        // Still open the window so the error is visible; continue.
    }

    // Open the trace DB (optional).
    if(!app.db_path.empty())
    {
        std::string error;
        if(app.store.Open(app.db_path, &error))
        {
            const std::vector<mcv::WorkloadRow>& workloads = app.store.Workloads();
            app.workload_idx = workloads.empty() ? -1 : 0;
            if(app.have_workload_arg)
            {
                for(size_t i = 0; i < workloads.size(); ++i)
                {
                    if(workloads[i].id == app.workload_arg)
                    {
                        app.workload_idx = (int) i;
                        break;
                    }
                }
            }
        }
        else
        {
            std::fprintf(stderr, "Warning: %s\n", error.c_str());
        }
    }

    ReloadAll(app);

    // --- Window + ImGui bootstrap (GLFW + OpenGL3) ---
    glfwSetErrorCallback(GlfwErrorCallback);
    if(!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    const char* glsl_version = "#version 130";
    GLFWwindow* window =
        glfwCreateWindow(1500, 950, "ROCm Optiq - Memory Chart Viewer", nullptr, nullptr);
    if(!window)
    {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // Don't litter an imgui.ini next to the tool.
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    const ImVec4 clear_color(0.06f, 0.07f, 0.09f, 1.0f);

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::Begin("MemoryChartViewer", nullptr, flags);

        DrawToolbar(app);
        ImGui::Separator();

        if(app.renderer.HasLayout())
        {
            app.renderer.Render();
        }
        else
        {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", app.layout_status.c_str());
        }

        ImGui::End();

        ImGui::Render();
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
