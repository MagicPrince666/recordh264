file(GLOB epoll_srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(epoll 
    ${epoll_srcs}
)

target_include_directories(epoll
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
