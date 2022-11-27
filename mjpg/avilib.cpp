// SLM
#ifdef WIN32
#include <io.h>
#define ftruncate _chsize
#define strncasecmp _strnicmp
typedef int ssize_t;
#endif

#ifdef __CYGWIN__
#include <unistd.h>
#endif

#include "avilib.h"
//#include <time.h>

#define HEADERBYTES 2048
#define AVI_MAX_LEN (UINT_MAX - (1 << 20) * 16 - HEADERBYTES)
#define FRAME_RATE_SCALE 1000000
#define MAX_INFO_STRLEN 64

static char id_str[MAX_INFO_STRLEN];

#define OUT4CC(s)                       \
    if (nhb <= HEADERBYTES - 4)         \
        memcpy(AVI_header + nhb, s, 4); \
    nhb += 4

#define OUTLONG(n)                     \
    if (nhb <= HEADERBYTES - 4)        \
        long2str(AVI_header + nhb, n); \
    nhb += 4

#define OUTSHRT(n)                             \
    if (nhb <= HEADERBYTES - 2) {              \
        AVI_header[nhb]     = (n)&0xff;        \
        AVI_header[nhb + 1] = (n >> 8) & 0xff; \
    }                                          \
    nhb += 2
#define PAD_EVEN(x) (((x) + 1) & ~1)

long AVI_errno = 0;

static size_t avi_write(int fd, char *buf, size_t len)
{
    size_t n = 0;
    size_t r = 0;

    while (r < len) {
        n = write(fd, buf + r, len - r);
        if ((ssize_t)n < 0)
            return n;

        r += n;
    }
    return r;
}

static void long2str(unsigned char *dst, int n)
{
    dst[0] = (n)&0xff;
    dst[1] = (n >> 8) & 0xff;
    dst[2] = (n >> 16) & 0xff;
    dst[3] = (n >> 24) & 0xff;
}

static int avi_sampsize(avi_t *AVI, int j)
{
    int s;
    s = ((AVI->track[j].a_bits + 7) / 8) * AVI->track[j].a_chans;
    //   if(s==0) s=1; /* avoid possible zero divisions */
    if (s < 4)
        s = 4; /* avoid possible zero divisions */
    return s;
}

/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk(avi_t *AVI, unsigned char *tag, unsigned char *data, int length)
{
    unsigned char c[8];

    /* Copy tag and length int c, so that we need only 1 write system call
       for these two values */

    memcpy(c, tag, 4);
    long2str(c + 4, length);

    /* Output tag, length and data, restore previous position
       if the write fails */

    length = PAD_EVEN(length);

    if (avi_write(AVI->fdes, (char *)c, 8) != 8 ||
        avi_write(AVI->fdes, (char *)data, length) != (unsigned int)length) {
        lseek(AVI->fdes, AVI->pos, SEEK_SET);
        AVI_errno = AVI_ERR_WRITE;
        return -1;
    }

    /* Update file position */

    AVI->pos += 8 + length;

    // fprintf(stderr, "pos=%lu %s\n", AVI->pos, tag);

    return 0;
}

static int avi_add_index_entry(avi_t *AVI, unsigned char *tag, long flags, unsigned long pos, unsigned long len)
{
    void *ptr;

    if (AVI->n_idx >= AVI->max_idx) {
        ptr = realloc((void *)AVI->idx, (AVI->max_idx + 4096) * 16);

        if (ptr == 0) {
            AVI_errno = AVI_ERR_NO_MEM;
            return -1;
        }
        AVI->max_idx += 4096;
        AVI->idx = (unsigned char((*)[16]))ptr;
    }

    /* Add index entry */

    //   fprintf(stderr, "INDEX %s %ld %lu %lu\n", tag, flags, pos, len);

    memcpy(AVI->idx[AVI->n_idx], tag, 4);
    long2str(AVI->idx[AVI->n_idx] + 4, flags);
    long2str(AVI->idx[AVI->n_idx] + 8, pos);
    long2str(AVI->idx[AVI->n_idx] + 12, len);

    /* Update counter */

    AVI->n_idx++;

    if (len > AVI->max_len)
        AVI->max_len = len;

    return 0;
}

