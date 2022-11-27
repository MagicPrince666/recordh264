#include "v4l2uvc.h"
#include <stdlib.h>

static int debug = 0;

V4l2Video::V4l2Video(std::string device) : v4l2_device_(device)
{
}

V4l2Video::~V4l2Video() {}

int V4l2Video::InitVideoIn(struct vdIn *vd, int width, int height, int fps,
                           int format, int grabmethod)
{
    if (vd == NULL || v4l2_device_.empty()) {
        return -1;
    }
    if (width == 0 || height == 0) {
        return -1;
    }
    if (grabmethod < 0 || grabmethod > 1) {
        grabmethod = 1; // mmap by default;
    }
    vd->videodevice = NULL;
    vd->status      = NULL;
    vd->pictName    = NULL;
    vd->videodevice = (char *)calloc(1, 16 * sizeof(char));
    vd->status      = (char *)calloc(1, 100 * sizeof(char));
    vd->pictName    = (char *)calloc(1, 80 * sizeof(char));
    snprintf(vd->videodevice, 12, "%s", v4l2_device_.c_str());
    printf("Device information:\n");
    printf("  Device path:  %s\n", vd->videodevice);
    vd->toggleAvi        = 0;
    vd->avifile          = NULL;
    vd->recordtime       = 0;
    vd->framecount       = 0;
    vd->recordstart      = 0;
    vd->getPict          = 0;
    vd->signalquit       = 1;
    vd->width            = width;
    vd->height           = height;
    vd->fps              = fps;
    vd->formatIn         = format;
    vd->grabmethod       = grabmethod;
    vd->fileCounter      = 0;
    vd->rawFrameCapture  = 0;
    vd->rfsBytesWritten  = 0;
    vd->rfsFramesWritten = 0;
    vd->captureFile      = NULL;
    vd->bytesWritten     = 0;
    vd->framesWritten    = 0;
    if (InitV4l2(vd) < 0) {
        printf(" Init v4L2 failed !! exit fatal\n");
        goto error;
        ;
    }
    /* alloc a temp buffer to reconstruct the pict */
    vd->framesizeIn = (vd->width * vd->height << 1);
    switch (vd->formatIn) {
    case V4L2_PIX_FMT_MJPEG:
        vd->tmpbuffer =
            (unsigned char *)calloc(1, (size_t)vd->framesizeIn);
        if (!vd->tmpbuffer)
            goto error;
        vd->framebuffer =
            (unsigned char *)calloc(1,
                                    (size_t)vd->width * (vd->height + 8) * 2);
        break;
    case V4L2_PIX_FMT_YUYV:
        vd->framebuffer =
            (unsigned char *)calloc(1, (size_t)vd->framesizeIn);
        break;
    default:
        printf(" should never arrive exit fatal !!\n");
        goto error;
        break;
    }
    if (!vd->framebuffer)
        goto error;
    return 0;
error:
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    close(vd->fd);
    return -1;
}

