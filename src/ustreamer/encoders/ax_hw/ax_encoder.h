#ifndef AX_ENCODER_H
#define AX_ENCODER_H

#include <stdint.h>
#include <pthread.h>
#include <ax_venc_comm.h>
#define CONFIG_DEVNAME_LEN     32

#define AXV_LOGI printf
#define AXV_LOGW printf
#define AXV_LOGE printf

#if __cplusplus
extern "C" {
#endif

typedef struct us_ax_encoder_s {
	char dev_name_[CONFIG_DEVNAME_LEN];
	int desired_fps;
	int is_alloc_;
	uint32_t width;
	uint32_t height;
	uint32_t quality;
	uint32_t bitrate;
	uint32_t gop;
	int state_;

	AX_VENC_CHN_ATTR_T stH264VencChnAttr;
	AX_VENC_CHN_ATTR_T stJPEGVencChnAttr;
#if 0
	pthread_t venc_thread_id_;
#endif
	int venc_h264_run_;
	int venc_jpeg_run_;
	VENC_CHN venc_h264_chn;
	VENC_CHN venc_jpeg_chn;
} us_ax_encoder_s;

us_ax_encoder_s *us_ax_encoder_init(const char *pdev_name, int width, int height, uint32_t fps, uint32_t quality, uint32_t bitrate, uint32_t gop);
int us_ax_encoder_destroy(us_ax_encoder_s *ax_enc);

int us_ax_enable_stream(VENC_CHN VencChn);
int us_ax_disable_stream(VENC_CHN VencChn);

int us_ax_get_h264_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame, bool force_key);
int us_ax_get_mjpeg_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame);

#if __cplusplus
}
#endif
#endif
