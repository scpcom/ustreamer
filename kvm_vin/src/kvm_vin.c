#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h>
#include <sys/mman.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
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

static int vin_socket_init()
{
  int iRet = 0;
  int *pError;
  char *sError;
  struct stat st;
  int sock_fd;
  struct sockaddr_un sock_addr;
  const uz max_sun_path = sizeof(sock_addr.sun_path) - 1;
  int event_fd;
  struct epoll_event event;
  uint uVal;

  sock_fd = socket(AF_UNIX, 1, 0);
  if (-1 < sock_fd) {
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, VIN_SOCKET_PATH, max_sun_path);
    memset(&st, 0, sizeof(st));
    if (stat(VIN_SOCKET_DIR, &st) == -1) {
      iRet = mkdir(VIN_SOCKET_DIR, 0755);
    }
    if (iRet < 0) {
      AXV_LOGE("Failed to create socket directory: %s", VIN_SOCKET_DIR);
    }
    else {
      unlink(VIN_SOCKET_PATH);
      iRet = bind(sock_fd, (struct sockaddr*)&sock_addr, sizeof(struct sockaddr_un));
      if (iRet < 0) {
        pError = __errno_location();
        sError = strerror(*pError);
        AXV_LOGE("Failed to bind socket: %s", sError);
        goto fail;
      }
      iRet = listen(sock_fd, 5);
      if (iRet < 0) {
        pError = __errno_location();
        sError = strerror(*pError);
        AXV_LOGE("Failed to listen on socket: %s", sError);
        goto fail;
      }

      AXV_LOGI("Socket listening on %s", VIN_SOCKET_PATH);

      uVal = fcntl(sock_fd,3,0);
      fcntl(sock_fd,4,(ulong)(uVal | 0x800));
      event_fd = epoll_create1(0);
      if (event_fd == -1) {
        pError = __errno_location();
        sError = strerror(*pError);
        AXV_LOGE("Failed to create epoll instance: %s", sError);
        goto fail;
      }
      event.events = 1;
      event.data.fd = sock_fd;
      iRet = epoll_ctl(event_fd,1,sock_fd,&event);
      if (iRet == -1) {
        pError = __errno_location();
        sError = strerror(*pError);
        AXV_LOGE("Failed to add socket to epoll: %s", sError);
        goto fail;
      }

      iRet = sock_fd;
    }
    goto done;
  }
  pError = __errno_location();
  sError = strerror(*pError);
  AXV_LOGE("Failed to create socket: %s", sError);
fail:
  iRet = -1;
done:
  return iRet;
}

static int vin_socket_destroy(int sock_fd)
{
  if (sock_fd < 0) return 0;
  close(sock_fd);
  return unlink(VIN_SOCKET_PATH);
}

int vin_loop(us_ax_capture_s *ax_cap) {
	int sock_fd;

	signal(SIGINT, sig_handle);
	signal(SIGTERM, sig_handle);

	sock_fd = vin_socket_init();
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
	vin_socket_destroy(sock_fd);
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
