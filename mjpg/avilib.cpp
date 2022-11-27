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
#include <new>

AviLib::AviLib(std::string filename) : avi_file_name_(filename)
{
    Avi_errno_ = 0;
    avi_       = nullptr;
}

AviLib::~AviLib()
{
    if (avi_) {
        delete avi_;
    }
}

size_t AviLib::AviWrite(int fd, char *buf, size_t len)
{
    size_t n = 0;
    size_t r = 0;

    while (r < len) {
        n = write(fd, buf + r, len - r);
        if ((ssize_t)n < 0) {
            return n;
        }

        r += n;
    }
    return r;
}

void AviLib::Long2Str(unsigned char *dst, int n)
{
    dst[0] = (n)&0xff;
    dst[1] = (n >> 8) & 0xff;
    dst[2] = (n >> 16) & 0xff;
    dst[3] = (n >> 24) & 0xff;
}

int AviLib::AviSampSize(int j)
{
    int s;
    s = ((avi_->track[j].a_bits + 7) / 8) * avi_->track[j].a_chans;
    //   if(s==0) s=1; /* avoid possible zero divisions */
    if (s < 4) {
        s = 4; /* avoid possible zero divisions */
    }
    return s;
}

/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

int AviLib::AviAddChunk(unsigned char *tag, unsigned char *data, int length)
{
    unsigned char c[8];

    /* Copy tag and length int c, so that we need only 1 write system call
       for these two values */

    memcpy(c, tag, 4);
    Long2Str(c + 4, length);

    /* Output tag, length and data, restore previous position
       if the write fails */

    length = PAD_EVEN(length);

    if (AviWrite(avi_->fdes, (char *)c, 8) != 8 ||
        AviWrite(avi_->fdes, (char *)data, length) != (unsigned int)length) {
        lseek(avi_->fdes, avi_->pos, SEEK_SET);
        Avi_errno_ = AVI_ERR_WRITE;
        return -1;
    }

    /* Update file position */

    avi_->pos += 8 + length;

    // fprintf(stderr, "pos=%lu %s\n", avi_->pos, tag);

    return 0;
}

int AviLib::AviAddIndexEntry(unsigned char *tag, long flags, unsigned long pos, unsigned long len)
{
    void *ptr;

    if (avi_->n_idx >= avi_->max_idx) {
        ptr = realloc((void *)avi_->idx, (avi_->max_idx + 4096) * 16);

        if (ptr == 0) {
            Avi_errno_ = AVI_ERR_NO_MEM;
            return -1;
        }
        avi_->max_idx += 4096;
        avi_->idx = (unsigned char((*)[16]))ptr;
    }

    /* Add index entry */

    //   fprintf(stderr, "INDEX %s %ld %lu %lu\n", tag, flags, pos, len);

    memcpy(avi_->idx[avi_->n_idx], tag, 4);
    Long2Str(avi_->idx[avi_->n_idx] + 4, flags);
    Long2Str(avi_->idx[avi_->n_idx] + 8, pos);
    Long2Str(avi_->idx[avi_->n_idx] + 12, len);

    /* Update counter */

    avi_->n_idx++;

    if (len > avi_->max_len) {
        avi_->max_len = len;
    }

    return 0;
}

bool AviLib::AviOpenOutputFile()
{
    int i;

    int mask;

    unsigned char AVI_header[HEADERBYTES];

    /* Allocate the avi_t struct and zero it */

    avi_ = new (std::nothrow) avi_t;
    if (!avi_) {
        Avi_errno_ = AVI_ERR_NO_MEM;
        return false;
    }
    memset((void *)avi_, 0, sizeof(avi_t));

    /* Since Linux needs a long time when deleting big files,
       we do not truncate the file when we open it.
       Instead it is truncated when the AVI file is closed */

    mask = umask(0);
    umask(mask);

#ifdef WIN32
    avi_->fdes = open(avi_file_name_.c_str(), O_RDWR | O_CREAT | O_BINARY, (S_IRUSR | S_IWUSR) & ~mask);
#else
    avi_->fdes = open(avi_file_name_.c_str(), O_RDWR | O_CREAT, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) & ~mask);
