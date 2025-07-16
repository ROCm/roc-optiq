include(CMakePrintHelpers)
cmake_minimum_required(VERSION 3.12)
project(sqlite3 VERSION 3.47.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c
   COMMAND cmake -E tar xzf sqlite3.zip WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   COMMENT "Unpacking sqlite3.zip" VERBATIM)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}sqlite3.c PROPERTIES GENERATED TRUE)

foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    STRING (REGEX REPLACE "/RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)

foreach(flag_var
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    STRING (REGEX REPLACE "-RTC[^ ]*" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)

add_library(sqlite3 STATIC
    sqlite3.c
)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(sqlite3 PRIVATE
    -O2          # Optimize for speed
    -DCPPBUILD
)
