######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME Linux)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

SET(CMAKE_SYSTEM_PROCESSOR mipsel)
# 工具链地址
SET(TOOLCHAIN_DIR  "/Volumes/unix/openwrt/staging_dir/toolchain-mipsel_24kc_gcc-11.2.0_musl/bin/")

# imx8mmini
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}mipsel-openwrt-linux-musl-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}mipsel-openwrt-linux-musl-g++)
