#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../../libs/frame.h"
#include "../../../libs/logging.h"

#include "ax_global_type.h"
#include "ax_venc_comm.h"
#include "ax_venc_api.h"
#include "ax_sys_api.h"
#include "ax_ivps_api.h"
#include "ax_vin_api.h"

#include "ax_encoder.h"

typedef enum {
	AX_ENCODER_NONT             = 0b00000,
	AX_ENCODER_HW_ENABLE       = 0b00010,
	AX_ENCODER_HW_OPEN         = 0b01000,
	AX_ENCODER_GET_FRAME_THREAD = 0b10000,
} AX_ENCODER_STATUS;

static AX_S32 AX_ENC_VENC_Init()
{
	AX_S32 s32Ret = 0;
	AX_VENC_MOD_ATTR_T stModAttr = {
		.enVencType                     = AX_VENC_MULTI_ENCODER,
		.stModThdAttr.u32TotalThreadNum = 1,
		.stModThdAttr.enSchedPolicy = AX_VENC_SCHED_OTHER,
		.stModThdAttr.u32SchedPriority = 0,
		.stModThdAttr.bExplicitSched    = AX_FALSE,
	};
	s32Ret = AX_VENC_Init(&stModAttr);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_VENC_Init failed, s32Ret:0x%x", s32Ret);
		return s32Ret;
	}
	return 0;
}

static AX_S32 AX_ENC_VENC_Chn_Init(VENC_CHN *pVencChn, const AX_VENC_CHN_ATTR_T *pstVencChnAttr)
{
	VENC_CHN VencChn = 0;
	if (pVencChn) VencChn = *pVencChn;
	AX_S32 ret = AX_VENC_CreateChn(VencChn, pstVencChnAttr);
	if (AX_SUCCESS != ret) {
		AXV_LOGE("VencChn %d: AX_VENC_CreateChn failed, s32Ret:0x%x", VencChn, ret);
		return -1;
	}
	AX_MOD_INFO_T srcMod, dstMod;
	srcMod.enModId  = AX_ID_VIN;
	srcMod.s32GrpId = 0;
	srcMod.s32ChnId = 0;
	dstMod.enModId  = AX_ID_VENC;
	dstMod.s32GrpId = 0;
	dstMod.s32ChnId = VencChn;
	ret = AX_SYS_Link(&srcMod, &dstMod);
	if (AX_SUCCESS != ret) {
		AXV_LOGE("VencChn %d: AX_SYS_Link failed, s32Ret:0x%x", VencChn, ret);
		return -1;
	}
	if (pVencChn) *pVencChn = VencChn;
	return 0;
}

static AX_S32 AX_ENC_VENC_Chn_DeInit(VENC_CHN *pVencChn, const AX_VENC_CHN_ATTR_T *pstVencChnAttr)
{
	VENC_CHN VencChn = 0;
	if (pVencChn) VencChn = *pVencChn;
	AX_S32 s32Ret = 0, s32Retry = 5;

	if (pstVencChnAttr->stVencAttr.enType == PT_PCMU) {
		return s32Ret;
	}
	AX_MOD_INFO_T srcMod, dstMod;
	srcMod.enModId  = AX_ID_VIN;
	srcMod.s32GrpId = 0;
	srcMod.s32ChnId = 0;
	dstMod.enModId  = AX_ID_VENC;
	dstMod.s32GrpId = 0;
	dstMod.s32ChnId = VencChn;
	s32Ret = AX_SYS_UnLink(&srcMod, &dstMod);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("VencChn %d: AX_SYS_UnLink failed, s32Retry=%d, s32Ret=0x%x", VencChn, s32Retry, s32Ret);
	}

	s32Retry = 5;
	do {
		s32Ret = AX_VENC_DestroyChn(VencChn);
		if (AX_ERR_VENC_BUSY == s32Ret) {
			AXV_LOGE("VencChn %d:AX_VENC_DestroyChn return AX_ERR_VENC_BUSY,retry...", VencChn);
			--s32Retry;
			usleep(100 * 1000);
		} else {
			break;
		}
	} while (s32Retry >= 0);

	if (s32Retry == -1 || AX_SUCCESS != s32Ret) {
		AXV_LOGE("VencChn %d: AX_VENC_DestroyChn failed, s32Retry=%d, s32Ret=0x%x", VencChn, s32Retry, s32Ret);
	}
	if (pVencChn) *pVencChn = -1; // INVALID
	return 0;
}

