/*
 * Copyright 2022 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MPP_DEBUG_H__
#define __MPP_DEBUG_H__

#include <stdlib.h>

#include "rockchip/rk_type.h"
#include "rockchip/mpp_err.h"
#include "rockchip/mpp_log.h"

#define MPP_DBG_TIMING                  (0x00000001)
#define MPP_DBG_PTS                     (0x00000002)
#define MPP_DBG_INFO                    (0x00000004)
#define MPP_DBG_PLATFORM                (0x00000010)

#define MPP_DBG_DUMP_LOG                (0x00000100)
#define MPP_DBG_DUMP_IN                 (0x00000200)
#define MPP_DBG_DUMP_OUT                (0x00000400)
#define MPP_DBG_DUMP_CFG                (0x00000800)

#define _mpp_dbg(debug, flag, fmt, ...)     mpp_log_c((debug) & (flag), fmt, ## __VA_ARGS__)
#define _mpp_dbg_f(debug, flag, fmt, ...)   mpp_log_cf((debug) & (flag), fmt, ## __VA_ARGS__)

#define mpp_dbg(flag, fmt, ...)         _mpp_dbg(mpp_debug, flag, fmt, ## __VA_ARGS__)
#define mpp_dbg_f(flag, fmt, ...)       _mpp_dbg_f(mpp_debug, flag, fmt, ## __VA_ARGS__)

#define mpp_dbg_pts(fmt, ...)           mpp_dbg(MPP_DBG_PTS, fmt, ## __VA_ARGS__)
#define mpp_dbg_info(fmt, ...)          mpp_dbg(MPP_DBG_INFO, fmt, ## __VA_ARGS__)
#define mpp_dbg_platform(fmt, ...)      mpp_dbg(MPP_DBG_PLATFORM, fmt, ## __VA_ARGS__)

#define MPP_ABORT                       (0x10000000)

/*
 * mpp_dbg usage:
 *
 * in h264d module define module debug flag variable like: h265d_debug
 * then define h265d_dbg macro as follow :
 *
 * extern RK_U32 h265d_debug;
 *
 * #define H265D_DBG_FUNCTION          (0x00000001)
 * #define H265D_DBG_VPS               (0x00000002)
 * #define H265D_DBG_SPS               (0x00000004)
 * #define H265D_DBG_PPS               (0x00000008)
 * #define H265D_DBG_SLICE_HDR         (0x00000010)
 *
 * #define h265d_dbg(flag, fmt, ...) mpp_dbg(h265d_debug, flag, fmt, ## __VA_ARGS__)
 *
 * finally use environment control the debug flag
 *
 * mpp_get_env_u32("h264d_debug", &h265d_debug, 0)
 *
 */
/*
 * sub-module debug flag usage example:
 * +------+-------------------+
 * | 8bit |      24bit        |
 * +------+-------------------+
 *  0~15 bit: software debug print
 * 16~23 bit: hardware debug print
 * 24~31 bit: information print format
 */

#define mpp_abort() do {                \
    if (mpp_debug & MPP_ABORT) {        \
        abort();                        \
    }                                   \
} while (0)

#define MPP_STRINGS(x)      MPP_TO_STRING(x)
#define MPP_TO_STRING(x)    #x

#define mpp_assert(cond) do {                                           \
    if (!(cond)) {                                                      \
        mpp_err("Assertion %s failed at %s:%d\n",                       \
               MPP_STRINGS(cond), __FUNCTION__, __LINE__);              \
        mpp_abort();                                                    \
    }                                                                   \
} while (0)


#ifdef __cplusplus
extern "C" {
#endif

extern RK_U32 mpp_debug;

#ifdef __cplusplus
}
#endif

#endif /*__MPP_DEBUG_H__*/
