#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <thread>
#include <new>
#include <sstream>
#include <iomanip>

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types

#include "avilib.h"
#include "epoll.h"
#include "mjpgrecord.h"

MjpgRecord::MjpgRecord(std::string device) :
v4l2_device_(device)
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
    if(avi_lib_) {
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