avi_t *AVI_open_output_file(char *filename)
{
    avi_t *AVI;
    int i;

    int mask;

    unsigned char AVI_header[HEADERBYTES];

    /* Allocate the avi_t struct and zero it */

    AVI = (avi_t *)malloc(sizeof(avi_t));
    if (AVI == 0) {
        AVI_errno = AVI_ERR_NO_MEM;
        return 0;
    }
    memset((void *)AVI, 0, sizeof(avi_t));

    /* Since Linux needs a long time when deleting big files,
       we do not truncate the file when we open it.
       Instead it is truncated when the AVI file is closed */

    mask = umask(0);
    umask(mask);

#ifdef WIN32
    AVI->fdes = open(filename, O_RDWR | O_CREAT | O_BINARY, (S_IRUSR | S_IWUSR) & ~mask);
#else
    AVI->fdes = open(filename, O_RDWR | O_CREAT, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) & ~mask);
#endif
    if (AVI->fdes < 0) {
        AVI_errno = AVI_ERR_OPEN;
        free(AVI);
        return 0;
    }

    /* Write out HEADERBYTES bytes, the header will go here
       when we are finished with writing */

    for (i = 0; i < HEADERBYTES; i++)
        AVI_header[i] = 0;
    i = avi_write(AVI->fdes, (char *)AVI_header, HEADERBYTES);
    if (i != HEADERBYTES) {
        close(AVI->fdes);
        AVI_errno = AVI_ERR_WRITE;
        free(AVI);
        return 0;
    }

    AVI->pos  = HEADERBYTES;
    AVI->mode = AVI_MODE_WRITE; /* open for writing */

    // init
    AVI->anum = 0;
    AVI->aptr = 0;

    return AVI;
}

//

