cmake_minimum_required(VERSION 3.12)
project(rocprof-visual-data-model-test VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_BUILD_TYPE Release)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")


file(GLOB MAIN_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB DATA_SRC CONFIGURE_DEPENDS "src/data/*.cpp")
file(GLOB DATABASE_SRC CONFIGURE_DEPENDS "src/database/*.cpp")
file(GLOB SQLITE_SRC CONFIGURE_DEPENDS "src/database/sqlite/*.cpp")
file(GLOB SQLITE3_SRC CONFIGURE_DEPENDS "src/database/sqlite/*.c")
file(GLOB ROCPD_SRC CONFIGURE_DEPENDS "src/database/sqlite/rocpd/*.cpp")
file(GLOB OATPP_SRC CONFIGURE_DEPENDS "src/oat/*.cpp")

add_executable(${PROJECT_NAME} ${MAIN_SRC} ${OATPP_SRC} )
add_library(rocprof-visual-data-model-lib ${DATA_SRC} ${DATABASE_SRC} ${SQLITE_SRC} ${SQLITE3_SRC} ${ROCPD_SRC})
find_package(oatpp  1.4.0 REQUIRED)
target_link_libraries(
        ${PROJECT_NAME}
        oatpp::oatpp
        oatpp::oatpp-test
        rocprof-visual-data-model-lib
)