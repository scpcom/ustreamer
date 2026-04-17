#ifndef AX_ENCODER_H
#define AX_ENCODER_H

#include <stdint.h>
#include <pthread.h>
#include <ax_venc_comm.h>
#define CONFIG_DEVNAME_LEN     32

#ifdef AXV_DEBUG
#define AXV_LOGD(x_msg, ...)	US_LOG_INFO("AX: " x_msg, ##__VA_ARGS__)
#else
#define AXV_LOGD(x_msg, ...)
#endif
#define AXV_LOGI(x_msg, ...)	US_LOG_INFO("AX: " x_msg, ##__VA_ARGS__)
#define AXV_LOGW(x_msg, ...)	US_LOG_INFO("AX: " x_msg, ##__VA_ARGS__)
#define AXV_LOGE(x_msg, ...)	US_LOG_ERROR("AX: " x_msg, ##__VA_ARGS__)

#if __cplusplus
extern "C" {
#endif

typedef struct us_ax_encoder_s {
	char dev_name_[CONFIG_DEVNAME_LEN];
	int is_alloc_;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t desired_fps;
	uint32_t quality;
	uint32_t bitrate;
	uint32_t gop;
	int state_;

	AX_VENC_H264_CBR_T stH264Cbr;
	AX_VENC_H264_VBR_T stH264Vbr;
	AX_VENC_H265_CBR_T stH265Cbr;
	AX_VENC_H265_VBR_T stH265Vbr;

	AX_VENC_CHN_ATTR_T stH264VencChnAttr;
	AX_VENC_CHN_ATTR_T stH265VencChnAttr;
	AX_VENC_CHN_ATTR_T stJPEGVencChnAttr;
#if 0
	pthread_t venc_thread_id_;
#endif
	int venc_h264_run_;
	int venc_h265_run_;
	int venc_jpeg_run_;
	VENC_CHN venc_h264_chn;
	VENC_CHN venc_h265_chn;
	VENC_CHN venc_jpeg_chn;
} us_ax_encoder_s;

us_ax_encoder_s *us_ax_encoder_init(const char *pdev_name, int width, int height, uint32_t fps, uint32_t quality, uint32_t bitrate, uint32_t gop);
int us_ax_encoder_destroy(us_ax_encoder_s *ax_enc);

int us_ax_enable_stream(VENC_CHN VencChn);
int us_ax_disable_stream(VENC_CHN VencChn);
int us_ax_disable_all_stream(us_ax_encoder_s *ax_enc);

int us_ax_encoder_check(us_ax_encoder_s *ax_enc, uint32_t width, uint32_t height, uint32_t fps);

int us_ax_set_resolution(VENC_CHN chn, AX_U32 width, AX_U32 height);
int us_ax_set_fps(VENC_CHN chn, uint32_t fps);
int us_ax_set_gop(VENC_CHN chn, uint32_t gop);
int us_ax_set_quality(VENC_CHN chn, uint32_t quality);
int us_ax_set_bitrate(VENC_CHN chn, uint32_t bitrate);
int us_ax_set_rate_control(us_ax_encoder_s *ax_enc, VENC_CHN chn, AX_VENC_RC_MODE_E rcMode);

int us_ax_get_h264_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame, bool force_key);
int us_ax_get_h265_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame, bool force_key);
int us_ax_get_mjpeg_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame);

#if __cplusplus
}
#endif
#endif
