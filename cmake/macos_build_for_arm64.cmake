######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME Linux)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CMAKE_SYSTEM_PROCESSOR aarch64)
# 工具链地址
SET(TOOLCHAIN_DIR  "/home/unix/host/bin/")

# imx8mmini
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}aarch64-openwrt-linux-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}aarch64-openwrt-linux-g++)
