######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME Linux)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

# 包含x264头文件
#include_directories(${CMAKE_SOURCE_DIR}/x264)

# x264库路径
#link_directories(${CMAKE_SOURCE_DIR}/x264)

# host
SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)