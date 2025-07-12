#include "calculate_rockchip.h"
#include <unistd.h>

#define MPP_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

CalculateRockchip::CalculateRockchip(uint32_t width, uint32_t height)
    : video_width_(width), video_height_(height)
{
}

CalculateRockchip::~CalculateRockchip()
{
    if (mpp_frame_buffer_) {
        mpp_buffer_put(mpp_frame_buffer_);
        mpp_frame_buffer_ = nullptr;
    }
    if (mpp_packet_buffer_) {
        mpp_buffer_put(mpp_packet_buffer_);
        mpp_packet_buffer_ = nullptr;
    }
    if (mpp_frame_group_) {
        mpp_buffer_group_put(mpp_frame_group_);
        mpp_frame_group_ = nullptr;
    }
    if (mpp_packet_group_) {
        mpp_buffer_group_put(mpp_packet_group_);
        mpp_packet_group_ = nullptr;
    }
    if (mpp_frame_) {
        mpp_frame_deinit(&mpp_frame_);
        mpp_frame_ = nullptr;
    }
    if (mpp_packet_) {
        mpp_packet_deinit(&mpp_packet_);
        mpp_packet_ = nullptr;
    }
    if (mpp_ctx_) {
        mpp_destroy(mpp_ctx_);
        mpp_ctx_ = nullptr;
    }
}

void CalculateRockchip::Init()
{
    MPP_RET ret = mpp_create(&mpp_ctx_, &mpp_api_);
    if (ret != MPP_OK) {
        std::cerr << "mpp_create failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_create failed");
    }
    MpiCmd mpi_cmd     = MPP_CMD_BASE;
    MppParam mpp_param = nullptr;

    mpi_cmd   = MPP_DEC_SET_PARSER_SPLIT_MODE;
    mpp_param = &need_split_;
    ret       = mpp_api_->control(mpp_ctx_, mpi_cmd, mpp_param);
    if (ret != MPP_OK) {
        std::cerr << "mpp_api_->control failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_api_->control failed");
    }
    ret = mpp_init(mpp_ctx_, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (ret != MPP_OK) {
        std::cerr << "mpp_init failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_init failed");
    }
    MppFrameFormat fmt = MPP_FMT_YUV420SP;
    mpp_param          = &fmt;
    ret                = mpp_api_->control(mpp_ctx_, MPP_DEC_SET_OUTPUT_FORMAT, mpp_param);
    if (ret != MPP_OK) {
        std::cerr << "mpp_api_->control failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_api_->control failed");
    }
    ret = mpp_frame_init(&mpp_frame_);
    if (ret != MPP_OK) {
        std::cerr << "mpp_frame_init failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_frame_init failed");
    }
    ret = mpp_buffer_group_get_internal(&mpp_frame_group_, MPP_BUFFER_TYPE_ION);
    if (ret != MPP_OK) {
        std::cerr << "mpp_buffer_group_get_internal failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_buffer_group_get_internal failed");
    }
    ret = mpp_buffer_group_get_internal(&mpp_packet_group_, MPP_BUFFER_TYPE_ION);
    if (ret != MPP_OK) {
        std::cerr << "mpp_buffer_group_get_internal failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_buffer_group_get_internal failed");
    }
    RK_U32 hor_stride = MPP_ALIGN(video_width_, 16);
    RK_U32 ver_stride = MPP_ALIGN(video_height_, 16);
    ret               = mpp_buffer_get(mpp_frame_group_, &mpp_frame_buffer_, hor_stride * ver_stride * 4);
    if (ret != MPP_OK) {
        std::cerr << "mpp_buffer_get failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_buffer_get failed");
    }
    mpp_frame_set_buffer(mpp_frame_, mpp_frame_buffer_);
    ret = mpp_buffer_get(mpp_packet_group_, &mpp_packet_buffer_, video_width_ * video_height_ * 3);
    if (ret != MPP_OK) {
        std::cerr << "mpp_buffer_get failed, ret = " << ret << std::endl;
        throw std::runtime_error("mpp_buffer_get failed");
    }
    mpp_packet_init_with_buffer(&mpp_packet_, mpp_packet_buffer_);
    data_buffer_ = (uint8_t *)mpp_buffer_get_ptr(mpp_packet_buffer_);
    if (!data_buffer_) {
        std::cerr << "mpp_buffer_get_ptr failed" << std::endl;
        throw std::runtime_error("mpp_buffer_get_ptr failed");
    }
}

bool CalculateRockchip::Yuv422Rgb(const uint8_t *yuyv, uint8_t *rgb, int width, int height)
{
    return Transfer(yuyv, rgb, width, height, RK_FORMAT_YUYV_422, RK_FORMAT_RGB_888);
}

bool CalculateRockchip::Nv12Rgb24(const uint8_t *nv12, uint8_t *rgb, int width, int height)
{
    return Transfer(nv12, rgb, width, height, RK_FORMAT_YCbCr_422_SP, RK_FORMAT_RGB_888);
}

bool CalculateRockchip::Nv12Yuv420p(const uint8_t *nv12, uint8_t *yuv420p, int width, int height)
{
    return Transfer(nv12, yuv420p, width, height, RK_FORMAT_YCbCr_420_SP, RK_FORMAT_YUYV_420);
}

bool CalculateRockchip::TransferRgb888(const uint8_t *raw, uint8_t *rgb, int width, int height, const uint32_t format)
{
    if (width <= 0 || height <= 0 || !raw || !rgb) {
        std::cerr << "Invalid input parameters" << std::endl;
        return false;
    }

    if (format == V4L2_PIX_FMT_MJPEG) {
        return Decode(raw, rgb, width, height, pix_fmt_map_[format], RK_FORMAT_RGB_888);
    }

    return Transfer(raw, rgb, width, height, pix_fmt_map_[format], RK_FORMAT_RGB_888);
}

