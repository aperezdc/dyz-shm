cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(Cairo REQUIRED)
find_package(GLIB REQUIRED)
find_package(WPE REQUIRED)

set(HOME_DIR $ENV{HOME})
set(WPE_LIB_DIR ${HOME_DIR}/wpe/lib)
set(WPE_DEPENDENCIES_INCLUDE_DIR ${HOME_DIR}/devel/WPEWebKit/WebKitBuild/DependenciesWPE/Root/include)

set(DYZ_SHM_INCLUDE_DIRECTORIES
    ${WPE_DEPENDENCIES_INCLUDE_DIR}
    ${CAIRO_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

set(DYZ_SHM_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${WPE_LIBRARIES}
)

set(DYZ_SHM_SOURCES dyz-shm.cpp)

link_directories(${WPE_LIB_DIR})
include_directories(${DYZ_SHM_INCLUDE_DIRECTORIES})
add_executable(dyz-shm ${DYZ_SHM_SOURCES})
target_link_libraries(dyz-shm ${DYZ_SHM_LIBRARIES})

