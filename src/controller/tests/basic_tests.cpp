
// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller.h"
#include "rocprofvis_core.h"
#include <iostream>
#include <cstdio>
#include <random>
#include <vector>
#if defined(_WIN32)
#include <conio.h>
#else
#include <curses.h>
#endif
#include <cstdarg>
#include <algorithm>
#include <string.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <iostream>

std::string g_input_file="../../../sample/trace_70b_1024_32.rpd";
bool        g_all_tracks=false;
bool        g_full_range=false;

int main(int argc, char** argv)
{
  Catch::Session session;

  using namespace Catch::Clara;
  auto cli
    = session.cli()
    | Opt( g_input_file, "input_file" )
         ["--input_file"]
         ("Path to input file")
    | Opt( g_all_tracks, "all_tracks" )
         ["--all_tracks"]
         ("Whether to load/query all tracks or only some")
    | Opt( g_full_range, "full_range" )
         ["--full_range"]
         ("Whether to load/query the full trace range or only a segment");

  // Now pass the new composite back to Catch2 so it uses that
  session.cli( cli );

  // Let Catch2 (using Clara) parse the command line
  int returnCode = session.applyCommandLine( argc, argv );
  if( returnCode != 0 ) // Indicates a command line error
      return returnCode;

  return session.run();
}

TEST_CASE( "Future Initialisation", "[require]" ) {
    
    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    REQUIRE(future != nullptr);
    rocprofvis_controller_future_free(future);
}

struct RocProfVisControllerFixture
{
    mutable rocprofvis_controller_t* m_controller = nullptr;
};

TEST_CASE_PERSISTENT_FIXTURE(RocProfVisControllerFixture, "Tests for the Controller")
{
    SECTION("Create Controller")
    {
        m_controller = rocprofvis_controller_alloc();
        REQUIRE(nullptr != m_controller);
    }

    SECTION("Controller Initialisation")
    {
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        REQUIRE(future != nullptr);

        auto result = rocprofvis_controller_load_async(m_controller, g_input_file.c_str(), future);
        REQUIRE(result == kRocProfVisResultSuccess);

        result = rocprofvis_controller_future_wait(future, FLT_MAX);
        REQUIRE(result == kRocProfVisResultSuccess);

        uint64_t future_result = 0;
        result = rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0, &future_result);
        REQUIRE(result == kRocProfVisResultSuccess);
        REQUIRE(future_result == kRocProfVisResultSuccess);
    }

    SECTION("Delete controller")
    {
        rocprofvis_controller_free(m_controller);
    }
}
