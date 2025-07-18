#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "spdlog/spdlog.h"

#include "h264_camera.h"
#include "h264encoder.h"
#ifdef USE_RK_HW_ENCODER
#include "calculate_rockchip.h"
#else
#include "calculate_cpu.h"
#endif

#define USE_BUF_LIST 0

V4l2H264hData::V4l2H264hData(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    : VideoStream(dev, width, height, fps)
{
    b_running_ = false;
    s_pause_   = false;
    h264_fp_   = nullptr;
    Init();
}

V4l2H264hData::~V4l2H264hData()
{
    CloseFile();

    if (p_capture_) {
        delete p_capture_;
    }

    if (encoder_) {
        delete encoder_;
    }

    if (h264_buf_) {
        delete[] h264_buf_;
    }

    if (camera_buf_) {
        delete[] camera_buf_;
    }
}

void V4l2H264hData::Init()
{
    uint32_t pixelformat = V4L2_PIX_FMT_NV12;
    uint32_t enc_pixelformat = pixelformat;
    if (pixelformat == V4L2_PIX_FMT_MJPEG) {
        enc_pixelformat = V4L2_PIX_FMT_NV12;
    }
    p_capture_ = new (std::nothrow) V4l2VideoCapture(dev_name_.c_str(), video_width_, video_height_, video_fps_);
    p_capture_->Init(pixelformat); // 初始化摄像头
    video_format_ = p_capture_->GetFormat();
    camera_buf_   = new (std::nothrow) uint8_t[p_capture_->GetFrameLength()];
    encoder_      = new (std::nothrow) H264Encoder(video_format_->width, video_format_->height, enc_pixelformat);
    encoder_->Init();

    yuv420_buf_ = new (std::nothrow) uint8_t[p_capture_->GetFrameLength()];

#ifdef USE_RK_HW_ENCODER
    calculate_ptr_ = std::make_shared<CalculateRockchip>(video_width_, video_height_);
#else
    calculate_ptr_ = std::make_shared<CalculateCpu>();
#endif
    calculate_ptr_->Init();

#if USE_BUF_LIST
#else
    // 申请H264缓存
    h264_buf_ = new (std::nothrow) uint8_t[p_capture_->GetFrameLength()];
#endif

    InitFile(false); // 存储264文件

    b_running_ = true;

    spdlog::info("V4l2H264hData::Init()");
}

void V4l2H264hData::RecordAndEncode()
{
    int32_t len = 0;
    if (p_capture_) {
        len = p_capture_->BuffOneFrame(camera_buf_);
    }

    if (len <= 0) {
        StopCap();
        return;
    }

#if USE_BUF_LIST
    Buffer h264_buf;
    memset(&h264_buf, 0, sizeof(struct Buffer));
    /*H.264压缩视频*/
    encoder_->CompressFrame(FRAME_TYPE_AUTO, camera_buf_, h264_buf.buf_ptr, h264_buf.length);

    if (h264_buf.length > 0) {
        if (h264_fp_) {
            fwrite(h264_buf.buf_ptr, h264_buf.length, 1, h264_fp_);
        }
    } else {
        spdlog::info("get size after encoder = {}", h264_buf.length);
    }

    if (h264_buf.buf_ptr) {
        delete[] h264_buf.buf_ptr;
    }
#else

    if (video_format_->v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_H264) {
        if (h264_fp_) {
            fwrite(camera_buf_, len, 1, h264_fp_);
        }
    } else {
        uint64_t length = 0;
        encoder_->CompressFrame(FRAME_TYPE_AUTO, camera_buf_, h264_buf_, length);
        if (length > 0) {
            if (h264_fp_) {
                fwrite(h264_buf_, length, 1, h264_fp_);
            }
        } else {
            spdlog::error("get size after encoder = {}", length);
        }
    }
#endif
}

int32_t V4l2H264hData::getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes)
{
    if (!b_running_) {
        spdlog::warn("V4l2H264hData::getData b_running_ = false");
        return 0;
    }

    int32_t len = 0;
    if (p_capture_) {
        len = p_capture_->BuffOneFrame(camera_buf_);
        // spdlog::info("get size after encoder = {}", len);
        if (video_format_->v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            calculate_ptr_->Transfer(camera_buf_, yuv420_buf_, video_width_, video_height_, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV12);
        }
    }

    if (len <= 0) {
        StopCap();
        fFrameSize = 0;
        return 0;
    }

    uint64_t length = 0;
    if (video_format_->v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
        encoder_->CompressFrame(FRAME_TYPE_AUTO, yuv420_buf_, h264_buf_, length);
    } else {
        encoder_->CompressFrame(FRAME_TYPE_AUTO, camera_buf_, h264_buf_, length);
    }
    if (length < fMaxSize) {
        memcpy(fTo, h264_buf_, length);
        fFrameSize         = length;
        fNumTruncatedBytes = 0;
    } else {
        memcpy(fTo, h264_buf_, fMaxSize);
        fNumTruncatedBytes = length - fMaxSize;
        fFrameSize         = fMaxSize;
    }

    return length;
}

