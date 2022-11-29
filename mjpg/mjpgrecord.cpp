#include <iomanip>
#include <new>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/spdlog.h"

#include "avilib.h"
#include "epoll.h"
#include "mjpgrecord.h"

MjpgRecord::MjpgRecord(std::string device) : v4l2_device_(device)
{
    mjpg_cap_  = nullptr;
    capturing_ = false;
}

MjpgRecord::~MjpgRecord()
{
    if (cat_avi_thread_.joinable()) {
        cat_avi_thread_.join();
    }

    if (mjpg_cap_) {
        mjpg_cap_->VideoDisable();
        mjpg_cap_->CloseV4l2();
        delete mjpg_cap_;
    }
    if (avi_lib_) {
        delete avi_lib_;
    }
}

bool MjpgRecord::Init()
{
    std::string filename = getCurrentTime8() + ".avi";
    mjpg_cap_            = new (std::nothrow) V4l2Video(v4l2_device_, 1280, 720, 30, V4L2_PIX_FMT_MJPEG, 1);
    avi_lib_             = new (std::nothrow) AviLib(filename);

    if (mjpg_cap_->InitVideoIn() < 0) {
        exit(1);
    }

    EnumV4l2Format();

    if (mjpg_cap_->VideoEnable() < 0) {
        exit(1);
    }

    video_ = mjpg_cap_->GetV4l2Info();

    avi_lib_->AviOpenOutputFile();

    spdlog::info("fd {} width {} height {} fps {}", video_->fd, video_->width, video_->height, video_->fps);
    avi_lib_->AviSetVideo(video_->width, video_->height, video_->fps, (char *)"MJPG");

    spdlog::info("Start video Capture and saving {}", filename);

    cat_avi_thread_ = std::thread([](MjpgRecord *p_this) { p_this->VideoCapThread(); }, this);

    return true;
}

bool MjpgRecord::EnumV4l2Format()
{
    /* 查询打开的设备是否属于摄像头：设备video不一定是摄像头*/
    int32_t ret = ioctl(video_->fd, VIDIOC_QUERYCAP, &video_->cap);
    if (-1 == ret) {
        perror("ioctl VIDIOC_QUERYCAP");
        return false;
    }
    if (video_->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        /* 如果为摄像头设备则打印摄像头驱动名字 */
        spdlog::info("Driver    Name: {}", (char *)video_->cap.driver);
    } else {
        spdlog::error("open file is not video");
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
        spdlog::info("Format: {}", (char *)fmt.description);

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
            spdlog::info("Framsize: {}x{}", frmsize.discrete.width, frmsize.discrete.height);
        }
    }
    return true;
}

void MjpgRecord::VideoCapThread()
{
    spdlog::info("{} start mjpg captrue", __FUNCTION__);
    if (!capturing_) {
        MY_EPOLL.EpollAdd(video_->fd, std::bind(&MjpgRecord::CapAndSaveVideo, this));
    }
    capturing_ = true;
    while (true) {
        if (!capturing_) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool MjpgRecord::CapAndSaveVideo()
{
    memset(&video_->buf, 0, sizeof(struct v4l2_buffer));
    video_->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_->buf.memory = V4L2_MEMORY_MMAP;
    int ret            = ioctl(video_->fd, VIDIOC_DQBUF, &video_->buf);
    if (ret < 0) {
        spdlog::error("Unable to dequeue buffer");
        exit(1);
    }

    // spdlog::info("{} fd = {}", __FUNCTION__, video_->fd);
    memcpy(video_->tmpbuffer, video_->mem[video_->buf.index], video_->buf.bytesused);

    avi_lib_->AviWriteFrame((char *)(video_->tmpbuffer), video_->buf.bytesused, video_->framecount);
    video_->framecount++;

    ret = ioctl(video_->fd, VIDIOC_QBUF, &video_->buf);
    if (ret < 0) {
        spdlog::error("Unable to requeue buffer");
        exit(1);
    }
    return true;
}

void MjpgRecord::StopCap()
{
    if (capturing_) {
        MY_EPOLL.EpollDel(video_->fd);
    }
    capturing_ = false;
}

std::string MjpgRecord::getCurrentTime8()
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
