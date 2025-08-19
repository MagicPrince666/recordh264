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

MjpgRecord::MjpgRecord(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
: VideoStream(dev, width, height, fps)
{
}

MjpgRecord::~MjpgRecord()
{
    if (mjpg_cap_) {
        mjpg_cap_->VideoDisable();
        mjpg_cap_->CloseV4l2();
    }
}

std::string MjpgRecord::GetAfterLastSlash(const std::string& str) {
    size_t pos = str.rfind('/');
    if (pos != std::string::npos && pos + 1 < str.length()) {
        return str.substr(pos + 1);
    }
    return str; // 如果没有'/'，返回整个字符串
}

void MjpgRecord::Init()
{
    std::string filename = GetAfterLastSlash(dev_name_) + "_" +getCurrentTime8() + ".avi";
    mjpg_cap_            = std::make_shared<V4l2Video>(dev_name_, video_width_, video_height_, video_fps_, V4L2_PIX_FMT_MJPEG, 1);
    avi_lib_             = std::make_shared<AviLib>(filename);

    if (mjpg_cap_->InitVideoIn() < 0) {
        return;
    }

    if (mjpg_cap_->VideoEnable() < 0) {
        return;
    }

    video_ = mjpg_cap_->GetV4l2Info();

    avi_lib_->AviOpenOutputFile();

    spdlog::info("fd {} width {} height {} fps {}", video_->fd, video_->width, video_->height, video_->fps);
    avi_lib_->AviSetVideo(video_->width, video_->height, video_->fps, (char *)"MJPG");

    spdlog::info("Start video Capture and saving {}", filename);

    MY_EPOLL.EpollAddRead(video_->fd, std::bind(&MjpgRecord::CapAndSaveVideo, this));
}

int32_t MjpgRecord::getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes)
{
    return 0;
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
    MY_EPOLL.EpollDel(video_->fd);
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