static AX_S32 AX_ENC_VENC_DeInit()
{
	AX_S32 s32Ret = AX_VENC_Deinit();
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_VENC_Deinit failed, s32Ret=0x%x", s32Ret);
		return s32Ret;
	}
	return 0;
}

int us_ax_encoder_init_from(us_ax_encoder_s *ax_enc)
{
	AX_S32 ret;
	AX_VENC_CHN_ATTR_T stVencChnAttr;
	uint32_t width, height, fps, desired_fps, bitrate, quality, gop;
	if (ax_enc == NULL) return -1;
	/* Check whether the encoder is already open or in an error state */
	AXV_LOGI("Open encoder %s...", ax_enc->dev_name_);
	if (ax_enc->state_ & AX_ENCODER_HW_OPEN) {
		AXV_LOGE("Error: encoder was open or meet error, now state is: %d", ax_enc->state_);
		goto ErrorHandle;
	}

	width = ax_enc->width;
	height = ax_enc->height;
	fps = ax_enc->fps;
	desired_fps = ax_enc->desired_fps;
	quality = ax_enc->quality;
	bitrate = ax_enc->bitrate;
	gop = ax_enc->gop;

	memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));

	stVencChnAttr.stVencAttr.stCropCfg.stRect.s32Y = 0;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.u32Width = 0;
	stVencChnAttr.stVencAttr.stCropCfg.bEnable = AX_FALSE;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.s32X = 0;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.u32Height = 0;
	stVencChnAttr.stVencAttr.enRotation = AX_ROTATION_0;

	stVencChnAttr.stVencAttr.enMemSource = AX_MEMORY_SOURCE_CMM;
	stVencChnAttr.stVencAttr.enType = PT_H264;
	stVencChnAttr.stVencAttr.enLevel = AX_VENC_H264_LEVEL_4_2;
	stVencChnAttr.stVencAttr.enProfile = AX_VENC_H264_MAIN_PROFILE;
	stVencChnAttr.stVencAttr.enTier = AX_VENC_HEVC_MAIN_TIER;

	stVencChnAttr.stVencAttr.u32MaxPicWidth = 3840;
	stVencChnAttr.stVencAttr.u32MaxPicHeight = 2400;
	stVencChnAttr.stVencAttr.u32BufSize = 13824000;

	stVencChnAttr.stVencAttr.u32PicWidthSrc = width;
	stVencChnAttr.stVencAttr.u32PicHeightSrc = height;

	stVencChnAttr.stVencAttr.enLinkMode = AX_LINK_MODE;
	stVencChnAttr.stVencAttr.bDeBreathEffect = AX_FALSE;
	stVencChnAttr.stVencAttr.bRefRingbuf = AX_TRUE;
	stVencChnAttr.stVencAttr.s32StopWaitTime = -1;
	stVencChnAttr.stVencAttr.u32SliceNum = 0;

	stVencChnAttr.stVencAttr.u8InFifoDepth = 1; //4;
	stVencChnAttr.stVencAttr.u8OutFifoDepth = 2; //4;

	stVencChnAttr.stGopAttr.enGopMode = AX_VENC_GOPMODE_NORMALP;

	stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = fps;
	stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = desired_fps;

	stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_H264CBR;
	stVencChnAttr.stRcAttr.s32FirstFrameStartQp = -1;

	stVencChnAttr.stRcAttr.stH264Cbr.u32Gop = gop;
	if (stVencChnAttr.stRcAttr.stH264Cbr.u32Gop == 0) {
		stVencChnAttr.stRcAttr.stH264Cbr.u32Gop = 30000;
	}

	stVencChnAttr.stRcAttr.stH264Cbr.u32MaxQp = 51;
	stVencChnAttr.stRcAttr.stH264Cbr.u32StatTime = 0;
	stVencChnAttr.stRcAttr.stH264Cbr.u32MinIQp = 10;
	stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIprop = 40;
	stVencChnAttr.stRcAttr.stH264Cbr.u32MinQp = 10;
	stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIQp = 51;
	stVencChnAttr.stRcAttr.stH264Cbr.s32DeBreathQpDelta = 0;
	stVencChnAttr.stRcAttr.stH264Cbr.u32IdrQpDeltaRange = 0;
	stVencChnAttr.stRcAttr.stH264Cbr.u32MinIprop = 10;
	stVencChnAttr.stRcAttr.stH264Cbr.s32IntraQpDelta = 0;
	stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockType = AX_VENC_QPMAP_BLOCK_DISABLE;
	stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockUnit = AX_VENC_QPMAP_BLOCK_UNIT_64x64;
	stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enCtbRcMode = AX_VENC_RC_CTBRC_DISABLE;
	stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapQpType = AX_VENC_QPMAP_QP_DISABLE;

	stVencChnAttr.stRcAttr.stH264Cbr.u32BitRate = bitrate;

	ax_enc->stH264VencChnAttr = stVencChnAttr;
	ax_enc->venc_h264_chn     = -1;
	ax_enc->venc_h264_run_    = 1;

	memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));

	stVencChnAttr.stVencAttr.stCropCfg.stRect.s32Y = 0;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.u32Width = 0;
	stVencChnAttr.stVencAttr.stCropCfg.bEnable = AX_FALSE;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.s32X = 0;
	stVencChnAttr.stVencAttr.stCropCfg.stRect.u32Height = 0;
	stVencChnAttr.stVencAttr.enRotation = AX_ROTATION_0;

	stVencChnAttr.stVencAttr.enMemSource = AX_MEMORY_SOURCE_CMM;
	stVencChnAttr.stVencAttr.enType = PT_MJPEG;
	stVencChnAttr.stVencAttr.enLevel = 0;
	stVencChnAttr.stVencAttr.enProfile = AX_VENC_HEVC_MAIN_PROFILE;
	stVencChnAttr.stVencAttr.enTier = AX_VENC_HEVC_MAIN_TIER;

	stVencChnAttr.stVencAttr.u32MaxPicWidth = 3840;
	stVencChnAttr.stVencAttr.u32MaxPicHeight = 2400;
	stVencChnAttr.stVencAttr.u32BufSize = 13824000;

	stVencChnAttr.stVencAttr.u32PicWidthSrc = width;
	stVencChnAttr.stVencAttr.u32PicHeightSrc = height;

	stVencChnAttr.stVencAttr.enLinkMode = AX_LINK_MODE;
	stVencChnAttr.stVencAttr.bDeBreathEffect = AX_FALSE;
	stVencChnAttr.stVencAttr.bRefRingbuf = AX_TRUE;
	stVencChnAttr.stVencAttr.s32StopWaitTime = -1;
	stVencChnAttr.stVencAttr.u32SliceNum = 0;

	stVencChnAttr.stVencAttr.u8InFifoDepth = 1;
	stVencChnAttr.stVencAttr.u8OutFifoDepth = 1;

	stVencChnAttr.stGopAttr.enGopMode = AX_VENC_GOPMODE_NORMALP;

	stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = fps;
	stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = desired_fps;

	stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_MJPEGFIXQP;
	stVencChnAttr.stRcAttr.s32FirstFrameStartQp = -1;

	stVencChnAttr.stRcAttr.stMjpegFixQp.s32FixedQp = 51 - (quality * 50) / 100;

	ax_enc->stJPEGVencChnAttr = stVencChnAttr;
	ax_enc->venc_jpeg_chn     = -1;
	ax_enc->venc_jpeg_run_    = 0;

	AX_SYS_Init();
	if (ax_enc->venc_h264_run_ || ax_enc->venc_jpeg_run_) {
		AX_ENC_VENC_Init();
	}
	if (ax_enc->venc_h264_run_) {
		ax_enc->venc_h264_chn = 0;
		ret = AX_ENC_VENC_Chn_Init(&ax_enc->venc_h264_chn, &ax_enc->stH264VencChnAttr);
		if (0 != ret)
			ax_enc->venc_h264_run_ = 0;
#if 0
		pthread_create(&ax_enc->venc_thread_id_, NULL, VencGetStreamProc, NULL);
#endif
	}
	if (ax_enc->venc_jpeg_run_) {
		ax_enc->venc_jpeg_chn = ax_enc->venc_h264_chn + 1;
		ret = AX_ENC_VENC_Chn_Init(&ax_enc->venc_jpeg_chn, &ax_enc->stJPEGVencChnAttr);
		if (0 != ret)
			ax_enc->venc_jpeg_run_ = 0;
	}

	ax_enc->state_ |= AX_ENCODER_HW_ENABLE;
	ax_enc->state_ |= AX_ENCODER_HW_OPEN;

	return 0;

