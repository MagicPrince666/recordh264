/**
 * @file avilib.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 录制视频
 * @version 0.1
 * @date 2022-11-27
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#ifndef __AVILIB_H__
#define __AVILIB_H__

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

// SLM
#ifdef __CYGWIN__
#include <sys/types.h>
#elif defined WIN32
#if defined __GNUWIN32__
#include <stdint.h>
#else
#define uint32_t unsigned __int32
#define uint8_t unsigned __int8
#define uint16_t unsigned __int16
#define uint64_t unsigned __int64
#endif
#else
#include <inttypes.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#define AVI_MAX_TRACKS 8

typedef struct
{
    off_t key;
    off_t pos;
    off_t len;
} video_index_entry;

typedef struct
{
    off_t pos;
    off_t len;
    off_t tot;
} audio_index_entry;

typedef struct track_s {

    long a_fmt;   /* Audio format, see #defines below */
    long a_chans; /* Audio channels, 0 for no audio */
    long a_rate;  /* Rate in Hz */
    long a_bits;  /* bits per audio sample */
    long mp3rate; /* mp3 bitrate kbs*/

    long audio_strn;   /* Audio stream number */
    off_t audio_bytes; /* Total number of bytes of audio data */
    long audio_chunks; /* Chunks of audio data in the file */

    char audio_tag[4]; /* Tag of audio data */
    long audio_posc;   /* Audio position: chunk */
    long audio_posb;   /* Audio position: byte within chunk */

    off_t a_codech_off; /* absolut offset of audio codec information */
    off_t a_codecf_off; /* absolut offset of audio codec information */

    audio_index_entry *audio_index;

} track_t;

typedef struct
{
    uint32_t bi_size;
    uint32_t bi_width;
    uint32_t bi_height;
    uint16_t bi_planes;
    uint16_t bi_bit_count;
    uint32_t bi_compression;
    uint32_t bi_size_image;
    uint32_t bi_x_pels_per_meter;
    uint32_t bi_y_pels_per_meter;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
} BITMAPINFOHEADER_avilib;

typedef struct
{
    uint16_t w_format_tag;
    uint16_t n_channels;
    uint32_t n_samples_per_sec;
    uint32_t n_avg_bytes_per_sec;
    uint16_t n_block_align;
    uint16_t w_bits_per_sample;
    uint16_t cb_size;
} WAVEFORMATEX_avilib;

typedef struct
{
    uint32_t fcc_type;
    uint32_t fcc_handler;
    uint32_t dw_flags;
    uint32_t dw_caps;
    uint16_t w_priority;
    uint16_t w_language;
    uint32_t dw_scale;
    uint32_t dw_rate;
    uint32_t dw_start;
    uint32_t dw_length;
    uint32_t dw_initial_frames;
    uint32_t dw_suggested_buffer_size;
    uint32_t dw_quality;
    uint32_t dw_sample_size;
    uint32_t dw_left;
    uint32_t dw_top;
    uint32_t dw_right;
    uint32_t dw_bottom;
    uint32_t dw_edit_count;
    uint32_t dw_format_change_count;
    char sz_name[64];
} AVISTREAMINFO;

typedef struct
{

    long fdes; /* File descriptor of AVI file */
    long mode; /* 0 for reading, 1 for writing */

    long width;          /* Width  of a video frame */
    long height;         /* Height of a video frame */
    double fps;          /* Frames per second */
    char compressor[8];  /* Type of compressor, 4 bytes + padding for 0 byte */
    char compressor2[8]; /* Type of compressor, 4 bytes + padding for 0 byte */
    long video_strn;     /* Video stream number */
    long video_frames;   /* Number of video frames */
    char video_tag[4];   /* Tag of video data */
    long video_pos;      /* Number of next frame to be read
                (if index present) */

    unsigned long max_len; /* maximum video chunk present */

    track_t track[AVI_MAX_TRACKS]; // up to AVI_MAX_TRACKS audio tracks supported

    off_t pos;    /* position in file */
    long n_idx;   /* number of index entries actually filled */
    long max_idx; /* number of index entries actually allocated */

    off_t v_codech_off; /* absolut offset of video codec (strh) info */
    off_t v_codecf_off; /* absolut offset of video codec (strf) info */

    unsigned char (*idx)[16]; /* index entries (AVI idx1 tag) */
    video_index_entry *video_index;

    off_t last_pos;         /* Position of last frame written */
    unsigned long last_len; /* Length of last frame written */
    int must_use_index;     /* Flag if frames are duplicated */
    off_t movi_start;

    int anum; // total number of audio tracks
    int aptr; // current audio working track

    BITMAPINFOHEADER_avilib *bitmap_info_header;
    WAVEFORMATEX_avilib *wave_format_ex[AVI_MAX_TRACKS];
} avi_t;

#define AVI_MODE_WRITE 0
#define AVI_MODE_READ 1

/* The error codes delivered by avi_open_input_file */
#define AVI_ERR_SIZELIM 1     /* The write of the data would exceed   \
                                 the maximum size of the AVI file.    \
                                 This is more a warning than an error \
                                 since the file may be closed safely */
#define AVI_ERR_OPEN 2        /* Error opening the AVI file - wrong path \
                                 name or file nor readable/writable */
