#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_trace.h"
#include "rocprofvis_home_screen.h"

namespace RocProfVis
{
namespace View
{

class AppWindow : public RocWidget
{
public:
    static AppWindow* getInstance();

    ~AppWindow();

    bool Init();
    virtual void Render();

private:
    AppWindow();
    static AppWindow* m_instance;

    rocprofvis_trace_data_t trace_object;
    std::shared_ptr<HomeScreen> home_screen; 
    std::shared_ptr<RocWidget> main_view;
    bool is_loading_trace;
    bool data_changed;
};

}  // namespace View
}  // namespace RocProfVis