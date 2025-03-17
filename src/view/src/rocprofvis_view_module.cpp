#include "rocprofvis_view_module.h"
#include "rocprofvis_appwindow.h"

using namespace RocProfVis::View;

bool rocprofvis_view_init() {
    bool result = AppWindow::getInstance()->Init();
    if(!result) {
        //log message
    }
    return result;
}

void rocprofvis_view_render() {
    AppWindow::getInstance()->Render();
}