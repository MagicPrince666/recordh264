######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME Linux)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CMAKE_SYSTEM_PROCESSOR arm)

# 包含x264头文件
include_directories(${CMAKE_SOURCE_DIR}/x264/h3)

# x264库路径
link_directories(${CMAKE_SOURCE_DIR}/x264/h3)

# 工具链地址
SET(TOOLCHAIN_DIR  "/home/prince/xos/output/host/bin/")

# sunxi h3
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}arm-neo-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}arm-neo-linux-gnueabihf-g++)
