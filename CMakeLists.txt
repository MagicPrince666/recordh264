CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
option(EXAMPLE "shall we complie example" on)

PROJECT(RecordVideo LANGUAGES C CXX)

cmake_host_system_information(RESULT var QUERY OS_PLATFORM)
message("-- processor platfrom: ${var}")

set(CMAKE_CXX_STANDARD 11)

option(USE_RK_HW_ENCODER "Use Rockchip hardware encoder" OFF)

MESSAGE(STATUS ${CMAKE_SYSTEM_NAME})

set(CMAKE_BUILD_TYPE "Debug")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
add_definitions(-DBACKTRACE_DEBUG)
endif ()

# 执行git命令，并把结果重定向到自定义变量（此处是我的变量<GIT_VERSION>）。
execute_process(
	COMMAND	git log -1 --format=%H 		
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} 
	OUTPUT_VARIABLE GIT_VERSION
)
execute_process(
	COMMAND git remote -v 			
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} 
	OUTPUT_VARIABLE GIT_REMOTE_PATH
)

# 对得到的结果进行处理，尤其注意 \n,\t,\r之类的特殊字符，在cmake时没问题，但是生成的.cmake文件有问题，导致make出错。
string (REGEX REPLACE ".*\t| .*" "" GIT_PATH ${GIT_REMOTE_PATH})
string (REGEX REPLACE "[\n\t\r]" "" GIT_VERSION ${GIT_VERSION})

# 增加编译选项，把宏导入源码
add_definitions( -DGIT_VERSION=\"${GIT_VERSION}\")
add_definitions( -DGIT_PATH=\"${GIT_PATH}\")

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
)

if (USE_RK_HW_ENCODER)
file(GLOB rk_mpp_srcs 
    rk_mpp/*.cpp
    rk_mpp/*.c
)
list(APPEND main_srcs ${rk_mpp_srcs})
endif ()

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

