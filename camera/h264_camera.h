/**
 * @file h264_camera.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 获取视频流并编码
 * @version 0.1
 * @date 2022-11-18
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#ifndef __H264_CAMERA_H__
#define __H264_CAMERA_H__

#include <list>
#include <memory>
#include <vector>

#include "video_capture.h"
#include "video_source.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

class V4l2H264hData : public VideoStream
{
public:
    V4l2H264hData(std::string dev = "/dev/video0", uint32_t width = 640, uint32_t height = 480, uint32_t fps = 30);
    virtual ~V4l2H264hData();

    /**
     * @brief 给live555用
     * @param fTo
     * @param fMaxSize
     * @param fFrameSize
     * @param fNumTruncatedBytes
     * @return int32_t
     */
    virtual int32_t getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes);

    /**
     * @brief 初始化资源
     */
    void Init();

private:

    /**
     * @brief 暂停线程
     * @param pause 是否暂停
     * @return true 暂停
     * @return false 继续
     */
    bool PauseCap(bool pause);

    /**
     * @brief 开始
     */
    void StartCap();

    /**
     * @brief 停止
     */
    void StopCap();

    /**
     * @brief 打开文件
     */
    void InitFile(bool yes);

    /**
     * 关闭文件
     */
    void CloseFile();

    /**
     * 获取并编码
     */
    void RecordAndEncode();

    /**
     * @brief 获取格式化时间戳
     * @return std::string
     */
    std::string getCurrentTime8();

    /**
     * @brief 获取文件夹大小
     * @param dir
     * @return uint64_t
     */
    uint64_t DirSize(const char *dir);

    /**
     * @brief 删除文件夹下的文件
     * @param path 文件夹目录
     * @return true
     * @return false
     */
    bool RmDirFiles(const std::string &path);

    /**
     * @brief 获取文件夹下所有文件
     * @param path 目标文件夹
     * @return std::vector<std::string> 文件集合
     */
    std::vector<std::string> GetFilesFromPath(std::string path);

private:
    struct Buffer {
        uint8_t *buf_ptr;
        uint64_t length;
    };

    V4l2VideoCapture *p_capture_;
    H264Encoder *encoder_;
    bool b_running_;
    bool s_pause_;

    uint8_t *camera_buf_;
    uint8_t *h264_buf_;

    FILE *h264_fp_;

    std::list<Buffer> h264_buf_list_;
    struct Camera *video_format_;
};

// 生产yuyv视频流的工厂，软件编码
class UvcYuyvCamera : public VideoFactory
{
public:
    VideoStream *createVideoStream(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    {
        return new V4l2H264hData(dev, width, height, fps);
    }
};

inline bool FileExists(const std::string &name);

#endif
