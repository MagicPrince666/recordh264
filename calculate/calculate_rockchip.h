#ifndef __CALCULATE_ROCKCHIP_H__
#define __CALCULATE_ROCKCHIP_H__

#include "calculate.h"
#include <unordered_map>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_log.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/mpp_rc_defs.h>
#include <rockchip/mpp_task.h>
#include <rockchip/rk_mpi.h>
#include <rga/RgaApi.h>
#include <linux/videodev2.h>

class CalculateRockchip : public Calculate
{
public:
    CalculateRockchip(uint32_t width, uint32_t height);
    ~CalculateRockchip();

    void Init();

    bool Yuv422Rgb(const uint8_t* yuv, uint8_t* rgb, int width, int height);

    bool Nv12Rgb24(const uint8_t* nv12, uint8_t* rgb, int width, int height);

    bool TransferRgb888(const uint8_t* raw, uint8_t* rgb, int width, int height, const uint32_t format);

    bool Nv12Yuv420p(const uint8_t* nv12, uint8_t* yuv420p, int width, int height);

    bool Transfer(const uint8_t* raw, uint8_t* dst, int width, int height, const uint32_t src_format, const uint32_t dst_format);

private:
    MppCtx mpp_ctx_ = nullptr;
    MppApi* mpp_api_ = nullptr;
    MppPacket mpp_packet_ = nullptr;
    MppFrame mpp_frame_ = nullptr;
    MppDecCfg mpp_dec_cfg_ = nullptr;
    MppBuffer mpp_frame_buffer_ = nullptr;
    MppBuffer mpp_packet_buffer_ = nullptr;
    uint8_t* data_buffer_ = nullptr;
    MppBufferGroup mpp_frame_group_ = nullptr;
    MppBufferGroup mpp_packet_group_ = nullptr;
    MppTask mpp_task_ = nullptr;
    uint32_t need_split_ = 0;
    uint8_t* rgb_buffer_ = nullptr;
    uint32_t video_width_;
    uint32_t video_height_;

    std::unordered_map<uint32_t, uint32_t> pix_fmt_map_ = {
        {V4L2_PIX_FMT_UYVY, RK_FORMAT_UYVY_422},
        {V4L2_PIX_FMT_NV12, RK_FORMAT_YCbCr_420_SP},
        {V4L2_PIX_FMT_NV16, RK_FORMAT_YCbCr_422_SP},
        {V4L2_PIX_FMT_YUYV, RK_FORMAT_YUYV_422},
        {V4L2_PIX_FMT_YUV420, RK_FORMAT_YUYV_420},
        {V4L2_PIX_FMT_RGB24, RK_FORMAT_RGB_888},
        {V4L2_PIX_FMT_MJPEG, RK_FORMAT_YCbCr_420_SP}
    };

    bool mppFrame2DstFormat(const MppFrame frame, uint8_t* data, const uint32_t src_format, const uint32_t dst_format);

    bool Decode(const uint8_t* raw, uint8_t* rgb, int width, int height, const uint32_t src_format, const uint32_t dst_format);
};

#endif
