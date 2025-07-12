#include <cstring>
#include "calculate_cpu.h"
#if defined(__linux__)
#include <linux/videodev2.h>
#endif

CalculateCpu::CalculateCpu() {}

CalculateCpu::~CalculateCpu() {}

void CalculateCpu::Init()
{
#if defined(__linux__)
    pix_fmt_fun_map_[V4L2_PIX_FMT_NV12] = std::bind(&CalculateCpu::Nv12Rgb24, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    pix_fmt_fun_map_[V4L2_PIX_FMT_YUYV] = std::bind(&CalculateCpu::Yuv422Rgb, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
#endif
}

bool CalculateCpu::Yuv422Rgb(const uint8_t* yuyv, uint8_t* rgb, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int yuyvIndex = y * width * 2 + x * 2;
            int rgbIndex = y * width * 3 + x * 3;

            uint8_t y0 = yuyv[yuyvIndex];
            uint8_t u = yuyv[yuyvIndex + 1];
            uint8_t y1 = yuyv[yuyvIndex + 2];
            uint8_t v = yuyv[yuyvIndex + 3];

            float r0, g0, b0, r1, g1, b1;
            r0 = y0 + 1.402f * (v - 128);
            g0 = y0 - 0.344136f * (u - 128) - 0.714136f * (v - 128);
            b0 = y0 + 1.772f * (u - 128);

            r1 = y1 + 1.402f * (v - 128);
            g1 = y1 - 0.344136f * (u - 128) - 0.714136f * (v - 128);
            b1 = y1 + 1.772f * (u - 128);

            rgb[rgbIndex] = b0;
            rgb[rgbIndex + 1] = g0;
            rgb[rgbIndex + 2] = r0;

            rgb[rgbIndex + 3] = b1;
            rgb[rgbIndex + 4] = g1;
            rgb[rgbIndex + 5] = r1;
        }
    }
    return true;
}

bool CalculateCpu::Nv12Rgb24(const uint8_t* nv12, uint8_t* rgb, int width, int height)
{
    if (width <= 0 || height <= 0 || !nv12 || !rgb) {
        std::cerr << "Invalid input parameters" << std::endl;
        return false;
    }
    
    const int y_size = width * height;
    const int uv_size = y_size / 2;  // UV分量大小
    
    const uint8_t* y_plane = nv12;         // Y分量平面
    const uint8_t* uv_plane = nv12 + y_size; // UV交错平面
    
    // 预计算系数 (基于ITU-R BT.601标准)
    const int32_t y_coeff = 1192;   // 1.164 * 1024
    const int32_t u_b_coeff = 2066; // 2.018 * 1024
    const int32_t u_g_coeff = -400; // -0.391 * 1024
    const int32_t v_g_coeff = -832; // -0.813 * 1024
    const int32_t v_r_coeff = 1634; // 1.596 * 1024
    
    // 处理图像中的每个像素
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 获取Y分量
            const int y_idx = y * width + x;
            const int32_t y_val = y_plane[y_idx] - 16;
            
            // 计算UV索引 (NV12使用4:2:0采样)
            const int uv_row = y / 2;
            const int uv_col = x / 2;
            const int uv_idx = uv_row * (width / 2) + uv_col;
            
            // 获取UV分量 (UV交错存储)
            const int32_t u_val = uv_plane[2 * uv_idx] - 128;
            const int32_t v_val = uv_plane[2 * uv_idx + 1] - 128;
            
            // 使用整数运算进行YUV->RGB转换
            int32_t r = (y_coeff * y_val + v_r_coeff * v_val) >> 10;
            int32_t g = (y_coeff * y_val + u_g_coeff * u_val + v_g_coeff * v_val) >> 10;
            int32_t b = (y_coeff * y_val + u_b_coeff * u_val) >> 10;
            
            // 裁剪到[0, 255]范围
            r = (r < 0) ? 0 : (r > 255) ? 255 : r;
            g = (g < 0) ? 0 : (g > 255) ? 255 : g;
            b = (b < 0) ? 0 : (b > 255) ? 255 : b;
            
            // 存储RGB值 (每个像素3个字节)
            const int rgb_idx = (y * width + x) * 3;
            rgb[rgb_idx]     = static_cast<uint8_t>(r);
            rgb[rgb_idx + 1] = static_cast<uint8_t>(g);
            rgb[rgb_idx + 2] = static_cast<uint8_t>(b);
        }
    }
    return true;
}

bool CalculateCpu::Nv12Yuv420p(const uint8_t* nv12, uint8_t* yuv420p, int width, int height)
{
    // 计算各平面大小
    int ySize = width * height;
    int uvSize = ySize / 4;  // 420采样，UV平面是Y平面的1/4
    
    // 创建输出缓冲区
    // std::vector<uint8_t> yuv420p;
    // yuv420p.resize(ySize + uvSize * 2);  // Y + U + V
    
    // 获取各平面指针
    uint8_t* yPlane = yuv420p;
    uint8_t* uPlane = yuv420p + ySize;
    uint8_t* vPlane = yuv420p + ySize + uvSize;
    
    // NV12的Y平面直接复制
    memcpy(yPlane, nv12, ySize);
    
    // 处理UV平面（NV12的UV是交错排列的）
    const uint8_t* uvSrc = nv12 + ySize;
    for (int i = 0; i < uvSize; ++i) {
        uPlane[i] = uvSrc[2 * i];      // U分量
        vPlane[i] = uvSrc[2 * i + 1];   // V分量
    }
    return true;
}

bool CalculateCpu::TransferRgb888(const uint8_t* raw, uint8_t* rgb, int width, int height, const uint32_t format)
{
    if (pix_fmt_fun_map_.count(format)) {
        return pix_fmt_fun_map_[format](raw, rgb, width, height);
    }
    return false;
}

bool CalculateCpu::Transfer(const uint8_t* raw, uint8_t* dst, int width, int height, const uint32_t src_format, const uint32_t dst_format)
{
    return true;
}
