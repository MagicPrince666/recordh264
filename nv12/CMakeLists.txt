file(GLOB nv12_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(nv12 
    ${nv12_srcs}
)

include_directories(
    ${CMAKE_SOURCE_DIR}/ringbuf
    ${CMAKE_SOURCE_DIR}/epoll
    ${CMAKE_SOURCE_DIR}/x264
)

target_include_directories(nv12
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
