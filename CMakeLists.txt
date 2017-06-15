cmake_minimum_required(VERSION 3.0)

option(USE_CAIRO "Use Cairo instead of Pixman" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -fno-rtti")

find_package(PkgConfig)
pkg_check_modules(DYZSHM REQUIRED glib-2.0 wpe-webkit)

if (USE_CAIRO)
	pkg_check_modules(DYZSHM_EXTRA REQUIRED cairo)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_CAIRO=1")
else ()
	pkg_check_modules(DYZSHM_EXTRA REQUIRED pixman-1)
endif ()

add_executable(dyz-shm dyz-shm.cpp)
target_include_directories(dyz-shm PUBLIC
	${DYZSHM_INCLUDE_DIRS}
	${DYZSHM_EXTRA_INCLUDE_DIRS}
)
target_link_libraries(dyz-shm
	${DYZSHM_LIBRARIES}
	${DYZSHM_EXTRA_LIBRARIES}
	-lWPEBackend-shm
)
install(TARGETS dyz-shm DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
