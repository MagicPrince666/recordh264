CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
option(EXAMPLE "shall we complie example" on)

PROJECT(RecordVideo LANGUAGES C CXX)

cmake_host_system_information(RESULT var QUERY OS_PLATFORM)
message("-- processor platfrom: ${var}")

set(CMAKE_CXX_STANDARD 11)

option(USE_RK_HW_ENCODER "Use Rockchip hardware encoder" OFF)

MESSAGE(STATUS ${CMAKE_SYSTEM_NAME})

find_package(PkgConfig REQUIRED)

include_directories(
    .
    include
    camera
    epoll
    h264camera
    mjpg
    nv12
    ringbuf
    sound
    mp3
)

file(GLOB main_srcs
    ./*.cpp
    camera/*.cpp
    epoll/*.cpp
    h264camera/*.cpp
    mjpg/*.cpp
    nv12/*.cpp
    ringbuf/*.cpp
    sound/*.cpp
    mp3/*.cpp
    if (USE_RK_HW_ENCODER)
        rk_mpp/*.cpp
        rk_mpp/*.c
    endif ()
)

set(COMMON_LIBRARIES
    x264
    pthread
    dl
    asound
    mpg123
    faac
)

if (USE_RK_HW_ENCODER)
    pkg_search_module(RK_MPP REQUIRED rockchip_mpp)
    if (NOT RK_MPP_FOUND)
        message(FATAL_ERROR "rockchip_mpp is not found")
    endif ()
    pkg_search_module(RGA librga)
    if (NOT RGA_FOUND)
        add_definitions(-DUSE_LIBYUV)
        message("librga is not found, use libyuv instead")
    endif ()
endif ()

if (USE_RK_HW_ENCODER)
    list(APPEND COMMON_LIBRARIES
        ${RK_MPP_LIBRARIES}
        ${RGA_LIBRARIES}
    )
endif ()

if (USE_RK_HW_ENCODER)
    add_definitions(-DUSE_RK_HW_ENCODER)
    list(APPEND COMMON_INCLUDE_DIRS
        ${RK_MPP_INCLUDE_DIRS}
        ${RGA_INCLUDE_DIRS}
    )
    list(APPEND COMMON_LIBRARIES
        ${RGA_LIBRARIES}
        ${RK_MPP_LIBRARIES}
    )
    if (NOT RGA_FOUND)
        list(APPEND COMMON_LIBRARIES
        -lyuv
        )
    endif ()
endif ()

add_executable(${PROJECT_NAME} ${main_srcs})

target_link_libraries(${PROJECT_NAME}
    ${COMMON_LIBRARIES}
)