int V4l2Video::InitV4l2(struct vdIn *vd)
{
    unsigned int i;
    int ret = 0;

    if ((vd->fd = open(vd->videodevice, O_RDWR)) == -1) {
        perror("ERROR opening V4L interface");
        exit(1);
    }
    memset(&vd->cap, 0, sizeof(struct v4l2_capability));
    ret = ioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
    if (ret < 0) {
        printf("Error opening device %s: unable to query device.\n",
               vd->videodevice);
        return -1;
    }

    if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
        printf("Error opening device %s: video capture not supported.\n",
               vd->videodevice);
        return -1;
        ;
    }
    if (vd->grabmethod) {
        if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
            printf("%s does not support streaming i/o\n", vd->videodevice);
            return -1;
        }
    } else {
        if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
            printf("%s does not support read i/o\n", vd->videodevice);
            return -1;
        }
    }

    printf("Stream settings:\n");

    // Enumerate the supported formats to check whether the requested one
    // is available. If not, we try to fall back to YUYV.
    unsigned int device_formats[16] = {0}; // Assume no device supports more than 16 formats
    int requested_format_found = 0, fallback_format = -1;
    if (EnumFrameFormats(vd->fd, device_formats, ARRAY_SIZE(device_formats))) {
        printf("Unable to enumerate frame formats");
        return -1;
    }
    for (i = 0; i < ARRAY_SIZE(device_formats) && device_formats[i]; i++) {
        if (device_formats[i] == (unsigned int)(vd->formatIn)) {
            requested_format_found = 1;
            break;
        }
        if (device_formats[i] == V4L2_PIX_FMT_MJPEG || device_formats[i] == V4L2_PIX_FMT_YUYV)
            fallback_format = i;
    }
    if (requested_format_found) {
        // The requested format is supported
        printf("  Frame format: " FOURCC_FORMAT "\n", FOURCC_ARGS(vd->formatIn));
    } else if (fallback_format >= 0) {
        // The requested format is not supported but there's a fallback format
        printf("  Frame format: " FOURCC_FORMAT " (" FOURCC_FORMAT
               " is not supported by device)\n",
               FOURCC_ARGS(device_formats[0]), FOURCC_ARGS(vd->formatIn));
        vd->formatIn = device_formats[0];
    } else {
        // The requested format is not supported and no fallback format is available
        printf("ERROR: Requested frame format " FOURCC_FORMAT " is not available "
               "and no fallback format was found.\n",
               FOURCC_ARGS(vd->formatIn));
        return -1;
    }

    // Set pixel format and frame size
    memset(&vd->fmt, 0, sizeof(struct v4l2_format));
    vd->fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->fmt.fmt.pix.width       = vd->width;
    vd->fmt.fmt.pix.height      = vd->height;
    vd->fmt.fmt.pix.pixelformat = vd->formatIn;
    vd->fmt.fmt.pix.field       = V4L2_FIELD_ANY;
    ret                         = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
    if (ret < 0) {
        perror("Unable to set format");
        return -1;
    }
    if ((vd->fmt.fmt.pix.width != (unsigned int)(vd->width)) ||
        (vd->fmt.fmt.pix.height != (unsigned int)(vd->height))) {
        printf("  Frame size:   %ux%u (requested size %ux%u is not supported by device)\n",
               vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height, vd->width, vd->height);
        vd->width  = vd->fmt.fmt.pix.width;
        vd->height = vd->fmt.fmt.pix.height;
        /* look the format is not part of the deal ??? */
        // vd->formatIn = vd->fmt.fmt.pix.pixelformat;
    } else {
        printf("  Frame size:   %dx%d\n", vd->width, vd->height);
    }

    /* set framerate */
    struct v4l2_streamparm *setfps;
    setfps = (struct v4l2_streamparm *)calloc(1, sizeof(struct v4l2_streamparm));
    memset(setfps, 0, sizeof(struct v4l2_streamparm));
    setfps->type                                  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps->parm.capture.timeperframe.numerator   = 1;
    setfps->parm.capture.timeperframe.denominator = vd->fps;
    ret                                           = ioctl(vd->fd, VIDIOC_S_PARM, setfps);
    if (ret == -1) {
        perror("Unable to set frame rate");
        return -1;
    }
    ret = ioctl(vd->fd, VIDIOC_G_PARM, setfps);
    if (ret == 0) {
        if (setfps->parm.capture.timeperframe.numerator != 1 ||
            setfps->parm.capture.timeperframe.denominator != (unsigned int)(vd->fps)) {
            printf("  Frame rate:   %u/%u fps (requested frame rate %u fps is "
                   "not supported by device)\n",
                   setfps->parm.capture.timeperframe.denominator,
                   setfps->parm.capture.timeperframe.numerator,
                   vd->fps);
        } else {
            printf("  Frame rate:   %d fps\n", vd->fps);
        }
    } else {
        perror("Unable to read out current frame rate");
        return -1;
    }

    /* request buffers */
    memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
    vd->rb.count  = NB_BUFFER;
    vd->rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vd->rb.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
    if (ret < 0) {
        perror("Unable to allocate buffers");
        return -1;
    }
    /* map the buffers */
    for (i = 0; i < NB_BUFFER; i++) {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index  = i;
        vd->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret            = ioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
        if (ret < 0) {
            perror("Unable to query buffer");
            return -1;
        }
        if (debug)
            printf("length: %u offset: %u\n", vd->buf.length,
                   vd->buf.m.offset);
        vd->mem[i] = mmap(0 /* start anywhere */,
                          vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
                          vd->buf.m.offset);
        if (vd->mem[i] == MAP_FAILED) {
            perror("Unable to map buffer");
            return -1;
        }
        if (debug)
            printf("Buffer mapped at address %p.\n", vd->mem[i]);
    }
    /* Queue the buffers. */
    for (i = 0; i < NB_BUFFER; ++i) {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.index  = i;
        vd->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret            = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0) {
            perror("Unable to queue buffer");
            return -1;
        }
    }
    return 0;
}

int V4l2Video::VideoEnable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(vd->fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        perror("Unable to start capture");
        return ret;
    }
    vd->isstreaming = 1;
    return 0;
}

int V4l2Video::VideoDisable(struct vdIn *vd)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret;

    ret = ioctl(vd->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        perror("Unable to stop capture");
        return ret;
    }
    vd->isstreaming = 0;
    return 0;
}

int V4l2Video::CloseV4l2(struct vdIn *vd)
{
    if (vd->isstreaming)
        VideoDisable(vd);
    if (vd->tmpbuffer)
        free(vd->tmpbuffer);
    vd->tmpbuffer = NULL;
    free(vd->framebuffer);
    vd->framebuffer = NULL;
    free(vd->videodevice);
    free(vd->status);
    free(vd->pictName);
    vd->videodevice = NULL;
    vd->status      = NULL;
    vd->pictName    = NULL;
    return 0;
}

int V4l2Video::EnumFrameIntervals(int dev, __u32 pixfmt, __u32 width, __u32 height)
{
    int ret;
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

int V4l2Video::EnumFrameSizes(int dev, __u32 pixfmt)
{
    int ret;
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

int V4l2Video::EnumFrameFormats(int dev, unsigned int *supported_formats, unsigned int max_formats)
{
    int ret;
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
