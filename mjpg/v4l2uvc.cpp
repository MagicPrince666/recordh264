#include "v4l2uvc.h"
#include <stdlib.h>

V4l2Video::V4l2Video(std::string device, int32_t width, int32_t height, int32_t fps, int32_t format, int32_t grabmethod)
    : v4l2_device_(device)
{
    video_             = new (std::nothrow) vdIn;
    video_->width      = width;
    video_->height     = height;
    video_->fps        = fps;
    video_->formatIn   = format;
    video_->grabmethod = grabmethod;
    debug_             = 0;
}

V4l2Video::~V4l2Video()
{
    CloseV4l2();
    if (video_) {
        if (video_->fd) {
            close(video_->fd);
        }
        delete video_;
    }
}

int32_t V4l2Video::InitVideoIn()
{
    if (video_ == nullptr || v4l2_device_.empty()) {
        return -1;
    }

    printf("Device information:\n");
    printf("  Device path:  %s\n", v4l2_device_.c_str());

    if (InitV4l2() < 0) {
        printf(" Init v4L2 failed !! exit fatal\n");
        goto error;
    }

    EnumV4l2Format();

    /* alloc a temp buffer to reconstruct the pict */
    video_->framesizeIn = (video_->width * video_->height << 1);
    switch (video_->formatIn) {
    case V4L2_PIX_FMT_MJPEG:
        video_->tmpbuffer = new (std::nothrow) uint8_t[video_->framesizeIn];
        if (!video_->tmpbuffer) {
            goto error;
        }
        video_->framebuffer = new (std::nothrow) uint8_t[(size_t)video_->width * (video_->height + 8) * 2];
        break;
    case V4L2_PIX_FMT_YUYV:
        video_->framebuffer = new (std::nothrow) uint8_t[(size_t)video_->framesizeIn];
        break;
    default:
        printf(" should never arrive exit fatal !!\n");
        goto error;
        break;
    }
    if (!video_->framebuffer) {
        goto error;
    }
    return 0;
error:
    if (video_->tmpbuffer) {
        delete[] video_->tmpbuffer;
    }
    if (video_->framebuffer) {
        delete[] video_->framebuffer;
    }
    close(video_->fd);
    return -1;
}

