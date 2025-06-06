#include <asm/types.h> /* for videodev2.h */
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* low-level i/o */
#include <linux/videodev2.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/spdlog.h"

#include "nv12_capture.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

Nv12VideoCap::Nv12VideoCap(std::string dev)
    : v4l2_device_(dev)
{
}

Nv12VideoCap::~Nv12VideoCap()
{
    V4l2Close();
}

void Nv12VideoCap::ErrnoExit(const char *s)
{
    spdlog::error("{} error {}, {}", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int32_t Nv12VideoCap::xioctl(int32_t fd, int32_t request, void *arg)
{
    int32_t r = 0;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

bool Nv12VideoCap::OpenCamera()
{
    struct stat st;

    if (-1 == stat(v4l2_device_.c_str(), &st)) {
        spdlog::error("Cannot identify '{}': {}, {}", v4l2_device_, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        spdlog::error("{} is no device", v4l2_device_);
        exit(EXIT_FAILURE);
    }

    camera_.fd = open(v4l2_device_.c_str(), O_RDWR, 0); //  | O_NONBLOCK

    if (camera_.fd <= 0) {
        spdlog::error("Cannot open '{}': {}, {}", v4l2_device_, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return true;
}

bool Nv12VideoCap::CloseCamera()
{
    if (camera_.fd <= 0) {
        spdlog::error("{} fd = {}", __FUNCTION__, camera_.fd);
        return false;
    }
    spdlog::info("{} fd = {}", __FUNCTION__, camera_.fd);
    if (-1 == close(camera_.fd)) {
        return false;
    }

    camera_.fd = -1;
    return true;
}

uint64_t Nv12VideoCap::BuffOneFrame(uint8_t *data)
{
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // this operator below will change buf.index and (0 <= buf.index <= 3)
    if (-1 == ioctl(camera_.fd, VIDIOC_DQBUF, &buf)) {
        spdlog::error("VIDIOC_DQBUF {}", strerror(errno));
        switch (errno) {
        case EAGAIN:
            return 0;
        case EIO:
            /* Could ignore EIO, see spec. */
            /* fall through */
        default:
            ErrnoExit("VIDIOC_DQBUF");
        }
    }

    uint64_t len = buf.bytesused;

    //把一帧数据拷贝到缓冲区
    memcpy(data, (uint8_t *)(camera_.buffers[buf.index].start), buf.bytesused);

    if (-1 == ioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
        ErrnoExit("VIDIOC_QBUF");
    }

    return len;
}

bool Nv12VideoCap::StartPreviewing()
{
    enum v4l2_buf_type type;

    for (uint32_t i = 0; i < n_buffers_; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (-1 == xioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
            ErrnoExit("VIDIOC_QBUF");
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == xioctl(camera_.fd, VIDIOC_STREAMON, &type)) {
        ErrnoExit("VIDIOC_STREAMON");
    }
    return true;
}

bool Nv12VideoCap::StopPreviewing()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(camera_.fd, VIDIOC_STREAMOFF, &type)) {
        ErrnoExit("VIDIOC_STREAMOFF");
    }
    spdlog::info("{} VIDIOC_STREAMOFF", __FUNCTION__);
    return true;
}

bool Nv12VideoCap::UninitCamera()
{
    for (uint32_t i = 0; i < n_buffers_; ++i) {
        if (-1 == munmap(camera_.buffers[i].start, camera_.buffers[i].length)) {
            ErrnoExit("munmap");
        }
        camera_.buffers[i].start = nullptr;
    }

    delete[] camera_.buffers;
    spdlog::info("{} munmap", __FUNCTION__);
    return true;
}

bool Nv12VideoCap::InitMmap()
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    //分配内存
    if (-1 == xioctl(camera_.fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            spdlog::error("{} does not support memory mapping", v4l2_device_);
            exit(EXIT_FAILURE);
        } else {
            ErrnoExit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        spdlog::error("Insufficient buffer memory on {}", v4l2_device_);
        exit(EXIT_FAILURE);
    }

    camera_.buffers = new (std::nothrow) Buffer[req.count];

    if (!camera_.buffers) {
        spdlog::error("Out of memory");
        exit(EXIT_FAILURE);
    }

    for (n_buffers_ = 0; n_buffers_ < req.count; ++n_buffers_) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers_;

        //将VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
        if (-1 == xioctl(camera_.fd, VIDIOC_QUERYBUF, &buf)) {
            ErrnoExit("VIDIOC_QUERYBUF");
        }

        camera_.buffers[n_buffers_].length = buf.length;
        camera_.buffers[n_buffers_].start  = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera_.fd, buf.m.offset);

        if (MAP_FAILED == camera_.buffers[n_buffers_].start) {
            ErrnoExit("mmap");
        }
    }
    return true;
}

int32_t Nv12VideoCap::GetFrameLength()
{
    return camera_.width * camera_.height * 2;
}

struct Nv12Camera *Nv12VideoCap::GetFormat()
{
    return &camera_;
}

bool Nv12VideoCap::EnumV4l2Format()
{
    /* 查询打开的设备是否属于摄像头：设备video不一定是摄像头*/
    int32_t ret = ioctl(camera_.fd, VIDIOC_QUERYCAP, &camera_.v4l2_cap);
    if (-1 == ret) {
        perror("ioctl VIDIOC_QUERYCAP");
        return false;
    }
    if (camera_.v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        /* 如果为摄像头设备则打印摄像头驱动名字 */
        spdlog::info("Driver    Name: {}", (char *)camera_.v4l2_cap.driver);
    } else {
        spdlog::error("open file is not video");
        return false;
    }

    /* 查询摄像头可捕捉的图片类型，VIDIOC_ENUM_FMT: 枚举摄像头帧格式 */
    struct v4l2_fmtdesc fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定需要枚举的类型
    for (uint32_t i = 0;; i++) {            // 有可能摄像头支持的图片格式不止一种
        fmt.index = i;
        ret       = ioctl(camera_.fd, VIDIOC_ENUM_FMT, &fmt);
        if (-1 == ret) { // 获取所有格式完成
            break;
        }

        /* 打印摄像头图片格式 */
        spdlog::info("Format: {}", (char *)fmt.description);

        /* 查询该图像格式所支持的分辨率 */
        struct v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmt.pixelformat;
        for (uint32_t j = 0;; j++) { //　该格式支持分辨率不止一种
            frmsize.index = j;
            ret           = ioctl(camera_.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (-1 == ret) { // 获取所有图片分辨率完成
                break;
            }

            /* 打印图片分辨率 */
            spdlog::info("Framsize: {}x{}", frmsize.discrete.width, frmsize.discrete.height);
        }
    }
    return true;
}

bool Nv12VideoCap::InitCamera()
{
    if (camera_.fd <= 0) {
        spdlog::error("Device = {} fd = {} not init", v4l2_device_, camera_.fd);
        return false;
    }
    struct v4l2_format *fmt = &(camera_.v4l2_fmt);

    CLEAR(*fmt);

    fmt->fmt.pix.width  = camera_.width;
    fmt->fmt.pix.height = camera_.height;
    fmt->type                = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_NV12; // 12  Y/CbCr 4:2:0
    fmt->fmt.pix.field       = V4L2_FIELD_ANY;

    if (-1 == xioctl(camera_.fd, VIDIOC_S_FMT, fmt)) {
        ErrnoExit("VIDIOC_S_FMT");
    }

    if (-1 == xioctl(camera_.fd, VIDIOC_G_FMT, fmt)) {
        ErrnoExit("VIDIOC_G_FMT");
    }

    camera_.width  = fmt->fmt.pix.width;
    camera_.height = fmt->fmt.pix.height;

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(struct v4l2_streamparm));
    parm.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.capturemode              = V4L2_MODE_HIGHQUALITY;
    parm.parm.capture.timeperframe.denominator = camera_.fps; //时间间隔分母
    parm.parm.capture.timeperframe.numerator   = 1;           //分子
    if (-1 == ioctl(camera_.fd, VIDIOC_S_PARM, &parm)) {
        perror("set param");
        exit(EXIT_FAILURE);
    }

    // get message
    if (-1 == xioctl(camera_.fd, VIDIOC_G_PARM, &parm)) {
        ErrnoExit("VIDIOC_G_PARM");
    }

    spdlog::info("Device = {}\t width = {}\t height = {}\t fps = {}",
                 v4l2_device_, fmt->fmt.pix.width, fmt->fmt.pix.height,
                 parm.parm.capture.timeperframe.denominator);

    InitMmap();
    return true;
}

bool Nv12VideoCap::Init()
{
    bool ret = false;
    ret |= OpenCamera();
    ret |= EnumV4l2Format();
    ret |= InitCamera();
    ret |= StartPreviewing();
    return ret;
}

bool Nv12VideoCap::V4l2Close()
{
    bool ret = false;
    ret |= StopPreviewing();
    ret |= UninitCamera();
    ret |= CloseCamera();
    return ret;
}
