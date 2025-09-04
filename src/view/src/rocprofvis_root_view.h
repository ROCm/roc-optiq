#pragma once

#include "widgets/rocprofvis_widget.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

class RootView : public RocWidget
{
public:
    RootView() {};
    virtual ~RootView() {};

    virtual std::shared_ptr<RocWidget> GetToolbar() {return nullptr;};
};

}
}
