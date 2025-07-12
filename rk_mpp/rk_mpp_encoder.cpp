#include "rk_mpp_encoder.h"
#include "calculate_rockchip.h"
#include <iostream>

RkMppEncoder::RkMppEncoder(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    : VideoStream(dev, width, height, fps),
      v4l2_ctx_(nullptr),
      m_ctx(nullptr), m_mpi(nullptr), m_buf_grp(nullptr), m_cfg(nullptr),
      m_width(width), m_height(height), m_fps(fps), m_bitrate(BIT_RATE), m_gop(GOP_SIZE),
      m_frame_size(width * height * 3 / 2)
{
    Init();
}

RkMppEncoder::~RkMppEncoder()
{
    if (m_mpi) {
        mpp_destroy(m_ctx);
        m_ctx = nullptr;
        m_mpi = nullptr;
    }
    
    if (m_buf_grp) {
        mpp_buffer_group_put(m_buf_grp);
        m_buf_grp = nullptr;
    }
    
    if (m_cfg) {
        mpp_enc_cfg_deinit(m_cfg);
        m_cfg = nullptr;
    }
}

bool RkMppEncoder::Init()
{
    std::cout << "RkMppEncoder::Init()" << std::endl;
    v4l2_ctx_ = std::make_shared<V4l2VideoCapture>(dev_name_, video_width_, video_height_, video_fps_);
    // v4l2_ctx_->AddCallback(std::bind(&RkMppEncoder::ProcessImage, this, std::placeholders::_1, std::placeholders::_2));
    camera_buf_ = new (std::nothrow) uint8_t[video_width_ * video_height_ * 3 / 2];

    pixelformat_ = V4L2_PIX_FMT_MJPEG;

    v4l2_ctx_->Init(pixelformat_); // V4L2_PIX_FMT_MJPEG

    if (pixelformat_ == V4L2_PIX_FMT_MJPEG) { // MJPG需要重编码
        yuv420_buf_ = new (std::nothrow) uint8_t[video_width_ * video_height_ * 3 / 2];
        calculate_ptr_ = std::make_shared<CalculateRockchip>(video_width_, video_height_);
        calculate_ptr_->Init();
    }

    MPP_RET ret = MPP_OK;
    
    // 创建MPP上下文和API
    ret = mpp_create(&m_ctx, &m_mpi);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_create failed\n");
        return false;
    }
    
    // 初始化编码器
    ret = mpp_init(m_ctx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_init failed\n");
        return false;
    }
    
    // 创建编码配置
    ret = mpp_enc_cfg_init(&m_cfg);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_enc_cfg_init failed\n");
        return false;
    }
    
    // 获取默认配置
    ret = m_mpi->control(m_ctx, MPP_ENC_GET_CFG, m_cfg);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP_ENC_GET_CFG failed\n");
        return false;
    }
    
    // 设置编码参数
    mpp_enc_cfg_set_s32(m_cfg, "prep:width", m_width);
    mpp_enc_cfg_set_s32(m_cfg, "prep:height", m_height);
    mpp_enc_cfg_set_s32(m_cfg, "prep:hor_stride", m_width);
    mpp_enc_cfg_set_s32(m_cfg, "prep:ver_stride", m_height);
    mpp_enc_cfg_set_s32(m_cfg, "prep:format", MPP_FMT_YUV420SP);
    
    mpp_enc_cfg_set_s32(m_cfg, "rc:mode", MPP_ENC_RC_MODE_CBR);
    mpp_enc_cfg_set_s32(m_cfg, "rc:bps_target", m_bitrate);
    mpp_enc_cfg_set_s32(m_cfg, "rc:bps_max", m_bitrate * 3 / 2);
    mpp_enc_cfg_set_s32(m_cfg, "rc:bps_min", m_bitrate / 2);
    
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_in_flex", 0);
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_in_num", m_fps);
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_in_denorm", 1);
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_out_flex", 0);
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_out_num", m_fps);
    mpp_enc_cfg_set_s32(m_cfg, "rc:fps_out_denorm", 1);
    
    mpp_enc_cfg_set_s32(m_cfg, "rc:gop", m_gop);
    mpp_enc_cfg_set_s32(m_cfg, "codec:type", MPP_VIDEO_CodingAVC);
    
    // 设置编码器配置
    ret = m_mpi->control(m_ctx, MPP_ENC_SET_CFG, m_cfg);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP_ENC_SET_CFG failed\n");
        return false;
    }
    
    // 创建缓冲区组
    ret = mpp_buffer_group_get_internal(&m_buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_buffer_group_get failed\n");
        return false;
    }
    return true;
}

