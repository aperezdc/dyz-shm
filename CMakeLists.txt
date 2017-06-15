cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(PkgConfig)
pkg_check_modules(DYZSHM REQUIRED glib-2.0 wpe-webkit pixman-1)

add_executable(dyz-shm dyz-shm.cpp)
target_include_directories(dyz-shm PUBLIC ${DYZSHM_INCLUDE_DIRS})
target_link_libraries(dyz-shm ${DYZSHM_LIBRARIES} -lWPEBackend-shm)
install(TARGETS dyz-shm DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
