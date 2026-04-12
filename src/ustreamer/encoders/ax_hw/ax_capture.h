#ifndef AX_CAPTURE_H
#define AX_CAPTURE_H

#include <ax_ivps_api.h>

typedef struct {
	uint8_t sys_run;
	uint8_t cap_run;
	uint8_t kvm_vin;
	uint8_t no_signal;
	uint32_t vin_chn;
	IVPS_GRP ivps_grp;
	void *libsns_handler;
	uint32_t dst_width;
	uint32_t dst_height;
	uint32_t src_fps;
} us_ax_capture_s;

us_ax_capture_s *us_ax_capture_init(int width, int height, uint32_t fps);
int us_ax_capture_destroy(us_ax_capture_s *ax_enc);

int us_ax_capture_open(us_ax_capture_s *ax_cap);
int us_ax_capture_close(us_ax_capture_s *ax_cap);

int us_ax_get_yuv_frame(us_ax_capture_s *ax_cap, us_frame_s *frame);

#endif
