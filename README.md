# UVC H264 camera record

## 安装faac
```bash
sudo apt install libfaac-dev
```
## 安装mp3解码器
```bash
sudo apt install libmpg123-dev
```
## 代码结构
record video for H264 camera
文件树：
```
.
├── CMakeLists.txt
├── README.md
├── build
├── cmake       # 各种平台的适配
│   ├── build_for_darwin.cmake
│   ├── build_for_f1c100s.cmake
│   ├── build_for_h3.cmake
│   ├── build_for_host.cmake
│   ├── build_for_mipsel.cmake
│   ├── build_for_v831.cmake
│   └── macos_build_for_arm64.cmake
├── epoll       # epoll 观察者模式
│   ├── CMakeLists.txt
│   ├── epoll.cpp
│   └── epoll.h
├── include
│   ├── signal_queue.h
│   └── spdlog
├── main.cpp
├── ringbuf     # 环形缓存区
│   ├── CMakeLists.txt
│   ├── ringbuffer.cpp
│   └── ringbuffer.h
├── sound       # 音频录制和编码
│   ├── recorder.cpp
│   └── recorder.h
└── video       # H264视频的获取
    ├── CMakeLists.txt
    ├── H264_UVC_Cap.cpp
    ├── H264_UVC_Cap.h
    ├── debug.h
    ├── h264_xu_ctrls.cpp
    ├── h264_xu_ctrls.h
    ├── v4l2uvc.cpp
    └── v4l2uvc.h
```

## 编译方式

### 编译全志H3
```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_h3.cmake ..
```
### 编译全志V831
```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_v831.cmake ..
```

### 编译全志f1c100s
```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_f1c100s.cmake ..
```
### 编译本机
```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_host.cmake ..
```
### 编译MacOs
```bash
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_darwin.cmake ..
```

### 编译x264
```bash
$ git clone https://code.videolan.org/videolan/x264.git
$ ./configure --host=arm-linux --disable-asm --prefix=$PWD/install 
```

编辑config.mak
CC=/home/ubuntu/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc \
LD=/home/ubuntu/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc \
AR=/home/ubuntu/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-ar \
RAMLIB=/home/ubuntu/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-ranlib

拷贝文件
```bash
cp libx264.a ../librtsp/v4l2demo/x264/
cp x264.h ../librtsp/v4l2demo/x264/
cp x264_config.h ../librtsp/v4l2demo/x264/
```

### 获取摄像头信息
```bash
sudo apt install v4l-utils
v4l2-ctl --list-devices
v4l2-ctl --all
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
```
### 查看MPP日志
```bash
tail -f /var/log/syslog
```
## Donation
码农不易 尊重劳动

作者：小王子与木头人

功能：uvc录制H264

QQ：846863428

TEL: 15220187476

mail: huangliquanprince@icloud.com

修改时间 ：2023-11-14

![alipay](docs/alipay.jpg)
![wechat](docs/wechat.png)
