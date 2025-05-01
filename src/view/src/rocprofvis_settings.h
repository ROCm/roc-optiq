// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include <utility>
#include <vector>
namespace RocProfVis
{
namespace View
{
enum class Colors
{
    kMetaDataColor
};
class Settings
{
public:
   static Settings& GetInstance(); 

   Settings(const Settings&) = delete;
   Settings& operator=(const Settings&) = delete;
   ImU32     GetColor(int value);

private:
    Settings();
    ~Settings();
    std::vector<ImU32> color_store;
 };

}  // namespace View
}  // namespace RocProfVis
