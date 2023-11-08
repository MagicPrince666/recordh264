/*****************************************************************************************

 * 文件名  H264_UVC_TestAP.cpp
 * 描述    ：录制H264裸流
 * 平台    ：linux
 * 版本    ：V1.0.0
 * 作者    ：Leo Huang  QQ：846863428
 * 邮箱    ：Leo.huang@junchentech.cn
 * 修改时间  ：2017-06-28

*****************************************************************************************/
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <linux/videodev2.h>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iomanip>
#include <sstream>

#include "H264_UVC_Cap.h"
#include "epoll.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

H264UvcCap::H264UvcCap(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    : VideoStream(dev, width, height, fps)
{
    capturing_     = false;
    rec_fp1_       = nullptr;
    h264_xu_ctrls_ = nullptr;
    video_         = nullptr;
}

H264UvcCap::~H264UvcCap()
{
    if (cat_h264_thread_.joinable()) {
        cat_h264_thread_.join();
    }
    printf("%s close camera\n", __FUNCTION__);

    if (rec_fp1_) {
        fclose(rec_fp1_);
    }

    UninitMmap();

    if (h264_xu_ctrls_) {
        delete h264_xu_ctrls_;
    }

    if (video_) {
        if (video_->fd) {
            close(video_->fd);
        }
        delete video_;
    }
}

int32_t errnoexit(const char *s)
{
    printf("%s error %d, %s\n", s, errno, strerror(errno));
    return -1;
}

int32_t xioctl(int32_t fd, uint32_t request, void *arg)
{
    int32_t r;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

bool H264UvcCap::CreateFile(bool yes)
{
    if (!yes) { // 不创建文件
        return false;
    }

    std::string file = getCurrentTime8() + ".h264";
    rec_fp1_         = fopen(file.c_str(), "wa+");
    if (rec_fp1_) {
        return true;
    }
    printf("Create file %s fail!!\n", file.c_str());
    return false;
}

bool H264UvcCap::OpenDevice()
{
    printf("Open device %s\n", dev_name_.c_str());
    struct stat st;

    if (-1 == stat(dev_name_.c_str(), &st)) {
        printf("Cannot identify '%s': %d, %s\n", dev_name_.c_str(), errno, strerror(errno));
        return false;
    }

    if (!S_ISCHR(st.st_mode)) {
        printf("%s is not a device\n", dev_name_.c_str());
        return false;
    }

    video_     = new (std::nothrow) vdIn;
    video_->fd = open(dev_name_.c_str(), O_RDWR);

    if (-1 == video_->fd) {
        printf("Cannot open '%s': %d, %s\n", dev_name_.c_str(), errno, strerror(errno));
        return false;
    }
    return true;
}

int32_t H264UvcCap::InitMmap(void)
{
    printf("Init mmap\n");
    struct v4l2_requestbuffers req;

    CLEAR(req);
    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(video_->fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            printf("%s does not support memory mapping\n", dev_name_.c_str());
            return -1;
        } else {
            return errnoexit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        printf("Insufficient buffer memory on %s\n", dev_name_.c_str());
        return -1;
    }

    video_->buffers = new (std::nothrow) buffer[req.count];

    if (!video_->buffers) {
        printf("Out of memory\n");
        return -1;
    }

    for (video_->n_buffers = 0; video_->n_buffers < req.count; ++video_->n_buffers) {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = video_->n_buffers;

        if (-1 == xioctl(video_->fd, VIDIOC_QUERYBUF, &buf)) {
            return errnoexit("VIDIOC_QUERYBUF");
        }

        video_->buffers[video_->n_buffers].length = buf.length;
        video_->buffers[video_->n_buffers].start =
            mmap(NULL,
                 buf.length,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 video_->fd, buf.m.offset);

        if (MAP_FAILED == video_->buffers[video_->n_buffers].start) {
            return errnoexit("mmap");
        }
    }

    return 0;
}

bool H264UvcCap::UninitMmap()
{
    if (!video_->buffers) {
        return false;
    }

    for (uint32_t i = 0; i < video_->n_buffers; ++i) {
        if (-1 == munmap(video_->buffers[i].start, video_->buffers[i].length)) {
            errnoexit("munmap");
        }
        video_->buffers[i].start = nullptr;
    }

    return true;
}

int32_t H264UvcCap::InitDevice(int32_t width, int32_t height, int32_t format)
{
    printf("%s width = %d height = %d format = %d\n", __FUNCTION__, width, height, format);
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    uint32_t min;

    if (-1 == xioctl(video_->fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            printf("%s is not a V4L2 device\n", dev_name_.c_str());
            return -1;
        } else {
            return errnoexit("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("%s is no video capture device\n", dev_name_.c_str());
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("%s does not support streaming i/o\n", dev_name_.c_str());
        return -1;
    }

    video_->cap = cap;

    CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(video_->fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c    = cropcap.defrect;

        if (-1 == xioctl(video_->fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
            case EINVAL:
                break;
            default:
                break;
            }
        }
    }

    CLEAR(fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if (-1 == xioctl(video_->fd, VIDIOC_S_FMT, &fmt)) {
        return errnoexit("VIDIOC_S_FMT");
    }

    min = fmt.fmt.pix.width * 2;

    if (fmt.fmt.pix.bytesperline < min) {
        fmt.fmt.pix.bytesperline = min;
    }
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min) {
        fmt.fmt.pix.sizeimage = min;
    }

    video_->fmt = fmt;

    if (-1 == xioctl(video_->fd, VIDIOC_G_FMT, &fmt)) {
        return errnoexit("VIDIOC_G_FMT");
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_H264) {
        printf("Not a H264 Camera!!\n");
        return -1;
    }

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof parm);
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(video_->fd, VIDIOC_G_PARM, &parm);
    parm.parm.capture.timeperframe.numerator   = 1;
    parm.parm.capture.timeperframe.denominator = 30;
    ioctl(video_->fd, VIDIOC_S_PARM, &parm);

    return InitMmap();
}

int32_t H264UvcCap::StartPreviewing()
{
    printf("Start Previewing\n");
    enum v4l2_buf_type type;

    for (uint32_t i = 0; i < video_->n_buffers; ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (-1 == xioctl(video_->fd, VIDIOC_QBUF, &buf)) {
            return errnoexit("VIDIOC_QBUF");
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(video_->fd, VIDIOC_STREAMON, &type)) {
        return errnoexit("VIDIOC_STREAMON");
    }

    return 0;
}

bool H264UvcCap::StopPreviewing()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(video_->fd, VIDIOC_STREAMOFF, &type)) {
        errnoexit("VIDIOC_STREAMOFF");
    }
    return true;
}

void H264UvcCap::Init(void)
{
    int32_t format = V4L2_PIX_FMT_H264;

    if (!OpenDevice()) {
        return;
    }

    for (uint32_t i = 0; i < 12; i++) { // 自行搜索设备
        if (InitDevice(video_width_, video_height_, format) < 0) {
            dev_name_ = "/dev/video" + std::to_string(i);
            if (video_->fd) {
                close(video_->fd);
            }
        } else {
            break;
        }
        if (!OpenDevice()) {
            return;
        }
    }

    StartPreviewing();

    h264_xu_ctrls_ = new H264XuCtrls(video_->fd);

    struct tm *tdate;
    time_t curdate;
    tdate = localtime(&curdate);
    h264_xu_ctrls_->XuOsdSetCarcamCtrl(0, 0, 0);
    if (h264_xu_ctrls_->XuOsdSetRTC(tdate->tm_year + 1900, tdate->tm_mon + 1, tdate->tm_mday, tdate->tm_hour, tdate->tm_min, tdate->tm_sec) < 0) {
        printf("XU_OSD_Set_RTC_fd = %d Failed\n", video_->fd);
    }
    if (h264_xu_ctrls_->XuOsdSetEnable(1, 1) < 0) {
        printf("XU_OSD_Set_Enable_fd = %d Failed\n", video_->fd);
    }

    int32_t ret = h264_xu_ctrls_->XuInitCtrl();
    if (ret < 0) {
        printf("XuH264SetBitRate Failed\n");
    } else {
        double m_BitRate = 2048 * 1024;
        // 设置码率
        if (h264_xu_ctrls_->XuH264SetBitRate(m_BitRate) < 0) {
            printf("XuH264SetBitRate %f Failed\n", m_BitRate);
        }

        h264_xu_ctrls_->XuH264GetBitRate(&m_BitRate);
        if (m_BitRate < 0) {
            printf("XuH264GetBitRate %f Failed\n", m_BitRate);
        }
    }

    CreateFile(false);

    // cat_h264_thread_ = std::thread([](H264UvcCap *p_this) { p_this->VideoCapThread(); }, this);
    capturing_ = true;

    printf("-----Init H264 Camera %s-----\n", dev_name_.c_str());
}

int64_t H264UvcCap::CapVideo()
{
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    int32_t ret = ioctl(video_->fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        printf("Unable to dequeue buffer!\n");
        return -1;
    }

    if (rec_fp1_) {
        fwrite(video_->buffers[buf.index].start, buf.bytesused, 1, rec_fp1_);
    }

    // printf("Get buffer size = %d\n", buf.bytesused);

    ret = ioctl(video_->fd, VIDIOC_QBUF, &buf);

    if (ret < 0) {
        printf("Unable to requeue buffer\n");
        return -1;
    }

    return buf.bytesused;
}

int32_t H264UvcCap::BitRateSetting(int32_t rate)
{
    int32_t ret = -1;
    printf("write to the setting\n");
    if (!capturing_) {        // 未有客户端接入
        if (video_->fd > 0) { // 未初始化不能访问
            ret = h264_xu_ctrls_->XuInitCtrl();
        }
        if (ret < 0) {
            printf("XuH264SetBitRate Failed\n");
        } else {
            double m_BitRate = (double)rate;

            if (h264_xu_ctrls_->XuH264SetBitRate(m_BitRate) < 0) {
                printf("XuH264SetBitRate Failed\n");
            }

            h264_xu_ctrls_->XuH264GetBitRate(&m_BitRate);
            if (m_BitRate < 0) {
                printf("XuH264GetBitRate Failed\n");
            }

            printf("----m_BitRate:%f----\n", m_BitRate);
        }
    } else {
        printf("camera no init\n");
        return -1;
    }
    return ret;
}

int32_t H264UvcCap::getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes)
{
    if (!capturing_) {
        printf("V4l2H264hData::getData capturing_ = false\n");
        return 0;
    }

    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    int32_t ret = ioctl(video_->fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        printf("Unable to dequeue buffer!\n");
        return -1;
    }

    unsigned len = buf.bytesused;

    // 拷贝视频到live555缓存
    if (len < fMaxSize) {
        memcpy(fTo, video_->buffers[buf.index].start, len);
        fFrameSize         = len;
        fNumTruncatedBytes = 0;
    } else {
        memcpy(fTo, video_->buffers[buf.index].start, fMaxSize);
        fNumTruncatedBytes = len - fMaxSize;
        fFrameSize         = fMaxSize;
    }

    ret = ioctl(video_->fd, VIDIOC_QBUF, &buf);

    if (ret < 0) {
        printf("Unable to requeue buffer\n");
        return -1;
    }

    return len;
}

void H264UvcCap::StartCap()
{
    if (!capturing_) {
        MY_EPOLL.EpollAddRead(video_->fd, std::bind(&H264UvcCap::CapVideo, this));
    }
    capturing_ = true;
}

void H264UvcCap::StopCap()
{
    if (capturing_) {
        MY_EPOLL.EpollDel(video_->fd);
    }
    capturing_ = false;
    printf("H264UvcCap StopCap\n");
}

void H264UvcCap::VideoCapThread()
{
    printf("%s start h264 captrue\n", __FUNCTION__);
    StartCap();
    while (true) {
        if (!capturing_) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

std::string H264UvcCap::getCurrentTime8()
{
    std::time_t result = std::time(nullptr) + 8 * 3600;
    auto sec           = std::chrono::seconds(result);
    std::chrono::time_point<std::chrono::system_clock> now(sec);
    auto timet     = std::chrono::system_clock::to_time_t(now);
    auto localTime = *std::gmtime(&timet);

    std::stringstream ss;
    std::string str;
    ss << std::put_time(&localTime, "%Y_%m_%d_%H_%M_%S");
    ss >> str;

    return str;
}
