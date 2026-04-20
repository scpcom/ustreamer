#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/frame.h"
#include "libs/logging.h"

#include "ax_global_type.h"
#include "ax_venc_comm.h"
#include "ax_venc_api.h"
#include "ax_sys_api.h"
#include "ax_ivps_api.h"
#include "ax_vin_api.h"

#include "ustreamer/encoders/ax_hw/ax_encoder.h"
#include "kvm_context.h"
#include "kvm_vision.h"

kvm_context_s *g_ctx = NULL;

static kvm_context_s *kvm_context_get_instance()
{
  if (g_ctx) return g_ctx;
  g_ctx = (kvm_context_s *)malloc(sizeof(kvm_context_s));
  if (g_ctx == NULL) return NULL;
  memset(g_ctx, 0, sizeof(kvm_context_s));
  return g_ctx;
}

void kvm_context_free()
{
  if (g_ctx) free(g_ctx);
  g_ctx = NULL;
}

void kvmv_init(uint8_t _debug_info_en)
{
  kvm_context_s *kvmCtx;

  kvmCtx = kvm_context_get_instance();

  if (kvmCtx->kvmv_run) {
    return;
  }

  kvmCtx->debug_en = 0;
  if (_debug_info_en) {
    kvmCtx->debug_en = 1;
  }

  kvmCtx->gop = 0;
  kvmCtx->fps = 0;
  kvmCtx->mode = 1;

  kvmCtx->frame = us_frame_init();
  kvmCtx->kvmv_run = 1;

  return;
}

static uint8_t *get_ve_on(char type)
{
  kvm_context_s *kvmCtx;
  uint8_t *pOn;

  kvmCtx = kvm_context_get_instance();

  if (kvmCtx != NULL) {
    if (type == 1) {
      pOn = &kvmCtx->venc_h264_on;
      goto done;
    }
    if (type == 5) {
      pOn = &kvmCtx->venc_h265_on;
      goto done;
    }
    if (type == 0) {
      pOn = &kvmCtx->venc_jpeg_on;
      goto done;
    }
  }
  pOn = NULL;
done:
  return pOn;
}

static int kvmv_disable_stream(VENC_CHN VeChn, uint8_t *pon)
{
  int32_t iRet;
  if (pon == NULL) return -1;
  if (*pon == 0) return 0;

  iRet = us_ax_disable_stream(VeChn);
  if (iRet != 0) {
    return iRet;
  }

  *pon = 0;
  return 0;
}

static int kvmv_enable_stream(VENC_CHN VeChn, uint8_t *pon)
{
  int32_t iRet;
  if (pon == NULL) return -1;
  if (*pon == 1) return 0;

  iRet = us_ax_enable_stream(VeChn);
  if (iRet != 0) {
    return iRet;
  }

  *pon = 1;
  return 0;
}

static int kvmv_disable_all_stream(us_ax_encoder_s *ax_enc)
{
  int32_t iRet;
  kvm_context_s *kvmCtx;

  kvmCtx = kvm_context_get_instance();

  iRet = kvmv_disable_stream(ax_enc->venc_jpeg_chn, &kvmCtx->venc_jpeg_on);
  if (iRet == 0) {
    iRet = kvmv_disable_stream(ax_enc->venc_h264_chn, &kvmCtx->venc_h264_on);
    if (iRet == 0) {
      iRet = kvmv_disable_stream(ax_enc->venc_h265_chn, &kvmCtx->venc_h265_on);
      if (iRet == 0) {
        return 0;
      }
      AXV_LOGE("us_ax_disable_stream for H265 failed, ret=%d", iRet);
    }
    else {
      AXV_LOGE("us_ax_disable_stream for H264 failed, ret=%d", iRet);
    }
  }
  else {
    AXV_LOGE("us_ax_disable_stream for MJPEG failed, ret=%d", iRet);
  }
  return iRet;
}

void kvmv_deinit(void)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;

  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  kvmCtx->deinit = 1;

  kvmCtx->kvmv_run = 0;
  kvmCtx->monitor_run = 0;

  if (kvmCtx->frame != NULL) {
    us_frame_destroy(kvmCtx->frame);
    kvmCtx->frame = NULL;
  }

  if (ax_enc != NULL) {
    kvmv_disable_all_stream(ax_enc);
    us_ax_encoder_destroy(ax_enc);
  }
  kvmCtx->ax_enc = NULL;

  kvm_context_free();

  return;
}

int kvmv_free_data(uint8_t **_pp_kvm_data)
{
  return 0;
}

