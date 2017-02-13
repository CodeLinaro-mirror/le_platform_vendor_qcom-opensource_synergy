cmake_minimum_required(VERSION 2.8.10)

# Default output locations
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
# system libs

find_package(Threads)
if (CMAKE_USE_PTHREADS_INIT)
    list(APPEND CONNX_SYSTEM_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

set(CSR_PLATFORM "LINUX")

if( NOT DEFINED CSR_BUILD_TYPE)
    set(CSR_BUILD_TYPE "release")
endif()

if(${CSR_BUILD_TYPE} STREQUAL "debug")
   set(CMAKE_BUILD_TYPE "Debug")
elseif(${CSR_BUILD_TYPE} STREQUAL "release")
   set(CMAKE_BUILD_TYPE "Release")
elseif(${CSR_BUILD_TYPE} STREQUAL "relwithdebinfo")
   set(CMAKE_BUILD_TYPE "RelWithDebInfo")
elseif(${CSR_BUILD_TYPE} STREQUAL "minsizerel")
   set(CMAKE_BUILD_TYPE "MinSizeRel")
else()
   set(CMAKE_BUILD_TYPE "None")
endif()

set(SRCS
    bas_a2dp.c
    bas_dbus.c
    bas_hfp.c
    bas_main.c
    bas_util.c)

set(HDRS
    bas_a2dp.h
    bas_dbus.h
    bas_hfp.h
    bas_main.h
    bas_util.h)

find_package(PkgConfig)
pkg_check_modules(DBUS dbus-1)
if ( NOT DBUS_FOUND)
    message(FATAL_ERROR "Dbus lib cannot be found")
endif (NOT DBUS_FOUND)

list(APPEND LIBS ${DBUS_LIBRARIES})
include_directories(${DBUS_INCLUDE_DIRS})
link_directories(${DBUS_LIBRARY_DIRS})
list(APPEND LIBS pulse)
list(APPEND LIBS pulsecore-6.0)
list(APPEND LIBS pulse-simple)

pkg_check_modules(ALSA alsa)
if ( NOT ALSA_FOUND)
    message(FATAL_ERROR "ALSA lib cannot be found")
endif (NOT ALSA_FOUND)
list(APPEND LIBS ${ALSA_LIBRARIES})
include_directories(${ALSA_INCLUDE_DIRS})
link_directories(${ALSA_LIBRARY_DIRS})

add_executable(bt_audio_service ${SRCS} ${HDRS})
target_link_libraries(bt_audio_service ${LIBS} ${CONNX_SYSTEM_LIBS})