ErrorHandle:
	AXV_LOGE("Encoder open meet error, now handle it");
	return -1;
}

us_ax_encoder_s *us_ax_encoder_init(const char *pdev_name, int width, int height, uint32_t fps, uint32_t quality, uint32_t bitrate, uint32_t gop)
{
	int res = -1;
	us_ax_encoder_s *ax_enc = (us_ax_encoder_s *)malloc(sizeof(us_ax_encoder_s));
	if (ax_enc == NULL) return NULL;
	memset(ax_enc, 0, sizeof(us_ax_encoder_s));

	int CopyLen = strlen(pdev_name);
	if (CopyLen > CONFIG_DEVNAME_LEN - 1) {
		AXV_LOGE("Error: device name length over limit: %d", CopyLen);
		goto ErrorHandle;
	}
	memset(ax_enc->dev_name_, 0, CONFIG_DEVNAME_LEN);
	memcpy(ax_enc->dev_name_, pdev_name, CopyLen);

	ax_enc->width          = width;
	ax_enc->height         = height;
	ax_enc->fps            = fps;
	ax_enc->desired_fps    = fps;
	ax_enc->quality        = quality;
	ax_enc->bitrate        = bitrate;
	ax_enc->gop            = gop;

	res = us_ax_encoder_init_from(ax_enc);
	if (res) {
		goto ErrorHandle;
	}
	ax_enc->is_alloc_ = 1;
	AXV_LOGI("Encoder %s open success", ax_enc->dev_name_);
	return ax_enc;

ErrorHandle:
	AXV_LOGE("Encoder open meet error, now handle it");
	free(ax_enc);
	return NULL;
}