int32_t V4l2Video::InitV4l2()
{
    uint32_t i;
    int32_t ret = 0;

    if ((video_->fd = open(v4l2_device_.c_str(), O_RDWR)) == -1) {
        perror("ERROR opening V4L interface");
        exit(1);
    }
    memset(&video_->cap, 0, sizeof(struct v4l2_capability));
    ret = ioctl(video_->fd, VIDIOC_QUERYCAP, &video_->cap);
    if (ret < 0) {
        printf("Error opening device %s: unable to query device.\n",
               v4l2_device_.c_str());
        return -1;
    }

    if ((video_->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        printf("Error opening device %s: video capture not supported.\n",
               v4l2_device_.c_str());
        return -1;
    }
    if (video_->grabmethod) {
        if (!(video_->cap.capabilities & V4L2_CAP_STREAMING)) {
            printf("%s does not support streaming i/o\n", v4l2_device_.c_str());
            return -1;
        }
    } else {
        if (!(video_->cap.capabilities & V4L2_CAP_READWRITE)) {
            printf("%s does not support read i/o\n", v4l2_device_.c_str());
            return -1;
        }
    }

    printf("Stream settings:\n");

    // Enumerate the supported formats to check whether the requested one
    // is available. If not, we try to fall back to YUYV.
    uint32_t device_formats[16]    = {0}; // Assume no device supports more than 16 formats
    int32_t requested_format_found = 0, fallback_format = -1;
    if (EnumFrameFormats(video_->fd, device_formats, ARRAY_SIZE(device_formats))) {
        printf("Unable to enumerate frame formats");
        return -1;
    }
    for (i = 0; i < ARRAY_SIZE(device_formats) && device_formats[i]; i++) {
        if (device_formats[i] == (uint32_t)(video_->formatIn)) {
            requested_format_found = 1;
            break;
        }
        if (device_formats[i] == V4L2_PIX_FMT_MJPEG || device_formats[i] == V4L2_PIX_FMT_YUYV)
            fallback_format = i;
    }
    if (requested_format_found) {
        // The requested format is supported
        printf("  Frame format: " FOURCC_FORMAT "\n", FOURCC_ARGS(video_->formatIn));
    } else if (fallback_format >= 0) {
        // The requested format is not supported but there's a fallback format
        printf("  Frame format: " FOURCC_FORMAT " (" FOURCC_FORMAT
               " is not supported by device)\n",
               FOURCC_ARGS(device_formats[0]), FOURCC_ARGS(video_->formatIn));
        video_->formatIn = device_formats[0];
    } else {
        // The requested format is not supported and no fallback format is available
        printf("ERROR: Requested frame format " FOURCC_FORMAT " is not available "
               "and no fallback format was found.\n",
               FOURCC_ARGS(video_->formatIn));
        return -1;
    }

    // Set pixel format and frame size
    memset(&video_->fmt, 0, sizeof(struct v4l2_format));
    video_->fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_->fmt.fmt.pix.width       = video_->width;
    video_->fmt.fmt.pix.height      = video_->height;
    video_->fmt.fmt.pix.pixelformat = video_->formatIn;
    video_->fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    ret                             = ioctl(video_->fd, VIDIOC_S_FMT, &video_->fmt);
    if (ret < 0) {
        perror("Unable to set format");
        return -1;
    }
    if ((video_->fmt.fmt.pix.width != (uint32_t)(video_->width)) ||
        (video_->fmt.fmt.pix.height != (uint32_t)(video_->height))) {
        printf("  Frame size:   %ux%u (requested size %ux%u is not supported by device)\n",
               video_->fmt.fmt.pix.width, video_->fmt.fmt.pix.height, video_->width, video_->height);
        video_->width  = video_->fmt.fmt.pix.width;
        video_->height = video_->fmt.fmt.pix.height;
        /* look the format is not part of the deal ??? */
        // video_->formatIn = video_->fmt.fmt.pix.pixelformat;
    } else {
        printf("  Frame size:   %dx%d\n", video_->width, video_->height);
    }

    /* set framerate */
    struct v4l2_streamparm setfps;
    memset(&setfps, 0, sizeof(struct v4l2_streamparm));
    setfps.type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator   = 1;
    setfps.parm.capture.timeperframe.denominator = video_->fps;
    ret                                          = ioctl(video_->fd, VIDIOC_S_PARM, &setfps);
    if (ret == -1) {
        perror("Unable to set frame rate");
        return -1;
    }
    ret = ioctl(video_->fd, VIDIOC_G_PARM, &setfps);
    if (ret == 0) {
        if (setfps.parm.capture.timeperframe.numerator != 1 ||
            setfps.parm.capture.timeperframe.denominator != (uint32_t)(video_->fps)) {
            printf("  Frame rate:   %u/%u fps (requested frame rate %u fps is "
                   "not supported by device)\n",
                   setfps.parm.capture.timeperframe.denominator,
                   setfps.parm.capture.timeperframe.numerator,
                   video_->fps);
        } else {
            printf("  Frame rate:   %d fps\n", video_->fps);
        }
    } else {
        perror("Unable to read out current frame rate");
        return -1;
    }

    /* request buffers */
    memset(&video_->rb, 0, sizeof(struct v4l2_requestbuffers));
    video_->rb.count  = NB_BUFFER;
    video_->rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_->rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(video_->fd, VIDIOC_REQBUFS, &video_->rb);
    if (ret < 0) {
        perror("Unable to allocate buffers");
        return -1;
    }
    /* map the buffers */
    for (i = 0; i < NB_BUFFER; i++) {
        memset(&video_->buf, 0, sizeof(struct v4l2_buffer));
        video_->buf.index  = i;
        video_->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        video_->buf.memory = V4L2_MEMORY_MMAP;
        ret                = ioctl(video_->fd, VIDIOC_QUERYBUF, &video_->buf);
        if (ret < 0) {
            perror("Unable to query buffer");
            return -1;
        }
        if (debug_) {
            printf("length: %u offset: %u\n", video_->buf.length,
                   video_->buf.m.offset);
        }
        video_->mem[i] = mmap(0 /* start anywhere */,
                              video_->buf.length, PROT_READ, MAP_SHARED, video_->fd,
                              video_->buf.m.offset);
        if (video_->mem[i] == MAP_FAILED) {
            perror("Unable to map buffer");
            return -1;
        }
        if (debug_) {
            printf("Buffer mapped at address %p.\n", video_->mem[i]);
        }
    }
    /* Queue the buffers. */
    for (i = 0; i < NB_BUFFER; ++i) {
        memset(&video_->buf, 0, sizeof(struct v4l2_buffer));
        video_->buf.index  = i;
        video_->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        video_->buf.memory = V4L2_MEMORY_MMAP;
        ret                = ioctl(video_->fd, VIDIOC_QBUF, &video_->buf);
        if (ret < 0) {
            perror("Unable to queue buffer");
            return -1;
        }
    }
    return 0;
}

struct vdIn *V4l2Video::GetV4l2Info()
{
    return video_;
}

int32_t V4l2Video::VideoEnable()
{
    int32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int32_t ret;

    ret = ioctl(video_->fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        perror("Unable to start capture");
        return ret;
    }
    video_->isstreaming = 1;
    return 0;
}

int32_t V4l2Video::VideoDisable()
{
    int32_t type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int32_t ret;

    ret = ioctl(video_->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        perror("Unable to stop capture");
        return ret;
    }
    video_->isstreaming = 0;
    return 0;
}

bool V4l2Video::UninitMmap()
{
    for (uint32_t i = 0; i < NB_BUFFER; ++i) {
        if (-1 == munmap(video_->mem[i], video_->buf.length)) {
            perror("munmap");
        }
        video_->mem[i] = nullptr;
    }

    return true;
}

int32_t V4l2Video::CloseV4l2()
{
    VideoDisable();
    UninitMmap();
    if (video_->tmpbuffer) {
        delete[] video_->tmpbuffer;
        video_->tmpbuffer = nullptr;
    }
    if (video_->framebuffer) {
        delete[] video_->framebuffer;
        video_->framebuffer = nullptr;
    }

    return 0;
}

int32_t V4l2Video::EnumFrameIntervals(int32_t dev, uint32_t pixfmt, uint32_t width, uint32_t height)
{
    int32_t ret;
    struct v4l2_frmivalenum fival;

    memset(&fival, 0, sizeof(fival));
    fival.index        = 0;
    fival.pixel_format = pixfmt;
    fival.width        = width;
    fival.height       = height;
    printf("\tTime interval between frame: ");
    while ((ret = ioctl(dev, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
        if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            printf("%u/%u, ",
                   fival.discrete.numerator, fival.discrete.denominator);
        } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
            printf("{min { %u/%u } .. max { %u/%u } }, ",
                   fival.stepwise.min.numerator, fival.stepwise.min.numerator,
                   fival.stepwise.max.denominator, fival.stepwise.max.denominator);
            break;
        } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
            printf("{min { %u/%u } .. max { %u/%u } / "
                   "stepsize { %u/%u } }, ",
                   fival.stepwise.min.numerator, fival.stepwise.min.denominator,
                   fival.stepwise.max.numerator, fival.stepwise.max.denominator,
                   fival.stepwise.step.numerator, fival.stepwise.step.denominator);
            break;
        }
        fival.index++;
    }
    printf("\n");
    if (ret != 0 && errno != EINVAL) {
        perror("ERROR enumerating frame intervals");
        return errno;
    }

    return 0;
}

