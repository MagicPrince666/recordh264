#ifndef __CALCULATE_H__
#define __CALCULATE_H__

#include <iostream>

class Calculate {
public:
    Calculate() {}
    ~Calculate() {}

    virtual void Init() = 0;

    virtual bool Yuv422Rgb(const uint8_t* yuv, uint8_t* rgb, int width, int height) = 0;

    virtual bool Nv12Rgb24(const uint8_t* nv12, uint8_t* rgb, int width, int height) = 0;

    virtual bool TransferRgb888(const uint8_t* raw, uint8_t* rgb, int width, int height, const uint32_t format) = 0;

    virtual bool Nv12Yuv420p(const uint8_t* nv12, uint8_t* yuv420p, int width, int height) = 0;

    virtual bool Transfer(const uint8_t* raw, uint8_t* dst, int width, int height, const uint32_t src_format, const uint32_t dst_format) = 0;

    void SetBufferSize(uint32_t size) {
        buff_size_ = size;
    }

protected:
    uint32_t buff_size_;
};

#endif
