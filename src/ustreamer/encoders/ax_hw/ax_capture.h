#ifndef AX_CAPTURE_H
#define AX_CAPTURE_H

#include <ax_ivps_api.h>

#include "ax_common.h"

#if __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t sys_run;
	uint8_t cap_run;
	uint8_t kvm_vin;
	uint8_t no_signal;
	uint32_t vin_chn;
	IVPS_GRP ivps_grp;
	void *libsns_handler;
	us_ax_mode_s mode;
	us_ax_mode_s ivps_mode;
} us_ax_capture_s;

us_ax_capture_s *us_ax_capture_init(int width, int height, uint32_t fps);
int us_ax_capture_destroy(us_ax_capture_s *ax_enc);

int us_ax_capture_open(us_ax_capture_s *ax_cap);
int us_ax_capture_close(us_ax_capture_s *ax_cap);

int us_ax_get_yuv_frame(us_ax_capture_s *ax_cap, us_frame_s *frame);

#if __cplusplus
}
#endif
#endif
