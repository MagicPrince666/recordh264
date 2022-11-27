# UVC H264 camera record

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
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_h3.cmake ..
```
### 编译全志V831
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_v831.cmake ..
```

### 编译全志f1c100s
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_f1c100s.cmake ..
```
### 编译本机
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_host.cmake ..
```
### 编译MacOs
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_TOOLCHAIN_FILE=cmake/build_for_darwin.cmake ..
```

## Donation
码农不易 尊重劳动

作者：小王子与木头人

功能：uvc录制H264

QQ：846863428

TEL: 15220187476

mail: huangliquanprince@icloud.com

修改时间 ：2018-05-16

![alipay](docs/alipay.jpg)
![wechat](docs/wechat.png)