void kvmv_free_all_data(void)
{
  return;
}

int kvmv_hdmi_control(uint8_t _en)
{
  return us_ax_set_lt_power(_en);
}

int kvmv_get_pps_frame(uint8_t **_pp_kvm_data,uint32_t *_p_kvmv_data_size)

{
  if (!_pp_kvm_data | !_p_kvmv_data_size) return -1;
  *_pp_kvm_data = NULL;
  *_p_kvmv_data_size = 0;
  return 0;
}

int kvmv_get_sps_frame(uint8_t **_pp_kvm_data,uint32_t *_p_kvmv_data_size)

{
  if (!_pp_kvm_data | !_p_kvmv_data_size) return -1;
  *_pp_kvm_data = NULL;
  *_p_kvmv_data_size = 0;
  return 0;
}

VENC_CHN get_ve_channel(char type)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  VENC_CHN VeChn;

  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  if (ax_enc != NULL) {
    if (type == 1) {
      VeChn = ax_enc->venc_h264_chn;
      goto done;
    }
    if (type == 5) {
      VeChn = ax_enc->venc_h265_chn;
      goto done;
    }
    if (type == 0) {
      VeChn = ax_enc->venc_jpeg_chn;
      goto done;
    }
  }
  VeChn = -1;
done:
  return VeChn;
}

int kvmv_get_fps(void)

{
  kvm_context_s *kvmCtx;

  kvmCtx = kvm_context_get_instance();
  return kvmCtx->fps;
}

int kvmv_set_fps(uint8_t _fps)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  int32_t iRet = 0;
  uint32_t lFPS;

  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  if (ax_enc == NULL) {
    kvmCtx->fps = _fps;
    AXV_LOGI("Encoder not initialized yet, fps will be applied on init");
  }
  else {
    lFPS = _fps ? _fps : ax_enc->desired_fps;
    if (kvmCtx->venc_jpeg_on) {
      iRet = us_ax_set_fps(ax_enc->venc_jpeg_chn, lFPS);
    }
    if (kvmCtx->venc_h264_on) {
      iRet = us_ax_set_fps(ax_enc->venc_h264_chn, lFPS);
    }
    if (kvmCtx->venc_h265_on) {
      iRet = us_ax_set_fps(ax_enc->venc_h265_chn, lFPS);
    }
    if (iRet != 0) {
      AXV_LOGE("Failed to set fps to %d", lFPS);
      goto done;
    }
    kvmCtx->fps = _fps;
    AXV_LOGI("FPS set to %d", lFPS);
  }
  iRet = 0;
done:
  return iRet;
}

int kvmv_set_gop(uint8_t _gop)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  int32_t iRes = 0;
  uint32_t lGOP;

  lGOP = _gop;
  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  if (ax_enc == NULL) {
    kvmCtx->gop = lGOP;
    AXV_LOGI("Encoder not initialized yet, gop will be applied on init");
  }
  else {
    if (kvmCtx->venc_h264_on) {
      iRes = us_ax_set_gop(ax_enc->venc_h264_chn, lGOP);
    }
    if (kvmCtx->venc_h265_on) {
      iRes = us_ax_set_gop(ax_enc->venc_h265_chn, lGOP);
    }
    if (iRes != 0) {
      AXV_LOGE("Failed to set gop to %d", lGOP);
      goto done;
    }
    kvmCtx->gop = lGOP;
    AXV_LOGI("GOP set to %d", lGOP);
  }
  iRes = 0;
done:
  return iRes;
}

int kvmv_set_rate_control(uint8_t mode)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  int iRes;
  uint32_t lMode;
  VENC_CHN VeChn;
  AX_VENC_RC_MODE_E rcMode;

  lMode = mode;
  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  if (ax_enc == NULL) {
    kvmCtx->mode = lMode;
    AXV_LOGI("Encoder not initialized yet, rate control mode will be applied on init");
  }
  else {
    if (lMode == 0) {
      iRes = 0;
      VeChn = get_ve_channel(1);
      if (kvmCtx->venc_h264_on) {
        iRes = us_ax_set_rate_control(ax_enc, VeChn, AX_VENC_RC_MODE_H264CBR);
      }
      VeChn = get_ve_channel(5);
      rcMode = AX_VENC_RC_MODE_H265CBR;
    }
    else {
      if (lMode != 1) {
        AXV_LOGE("Invalid rate control mode: %d", lMode);
        iRes = -1;
        goto done;
      }
      iRes = 0;
      VeChn = get_ve_channel(1);
      if (kvmCtx->venc_h264_on) {
        iRes = us_ax_set_rate_control(ax_enc, VeChn, AX_VENC_RC_MODE_H264VBR);
      }
      VeChn = get_ve_channel(5);
      rcMode = AX_VENC_RC_MODE_H265VBR;
    }
    if (kvmCtx->venc_h265_on) {
      iRes = us_ax_set_rate_control(ax_enc, VeChn, rcMode);
    }
    if (iRes != 0) {
      AXV_LOGE("Failed to set rate control mode to %d", lMode);
      goto done;
    }
    kvmCtx->mode = lMode;
    AXV_LOGI("Rate control mode set to %d", lMode);
  }
  iRes = 0;
