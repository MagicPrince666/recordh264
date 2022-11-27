file(GLOB ringbuf_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(ringbuf 
    ${ringbuf_srcs}
)

target_include_directories(ringbuf
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
