# UVC H264 camera record

# record264
record video for H264 camera
<br>文件夹：
<br>include/  包含源码编译所需要的alsa库头文件以及faac库的头文件
<br>lib/  linux系统的X86库文件包含alsa-lib动态库以及libfaac静态库
<br>lib_arm/  linux系统的am335x库文件包含alsa-lib动态库以及libfaac静态库 
<br>lib_mips/  openwrt系统的mipsel库文件包含alsa-lib动态库以及libfaac静态库 编译工具链是openwrt导出工具链
<br>sound/  音频获取与编码
<br>video/  H264视频的获取

<br>编译方式：
<br>在linux平台下编译，使用make CROSS_COMPILE=XXX指定交叉工具链，或者不指定默认编译X86平台

<br>
<br>码农不易 尊重劳动
<br>作者：小王子与木头人
<br>功能：uvc录制H264
<br>QQ：846863428 
<br>TEL: 15220187476 
<br>mail: huangliquanprince@icloud.com 
<br>修改时间 ：2018-05-16 