int32_t V4l2Video::EnumFrameSizes(int32_t dev, uint32_t pixfmt)
{
    int32_t ret;
    struct v4l2_frmsizeenum fsize;

    memset(&fsize, 0, sizeof(fsize));
    fsize.index        = 0;
    fsize.pixel_format = pixfmt;
    while ((ret = ioctl(dev, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
        if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            printf("{ discrete: width = %u, height = %u }\n",
                   fsize.discrete.width, fsize.discrete.height);
            ret = EnumFrameIntervals(dev, pixfmt,
                                     fsize.discrete.width, fsize.discrete.height);
            if (ret != 0)
                printf("  Unable to enumerate frame sizes.\n");
        } else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
            printf("{ continuous: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
            printf("{ stepwise: min { width = %u, height = %u } .. "
                   "max { width = %u, height = %u } / "
                   "stepsize { width = %u, height = %u } }\n",
                   fsize.stepwise.min_width, fsize.stepwise.min_height,
                   fsize.stepwise.max_width, fsize.stepwise.max_height,
                   fsize.stepwise.step_width, fsize.stepwise.step_height);
            printf("  Refusing to enumerate frame intervals.\n");
            break;
        }
        fsize.index++;
    }
    if (ret != 0 && errno != EINVAL) {
        perror("ERROR enumerating frame sizes");
        return errno;
    }

    return 0;
}

int32_t V4l2Video::EnumFrameFormats(int32_t dev, uint32_t *supported_formats, uint32_t max_formats)
{
    int32_t ret;
    struct v4l2_fmtdesc fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.index = 0;
    fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while ((ret = ioctl(dev, VIDIOC_ENUM_FMT, &fmt)) == 0) {
        if (supported_formats == NULL) {
            printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
                   fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
                   (fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
                   fmt.description);
            ret = EnumFrameSizes(dev, fmt.pixelformat);
            if (ret != 0)
                printf("  Unable to enumerate frame sizes.\n");
        } else if (fmt.index < max_formats) {
            supported_formats[fmt.index] = fmt.pixelformat;
        }

        fmt.index++;
    }
    if (errno != EINVAL) {
        perror("ERROR enumerating frame formats");
        return errno;
    }

    return 0;
}

bool V4l2Video::EnumV4l2Format()
{
    /* 查询打开的设备是否属于摄像头：设备video不一定是摄像头*/
    int32_t ret = ioctl(video_->fd, VIDIOC_QUERYCAP, &video_->cap);
    if (-1 == ret) {
        perror("ioctl VIDIOC_QUERYCAP");
        return false;
    }
    if (video_->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        /* 如果为摄像头设备则打印摄像头驱动名字 */
        printf("Driver    Name: %s\n", (char *)video_->cap.driver);
    } else {
        perror("open file is not video\n");
        return false;
    }

    /* 查询摄像头可捕捉的图片类型，VIDIOC_ENUM_FMT: 枚举摄像头帧格式 */
    struct v4l2_fmtdesc fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定需要枚举的类型
    for (uint32_t i = 0;; i++) {            // 有可能摄像头支持的图片格式不止一种
        fmt.index = i;
        ret       = ioctl(video_->fd, VIDIOC_ENUM_FMT, &fmt);
        if (-1 == ret) { // 获取所有格式完成
            break;
        }

        /* 打印摄像头图片格式 */
        printf("Format: %s\n", (char *)fmt.description);

        /* 查询该图像格式所支持的分辨率 */
        struct v4l2_frmsizeenum frmsize;
        frmsize.pixel_format = fmt.pixelformat;
        for (uint32_t j = 0;; j++) { //　该格式支持分辨率不止一种
            frmsize.index = j;
            ret           = ioctl(video_->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (-1 == ret) { // 获取所有图片分辨率完成
                break;
            }

            /* 打印图片分辨率 */
            printf("Framsize: %dx%d\n", frmsize.discrete.width, frmsize.discrete.height);
        }
    }
    return true;
}
