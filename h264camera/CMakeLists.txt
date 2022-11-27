file(GLOB h264camera_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(h264camera 
    ${h264camera_srcs}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/ringbuf
    ${CMAKE_SOURCE_DIR}/epoll
)

target_include_directories(h264camera
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
