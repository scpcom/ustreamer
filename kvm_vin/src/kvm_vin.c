#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

#include "libs/frame.h"
#include "libs/logging.h"

#include "ustreamer/encoders/ax_hw/ax_capture.h"

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

int exit_flag = 0;
static void sig_handle(AX_S32 signo)
{
	UNUSED(signo);
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	exit_flag = 1;
}

int vin_loop(us_ax_capture_s *ax_cap) {
	signal(SIGINT, sig_handle);
	signal(SIGTERM, sig_handle);

	while (!exit_flag) {
		us_ax_mode_s mode = ax_cap->mode;
		int res = us_ax_capture_open(ax_cap);
		usleep(5 * 1000);
		if (res != 0) continue;
		if (ax_cap->mode.width != mode.width ||
		    ax_cap->mode.height != mode.height ||
		    ax_cap->mode.fps != mode.fps) {
			AXV_LOGI("Using %dx%d %d fps", ax_cap->mode.width, ax_cap->mode.height, ax_cap->mode.fps);
		}
		us_ax_capture_close(ax_cap);
	}
	return 0;
}

int main() {
	uint32_t width = 1920;
	uint32_t height = 1080;
	uint32_t fps = 60;

	us_ax_capture_s *cap = us_ax_capture_init(width, height, fps);
	if (!cap)
		return -1;

	vin_loop(cap);

	us_ax_capture_destroy(cap);

	return 0;
}
