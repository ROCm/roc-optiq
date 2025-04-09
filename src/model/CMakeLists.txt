include(CMakePrintHelpers)
cmake_minimum_required(VERSION 3.12)
project(datamodel VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/database/sqlite3/sqlite3.c
   COMMAND cmake -E tar xzf sqlite3.zip WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/database/sqlite3/
   COMMENT "Unpacking sqlite3.zip" VERBATIM)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/database/sqlite3/sqlite3.c PROPERTIES GENERATED TRUE)

include_directories(inc ${CMAKE_CURRENT_SOURCE_DIR}/src/common ${CMAKE_CURRENT_SOURCE_DIR}/src/database ${CMAKE_CURRENT_SOURCE_DIR}/src/database/sqlite3 ${CMAKE_CURRENT_SOURCE_DIR}/src/datamodel )

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


file(GLOB COMMON_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp")
file(GLOB DATAMODEL_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/datamodel/*.cpp")
file(GLOB DATABASE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/database/*.cpp")
set(SQLITE3_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/database/sqlite3/sqlite3.c")
file(GLOB MAIN_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/test/*.cpp")



add_library(${PROJECT_NAME} ${DATAMODEL_SRC} ${DATABASE_SRC} ${DATABASE_SRC} ${COMMON_SRC} ${SQLITE3_SRC} )
add_executable(${PROJECT_NAME}-test ${MAIN_SRC} )
target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})

add_subdirectory(../../thirdparty/catch2 thirdparty/catch2)

add_executable(${PROJECT_NAME}-basic-tests src/tests/basic_tests.cpp)
target_link_libraries(${PROJECT_NAME}-basic-tests PRIVATE ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}-basic-tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME ${PROJECT_NAME}-basic-tests COMMAND ${PROJECT_NAME}-basic-tests)
set_tests_properties(${PROJECT_NAME}-basic-tests PROPERTIES RUN_SERIAL ON)
