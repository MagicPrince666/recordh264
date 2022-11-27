file(GLOB mjpg_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(mjpg 
    ${mjpg_srcs}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/ringbuf
    ${CMAKE_SOURCE_DIR}/epoll
)

target_include_directories(mjpg
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
