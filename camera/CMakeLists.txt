file(GLOB camera_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(camera 
    ${camera_srcs}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/ringbuf
    ${CMAKE_SOURCE_DIR}/epoll
    ${CMAKE_SOURCE_DIR}/x264
)

target_include_directories(camera
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