done:
  return iRes;
}

static int kvmv_encoder_check(us_ax_mode_s *mode)
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  uint32_t lFPS;

  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;

  if (ax_enc == NULL) return -1;

  lFPS = kvmCtx->fps ? kvmCtx->fps : ax_enc->mode.fps;
  if (ax_enc->mode.width == mode->width &&
      ax_enc->mode.height == mode->height &&
      lFPS == mode->fps) {
    return 0;
  }

  ax_enc->mode.width  = mode->width;
  ax_enc->mode.height = mode->height;
  ax_enc->mode.fps    = mode->fps;

  AXV_LOGI("Using %dx%d %d fps", ax_enc->mode.width, ax_enc->mode.height, ax_enc->mode.fps);

  if (ax_enc->venc_jpeg_run_ && kvmCtx->venc_jpeg_on) {
    us_ax_set_resolution(ax_enc->venc_jpeg_chn, ax_enc->mode.width, ax_enc->mode.height);
    us_ax_set_fps(ax_enc->venc_jpeg_chn, ax_enc->mode.fps);
    us_ax_enable_stream(ax_enc->venc_jpeg_chn);
  }
  if (ax_enc->venc_h265_run_ && kvmCtx->venc_h265_on) {
    us_ax_set_resolution(ax_enc->venc_h265_chn, ax_enc->mode.width, ax_enc->mode.height);
    us_ax_set_fps(ax_enc->venc_h265_chn, ax_enc->mode.fps);
    us_ax_enable_stream(ax_enc->venc_h265_chn);
  }
  if (ax_enc->venc_h264_run_ && kvmCtx->venc_h264_on) {
    us_ax_set_resolution(ax_enc->venc_h264_chn, ax_enc->mode.width, ax_enc->mode.height);
    us_ax_set_fps(ax_enc->venc_h264_chn, ax_enc->mode.fps);
    us_ax_enable_stream(ax_enc->venc_h264_chn);
  }
  return 0;
}

static int kvmv_check_signal()
{
  kvm_context_s *kvmCtx;
  us_ax_encoder_s *ax_enc;
  us_ax_mode_s mode;
  int res = 0;

  kvmCtx = kvm_context_get_instance();
  ax_enc = kvmCtx->ax_enc;
  if (ax_enc == NULL) return -1;
  if (us_ax_is_lt_status("disappear")) {
    kvmCtx->no_signal = 1;
    res = -1;
  }
  if (res < 0) {
    return res;
  }
  if (kvmCtx->no_signal) {
    mode = ax_enc->mode;
    us_ax_get_lt_info(&mode);
    kvmv_encoder_check(&mode);
  }
  kvmCtx->no_signal = 0;
  return res;
}

