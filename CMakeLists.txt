CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
option(EXAMPLE "shall we complie example" on)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

PROJECT(RecordH264)

cmake_host_system_information(RESULT var QUERY OS_PLATFORM)
message("processor platfrom: ${var}")

set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

MESSAGE(STATUS ${CMAKE_SYSTEM_NAME})

add_subdirectory(h264camera)
add_subdirectory(camera)
add_subdirectory(epoll)
add_subdirectory(ringbuf)
add_subdirectory(mjpg)

set(EXEC_NAME Record_H264)

add_executable(${EXEC_NAME} main.cpp
)

target_link_libraries(${EXEC_NAME}
    pthread
    h264camera
)

