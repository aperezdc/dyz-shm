cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

link_directories(/home/gyuyoung/WPEWebKit-install-2/lib)
find_package(Cairo REQUIRED)
find_package(WPE REQUIRED)
find_package(GLIB REQUIRED)

set(DYZ_SHM_INCLUDE_DIRECTORIES
    "include"
    "/home/gyuyoung/devel/WPEWebKit/WebKitBuild/DependenciesWPE/Root/include"
    ${CAIRO_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

set(DYZ_SHM_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${WPE_LIBRARIES}
    ${GLIB_LIBRARIES}
)

set(DYZ_SHM_SOURCES dyz-shm.cpp)

include_directories(${DYZ_SHM_INCLUDE_DIRECTORIES})
add_executable(dyz-shm ${DYZ_SHM_SOURCES})
target_link_libraries(dyz-shm ${DYZ_SHM_LIBRARIES})

