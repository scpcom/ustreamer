#ifndef AX_COMMON_H
#define AX_COMMON_H

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

typedef struct {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
} us_ax_mode_s;

void us_ax_get_lt_info(us_ax_mode_s *ax_mode);
int us_ax_set_lt_power(uint8_t _en);

#if __cplusplus
}
#endif
#endif
