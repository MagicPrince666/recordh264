CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
option(EXAMPLE "shall we complie example" on)

PROJECT(RecordVideo)

cmake_host_system_information(RESULT var QUERY OS_PLATFORM)
message("-- processor platfrom: ${var}")

set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

MESSAGE(STATUS ${CMAKE_SYSTEM_NAME})

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

set(EXEC_NAME RecordVideo)

add_executable(${EXEC_NAME} ${main_srcs})

target_link_libraries(${EXEC_NAME}
    x264
    pthread
    dl
    asound
    mpg123
    faac
)

