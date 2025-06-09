// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class EventsView
{
public:
    EventsView();
    ~EventsView();

    void Render();

private:
    std::vector<std::string> events_;
};

}  // namespace View
}  // namespace RocProfVis