#endif
    if (avi_->fdes < 0) {
        Avi_errno_ = AVI_ERR_OPEN;
        delete avi_;
        return false;
    }

    /* Write out HEADERBYTES bytes, the header will go here
       when we are finished with writing */

    for (i = 0; i < HEADERBYTES; i++) {
        AVI_header[i] = 0;
    }
    i = AviWrite(avi_->fdes, (char *)AVI_header, HEADERBYTES);
    if (i != HEADERBYTES) {
        close(avi_->fdes);
        Avi_errno_ = AVI_ERR_WRITE;
        delete avi_;
        return false;
    }

    avi_->pos  = HEADERBYTES;
    avi_->mode = AVI_MODE_WRITE; /* open for writing */

    // init
    avi_->anum = 0;
    avi_->aptr = 0;

    return true;
}

// ThOe write preliminary AVI file header: 0 frames, max vid/aud size
int AviLib::AviUpdateHeader()
{
    int njunk, sampsize, hasIndex, ms_per_frame, frate, flag;
    int movi_len, hdrl_start, strl_start, j;
    unsigned char AVI_header[HEADERBYTES];
    long nhb;

    // assume max size
    movi_len = AVI_MAX_LEN - HEADERBYTES + 4;

    // assume index will be written
    hasIndex = 1;

    if (avi_->fps < 0.001) {
        frate        = 0;
        ms_per_frame = 0;
    } else {
        frate        = (int)(FRAME_RATE_SCALE * avi_->fps + 0.5);
        ms_per_frame = (int)(1000000 / avi_->fps + 0.5);
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
    if (hasIndex) {
        flag |= AVIF_HASINDEX;
    }
    if (hasIndex && avi_->must_use_index) {
        flag |= AVIF_MUSTUSEINDEX;
    }
    OUTLONG(flag); /* Flags */
    OUTLONG(0);    // no frames yet
    OUTLONG(0);    /* InitialFrames */

    OUTLONG(avi_->anum + 1);

    OUTLONG(0);            /* SuggestedBufferSize */
    OUTLONG(avi_->width);  /* Width */
    OUTLONG(avi_->height); /* Height */
                           /* MS calls the following 'reserved': */
    OUTLONG(0);            /* TimeScale:  Unit used to measure time */
    OUTLONG(0);            /* DataRate:   Data rate of playback     */
    OUTLONG(0);            /* StartTime:  Starting time of AVI data */
    OUTLONG(0);            /* DataLength: Size of AVI data chunk    */

    /* Start the video stream list ---------------------------------- */

    OUT4CC("LIST");
    OUTLONG(0);       /* Length of list in bytes, don't know yet */
    strl_start = nhb; /* Store start position */
    OUT4CC("strl");

    /* The video stream header */

    OUT4CC("strh");
    OUTLONG(56);               /* # of bytes to follow */
    OUT4CC("vids");            /* Type */
    OUT4CC(avi_->compressor);  /* Handler */
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
    OUTLONG(40);           /* # of bytes to follow */
    OUTLONG(40);           /* Size */
    OUTLONG(avi_->width);  /* Width */
    OUTLONG(avi_->height); /* Height */
    OUTSHRT(1);
    OUTSHRT(24);              /* Planes, Count */
    OUT4CC(avi_->compressor); /* Compression */
    // ThOe (*3)
    OUTLONG(avi_->width * avi_->height * 3); /* SizeImage (in bytes?) */
    OUTLONG(0);                              /* XPelsPerMeter */
    OUTLONG(0);                              /* YPelsPerMeter */
    OUTLONG(0);                              /* ClrUsed: Number of colors used */
    OUTLONG(0);                              /* ClrImportant: Number of colors important */

    /* Finish stream list, i.e. put number of bytes in the list to proper pos */

    Long2Str(AVI_header + strl_start - 4, nhb - strl_start);

    /* Start the audio stream list ---------------------------------- */

    for (j = 0; j < avi_->anum; ++j) {

        sampsize = AviSampSize(j);

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
        OUTLONG(1000 * avi_->track[j].mp3rate / 8);
        OUTLONG(0);                                         /* Start */
        OUTLONG(4 * avi_->track[j].audio_bytes / sampsize); /* Length */
        OUTLONG(0);                                         /* SuggestedBufferSize */
        OUTLONG(-1);                                        /* Quality */

        // ThOe /4
        OUTLONG(sampsize / 4); /* SampleSize */

        OUTLONG(0); /* Frame */
        OUTLONG(0); /* Frame */
        //       OUTLONG(0);             /* Frame */
        // OUTLONG(0);             /* Frame */

        /* The audio stream format */

        OUT4CC("strf");
        OUTLONG(16);                     /* # of bytes to follow */
        OUTSHRT(avi_->track[j].a_fmt);   /* Format */
        OUTSHRT(avi_->track[j].a_chans); /* Number of channels */
        OUTLONG(avi_->track[j].a_rate);  /* SamplesPerSec */
        // ThOe
        OUTLONG(1000 * avi_->track[j].mp3rate / 8);
        // ThOe (/4)

        OUTSHRT(sampsize / 4); /* BlockAlign */

        OUTSHRT(avi_->track[j].a_bits); /* BitsPerSample */

        /* Finish stream list, i.e. put number of bytes in the list to proper pos */

        Long2Str(AVI_header + strl_start - 4, nhb - strl_start);
    }

    /* Finish header list */

    Long2Str(AVI_header + hdrl_start - 4, nhb - hdrl_start);

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

    if ((unsigned int)njunk > strlen(id_str_) + 8) {
        // sprintf(id_str_, "%s-%s", PACKAGE, VERSION);
        // memcpy(AVI_header+nhb, id_str_, strlen(id_str_));
    }

    nhb += njunk;

    /* Start the movi list */

    OUT4CC("LIST");
    OUTLONG(movi_len); /* Length of list in bytes */
    OUT4CC("movi");

    /* Output the header, truncate the file to the number of bytes
       actually written, report an error if someting goes wrong */

    if (lseek(avi_->fdes, 0, SEEK_SET) < 0 ||
        AviWrite(avi_->fdes, (char *)AVI_header, HEADERBYTES) != HEADERBYTES ||
        lseek(avi_->fdes, avi_->pos, SEEK_SET) < 0) {
        Avi_errno_ = AVI_ERR_CLOSE;
        return -1;
    }

    return 0;
}

int AviLib::AviWriteData(char *data, unsigned long length, int audio, int keyframe)
{
    int n;

    uint8_t astr[5];

    /* Check for maximum file length */

    if ((avi_->pos + 8 + length + 8 + (avi_->n_idx + 1) * 16) > AVI_MAX_LEN) {
        Avi_errno_ = AVI_ERR_SIZELIM;
        return -1;
    }

    /* Add index entry */

    // set tag for current audio track
    sprintf((char *)astr, "0%1dwb", (int)(avi_->aptr + 1));

    if (audio) {
        n = AviAddIndexEntry(astr, 0x00, avi_->pos, length);
    } else {
        n = AviAddIndexEntry((unsigned char *)"00db", ((keyframe) ? 0x10 : 0x0), avi_->pos, length);
    }

    if (n) {
        return -1;
    }

    /* Output tag and data */

    if (audio) {
        n = AviAddChunk(astr, (unsigned char *)data, length);
    } else {
        n = AviAddChunk((unsigned char *)"00db", (unsigned char *)data, length);
    }

    if (n) {
        return -1;
    }

    return 0;
}

void AviLib::AviSetVideo(int width, int height, double fps, char *compressor)
{
    /* may only be called if file is open for writing */

    if (avi_->mode == AVI_MODE_READ) {
        return;
    }

    avi_->width  = width;
    avi_->height = height;
    avi_->fps    = fps;

    if (strncmp(compressor, "RGB", 3) == 0) {
        memset(avi_->compressor, 0, 4);
    } else {
        memcpy(avi_->compressor, compressor, 4);
    }

    avi_->compressor[4] = 0;

    AviUpdateHeader();
}

int AviLib::AviWriteFrame(char *data, long bytes, int keyframe)
{
    uint64_t pos;

    if (avi_->mode == AVI_MODE_READ) {
        Avi_errno_ = AVI_ERR_NOT_PERM;
        return -1;
    }

    pos = avi_->pos;

    if (AviWriteData(data, bytes, 0, keyframe))
        return -1;

    avi_->last_pos = pos;
    avi_->last_len = bytes;
    avi_->video_frames++;
    return 0;
}