void V4l2H264hData::StartCap()
{
    b_running_ = true;
    spdlog::info("V4l2H264hData StartCap");
}

void V4l2H264hData::StopCap()
{
    b_running_ = false;
    spdlog::info("V4l2H264hData StopCap");
}

inline bool FileExists(const std::string &name)
{
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

void V4l2H264hData::InitFile(bool yes)
{
    if (!yes) {
        return;
    }

    std::string h264_file_name = getCurrentTime8() + ".h264";
    if (FileExists(h264_file_name)) {
        remove(h264_file_name.c_str());
    }
    h264_fp_ = fopen(h264_file_name.c_str(), "wa+");
}

void V4l2H264hData::CloseFile()
{
    if (h264_fp_) {
        fclose(h264_fp_);
    }
}

bool V4l2H264hData::PauseCap(bool pause)
{
    s_pause_ = pause;
    return s_pause_;
}

std::string V4l2H264hData::getCurrentTime8()
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

uint64_t V4l2H264hData::DirSize(const char *dir)
{
#ifndef _WIN32
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    long long int totalSize = 0;
    if ((dp = opendir(dir)) == NULL) {
        fprintf(stderr, "Cannot open dir: %s\n", dir);
        return 0; // 可能是个文件，或者目录不存在
    }

    // 先加上自身目录的大小
    lstat(dir, &statbuf);
    totalSize += statbuf.st_size;

    while ((entry = readdir(dp)) != NULL) {
        char subdir[257];
        sprintf(subdir, "%s/%s", dir, entry->d_name);
        lstat(subdir, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) {
                continue;
            }

            uint64_t subDirSize = DirSize(subdir);
            totalSize += subDirSize;
        } else {
            totalSize += statbuf.st_size;
        }
    }

    closedir(dp);
    return totalSize;
#else
    return 0;
#endif
}

bool V4l2H264hData::RmDirFiles(const std::string &path)
{
#ifndef _WIN32
    std::string strPath = path;
    if (strPath.at(strPath.length() - 1) != '\\' || strPath.at(strPath.length() - 1) != '/') {
        strPath.append("/");
    }

    DIR *directory = opendir(strPath.c_str()); // 打开这个目录
    if (directory != NULL) {
        for (struct dirent *dt = readdir(directory); dt != nullptr;
             dt                = readdir(directory)) { // 逐个读取目录中的文件到dt
            // 系统有个系统文件，名为“..”和“.”,对它不做处理
            if (strcmp(dt->d_name, "..") != 0 && strcmp(dt->d_name, ".") != 0) { // 判断是否为系统隐藏文件
                struct stat st;                                                  // 文件的信息
                std::string fileName;                                            // 文件夹中的文件名
                fileName = strPath + std::string(dt->d_name);
                stat(fileName.c_str(), &st);
                if (!S_ISDIR(st.st_mode)) {
                    // 删除文件即可
                    remove(fileName.c_str());
                }
            }
        }
        closedir(directory);
    }

#endif
    return true;
}

std::vector<std::string> V4l2H264hData::GetFilesFromPath(std::string path)
{
    std::vector<std::string> files;
    // check the parameter !
    if (path.empty()) {
        return files;
    }
    // check if dir_name is a valid dir
    struct stat s;
    lstat(path.c_str(), &s);
    if (!S_ISDIR(s.st_mode)) {
        return files;
    }

    struct dirent *filename; // return value for readdir()
    DIR *dir;                // return value for opendir()
    dir = opendir(path.c_str());
    if (NULL == dir) {
        return files;
    }

    /* read all the files in the dir ~ */
    while ((filename = readdir(dir)) != NULL) {
        // get rid of "." and ".."
        if (strcmp(filename->d_name, ".") == 0 ||
            strcmp(filename->d_name, "..") == 0) {
            continue;
        }
        std::string full_path = path + filename->d_name;
        struct stat s;
        lstat(full_path.c_str(), &s);
        if (S_ISDIR(s.st_mode)) {
            continue;
        }
        files.push_back(full_path);
    }
    return files;
}
