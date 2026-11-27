// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace RocProfVis
{
namespace View
{

// The endpoint URL as it may be saved and shown: without userinfo, a fragment,
// or any query parameter but an API version, any of which may be a pasted key.
// In every build, because every build reads and writes the settings file.
std::string AssistantStorableUrl(const std::string& url);

}  // namespace View
}  // namespace RocProfVis
