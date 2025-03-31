include(CMakePrintHelpers)
cmake_minimum_required(VERSION 3.12)
project(rocprofvis-datamodel VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_custom_command(OUTPUT ../src/database/sqlite3/sqlite3.c
   COMMAND cmake -E tar xzf sqlite3.zip WORKING_DIRECTORY ../src/database/sqlite3/
   COMMENT "Unpacking sqlite3.zip" VERBATIM)
set_source_files_properties(../src/database/sqlite3/sqlite3.c PROPERTIES GENERATED TRUE)

include_directories(inc src/common src/database src/database/sqlite3 src/datamodel )

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_BUILD_TYPE Release)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPBUILD")
if (UNIX)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif(UNIX)
if(TEST)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEST -O2 -DDEBUG")
if (WIN32)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Zi")
endif(WIN32)
if (UNIX)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif(UNIX)
endif(TEST)
cmake_print_variables(CMAKE_CXX_FLAGS)
cmake_print_variables(CMAKE_C_FLAGS)


file(GLOB COMMON_SRC CONFIGURE_DEPENDS "src/common/*.cpp")
file(GLOB DATAMODEL_SRC CONFIGURE_DEPENDS "src/datamodel/*.cpp")
file(GLOB DATABASE_SRC CONFIGURE_DEPENDS "src/database/*.cpp")
set(SQLITE3_SRC "src/database/sqlite3/sqlite3.c")
file(GLOB MAIN_SRC CONFIGURE_DEPENDS "src/test/*.cpp")



add_library(${PROJECT_NAME} ${DATAMODEL_SRC} ${DATABASE_SRC} ${DATABASE_SRC} ${COMMON_SRC} ${SQLITE3_SRC} )
add_executable(${PROJECT_NAME}-test ${MAIN_SRC} )
target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})