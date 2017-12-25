#ifndef __H264_UVC_TESTAP_H_
#define __H264_UVC_TESTAP_H_

extern int capturing;
extern FILE *rec_fp1;
void Init_264camera(void);
void * cap_video (void *arg);

#endif