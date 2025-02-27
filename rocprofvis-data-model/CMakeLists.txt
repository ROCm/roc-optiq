cmake_minimum_required(VERSION 3.12)
project(rocprofvis-datamodel-lib VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CMAKE_BUILD_TYPE Release)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")


#file(GLOB MAIN_SRC CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB COMMON_SRC CONFIGURE_DEPENDS "src/common/*.cpp")
file(GLOB DATAMODEL_SRC CONFIGURE_DEPENDS "src/datamodel/*.cpp")
file(GLOB DATABASE_SRC CONFIGURE_DEPENDS "src/database/*.cpp")
#file(GLOB SQLITE_SRC CONFIGURE_DEPENDS "src/database/sqlite/*.cpp")
#file(GLOB SQLITE3_SRC CONFIGURE_DEPENDS "src/database/sqlite3/*.c")
#file(GLOB ROCPD_SRC CONFIGURE_DEPENDS "src/database/sqlite/rocpd/*.cpp")

#add_executable(${PROJECT_NAME}-test ${MAIN_SRC})
add_library(${PROJECT_NAME} ${DATAMODEL_SRC} ${DATABASE_SRC})
#add_library(${PROJECT_NAME} ${DATAMODEL_SRC} ${DATABASE_SRC} ${SQLITE_SRC} ${SQLITE3_SRC} ${ROCPD_SRC})
#target_link_libraries(
#        ${PROJECT_NAME}-test 
#        ${PROJECT_NAME}
#)