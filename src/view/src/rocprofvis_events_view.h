// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once
#include "rocprofvis_data_provider.h"
#include <string>
#include <vector>
#include "../src/view/src/widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class EventsView : public RocCustomWidget
{
public:
    EventsView(DataProvider& dp);
    ~EventsView();

    void Render();

private:
    std::vector<std::string> events_;
    DataProvider&            m_data_provider;
};

}  // namespace View
}  // namespace RocProfVis