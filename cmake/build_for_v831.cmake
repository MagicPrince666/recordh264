######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME Linux)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CMAKE_SYSTEM_PROCESSOR arm)

# 包含x264头文件
include_directories(${CMAKE_SOURCE_DIR}/x264)

# x264库路径
link_directories(${CMAKE_SOURCE_DIR}/x264)

# 工具链地址
SET(TOOLCHAIN_DIR  "/home/leo/toolchain-sunxi-musl/toolchain/bin/")

# sunxi v831
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}arm-openwrt-linux-muslgnueabi-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}arm-openwrt-linux-muslgnueabi-g++)
