######## cross compile env define ###################
SET(CMAKE_SYSTEM_NAME darwin)
# 配置库的安装路径
SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)

# execute_process(COMMAND 
#     export PKG_CONFIG_PATH="/usr/local/opt/openssl@3/lib/pkgconfig"
#     export PKG_CONFIG_PATH="/opt/homebrew/opt/openssl@3/lib/pkgconfig"
# )

# find_package(OpenSSL REQUIRED)
# if (OPENSSL_FOUND)
#     include_directories(${OPENSSL_INCLUDE_DIR})
#     link_directories(${OPENSSL_LIBRARIES})
#     message(STATUS "OpenSSL Found: ${OPENSSL_VERSION}")
#     message(STATUS "OpenSSL Include: ${OPENSSL_INCLUDE_DIR}")
#     message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
# endif()

# x86_64
SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)