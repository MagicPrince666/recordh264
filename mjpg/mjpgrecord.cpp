#include <new>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include "avilib.h"
#include "epoll.h"
#include "mjpgrecord.h"

MjpgRecord::MjpgRecord(std::string file_name) : file_name_(file_name)
{
    mjpg_cap_  = nullptr;
    avi_file_  = nullptr;
    capturing_ = false;
}

MjpgRecord::~MjpgRecord()
{
    if (!avi_file_) {
        fclose(avi_file_);
    }
    if (mjpg_cap_) {
        mjpg_cap_->CloseV4l2();
        delete mjpg_cap_;
        delete video_;
    }
}

bool MjpgRecord::Init()
{
    std::string avi_file = "test.avi";
    int grabmethod = 1;
    int fps        = 30;

    video_               = new (std::nothrow) vdIn;
    mjpg_cap_            = new (std::nothrow) V4l2Video("/dev/video0", 1280, 720, fps, V4L2_PIX_FMT_MJPEG, grabmethod);
    avi_lib_             = new (std::nothrow) AviLib(avi_file);

    if (mjpg_cap_->InitVideoIn() < 0) {
        exit(1);
    }

    avi_file_ = fopen(file_name_.c_str(), "wb");
    if (!avi_file_) {
        printf("Unable to open file for raw frame capturing\n ");
        exit(1);
    }

    if (mjpg_cap_->VideoEnable()) {
        exit(1);
    }

    avi_lib_->AviOpenOutputFile();

    avi_lib_->AviSetVideo(video_->width, video_->height, fps, (char *)"MJPG");
    printf("recording to %s\n", video_->avifilename);

    cat_avi_thread_ = std::thread([](MjpgRecord *p_this) { p_this->VideoCapThread(); }, this);
    return true;
}

void MjpgRecord::VideoCapThread()
{
    if (!capturing_) {
        MY_EPOLL.EpollAdd(video_->fd, std::bind(&MjpgRecord::CapAndSaveVideo, this));
    }
    capturing_ = true;
    while (1) {
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
        printf("Unable to dequeue buffer");
        return false;
    }

    memcpy(video_->tmpbuffer, video_->mem[video_->buf.index], video_->buf.bytesused);

    avi_lib_->AviWriteFrame((char *)(video_->tmpbuffer), video_->buf.bytesused, video_->framecount);
    video_->framecount++;

    ret = ioctl(video_->fd, VIDIOC_QBUF, &video_->buf);
    if (ret < 0) {
        printf("Unable to requeue buffer");
        return false;
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