int kvmv_read_img(uint16_t _width,uint16_t _height,uint8_t _type,uint16_t _qlty,
                 uint8_t **_pp_kvm_data,uint32_t *_p_kvmv_data_size)
{
  uint8_t bKey;
  VENC_CHN VeChn;
  uint8_t *pOn;
  int32_t iRet2;
  kvm_context_s *kvmCtx;
  long lTS = 0;
  us_ax_encoder_s *ax_enc;
  size_t uDataSize;
  uint32_t quality;
  uint32_t bitrate;
  int iRet;
  us_frame_s *frame;

  kvmCtx = kvm_context_get_instance();
  if (kvmCtx->deinit != 0) {
    return -2;
  }
  lTS = us_get_now_monotonic_u64();
  kvmCtx->ts = lTS / 1000000;
  if (kvmCtx->ax_enc == NULL) {
    quality = _qlty;
    bitrate = quality;
    if (_type == 0) {
      bitrate = 100;
    }
    if (_type != 0) {
      quality = 50;
    }
    kvmCtx->quality = quality;
    kvmCtx->bitrate = bitrate;
    kvmCtx->type = _type;
    ax_enc = us_ax_encoder_init("kvmv",_width,_height,0,quality,bitrate,kvmCtx->gop);
    kvmCtx->ax_enc = ax_enc;
    if ((((ax_enc == NULL) ||
         (iRet = kvmv_disable_all_stream(ax_enc), iRet != 0)) ||
        (VeChn = get_ve_channel(_type), VeChn == -1) ||
        (pOn = get_ve_on(_type), pOn == NULL)) ||
       (((iRet2 = kvmv_enable_stream(VeChn, pOn), iRet2 != 0 ||
         (iRet = kvmv_set_gop(kvmCtx->gop), iRet != 0)) ||
        ((iRet = kvmv_set_fps(kvmCtx->fps), iRet != 0 ||
         (iRet = kvmv_set_rate_control(kvmCtx->mode), iRet != 0)))))) {
      iRet = -2;
      goto done;
    }
  }
  ax_enc = kvmCtx->ax_enc;
  if ((kvmCtx->type != _type) && (ax_enc != NULL)) {
    iRet = kvmv_disable_all_stream(ax_enc);
    if ((iRet == 0) &&
       (((VeChn = get_ve_channel(_type), VeChn != -1) &&
         (pOn = get_ve_on(_type), pOn != NULL) &&
        (iRet2 = kvmv_enable_stream(VeChn, pOn), iRet2 == 0)))) {
      kvmCtx->type = _type;
      goto set_quality;
    }
    goto venc_fail;
  }
set_quality:
  if (_type == 0) {
    if (quality = _qlty, kvmCtx->quality != quality) {
      iRet2 = us_ax_set_quality(ax_enc->venc_jpeg_chn, quality);
      if (iRet2 != 0) {
        AXV_LOGE("Failed to set quality to %d", quality);
        goto venc_fail;
      }
      kvmCtx->quality = quality;
      AXV_LOGI("Quality set to %d", quality);
    }
    frame = kvmCtx->frame;
get_mjpeg:
    iRet = us_ax_get_mjpeg_frame(ax_enc,frame);
    goto set_data;
  }
  if (((_type & 0xfb) == 1) && (bitrate = _qlty, kvmCtx->bitrate != bitrate)) {
    iRet2 = 0;
    VeChn = get_ve_channel(1);
    if (kvmCtx->venc_h264_on) {
      iRet2 = us_ax_set_bitrate(VeChn, bitrate);
    }
    if (iRet2 == 0) {
      VeChn = get_ve_channel(5);
      if (kvmCtx->venc_h265_on) {
        iRet2 = us_ax_set_bitrate(VeChn, bitrate);
      }
      if (iRet2 == 0) {
        kvmCtx->bitrate = bitrate;
        AXV_LOGI("Bitrate set to %d", bitrate);
        goto get_h264;
      }
      AXV_LOGE("Failed to set bitrate to %d", bitrate);
    }
venc_fail:
    iRet = -2;
  }
  else {
get_h264:
    frame = kvmCtx->frame;
    if (_type == 1) {
      iRet = us_ax_get_h264_frame(ax_enc,frame,false);
set_data:
      if (iRet < 0) {
        kvmv_check_signal();
      }
      if (((iRet < 0 || frame == NULL) || (frame->data == NULL)) ||
         (uDataSize = frame->used, uDataSize == 0)) goto fail;
      *_pp_kvm_data = frame->data;
      *_p_kvmv_data_size = uDataSize;
      if (_type == 0) {
        iRet = 0;
      }
      else {
        frame->key = frame->key || (iRet == 4);
        if (_type == 1) {
          bKey = frame->key;
          iRet = 4;
        }
        else {
          if (_type != 5) goto fail;
          bKey = frame->key;
          iRet = 8;
        }
        iRet = iRet - (uint)bKey;
      }
    }
    else {
      if (_type == 5) {
        iRet = us_ax_get_h265_frame(ax_enc,frame,false);
        goto set_data;
      }
      if (_type == 0) goto get_mjpeg;
fail:
      iRet = -1;
    }
  }
done:
  return iRet;
}

int kvmv_read_audio(uint8_t **_pp_kvm_data,uint32_t *_p_kvmv_data_size)
{
  if (!_pp_kvm_data | !_p_kvmv_data_size) return -1;
  *_pp_kvm_data = NULL;
  *_p_kvmv_data_size = 0;
  return 0;
}
