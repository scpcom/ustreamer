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

static int SAMPLE_IVPS_Init(AX_S32 nGrpId, AX_IVPS_PIPELINE_ATTR_T *pstPipelineAttr)
{
	AX_S32 s32Ret                          = 0, nChn;
	AX_IVPS_GRP_ATTR_T stGrpAttr           = {0};

	s32Ret = AX_IVPS_Init();
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_Init failed,s32Ret:0x%x", s32Ret);
		return s32Ret;
	}

	stGrpAttr.nInFifoDepth = 2;
	stGrpAttr.ePipeline    = AX_IVPS_PIPELINE_DEFAULT;
	s32Ret                 = AX_IVPS_CreateGrp(nGrpId, &stGrpAttr);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_CreateGrp failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}
	s32Ret = AX_IVPS_SetPipelineAttr(nGrpId, pstPipelineAttr);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_SetPipelineAttr failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}
	for (nChn = 0; nChn < pstPipelineAttr->nOutChnNum; nChn++) {
		s32Ret = AX_IVPS_EnableChn(nGrpId, nChn);
		if (AX_SUCCESS != s32Ret) {
			AXV_LOGE("AX_IVPS_EnableChn failed,nGrp %d,nChn %d,s32Ret:0x%x", nGrpId, nChn, s32Ret);
			return s32Ret;
		}
	}
	s32Ret = AX_IVPS_StartGrp(nGrpId);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_StartGrp failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}
#ifdef SAMPLE_IVPS_CROPRESIZE_ENABLE
	s32Ret = IVPS_CropResizeThreadStart(nGrpId, nChnGetId);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("IVPS_CropResizeThreadStart failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}
#endif
	AX_MOD_INFO_T srcMod, dstMod;
	srcMod.enModId  = AX_ID_VIN;
	srcMod.s32GrpId = 0;
	srcMod.s32ChnId = 0;

	dstMod.enModId  = AX_ID_IVPS;
	dstMod.s32GrpId = nGrpId;
	dstMod.s32ChnId = 0;
	AX_SYS_Link(&srcMod, &dstMod);
	return 0;
}

static AX_S32 SAMPLE_IVPS_DeInit(AX_S32 nGrpId)
{
	AX_S32 s32Ret = 0, nChn = 0;

	AX_MOD_INFO_T srcMod, dstMod;
	srcMod.enModId  = AX_ID_VIN;
	srcMod.s32GrpId = 0;
	srcMod.s32ChnId = 0;
	dstMod.enModId  = AX_ID_IVPS;
	dstMod.s32GrpId = nGrpId;
	dstMod.s32ChnId = 0;
	AX_SYS_UnLink(&srcMod, &dstMod);

#ifdef SAMPLE_IVPS_CROPRESIZE_ENABLE
	IVPS_CropResizeThreadStop();
#endif

	s32Ret = AX_IVPS_StopGrp(nGrpId);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}

	for (nChn = 0; nChn < 3; nChn++) {
		s32Ret = AX_IVPS_DisableChn(nGrpId, nChn);
		if (AX_SUCCESS != s32Ret) {
			AXV_LOGE("AX_IVPS_DisableChn failed,nGrp %d,nChn %d,s32Ret:0x%x", nGrpId, nChn, s32Ret);
			return s32Ret;
		}
	}

	s32Ret = AX_IVPS_DestoryGrp(nGrpId);
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_DestoryGrp failed,nGrp %d,s32Ret:0x%x", nGrpId, s32Ret);
		return s32Ret;
	}

	s32Ret = AX_IVPS_Deinit();
	if (AX_SUCCESS != s32Ret) {
		AXV_LOGE("AX_IVPS_Deinit failed,s32Ret:0x%x", s32Ret);
		return s32Ret;
	}

	return 0;
}

static AX_S32 SAMPLE_VIN_StartDev(AX_U8 devId, AX_BOOL bEnableDev, AX_VIN_DEV_ATTR_T *pDevAttr)
{
	AX_S32 nRet = 0;
	AX_VIN_DUMP_ATTR_T  tDumpAttr = {0};

	if (bEnableDev) {
		if (AX_VIN_DEV_OFFLINE == pDevAttr->eDevMode) {
			tDumpAttr.bEnable = AX_TRUE;
			tDumpAttr.nDepth = 3;
			nRet = AX_VIN_SetDevDumpAttr(devId, AX_VIN_DUMP_QUEUE_TYPE_DEV, &tDumpAttr);
			if (0 != nRet) {
				AXV_LOGE("AX_VIN_SetDevDumpAttr failed, ret=0x%x.", nRet);
				return -1;
			}
		}

		nRet = AX_VIN_EnableDev(devId);
		if (0 != nRet) {
			AXV_LOGE("AX_VIN_EnableDev failed, ret=0x%x.", nRet);
			return -1;
		}
	}

	return 0;
}

static AX_S32 SAMPLE_VIN_StopDev(AX_U8 devId, AX_BOOL bEnableDev)
{
	AX_S32 axRet;
	AX_VIN_DEV_ATTR_T tDevAttr = {0};
	AX_VIN_DUMP_ATTR_T tDumpAttr = {0};

	AX_VIN_GetDevAttr(devId, &tDevAttr);

	if (bEnableDev) {
		axRet = AX_VIN_DisableDev(devId);
		if (0 != axRet) {
			AXV_LOGE("AX_VIN_DisableDev failed, devId=%d, ret=0x%x.", devId, axRet);
		}

		if (AX_VIN_DEV_OFFLINE == tDevAttr.eDevMode) {
			tDumpAttr.bEnable = AX_FALSE;
			axRet = AX_VIN_SetDevDumpAttr(devId, AX_VIN_DUMP_QUEUE_TYPE_DEV, &tDumpAttr);
			if (0 != axRet) {
				AXV_LOGE("AX_VIN_SetDevDumpAttr failed, ret=0x%x.", axRet);
			}
		}

	}

	return 0;
}

static void us_ax_get_lt_info(us_ax_encoder_s *ax_enc)
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
		ax_enc->width = width;
		ax_enc->height = height;
	}
	else {
		ax_enc->width = 1920;
		ax_enc->height = 1080;
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
	ax_enc->fps = fps;
	if (!ax_enc->desired_fps)
		ax_enc->desired_fps = fps;
	AXV_LOGI("Using %dx%d %d fps", ax_enc->width, ax_enc->height, ax_enc->desired_fps);
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

	us_ax_get_lt_info(ax_enc);

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
#if 0
	SAMPLE_IVPS_Init(0, ax_enc);
#endif
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
	SAMPLE_IVPS_DeInit(0);
#endif
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
	int res;
	VENC_CHN VencChn = ax_enc->venc_h264_chn;
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
	VENC_CHN VencChn = ax_enc->venc_jpeg_chn;
	if (!ax_enc->venc_jpeg_run_)
		return -1;
	return us_ax_get_stream_frame(VencChn, frame);
}