// ThOe write preliminary AVI file header: 0 frames, max vid/aud size
int avi_update_header(avi_t *AVI)
{
    int njunk, sampsize, hasIndex, ms_per_frame, frate, flag;
    int movi_len, hdrl_start, strl_start, j;
    unsigned char AVI_header[HEADERBYTES];
    long nhb;

    // assume max size
    movi_len = AVI_MAX_LEN - HEADERBYTES + 4;

    // assume index will be written
    hasIndex = 1;

    if (AVI->fps < 0.001) {
        frate        = 0;
        ms_per_frame = 0;
    } else {
        frate        = (int)(FRAME_RATE_SCALE * AVI->fps + 0.5);
        ms_per_frame = (int)(1000000 / AVI->fps + 0.5);
    }

    /* Prepare the file header */

    nhb = 0;

    /* The RIFF header */

    OUT4CC("RIFF");
    OUTLONG(movi_len); // assume max size
    OUT4CC("AVI ");

    /* Start the header list */

    OUT4CC("LIST");
    OUTLONG(0);       /* Length of list in bytes, don't know yet */
    hdrl_start = nhb; /* Store start position */
    OUT4CC("hdrl");

    /* The main AVI header */

    /* The Flags in AVI File header */

#define AVIF_HASINDEX 0x00000010 /* Index at end of file */
#define AVIF_MUSTUSEINDEX 0x00000020
#define AVIF_ISINTERLEAVED 0x00000100
#define AVIF_TRUSTCKTYPE 0x00000800 /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED 0x00020000

    OUT4CC("avih");
    OUTLONG(56);           /* # of bytes to follow */
    OUTLONG(ms_per_frame); /* Microseconds per frame */
    // ThOe ->0
    //    OUTLONG(10000000);           /* MaxBytesPerSec, I hope this will never be used */
    OUTLONG(0);
    OUTLONG(0); /* PaddingGranularity (whatever that might be) */
                /* Other sources call it 'reserved' */
    flag = AVIF_ISINTERLEAVED;
    if (hasIndex)
        flag |= AVIF_HASINDEX;
    if (hasIndex && AVI->must_use_index)
        flag |= AVIF_MUSTUSEINDEX;
    OUTLONG(flag); /* Flags */
    OUTLONG(0);    // no frames yet
    OUTLONG(0);    /* InitialFrames */

    OUTLONG(AVI->anum + 1);

    OUTLONG(0);           /* SuggestedBufferSize */
    OUTLONG(AVI->width);  /* Width */
    OUTLONG(AVI->height); /* Height */
                          /* MS calls the following 'reserved': */
    OUTLONG(0);           /* TimeScale:  Unit used to measure time */
    OUTLONG(0);           /* DataRate:   Data rate of playback     */
    OUTLONG(0);           /* StartTime:  Starting time of AVI data */
    OUTLONG(0);           /* DataLength: Size of AVI data chunk    */

    /* Start the video stream list ---------------------------------- */

    OUT4CC("LIST");
    OUTLONG(0);       /* Length of list in bytes, don't know yet */
    strl_start = nhb; /* Store start position */
    OUT4CC("strl");

    /* The video stream header */

    OUT4CC("strh");
    OUTLONG(56);               /* # of bytes to follow */
    OUT4CC("vids");            /* Type */
    OUT4CC(AVI->compressor);   /* Handler */
    OUTLONG(0);                /* Flags */
    OUTLONG(0);                /* Reserved, MS says: wPriority, wLanguage */
    OUTLONG(0);                /* InitialFrames */
    OUTLONG(FRAME_RATE_SCALE); /* Scale */
    OUTLONG(frate);            /* Rate: Rate/Scale == samples/second */
    OUTLONG(0);                /* Start */
    OUTLONG(0);                // no frames yet
    OUTLONG(0);                /* SuggestedBufferSize */
    OUTLONG(-1);               /* Quality */
    OUTLONG(0);                /* SampleSize */
    OUTLONG(0);                /* Frame */
    OUTLONG(0);                /* Frame */
    //   OUTLONG(0);                  /* Frame */
    // OUTLONG(0);                  /* Frame */

    /* The video stream format */

    OUT4CC("strf");
    OUTLONG(40);          /* # of bytes to follow */
    OUTLONG(40);          /* Size */
    OUTLONG(AVI->width);  /* Width */
    OUTLONG(AVI->height); /* Height */
    OUTSHRT(1);
    OUTSHRT(24);             /* Planes, Count */
    OUT4CC(AVI->compressor); /* Compression */
    // ThOe (*3)
    OUTLONG(AVI->width * AVI->height * 3); /* SizeImage (in bytes?) */
    OUTLONG(0);                            /* XPelsPerMeter */
    OUTLONG(0);                            /* YPelsPerMeter */
    OUTLONG(0);                            /* ClrUsed: Number of colors used */
    OUTLONG(0);                            /* ClrImportant: Number of colors important */

    /* Finish stream list, i.e. put number of bytes in the list to proper pos */

    long2str(AVI_header + strl_start - 4, nhb - strl_start);

    /* Start the audio stream list ---------------------------------- */

    for (j = 0; j < AVI->anum; ++j) {

        sampsize = avi_sampsize(AVI, j);

        OUT4CC("LIST");
        OUTLONG(0);       /* Length of list in bytes, don't know yet */
        strl_start = nhb; /* Store start position */
        OUT4CC("strl");

        /* The audio stream header */

        OUT4CC("strh");
        OUTLONG(56); /* # of bytes to follow */
        OUT4CC("auds");

        // -----------
        // ThOe
        OUTLONG(0); /* Format (Optionally) */
        // -----------

        OUTLONG(0); /* Flags */
        OUTLONG(0); /* Reserved, MS says: wPriority, wLanguage */
        OUTLONG(0); /* InitialFrames */

        // ThOe /4
        OUTLONG(sampsize / 4); /* Scale */
        OUTLONG(1000 * AVI->track[j].mp3rate / 8);
        OUTLONG(0);                                        /* Start */
        OUTLONG(4 * AVI->track[j].audio_bytes / sampsize); /* Length */
        OUTLONG(0);                                        /* SuggestedBufferSize */
        OUTLONG(-1);                                       /* Quality */

        // ThOe /4
        OUTLONG(sampsize / 4); /* SampleSize */

        OUTLONG(0); /* Frame */
        OUTLONG(0); /* Frame */
        //       OUTLONG(0);             /* Frame */
        // OUTLONG(0);             /* Frame */

        /* The audio stream format */

        OUT4CC("strf");
        OUTLONG(16);                    /* # of bytes to follow */
        OUTSHRT(AVI->track[j].a_fmt);   /* Format */
        OUTSHRT(AVI->track[j].a_chans); /* Number of channels */
        OUTLONG(AVI->track[j].a_rate);  /* SamplesPerSec */
        // ThOe
        OUTLONG(1000 * AVI->track[j].mp3rate / 8);
        // ThOe (/4)

        OUTSHRT(sampsize / 4); /* BlockAlign */

        OUTSHRT(AVI->track[j].a_bits); /* BitsPerSample */

        /* Finish stream list, i.e. put number of bytes in the list to proper pos */

        long2str(AVI_header + strl_start - 4, nhb - strl_start);
    }

    /* Finish header list */

    long2str(AVI_header + hdrl_start - 4, nhb - hdrl_start);

    /* Calculate the needed amount of junk bytes, output junk */

    njunk = HEADERBYTES - nhb - 8 - 12;

    /* Safety first: if njunk <= 0, somebody has played with
       HEADERBYTES without knowing what (s)he did.
       This is a fatal error */

    if (njunk <= 0) {
        fprintf(stderr, "AVI_close_output_file: # of header bytes too small\n");
        exit(1);
    }

    OUT4CC("JUNK");
    OUTLONG(njunk);
    memset(AVI_header + nhb, 0, njunk);

    // 2001-11-14 added id string

    if ((unsigned int)njunk > strlen(id_str) + 8) {
        // sprintf(id_str, "%s-%s", PACKAGE, VERSION);
        // memcpy(AVI_header+nhb, id_str, strlen(id_str));
    }

    nhb += njunk;

    /* Start the movi list */

    OUT4CC("LIST");
    OUTLONG(movi_len); /* Length of list in bytes */
    OUT4CC("movi");

    /* Output the header, truncate the file to the number of bytes
       actually written, report an error if someting goes wrong */

    if (lseek(AVI->fdes, 0, SEEK_SET) < 0 ||
        avi_write(AVI->fdes, (char *)AVI_header, HEADERBYTES) != HEADERBYTES ||
        lseek(AVI->fdes, AVI->pos, SEEK_SET) < 0) {
        AVI_errno = AVI_ERR_CLOSE;
        return -1;
    }

    return 0;
}

