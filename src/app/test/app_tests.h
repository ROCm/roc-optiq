// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

struct ImGuiTestEngine;

void RegisterAppTests(ImGuiTestEngine* e);

#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
// Ask Optiq end to end, against an in-process scripted endpoint. Category
// "ask_optiq"; see ask_optiq_tests.cpp.
void RegisterAskOptiqTests(ImGuiTestEngine* e);
#endif

