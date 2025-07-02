#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h> /* low-level i/o */
#include <fstream>
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
#include "xepoll.h"

#if defined(__linux__)
#include <linux/videodev2.h>
#include <asm/types.h> /* for videodev2.h */

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define FMT_NUM_PLANES 1

V4l2VideoCapture::V4l2VideoCapture(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    : v4l2_device_(dev), can_ouput_mjpg_(false), can_ouput_stream_(false)
{
    n_buffers_     = 0;
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
    std::cerr << s << " error " << strerror(errno) << std::endl;
    MY_EPOLL.EpoolQuit();
    exit(EXIT_FAILURE);
}

int32_t V4l2VideoCapture::xioctl(int32_t fd, int32_t request, void *arg)
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
        std::cerr << "Cannot identify " << v4l2_device_ << " " <<  strerror(errno) << std::endl;
        return false;
    }

    if (!S_ISCHR(st.st_mode)) {
        std::cerr << v4l2_device_ << " is no device!!" << std::endl;
        return false;
    }

    camera_.fd = open(v4l2_device_.c_str(), O_RDWR | O_CLOEXEC); //  | O_NONBLOCK

    if (camera_.fd <= 0) {
        std::cerr << "Cannot open " << v4l2_device_ << " " <<  strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool V4l2VideoCapture::CloseCamera()
{
    if (camera_.fd < 0) {
        return false;
    }

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

    buf.type   = camera_.v4l2_fmt.type;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = 0;

    struct v4l2_plane planes[FMT_NUM_PLANES];
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == camera_.v4l2_fmt.type) {
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    // this operator below will change buf.index and (0 <= buf.index <= 3)
    if (-1 == ioctl(camera_.fd, VIDIOC_DQBUF, &buf)) { // 这里卡住了
        std::cerr << "VIDIOC_DQBUF error " <<  strerror(errno) << std::endl;
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

    uint64_t len = 0;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == camera_.v4l2_fmt.type) {
        len = buf.m.planes[0].bytesused;
    } else {
        len = buf.bytesused;
    }

    // 把一帧数据拷贝到缓冲区
    memcpy(data, (uint8_t *)(camera_.buffers[buf.index].start), len);

    if (-1 == ioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
        std::cerr << "VIDIOC_QBUF error " <<  strerror(errno) << std::endl;
    }

    return len;
}

void V4l2VideoCapture::ReadBuffOneFrame()
{
    struct v4l2_buffer buf;
    CLEAR(buf);

    buf.type   = camera_.v4l2_fmt.type;
    buf.memory = V4L2_MEMORY_MMAP;

    struct v4l2_plane planes[FMT_NUM_PLANES];
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == camera_.v4l2_fmt.type) {
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    // this operator below will change buf.index and (0 <= buf.index <= 3)
    if (-1 == ioctl(camera_.fd, VIDIOC_DQBUF, &buf)) {
        std::cerr << "VIDIOC_DQBUF error " <<  strerror(errno) << std::endl;
        switch (errno) {
        case EAGAIN:
            return;
        case EIO:
            /* Could ignore EIO, see spec. */
            /* fall through */
        default:
            ErrnoExit("VIDIOC_DQBUF");
        }
    }

    // yuyv422ToYuv420p(camera_.width, camera_.height, (uint8_t *)(camera_.buffers[buf.index].start), data);

    uint64_t len = 0;
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == camera_.v4l2_fmt.type) {
        len = buf.m.planes[0].bytesused;
    } else {
        len = buf.bytesused;
    }

    // 把一帧数据拷贝到缓冲区
    if (process_image_) {
        process_image_((uint8_t *)(camera_.buffers[buf.index].start), len);
    }

    if (-1 == ioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
        std::cerr << "VIDIOC_QBUF error " << strerror(errno) << std::endl;
    }
}

