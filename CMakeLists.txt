cmake_minimum_required(VERSION 3.0)

set(ALL_GRAPHICS
	cairo
	pixman
)
set(GRAPHICS "cairo" CACHE STRING "Choose the graphics backend (one of ${ALL_GRAPHICS}")

list(FIND ALL_GRAPHICS ${GRAPHICS} RET)
if (${RET} EQUAL -1)
	message(FATAL_ERROR "Please choose a valid graphics backend (one of ${ALL_GRAPHICS}")
endif ()

string(TOUPPER ${GRAPHICS} GRAPHICS_MACRO_NAME)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGRAPHICS_${GRAPHICS_MACRO_NAME}=1 -std=c++11 -fno-exceptions -fno-rtti")

find_package(PkgConfig)
pkg_check_modules(DYZSHM REQUIRED glib-2.0 wpe-webkit)

if (${GRAPHICS} STREQUAL "cairo")
	pkg_check_modules(DYZSHM_EXTRA REQUIRED cairo)
elseif (${GRAPHICS} STREQUAL "pixman")
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
