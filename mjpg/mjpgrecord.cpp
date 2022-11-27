#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "avilib.h"
#include "v4l2uvc.h"
#include "mjpgrecord.h"


MjpgRecord::MjpgRecord() {}

MjpgRecord::~MjpgRecord() {}

int test(int argc, char *argv[]) // -s 1280x720   -d  /dev/video1   -a  test.avi  -i 30
{
    FILE *file = NULL;
    struct vdIn *vd;
    //	unsigned char *tmpbuffer;
    char *filename    = NULL;
    char *avifilename = NULL;

    char *sizestring = NULL;
    char *fpsstring  = NULL;
    char *separateur = NULL;
    int width        = 1280;
    int height       = 720;
    int fps          = 30;
    int i;

	V4l2Video dev;

    for (i = 0; i < argc; i++) {
        printf("No ffff %d\n", argc);
        if (1 == argc) {
            printf("No for for \n");

            width       = 1280;
            height      = 720;
            fps         = 30;
            filename    = (char *)"/dev/video0";
            avifilename = (char *)"test.avi";
        }

        if (argv[i] == NULL || *argv[i] == 0 || *argv[i] != '-') {
            continue;
        }

        if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc) {
                printf("No parameter specified with -s, aborting.\n");
                exit(1);
            }

            sizestring = strdup(argv[i + 1]);

            width = strtoul(sizestring, &separateur, 10);
            if (*separateur != 'x') {
                printf("Error in size use -s widthxheight\n");
                exit(1);
            } else {
                ++separateur;
                height = strtoul(separateur, &separateur, 10);
                if (*separateur != 0)
                    printf("hmm.. dont like that!! trying this height\n");
            }
        } else {
            width  = 640;
            height = 480;
        }

        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                printf("No parameter specified with -d, aborting.\n");
                exit(1);
            }
            filename = strdup(argv[i + 1]);
        }

        if (strcmp(argv[i], "-a") == 0) {
            if (i + 1 >= argc) {
                printf("No parameter specified with -o, aborting.\n");
                exit(1);
            }
            avifilename = strdup(argv[i + 1]);
        }

        if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) {
                printf("No parameter specified with -i, aborting.\n");
                exit(1);
            }
            fpsstring = argv[i + 1];
            fps       = strtoul(fpsstring, &separateur, 10);
            if (*separateur != '\0') {
                printf("Invalid frame rate '%s' specified with -i. "
                       "Only integers are supported. Aborting.\n",
                       fpsstring);
                exit(1);
            }
        } else {
            fps = 30;
        }
    }

    if (filename == NULL || *filename == 0) {
        filename = (char *)"/dev/video0";
    }

    if (avifilename == NULL || *avifilename == 0) {
        avifilename = (char *)"test.avi";
    }

    int format = V4L2_PIX_FMT_MJPEG;
    int ret;
    int grabmethod = 1;

    file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Unable to open file for raw frame capturing\n ");
        exit(1);
    }

    // v4l2 init
    vd = (struct vdIn *)calloc(1, sizeof(struct vdIn));
    if (dev.InitVideoIn(vd, (char *)filename, width, height, fps, format, grabmethod, avifilename) < 0) {
        exit(1);
    }

    if (dev.VideoEnable(vd)) {
        exit(1);
    }

    vd->avifile = AVI_open_output_file(vd->avifilename);
    if (vd->avifile == NULL) {
        printf("Error opening avifile %s\n", vd->avifilename);
        exit(1);
    }

    AVI_set_video(vd->avifile, vd->width, vd->height, fps, (char *)"MJPG");
    printf("recording to %s\n", vd->avifilename);

    while (1) {
        memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
        vd->buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vd->buf.memory = V4L2_MEMORY_MMAP;
        ret            = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
        if (ret < 0) {
            printf("Unable to dequeue buffer");
            exit(1);
        }

        memcpy(vd->tmpbuffer, vd->mem[vd->buf.index], vd->buf.bytesused);

        AVI_write_frame(vd->avifile, (char *)(vd->tmpbuffer), vd->buf.bytesused, vd->framecount);

        vd->framecount++;

        ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
        if (ret < 0) {
            printf("Unable to requeue buffer");
            exit(1);
        }
    }
    fclose(file);
    dev.CloseV4l2(vd);
}