int us_ax_encoder_destroy(us_ax_encoder_s *ax_enc)
{
	int venc_run;
	if (ax_enc == NULL) return -1;
	if (ax_enc->state_ & AX_ENCODER_HW_OPEN) {
		ax_enc->state_ &= ~((int)AX_ENCODER_HW_OPEN);
	}

	if (ax_enc->state_ & AX_ENCODER_HW_ENABLE) {
		ax_enc->state_ &= ~((int)AX_ENCODER_HW_ENABLE);
	}
	ax_enc->state_ = AX_ENCODER_NONT;
#if 0
	pthread_join(ax_enc->venc_thread_id_, NULL);
#endif
	venc_run = ax_enc->venc_h264_run_ || ax_enc->venc_jpeg_run_;
	if (ax_enc->venc_h264_run_) {
		AX_ENC_VENC_Chn_DeInit(&ax_enc->venc_h264_chn, &ax_enc->stH264VencChnAttr);
		ax_enc->venc_h264_run_ = 0;
	}
	if (ax_enc->venc_jpeg_run_) {
		AX_ENC_VENC_Chn_DeInit(&ax_enc->venc_jpeg_chn, &ax_enc->stJPEGVencChnAttr);
		ax_enc->venc_jpeg_run_ = 0;
	}
	if (venc_run) {
		AX_ENC_VENC_DeInit();
	}
	AX_SYS_Deinit();
	if (ax_enc->is_alloc_) free(ax_enc);

	AXV_LOGI("Encoder closed");

	return 0;
}

