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

#include "video_capture.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

V4l2VideoCapture::V4l2VideoCapture(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    : v4l2_device_(dev)
{
    camera_.fd     = -1;
    camera_.width  = width;
    camera_.height = height;
    camera_.fps    = fps;
}

V4l2VideoCapture::~V4l2VideoCapture()
{
    V4l2Close();
}

void V4l2VideoCapture::ErrnoExit(const char *s)
{
    printf("%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int32_t V4l2VideoCapture::xioctl(int32_t fd, uint32_t request, void *arg)
{
    int32_t r = 0;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

bool V4l2VideoCapture::OpenCamera()
{
    struct stat st;

    if (-1 == stat(v4l2_device_.c_str(), &st)) {
        printf("Cannot identify '%s': %d, %s\n", v4l2_device_.c_str(), errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        printf("%s is no device\n", v4l2_device_.c_str());
        exit(EXIT_FAILURE);
    }

    camera_.fd = open(v4l2_device_.c_str(), O_RDWR, 0); //  | O_NONBLOCK

    if (camera_.fd <= 0) {
        printf("Cannot open '%s': %d, %s\n", v4l2_device_.c_str(), errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("%s fd = %d\n", __FUNCTION__, camera_.fd);
    return true;
}

bool V4l2VideoCapture::CloseCamera()
{
    if (camera_.fd <= 0) {
        printf("error %s fd = %d\n", __FUNCTION__, camera_.fd);
        return false;
    }
    printf("%s fd = %d\n", __FUNCTION__, camera_.fd);
    if (-1 == close(camera_.fd)) {
        return false;
    }

    camera_.fd = -1;
    return true;
}

uint64_t V4l2VideoCapture::BuffOneFrame(uint8_t *data)
{
    struct v4l2_buffer buf;
    CLEAR(buf);
    // printf("Get data start\n");

    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // this operator below will change buf.index and (0 <= buf.index <= 3)
    if (-1 == ioctl(camera_.fd, VIDIOC_DQBUF, &buf)) { // 这里卡住了
        printf("VIDIOC_DQBUF %s\n", strerror(errno));
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

    // yuyv422ToYuv420p(camera_.width, camera_.height, (uint8_t *)(camera_.buffers[buf.index].start), data);

    uint64_t len = buf.bytesused;

    // 把一帧数据拷贝到缓冲区
    memcpy(data, (uint8_t *)(camera_.buffers[buf.index].start), buf.bytesused);

    if (-1 == ioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
        ErrnoExit("VIDIOC_QBUF");
    }

    return len;
}

bool V4l2VideoCapture::StartPreviewing()
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

bool V4l2VideoCapture::StopPreviewing()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(camera_.fd, VIDIOC_STREAMOFF, &type)) {
        ErrnoExit("VIDIOC_STREAMOFF");
    }
    printf("%s VIDIOC_STREAMOFF\n", __FUNCTION__);
    return true;
}

bool V4l2VideoCapture::UninitCamera()
{
    for (uint32_t i = 0; i < n_buffers_; ++i) {
        if (-1 == munmap(camera_.buffers[i].start, camera_.buffers[i].length)) {
            ErrnoExit("munmap");
        }
        camera_.buffers[i].start = nullptr;
    }

    delete[] camera_.buffers;
    printf("%s munmap\n", __FUNCTION__);
    return true;
}

bool V4l2VideoCapture::InitMmap()
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count  = 4;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    // 分配内存
    if (-1 == xioctl(camera_.fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            printf("%s does not support memory mapping\n", v4l2_device_.c_str());
            exit(EXIT_FAILURE);
        } else {
            ErrnoExit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        printf("Insufficient buffer memory on %s\n", v4l2_device_.c_str());
        exit(EXIT_FAILURE);
    }

    camera_.buffers = new (std::nothrow) Buffer[req.count];

    if (!camera_.buffers) {
        printf("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers_ = 0; n_buffers_ < req.count; ++n_buffers_) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers_;

        // 将VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
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

int32_t V4l2VideoCapture::GetFrameLength()
{
    return camera_.width * camera_.height * 2;
}

struct Camera *V4l2VideoCapture::GetFormat()
{
    return &camera_;
}

bool V4l2VideoCapture::InitCamera()
{
    if (camera_.fd <= 0) {
        printf("Device = %s fd = %d not init\n", v4l2_device_.c_str(), camera_.fd);
        return false;
    }

    EnumV4l2Format();

    struct v4l2_format *fmt = &(camera_.v4l2_fmt);

    CLEAR(*fmt);

    fmt->type           = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt->fmt.pix.width  = camera_.width;
    fmt->fmt.pix.height = camera_.height;
#ifdef USE_NV12_FORMAT
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_NV12; // 12  Y/CbCr 4:2:0
    fmt->fmt.pix.field       = V4L2_FIELD_ANY;
#else
    fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // 16  YUV 4:2:2
    fmt->fmt.pix.field       = V4L2_FIELD_ANY;    // 隔行扫描
    // fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420; // 12  YUV 4:2:0
    // fmt->fmt.pix.field = V4L2_FIELD_ANY;
#endif

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
    parm.parm.capture.timeperframe.denominator = camera_.fps; // 时间间隔分母
    parm.parm.capture.timeperframe.numerator   = 1;           // 分子
    if (-1 == ioctl(camera_.fd, VIDIOC_S_PARM, &parm)) {
        perror("set param:");
        exit(EXIT_FAILURE);
    }

    // get message
    if (-1 == xioctl(camera_.fd, VIDIOC_G_PARM, &parm)) {
        ErrnoExit("VIDIOC_G_PARM");
    }

    printf("Device = %s\t width = %d\t height = %d\t fps = %d\n",
                 v4l2_device_.c_str(), fmt->fmt.pix.width, fmt->fmt.pix.height,
                 parm.parm.capture.timeperframe.denominator);

    InitMmap();
    return true;
}

bool V4l2VideoCapture::EnumV4l2Format()
{
    /* 查询打开的设备是否属于摄像头：设备video不一定是摄像头*/
    int32_t ret = ioctl(camera_.fd, VIDIOC_QUERYCAP, &camera_.v4l2_cap);
    if (-1 == ret) {
        perror("ioctl VIDIOC_QUERYCAP");
        return false;
    }
    if (camera_.v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        /* 如果为摄像头设备则打印摄像头驱动名字 */
        printf("Driver    Name: %s\n", (char *)camera_.v4l2_cap.driver);
    } else {
        perror("open file is not video\n");
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
        printf("Format: %s\n", (char *)fmt.description);

        /* 查询该图像格式所支持的分辨率 */
        struct v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmt.pixelformat;
        for (uint32_t j = 0;; j++) { // 　该格式支持分辨率不止一种
            frmsize.index = j;
            ret           = ioctl(camera_.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (-1 == ret) { // 获取所有图片分辨率完成
                break;
            }

            /* 打印图片分辨率 */
            printf("Framsize: %dx%d\n", frmsize.discrete.width, frmsize.discrete.height);
        }
    }
    return true;
}

void V4l2VideoCapture::yuyv422ToYuv420p(int inWidth, int inHeight, uint8_t *pSrc, uint8_t *pDest)
{
    int i, j;
    // 首先对I420的数据整体布局指定
    uint8_t *u = pDest + (inWidth * inHeight);
    uint8_t *v = u + (inWidth * inHeight) / 4;

    for (i = 0; i < inHeight / 2; i++) {
        /*采取的策略是:在外层循环里面，取两个相邻的行*/
        uint8_t *src_l1 = pSrc + inWidth * 2 * 2 * i; // 因为4:2:2的原因，所以占用内存，相当一个像素占2个字节，2个像素合成4个字节
        uint8_t *src_l2 = src_l1 + inWidth * 2;       // YUY2的偶数行下一行
        uint8_t *y_l1   = pDest + inWidth * 2 * i;    // 偶数行
        uint8_t *y_l2   = y_l1 + inWidth;             // 偶数行的下一行
        for (j = 0; j < inWidth / 2; j++)             // 内层循环
        {
            // two pels in one go//一次合成两个像素
            // 偶数行，取完整像素;Y,U,V;偶数行的下一行，只取Y
            *y_l1++ = src_l1[0]; // Y
            *u++    = src_l1[1]; // U
            *y_l1++ = src_l1[2]; // Y
            *v++    = src_l1[3]; // V
            // 这里只有取Y
            *y_l2++ = src_l2[0];
            *y_l2++ = src_l2[2];
            // YUY2,4个像素为一组
            src_l1 += 4;
            src_l2 += 4;
        }
    }
}

bool V4l2VideoCapture::Init()
{
    bool ret = false;
    ret |= OpenCamera();
    ret |= InitCamera();
    ret |= StartPreviewing();
    if (ret) {
        printf("Carmera init success\n");
    } else {
        printf("Carmera init fail\n");
    }
    return ret;
}

bool V4l2VideoCapture::V4l2Close()
{
    bool ret = false;
    ret |= StopPreviewing();
    ret |= UninitCamera();
    ret |= CloseCamera();
    return ret;
}