#define AVI_ERR_READ 3        /* Error reading from AVI File */
#define AVI_ERR_WRITE 4       /* Error writing to AVI File, \
                                 disk full ??? */
#define AVI_ERR_WRITE_INDEX 5 /* Could not write index to AVI file \
                                 during close, file may still be   \
                                 usable */
#define AVI_ERR_CLOSE 6       /* Could not write header to AVI file     \
                                 or not truncate the file during close, \
                                 file is most probably corrupted */
#define AVI_ERR_NOT_PERM 7    /* Operation not permitted:        \
                                 trying to read from a file open \
                                 for writing or vice versa */
#define AVI_ERR_NO_MEM 8      /* malloc failed */
#define AVI_ERR_NO_AVI 9      /* Not an AVI file */
#define AVI_ERR_NO_HDRL 10    /* AVI file has no has no header list, \
                                 corrupted ??? */
#define AVI_ERR_NO_MOVI 11    /* AVI file has no has no MOVI list, \
                                 corrupted ??? */
#define AVI_ERR_NO_VIDS 12    /* AVI file contains no video data */
#define AVI_ERR_NO_IDX 13     /* The file has been opened with          \
                                 getIndex==0, but an operation has been \
                                 performed that needs an index */

/* Possible Audio formats */

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_UNKNOWN (0x0000)
#define WAVE_FORMAT_PCM (0x0001)
#define WAVE_FORMAT_ADPCM (0x0002)
#define WAVE_FORMAT_IBM_CVSD (0x0005)
#define WAVE_FORMAT_ALAW (0x0006)
#define WAVE_FORMAT_MULAW (0x0007)
#define WAVE_FORMAT_OKI_ADPCM (0x0010)
#define WAVE_FORMAT_DVI_ADPCM (0x0011)
#define WAVE_FORMAT_DIGISTD (0x0015)
#define WAVE_FORMAT_DIGIFIX (0x0016)
#define WAVE_FORMAT_YAMAHA_ADPCM (0x0020)
#define WAVE_FORMAT_DSP_TRUESPEECH (0x0022)
#define WAVE_FORMAT_GSM610 (0x0031)
#define IBM_FORMAT_MULAW (0x0101)
#define IBM_FORMAT_ALAW (0x0102)
#define IBM_FORMAT_ADPCM (0x0103)
#endif

#define HEADERBYTES 2048
#define AVI_MAX_LEN (UINT_MAX - (1 << 20) * 16 - HEADERBYTES)
#define FRAME_RATE_SCALE 1000000
#define MAX_INFO_STRLEN 64

#define OUT4CC(s)                       \
    if (nhb <= HEADERBYTES - 4)         \
        memcpy(AVI_header + nhb, s, 4); \
    nhb += 4

#define OUTLONG(n)                     \
    if (nhb <= HEADERBYTES - 4)        \
        Long2Str(AVI_header + nhb, n); \
    nhb += 4

#define OUTSHRT(n)                             \
    if (nhb <= HEADERBYTES - 2) {              \
        AVI_header[nhb]     = (n)&0xff;        \
        AVI_header[nhb + 1] = (n >> 8) & 0xff; \
    }                                          \
    nhb += 2
#define PAD_EVEN(x) (((x) + 1) & ~1)

class AviLib
{
public:
    AviLib(std::string filename);
    ~AviLib();

    bool AviOpenOutputFile();
    void AviSetVideo(int width, int height, double fps, char *compressor);
    int AviWriteFrame(char *data, long bytes, int keyframe);

private:
    struct riff_struct {
        unsigned char id[4]; /* RIFF */
        uint32_t len;
        unsigned char wave_id[4]; /* WAVE */
    };

    struct chunk_struct {
        unsigned char id[4];
        uint32_t len;
    };

    struct common_struct {
        uint16_t wFormatTag;
        uint16_t wChannels;
        uint32_t dwSamplesPerSec;
        uint32_t dwAvgBytesPerSec;
        uint16_t wBlockAlign;
        uint16_t wBitsPerSample; /* Only for PCM */
    };

    struct wave_header {
        struct riff_struct riff;
        struct chunk_struct format;
        struct common_struct common;
        struct chunk_struct data;
    };

    struct AVIStreamHeader {
        long fccType;
        long fccHandler;
        long dwFlags;
        long dwPriority;
        long dwInitialFrames;
        long dwScale;
        long dwRate;
        long dwStart;
        long dwLength;
        long dwSuggestedBufferSize;
        long dwQuality;
        long dwSampleSize;
    };

    avi_t *avi_;
    char id_str_[MAX_INFO_STRLEN];
    int64_t avi_errno_;
    std::string avi_file_name_;

private:
    int AviUpdateHeader();
    size_t AviWrite(int fd, char *buf, size_t len);
    void Long2Str(unsigned char *dst, int n);
    int AviSampSize(int j);
    int AviAddChunk(unsigned char *tag, unsigned char *data, int length);
    int AviAddIndexEntry(unsigned char *tag, long flags, unsigned long pos, unsigned long len);
    int AviWriteData(char *data, unsigned long length, int audio, int keyframe);
};

#endif
