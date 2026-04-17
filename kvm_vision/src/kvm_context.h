#ifndef KVM_CONTEXT_H_
#define KVM_CONTEXT_H_

typedef struct {
	uint16_t quality;
	uint16_t bitrate;
	uint8_t type;
	uint8_t gop;
	uint8_t fps;
	uint8_t mode;
	us_ax_encoder_s *ax_enc;
	uint32_t kvmv_run;
	uint32_t monitor_run;
	uint64_t ts;
	us_frame_s *frame;
	uint8_t debug_en;
	uint32_t deinit;
	uint8_t venc_h264_on;
	uint8_t venc_h265_on;
	uint8_t venc_jpeg_on;
} kvm_context_s;

#endif  // KVM_CONTEXT_H_
