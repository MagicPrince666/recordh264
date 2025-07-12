#ifndef __CALCULATE_CPU_H__
#define __CALCULATE_CPU_H__

#include "calculate.h"
#include <unordered_map>
#include <functional>

class CalculateCpu : public Calculate
{
public:
    CalculateCpu();
    ~CalculateCpu();

    void Init();

    bool Yuv422Rgb(const uint8_t* yuv, uint8_t* rgb, int width, int height);

    bool Nv12Rgb24(const uint8_t* nv12, uint8_t* rgb, int width, int height);

    bool TransferRgb888(const uint8_t* raw, uint8_t* rgb, int width, int height, const uint32_t format);

    bool Nv12Yuv420p(const uint8_t* nv12, uint8_t* yuv420p, int width, int height);

    bool Transfer(const uint8_t* raw, uint8_t* dst, int width, int height, const uint32_t src_format, const uint32_t dst_format);

private:
    std::unordered_map<uint32_t, std::function<bool(const uint8_t*, uint8_t*, int, int)>> pix_fmt_fun_map_;
};

#endif
