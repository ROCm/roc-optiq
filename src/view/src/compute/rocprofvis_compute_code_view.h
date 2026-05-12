// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_settings_manager.h"

#include <string>

namespace RocProfVis
{
namespace View
{

class ComputeCodeView : public RocWidget
{
public:
    ComputeCodeView();
    ~ComputeCodeView() = default;

    void Render() override;

private:
    SettingsManager&  m_settings;
    const std::string m_test_code = R"(
#include <iostream>

int main()
{
    std::cout << "Hello world\n";
    return 0;
}
)";
};

}  // namespace View
}  // namespace RocProfVis