int us_ax_enable_stream(VENC_CHN VencChn)
{
	AX_VENC_RECV_PIC_PARAM_T stRecvParam;
	AX_S32 s32Ret;
	stRecvParam.s32RecvPicNum = -1;
	s32Ret = AX_VENC_StartRecvFrame(VencChn, &stRecvParam);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGI("VencChn %d: AX_VENC_StartRecvFrame failed, s32Ret:0x%x", VencChn, s32Ret);
		return -1;
	}
	return 0;
}

int us_ax_disable_stream(VENC_CHN VencChn)
{
	AX_S32 s32Ret = AX_VENC_StopRecvFrame(VencChn);
	if (0 != s32Ret) {
		AXV_LOGE("VencChn %d: AX_VENC_StopRecvFrame failed, s32Ret:0x%x", VencChn, s32Ret);
		return -1;
	}
	return 0;
}

int us_ax_set_resolution(VENC_CHN chn, AX_U32 width, AX_U32 height)
{
  AX_S32 s32Ret;
  int iRet;
  AX_VENC_CHN_ATTR_T stVencChnAttr;

  s32Ret = AX_VENC_StopRecvFrame(chn);
  if (s32Ret != 0) {
    AXV_LOGE("[%d] AX_VENC_StopRecvFrame failed, ret=0x%x", chn, s32Ret);
  }
  s32Ret = AX_VENC_ResetChn(chn);
  if (s32Ret == 0) {
    memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));
    s32Ret = AX_VENC_GetChnAttr(chn,&stVencChnAttr);
    if (s32Ret == 0) {
      stVencChnAttr.stVencAttr.u32PicWidthSrc = width;
      stVencChnAttr.stVencAttr.u32PicHeightSrc = height;
      s32Ret = AX_VENC_SetChnAttr(chn,&stVencChnAttr);
      iRet = 0;
      if (s32Ret == 0) goto done;
      AXV_LOGE("[%d] AX_VENC_SetChnAttr failed, ret=0x%x", chn, s32Ret);
    }
    else {
      AXV_LOGE("[%d] AX_VENC_GetChnAttr failed, ret=0x%x", chn, s32Ret);
    }
  }
  else {
    AXV_LOGE("[%d] AX_VENC_ResetChn failed, ret=0x%x", chn, s32Ret);
  }
  iRet = -1;
done:
  return iRet;
}

int us_ax_set_fps(VENC_CHN chn, uint32_t fps)
{
  AX_S32 s32Ret;
  int iRet;
  AX_VENC_RC_PARAM_T stRcParam;

  memset(&stRcParam, 0, sizeof(stRcParam));
  s32Ret = AX_VENC_GetRcParam(chn,&stRcParam);
  if (s32Ret == 0) {
    stRcParam.stFrameRate.fSrcFrameRate = fps;
    stRcParam.stFrameRate.fDstFrameRate = fps;
    s32Ret = AX_VENC_SetRcParam(chn,&stRcParam);
    iRet = 0;
    if (s32Ret == 0) goto done;
    AXV_LOGE("AX_VENC_SetRcParam failed, ret=0x%x", s32Ret);
  }
  else {
    AXV_LOGE("AX_VENC_GetRcParam failed, ret=0x%x", s32Ret);
  }
  iRet = -1;
done:
  return iRet;
}

