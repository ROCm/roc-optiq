// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Entry point for the View-layer unit tests. We provide our own main (rather
// than linking Catch2WithMain) to mirror the controller/model test harnesses
// and to leave room for future command-line options.

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

int
main(int argc, char** argv)
{
    return Catch::Session().run(argc, argv);
}