bool RkMppEncoder::EncodeFrame(void* inputData, uint8_t* &outputData, size_t* outputSize) {
    MPP_RET ret = MPP_OK;
    MppBuffer input_buf = nullptr;
    MppBuffer output_buf = nullptr;
    MppPacket packet = nullptr;
    MppFrame frame = nullptr;
    MppTask task = nullptr;
    
    // 创建输入帧
    ret = mpp_frame_init(&frame);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_frame_init failed\n");
        return false;
    }
    
    // 设置输入帧属性
    mpp_frame_set_width(frame, m_width);
    mpp_frame_set_height(frame, m_height);
    mpp_frame_set_hor_stride(frame, m_width);
    mpp_frame_set_ver_stride(frame, m_height);
    mpp_frame_set_fmt(frame, MPP_FMT_YUV420SP);
    mpp_frame_set_eos(frame, 0);
    
    // 创建输入缓冲区
    ret = mpp_buffer_get(m_buf_grp, &input_buf, m_frame_size);
    if (ret != MPP_OK) {
        fprintf(stderr, "mpp_buffer_get failed\n");
        mpp_frame_deinit(&frame);
        return false;
    }
    
    // 复制输入数据到缓冲区
    memcpy(mpp_buffer_get_ptr(input_buf), inputData, m_frame_size);
    mpp_frame_set_buffer(frame, input_buf);
    
    // 提交帧到编码器
    ret = m_mpi->poll(m_ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP input poll failed\n");
        mpp_buffer_put(input_buf);
        mpp_frame_deinit(&frame);
        return false;
    }
    
    ret = m_mpi->dequeue(m_ctx, MPP_PORT_INPUT, &task);
    if (ret != MPP_OK || !task) {
        fprintf(stderr, "MPP input dequeue failed\n");
        mpp_buffer_put(input_buf);
        mpp_frame_deinit(&frame);
        return false;
    }
    
    mpp_task_meta_set_frame(task, KEY_INPUT_FRAME, frame);
    mpp_task_meta_set_packet(task, KEY_OUTPUT_PACKET, packet);
    
    ret = m_mpi->enqueue(m_ctx, MPP_PORT_INPUT, task);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP input enqueue failed\n");
        mpp_buffer_put(input_buf);
        mpp_frame_deinit(&frame);
        return false;
    }
    
    // 获取编码后的数据
    ret = m_mpi->poll(m_ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret != MPP_OK) {
        fprintf(stderr, "MPP output poll failed\n");
        return false;
    }
    
    ret = m_mpi->dequeue(m_ctx, MPP_PORT_OUTPUT, &task);
    if (ret != MPP_OK || !task) {
        fprintf(stderr, "MPP output dequeue failed\n");
        return false;
    }
    
    ret = mpp_task_meta_get_packet(task, KEY_OUTPUT_PACKET, &packet);
    if (ret != MPP_OK || !packet) {
        fprintf(stderr, "MPP get packet failed\n");
        return false;
    }
    
    // 获取编码后的数据指针和大小
    void* data = mpp_packet_get_data(packet);
    size_t length = mpp_packet_get_length(packet);
    
    // 复制输出数据
    outputData = new uint8_t[length];
    if (!outputData) {
        fprintf(stderr, "malloc output buffer failed\n");
        return false;
    }
    
    memcpy(outputData, data, length);
    *outputSize = length;
    
    // 释放资源
    mpp_buffer_put(input_buf);
    mpp_frame_deinit(&frame);
    mpp_task_meta_set_packet(task, KEY_OUTPUT_PACKET, nullptr);
    m_mpi->enqueue(m_ctx, MPP_PORT_OUTPUT, task);
    
    return true;
}

int32_t RkMppEncoder::getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes)
{
    uint8_t * target_buff;
    memset(camera_buf_, 0, video_width_ * video_height_ * 3 / 2);
    uint32_t len = v4l2_ctx_->BuffOneFrame(camera_buf_);

    if (calculate_ptr_) {
        memset(yuv420_buf_, 0, video_width_ * video_height_ * 3 / 2);
        calculate_ptr_->SetBufferSize(len);
        calculate_ptr_->Transfer(camera_buf_, yuv420_buf_, video_width_, video_height_, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV12);
        target_buff = yuv420_buf_;
    } else {
        target_buff = camera_buf_;
    }

    uint8_t* encoded_data = nullptr;
    size_t encoded_size = 0;
    if (!EncodeFrame(target_buff, encoded_data, &encoded_size)) {
        fprintf(stderr, "Encode frame failed\n");
        return -1;
    }

    if (encoded_size < fMaxSize) {
        memcpy(fTo, encoded_data, encoded_size);
        fFrameSize         = encoded_size;
        fNumTruncatedBytes = 0;
    } else {
        memcpy(fTo, encoded_data, fMaxSize);
        fNumTruncatedBytes = encoded_size - fMaxSize;
        fFrameSize         = fMaxSize;
    }
    delete[] encoded_data;
    return fFrameSize;
}