bool CalculateRockchip::Transfer(const uint8_t *raw, uint8_t *dst, int width, int height, const uint32_t src_format, const uint32_t dst_format)
{
    if (src_format == V4L2_PIX_FMT_MJPEG && pix_fmt_map_.count(dst_format)) {
        return Decode(raw, dst, width, height, pix_fmt_map_[src_format], pix_fmt_map_[dst_format]);
    } else {
        MppFrame frame;
        return mppFrame2DstFormat(frame, dst, pix_fmt_map_[src_format], pix_fmt_map_[dst_format]);
    }
    return true;
}

bool CalculateRockchip::mppFrame2DstFormat(const MppFrame frame, uint8_t *data, const uint32_t src_format, const uint32_t dst_format)
{
    int width        = mpp_frame_get_width(frame);
    int height       = mpp_frame_get_height(frame);
    MppBuffer buffer = mpp_frame_get_buffer(frame);
    memset(data, 0, width * height * 3);
    auto buffer_ptr = mpp_buffer_get_ptr(buffer);
    rga_info_t src_info;
    rga_info_t dst_info;
    // NOTE: memset to zero is MUST
    memset(&src_info, 0, sizeof(rga_info_t));
    memset(&dst_info, 0, sizeof(rga_info_t));
    src_info.fd      = -1;
    src_info.mmuFlag = 1;
    src_info.virAddr = buffer_ptr;
    src_info.format  = src_format;
    dst_info.fd      = -1;
    dst_info.mmuFlag = 1;
    dst_info.virAddr = data;
    dst_info.format  = dst_format;
    rga_set_rect(&src_info.rect, 0, 0, width, height, width, height, src_format);
    rga_set_rect(&dst_info.rect, 0, 0, width, height, width, height, dst_format);
    int ret = c_RkRgaBlit(&src_info, &dst_info, nullptr);
    if (ret) {
        std::cerr << "c_RkRgaBlit error " << ret << " errno " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool CalculateRockchip::Decode(const uint8_t *raw, uint8_t *rgb, int width, int height, const uint32_t src_format, const uint32_t dst_format)
{
    MPP_RET ret          = MPP_OK;
    uint32_t camera_size = buff_size_;
    memset(data_buffer_, 0, camera_size);
    memcpy(data_buffer_, raw, camera_size);
    mpp_packet_set_pos(mpp_packet_, data_buffer_); // MPP 数据包(MppPacket)的当前读写位置
    mpp_packet_set_length(mpp_packet_, camera_size); // MPP 数据包(MppPacket)的有效数据长度
    mpp_packet_set_eos(mpp_packet_); // 设置数据包结束标志

    ret = mpp_api_->poll(mpp_ctx_, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        std::cerr << "mpp poll failed " << ret << std::endl;
        return false;
    }
    ret = mpp_api_->dequeue(mpp_ctx_, MPP_PORT_INPUT, &mpp_task_);
    if (ret != MPP_OK) {
        std::cerr << "mpp dequeue failed " << ret << std::endl;
        return false;
    }
    mpp_task_meta_set_packet(mpp_task_, KEY_INPUT_PACKET, mpp_packet_);
    mpp_task_meta_set_frame(mpp_task_, KEY_OUTPUT_FRAME, mpp_frame_);
    ret = mpp_api_->enqueue(mpp_ctx_, MPP_PORT_INPUT, mpp_task_);
    if (ret != MPP_OK) {
        std::cerr << "mpp enqueue failed " << ret << std::endl;
        return false;
    }
    ret = mpp_api_->poll(mpp_ctx_, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        std::cerr << "mpp poll failed " << ret << std::endl;
        return false;
    }
    ret = mpp_api_->dequeue(mpp_ctx_, MPP_PORT_OUTPUT, &mpp_task_);
    if (ret != MPP_OK) {
        std::cerr << "mpp dequeue failed " << ret << std::endl;
        return false;
    }
    if (mpp_task_) {
        MppFrame output_frame = nullptr;
        mpp_task_meta_get_frame(mpp_task_, KEY_OUTPUT_FRAME, &output_frame);
        if (mpp_frame_) {
            int tmp_width  = mpp_frame_get_width(mpp_frame_);
            int tmp_height = mpp_frame_get_height(mpp_frame_);
            if (width != tmp_width || height != tmp_height) {
                std::cerr << "mpp frame size error " << tmp_height << " " << tmp_height << std::endl;
                return false;
            }

            // std::cout << "src_format " << src_format << " dst_format " << dst_format << std::endl;
            if (src_format == dst_format) {
                // 格式相同则无需转换
                MppBuffer buffer = mpp_frame_get_buffer(mpp_frame_);
                uint8_t* buffer_ptr = (uint8_t*)mpp_buffer_get_ptr(buffer);
                memcpy(rgb, buffer_ptr, tmp_width * tmp_height * 3 / 2);
            } else {
                if (!mppFrame2DstFormat(mpp_frame_, rgb, src_format, dst_format)) {
                    std::cerr << "mpp frame to dst format error" << std::endl;
                    return false;
                }
            }
            if (mpp_frame_get_eos(output_frame)) {
                std::cout << "mpp frame get eos" << std::endl;
            }
        }
        ret = mpp_api_->enqueue(mpp_ctx_, MPP_PORT_OUTPUT, mpp_task_);
        if (ret != MPP_OK) {
            std::cerr << "mpp enqueue failed " << ret << std::endl;
            return false;
        }
        // memcpy(dest, rgb_buffer_, video_width_ * video_height_ * 3);
        return true;
    }
    return false;
}
