#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../../libs/logging.h"

#include "ax_common.h"

static char* file_to_string(const char *file, size_t max_len)
{
	char *m_ptr = NULL;
	size_t m_capacity = 0;
	FILE* fp = fopen(file, "r");

	if(fp) {
		m_capacity = max_len;
		if (m_capacity) {
			m_ptr = (char*)malloc(m_capacity+1);
		}
		if (m_ptr) {
			fgets(m_ptr, m_capacity, fp);
			m_ptr[m_capacity] = 0;
		}

		fclose(fp);
	}

	if (m_ptr) {
	        uint8_t j=0;
	        while (m_ptr[j] != '\0' && m_ptr[j] != '\r' && m_ptr[j] != '\n')
			j++;
		m_ptr[j] = 0;
	}

	return m_ptr;
}

void us_ax_get_lt_info(us_ax_mode_s *ax_mode)
{
	int res;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t fps = 0;
	FILE *pFile = fopen("/proc/lt6911_info/status","r");
	if (pFile != NULL) {
		fclose(pFile);
		pFile = fopen("/proc/lt6911_info/width","r");
		if (pFile != NULL) {
			res = fscanf(pFile,"%d",&width);
			if (res != 1) {
				width = 0;
				AXV_LOGE("Failed to read width, use default");
			}
			fclose(pFile);
		}
		else {
			AXV_LOGE("Failed to open width file, use default");
		}
		pFile = fopen("/proc/lt6911_info/height","r");
		if (pFile != NULL) {
			res = fscanf(pFile,"%d",&height);
			if (res != 1) {
				height = 0;
				AXV_LOGE("Failed to read height, use default");
			}
			fclose(pFile);
		}
		else {
			AXV_LOGE("Failed to open height file, use default");
		}
	}
	else {
		AXV_LOGE("Failed to open /proc/lt6911_info/status");
	}
	if (width != 0 && height != 0) {
		ax_mode->width = width;
		ax_mode->height = height;
	}
	else {
		ax_mode->width = 1920;
		ax_mode->height = 1080;
		AXV_LOGE("Width or height is 0, use default values");
	}
	pFile = fopen("/proc/lt6911_info/fps","r");
	if (pFile == NULL) {
		fps = 60;
		AXV_LOGE("Failed to open fps file, set fps to 60");
	}
	else {
		res = fscanf(pFile,"%d",&fps);
		if (res != 1) {
			fps = 0;
			AXV_LOGE("Failed to read fps, use default");
		}
		fclose(pFile);
		if (fps == 0) {
			AXV_LOGE("Invalid fps value (%d), set fps to 30", fps);
			fps = 30;
		}
	}
	ax_mode->fps = fps;
	AXV_LOGI("Using %dx%d %d fps", ax_mode->width, ax_mode->height, ax_mode->fps);
}

int us_ax_set_lt_power(uint8_t _en)
{
	int res = 0;
	char value = _en ? '1' : '0';
	FILE *pFile = fopen("/proc/lt6911_info/status","r");
	if (pFile != NULL) {
		fclose(pFile);
		res = -1;
		pFile = fopen("/proc/lt6911_info/power","w");
		if (pFile != NULL) {
			res = fwrite(&value, 1, 1, pFile);
			if (res != 1) {
				AXV_LOGE("Failed to write power");
			}
			else {
				res = 0;
			}
			fclose(pFile);
		}
		else {
			AXV_LOGE("Failed to open power file");
		}
	}
	else {
		AXV_LOGE("Failed to open /proc/lt6911_info/status");
	}
	return res;
}

uint8_t us_ax_get_lt_status(const char* compare)
{
	int res = 0;
	char* str;
	str = file_to_string("/proc/lt6911_info/status", 32);
	if (str)
	{
		if (!strcmp(str, compare)) {
			res = 1;
		}
		free(str);
	}
	return res;
}