static int avi_write_data(avi_t *AVI, char *data, unsigned long length, int audio, int keyframe)
{
    int n;

    unsigned char astr[5];

    /* Check for maximum file length */

    if ((AVI->pos + 8 + length + 8 + (AVI->n_idx + 1) * 16) > AVI_MAX_LEN) {
        AVI_errno = AVI_ERR_SIZELIM;
        return -1;
    }

    /* Add index entry */

    // set tag for current audio track
    sprintf((char *)astr, "0%1dwb", (int)(AVI->aptr + 1));

    if (audio)
        n = avi_add_index_entry(AVI, astr, 0x00, AVI->pos, length);
    else
        n = avi_add_index_entry(AVI, (unsigned char *)"00db", ((keyframe) ? 0x10 : 0x0), AVI->pos, length);

    if (n)
        return -1;

    /* Output tag and data */

    if (audio)
        n = avi_add_chunk(AVI, astr, (unsigned char *)data, length);
    else
        n = avi_add_chunk(AVI, (unsigned char *)"00db", (unsigned char *)data, length);

    if (n)
        return -1;

    return 0;
}

void AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor)
{
    /* may only be called if file is open for writing */

    if (AVI->mode == AVI_MODE_READ)
        return;

    AVI->width  = width;
    AVI->height = height;
    AVI->fps    = fps;

    if (strncmp(compressor, "RGB", 3) == 0) {
        memset(AVI->compressor, 0, 4);
    } else {
        memcpy(AVI->compressor, compressor, 4);
    }

    AVI->compressor[4] = 0;

    avi_update_header(AVI);
}

int AVI_write_frame(avi_t *AVI, char *data, long bytes, int keyframe)
{
    unsigned long pos;

    if (AVI->mode == AVI_MODE_READ) {
        AVI_errno = AVI_ERR_NOT_PERM;
        return -1;
    }

    pos = AVI->pos;

    if (avi_write_data(AVI, data, bytes, 0, keyframe))
        return -1;

    AVI->last_pos = pos;
    AVI->last_len = bytes;
    AVI->video_frames++;
    return 0;
}