bool V4l2VideoCapture::StartPreviewing()
{
    enum v4l2_buf_type type = (v4l2_buf_type)camera_.v4l2_fmt.type;

    for (uint32_t i = 0; i < n_buffers_; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        CLEAR(buf);
        CLEAR(planes);

        buf.type   = camera_.v4l2_fmt.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (-1 == xioctl(camera_.fd, VIDIOC_QBUF, &buf)) {
            std::cerr << "VIDIOC_QBUF error " << strerror(errno) << std::endl;
            return false;
        }
    }

    if (-1 == xioctl(camera_.fd, VIDIOC_STREAMON, &type)) {
        std::cerr << "VIDIOC_STREAMON error " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool V4l2VideoCapture::StopPreviewing()
{
    if (camera_.fd <= 0) {
        return false;
    }
    enum v4l2_buf_type type = (v4l2_buf_type)camera_.v4l2_fmt.type;
    if (-1 == xioctl(camera_.fd, VIDIOC_STREAMOFF, &type)) {
        std::cerr << "VIDIOC_STREAMOFF error " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool V4l2VideoCapture::UninitCamera()
{
    if (camera_.fd < 0) {
        return false;
    }
    for (uint32_t i = 0; i < n_buffers_; ++i) {
        if (-1 == munmap(camera_.buffers[i].start, camera_.buffers[i].length)) {
            std::cerr << "munmap error " << strerror(errno) << std::endl;
            return false;
        }
        camera_.buffers[i].start = nullptr;
    }

    delete[] camera_.buffers;
    return true;
}

bool V4l2VideoCapture::InitMmap()
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    if (can_ouput_mjpg_) {
        req.count = 1;
    } else {
        req.count = 6;
    }
    req.type   = camera_.v4l2_fmt.type;
    req.memory = V4L2_MEMORY_MMAP;

    // 分配内存
    if (-1 == xioctl(camera_.fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            std::cerr << v4l2_device_ << " does not support memory mapping" << std::endl;
            return false;
        }
    }

    camera_.buffers = new (std::nothrow) Buffer[req.count];

    if (!camera_.buffers) {
        std::cerr << "Out of memory" << std::endl;
        return false;
    }

    for (n_buffers_ = 0; n_buffers_ < req.count; ++n_buffers_) {
        struct v4l2_buffer buf;
        CLEAR(buf);

        buf.type   = camera_.v4l2_fmt.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = n_buffers_;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        // 将VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
        if (-1 == xioctl(camera_.fd, VIDIOC_QUERYBUF, &buf)) {
            return false;
        }

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type) {
            camera_.buffers[n_buffers_].length = buf.m.planes[0].length;
            camera_.buffers[n_buffers_].start  = mmap(NULL, 
                buf.m.planes[0].length, 
                PROT_READ | PROT_WRITE, 
                MAP_SHARED, 
                camera_.fd, 
                buf.m.planes[0].m.mem_offset);
        } else {
            camera_.buffers[n_buffers_].length = buf.length;
            camera_.buffers[n_buffers_].start  = mmap(NULL, 
                buf.length, 
                PROT_READ | PROT_WRITE, 
                MAP_SHARED, 
                camera_.fd, 
                buf.m.offset);
        }

        if (MAP_FAILED == camera_.buffers[n_buffers_].start) {
            return false;
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
        std::cerr << "Device = " << v4l2_device_ << " fd = " << camera_.fd << " not init" << std::endl;
        return false;
    }

    // EnumV4l2Format();
    if (-1 == xioctl(camera_.fd, VIDIOC_QUERYCAP, &camera_.v4l2_cap)) {
        std::cerr << "VIDIOC_QUERYCAP error " << strerror(errno) << std::endl;
        return false;
    }

    struct v4l2_format *fmt = &(camera_.v4l2_fmt);

    CLEAR(*fmt);

    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (camera_.v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }

    fmt->fmt.pix.width       = camera_.width;
    fmt->fmt.pix.height      = camera_.height;
    fmt->fmt.pix.pixelformat = pixelformat_;   // 16  YUV 4:2:2
    // fmt->fmt.pix.field       = V4L2_FIELD_ANY; // 隔行扫描

    if (-1 == xioctl(camera_.fd, VIDIOC_S_FMT, fmt)) {
        std::cerr << "VIDIOC_S_FMT error " << strerror(errno) << std::endl;
        return false;
    }

    if (-1 == xioctl(camera_.fd, VIDIOC_G_FMT, fmt)) {
        std::cerr << "VIDIOC_G_FMT error " << strerror(errno) << std::endl;
        return false;
    }

    camera_.width  = fmt->fmt.pix.width;
    camera_.height = fmt->fmt.pix.height;

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(struct v4l2_streamparm));
    if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        parm.type                     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
        parm.parm.capture.timeperframe.denominator = camera_.fps; // 时间间隔分母
        parm.parm.capture.timeperframe.numerator   = 1;           // 分子

        if (-1 == xioctl(camera_.fd, VIDIOC_S_PARM, &parm)) {
            std::cerr << "VIDIOC_S_PARM error " << strerror(errno) << std::endl;
            return false;
        }

        // get message
        if (-1 == xioctl(camera_.fd, VIDIOC_G_PARM, &parm)) {
            std::cerr << "VIDIOC_G_PARM error " << strerror(errno) << std::endl;
            return false;
        }
    }

    printf("Device = %s\t %s\t %dx%d\t fps = %d\n",
                 v4l2_device_.c_str(), V4l2FormatToString(fmt->fmt.pix.pixelformat),
                 fmt->fmt.pix.width, fmt->fmt.pix.height,
                 parm.parm.capture.timeperframe.denominator);

    InitMmap();
    return true;
}

const char *V4l2VideoCapture::V4l2FormatToString(uint32_t pixelformat)
{
    // 静态缓冲区避免多次调用冲突
    static char str[5];

    // 小端系统处理：低字节在前
    str[0] = (char)(pixelformat & 0xFF);
    str[1] = (char)((pixelformat >> 8) & 0xFF);
    str[2] = (char)((pixelformat >> 16) & 0xFF);
    str[3] = (char)((pixelformat >> 24) & 0xFF);
    str[4] = '\0'; // 字符串终止符

    // 检查是否可打印字符
    for (int i = 0; i < 4; i++) {
        if (str[i] < 32 || str[i] > 126) {
            str[i] = '?'; // 替换不可打印字符
        }
    }

    return str;
}

bool V4l2VideoCapture::EnumV4l2Format()
{
    /* 查询打开的设备是否属于摄像头：设备video不一定是摄像头*/
    int32_t ret = ioctl(camera_.fd, VIDIOC_QUERYCAP, &camera_.v4l2_cap);
    if (-1 == ret) {
        printf("%s ioctl VIDIOC_QUERYCAP\n", v4l2_device_.c_str());
        return false;
    }


    if (camera_.v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        std::string card = (char *)(camera_.v4l2_cap.card);
        printf("Device %s %s %08X support Multiplanar\n", v4l2_device_.c_str(), card.c_str(), camera_.v4l2_cap.capabilities);
        /* 查询摄像头可捕捉的图片类型，VIDIOC_ENUM_FMT: 枚举摄像头帧格式 */
        struct v4l2_fmtdesc fmt;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; // 指定需要枚举的类型
        for (uint32_t i = 0;; i++) {                   // 有可能摄像头支持的图片格式不止一种
            fmt.index = i;
            ret       = ioctl(camera_.fd, VIDIOC_ENUM_FMT, &fmt);
            if (-1 == ret) { // 获取所有格式完成
                break;
            }

            /* 打印摄像头图片格式 */
            std::string format = (char *)fmt.description;
            printf("Format: [%d] \'%s\' %s\n", i, V4l2FormatToString(fmt.pixelformat), format.c_str());

            /* 查询该图像格式所支持的分辨率 */
            struct v4l2_frmsizeenum frmsize;
            frmsize.pixel_format = fmt.pixelformat;
            for (uint32_t j = 0;; j++) { // 　该格式支持分辨率不止一种
                frmsize.index = j;
                ret           = ioctl(camera_.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
                if (-1 == ret) { // 获取所有图片分辨率完成
                    break;
                }
                can_ouput_stream_ = true;
                /* 打印图片分辨率 */
                printf("\t\tFramsize: %dx%d - %dx%d\n", frmsize.stepwise.min_width, frmsize.stepwise.min_height, frmsize.stepwise.max_width, frmsize.stepwise.max_height);
            }
        }
        if (card.find("rkisp_mainpath") == std::string::npos) {
            return false;
        }
    } else if (camera_.v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        /* 如果为摄像头设备则打印摄像头驱动名字 */
        printf("%s Driver    Name: %s %s\n", v4l2_device_.c_str(), (char *)camera_.v4l2_cap.driver, (char *)(camera_.v4l2_cap.card));
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
            std::string format = (char *)fmt.description;
            if (format.find("Motion-JPEG") != std::string::npos) {
                can_ouput_mjpg_ = true;
            }
            printf("Format: %s %s %s\n", V4l2FormatToString(fmt.pixelformat), format.c_str(), (can_ouput_mjpg_ == true ? "mjpg" : "raw"));

            /* 查询该图像格式所支持的分辨率 */
            struct v4l2_frmsizeenum frmsize;
            frmsize.pixel_format = fmt.pixelformat;
            for (uint32_t j = 0;; j++) { // 　该格式支持分辨率不止一种
                frmsize.index = j;
                ret           = ioctl(camera_.fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
                if (-1 == ret) { // 获取所有图片分辨率完成
                    break;
                }
                can_ouput_stream_ = true;
                /* 打印图片分辨率 */
                printf("Framsize: %dx%d\n", frmsize.discrete.width, frmsize.discrete.height);
            }
        }
    } else {
        printf("%s %08X is not video\n", v4l2_device_.c_str(), camera_.v4l2_cap.capabilities);
        return false;
    }

    return true;
}

void V4l2VideoCapture::NV12_to_YUYV(int width, int height, void *src, void *dst)
{
    int i = 0, j = 0;
    int *src_y  = (int *)src;
    int *src_uv = (int *)((char *)src + width * height);
    int *line   = (int *)dst;

    for (j = 0; j < height; j++) {
        if (j % 2 != 0)
            src_uv -= width >> 2;
        for (i = 0; i < width >> 2; i++) {
            *line++ = ((*src_y & 0x000000ff)) | ((*src_y & 0x0000ff00) << 8) |
                      ((*src_uv & 0x000000ff) << 8) | ((*src_uv & 0x0000ff00) << 16);
            *line++ = ((*src_y & 0x00ff0000) >> 16) | ((*src_y & 0xff000000) >> 8) |
                      ((*src_uv & 0x00ff0000) >> 8) | ((*src_uv & 0xff000000));
            src_y++;
            src_uv++;
        }
    }
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

void V4l2VideoCapture::AddCallback(std::function<bool(const uint8_t *, const uint32_t)> handler)
{
    process_image_ = handler;
}

bool V4l2VideoCapture::Init(uint32_t pixelformat)
{
    pixelformat_ = pixelformat;
    bool ret     = false;
    if (camera_.fd < 0) {
        ret |= OpenCamera();
    }
    ret |= InitCamera();
    ret |= StartPreviewing();
    if (ret) {
        if (process_image_ && MY_EPOLL.EpollLoopRunning()) {
            MY_EPOLL.EpollAddRead(camera_.fd, std::bind(&V4l2VideoCapture::ReadBuffOneFrame, this));
        }
    } else {
        std::cerr << "Carmera init fail" << std::endl;
    }
    return ret;
}

bool V4l2VideoCapture::V4l2Close()
{
    bool ret = false;
    MY_EPOLL.EpollDel(camera_.fd);
    ret |= StopPreviewing();
    ret |= UninitCamera();
    ret |= CloseCamera();
    return ret;
}

void V4l2VideoCapture::SaveJpeg(void *buffer, uint64_t size, std::string name)
{
    if (!can_ouput_mjpg_) {
        return;
    }

    std::ofstream image;
    image.open(name, std::ios::out | std::ios::binary);
    image.write((char *)buffer, size);
    image.close();
}

#endif
