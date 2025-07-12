#ifndef __RK_MPP_ENCODER_H__
#define __RK_MPP_ENCODER_H__

#include "video_capture.h"
#include "video_source.h"
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include "calculate.h"

#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_log.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_rc_defs.h>
#include <rockchip/mpp_task.h>
#include <rockchip/rk_mpi.h>
#include <rga/RgaApi.h>

#define MPP_ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))
#define FRAME_RATE 30
#define GOP_SIZE   30
#define BIT_RATE   2000000  // 4Mbps

class RkMppEncoder : public VideoStream
{
public:
    RkMppEncoder(std::string dev, uint32_t width, uint32_t height, uint32_t fps);
    ~RkMppEncoder();

    int32_t getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes);

private:
    std::shared_ptr<V4l2VideoCapture> v4l2_ctx_;
    std::shared_ptr<Calculate> calculate_ptr_;
    uint8_t *camera_buf_;
    uint8_t *yuv420_buf_;
    int h264_lenght_;
    uint32_t pixelformat_;

    MppCtx m_ctx;
    MppApi* m_mpi;
    MppBufferGroup m_buf_grp;
    MppEncCfg m_cfg;

    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;
    int m_gop;
    size_t m_frame_size;

    void Init();

    bool EncodeFrame(void* inputData, uint8_t* &outputData, size_t* outputSize);
};

class MppCamera : public VideoFactory
{
public:
    VideoStream *createVideoStream(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    {
        return new RkMppEncoder(dev, width, height, fps);
    }
};

#endif