int us_ax_encoder_check(us_ax_encoder_s *ax_enc, uint32_t width, uint32_t height, uint32_t fps)
{
	if (ax_enc->width == width &&
	    ax_enc->height == height &&
	    ax_enc->fps == fps) {
		return 0;
	}

	ax_enc->width      = width;
	ax_enc->height     = height;
	ax_enc->fps        = fps;

	AXV_LOGI("Using %dx%d %d fps", ax_enc->width, ax_enc->height, ax_enc->fps);

	if (ax_enc->venc_jpeg_run_) {
		us_ax_set_resolution(ax_enc->venc_jpeg_chn, ax_enc->width, ax_enc->height);
		us_ax_set_fps(ax_enc->venc_jpeg_chn, ax_enc->fps);
		us_ax_enable_stream(ax_enc->venc_jpeg_chn);
	}
	if (ax_enc->venc_h264_run_) {
		us_ax_set_resolution(ax_enc->venc_h264_chn, ax_enc->width, ax_enc->height);
		us_ax_set_fps(ax_enc->venc_h264_chn, ax_enc->fps);
		us_ax_enable_stream(ax_enc->venc_h264_chn);
	}
	return 0;
}

void us_ax_request_key_frame(VENC_CHN VencChn)
{
	AX_VENC_RequestIDR(VencChn, AX_FALSE);
}

int us_ax_get_stream_frame(VENC_CHN VencChn, us_frame_s *frame)
{
	AX_VENC_STREAM_T stStream = {0};
	AX_S32 s32Ret = AX_VENC_GetStream(VencChn, &stStream, 100);
	if (AX_SUCCESS == s32Ret) {
		AX_BOOL bIFrame = (AX_VENC_INTRA_FRAME == stStream.stPack.enCodingType) ? AX_TRUE : AX_FALSE;
		us_frame_set_data(frame, stStream.stPack.pu8Addr, stStream.stPack.u32Len);
		AXV_LOGD("VencChn %d: u64PTS:%lld pu8Addr:%p u32Len:%d enCodingType:%d", VencChn, stStream.stPack.u64PTS,
			stStream.stPack.pu8Addr, stStream.stPack.u32Len, stStream.stPack.enCodingType);
		s32Ret = AX_VENC_ReleaseStream(VencChn, &stStream);
		if (AX_SUCCESS != s32Ret) {
			AXV_LOGE("VencChn %d: AX_VENC_ReleaseStream failed! s32Ret:0x%x", VencChn, s32Ret);
			usleep(10000);
			return -1;
		}
		if (bIFrame) {
			return 3;
		}
		return 0;
	} else if (AX_ERR_VENC_FLOW_END == s32Ret) {
		AXV_LOGE("VencChn %d: AX_VENC_GetStream end flow, exit!", VencChn);
		usleep(10000);
		return -1;
	} else if (AX_ERR_VENC_QUEUE_EMPTY == s32Ret) {
		AXV_LOGD("VencChn %d: AX_VENC_GetStream queue empty", VencChn);
		return -1;
	} else {
		AXV_LOGW("VencChn %d: AX_VENC_GetStream failed, s32Ret:0x%x", VencChn, s32Ret);
	}
	return -1;
}

int us_ax_get_h264_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame, bool force_key) {
	VENC_CHN VencChn;
	int res;

	if (ax_enc == NULL) return -1;
	VencChn = ax_enc->venc_h264_chn;
	if (!ax_enc->venc_h264_run_)
		return -1;
	if (force_key)
		us_ax_request_key_frame(VencChn);
	res = us_ax_get_stream_frame(VencChn, frame);
	if (res == 0)
		return 4;
	return res;
}

int us_ax_get_mjpeg_frame(us_ax_encoder_s *ax_enc, us_frame_s *frame) {
	VENC_CHN VencChn;

	if (ax_enc == NULL) return -1;
	VencChn = ax_enc->venc_jpeg_chn;
	if (!ax_enc->venc_jpeg_run_)
		return -1;
	return us_ax_get_stream_frame(VencChn, frame);
}
