#include <fcntl.h> /* low-level i/o */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h>
#include <sys/mman.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "../../../libs/frame.h"
#include "../../../libs/logging.h"

#include "ax_global_type.h"
#include "ax_venc_comm.h"
#include "ax_venc_api.h"
#include "ax_sys_api.h"
#include "ax_ivps_api.h"
#include "ax_vin_api.h"
#include "ax_isp_api.h"
#include "ax_mipi_rx_api.h"

#include "ax_capture.h"
#include "ax_encoder.h"

static bool AX_CAP_VIN_Init(us_ax_capture_s *ax_cap, uint32_t max_width, uint32_t max_height)
{
  uint uTmp;
  bool bRet;
  AX_S32 s32Ret;
  AX_POOL_FLOORPLAN_T tPoolFloorPlan;

  s32Ret = AX_POOL_Exit();
  if (s32Ret == 0) {
    memset(&tPoolFloorPlan, 0, sizeof(tPoolFloorPlan));
    uTmp = max_width >> 2 & 0x3ffffff;
    if ((max_width & 3) != 0) {
      uTmp = uTmp + 1;
    }
    tPoolFloorPlan.CommPool[0].MetaSize = 4096;
    tPoolFloorPlan.CommPool[0].BlkCnt = 6;
    tPoolFloorPlan.CommPool[0].BlkSize =
         (AX_U64)(((max_height + 7U) & 0xfffffff8) * ((uTmp * 8 + 0x3f) >> 3 & 0x1ffffff8) * 8);
    strcpy((char*)tPoolFloorPlan.CommPool[0].PartitionName,"anonymous");
    tPoolFloorPlan.CommPool[1].BlkSize = 112640;
    tPoolFloorPlan.CommPool[1].BlkCnt = 4;
    tPoolFloorPlan.CommPool[1].MetaSize = 4096;
    tPoolFloorPlan.CommPool[1].CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
    tPoolFloorPlan.CommPool[1].PartitionName[0] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[1] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[2] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[3] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[4] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[5] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[6] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[7] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[8] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[9] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[10] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0xb] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0xc] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0xd] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0xe] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0xf] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x10] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x11] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x12] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x13] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x14] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x15] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x16] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x17] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x18] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x19] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1a] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1b] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1c] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1d] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1e] = '\0';
    tPoolFloorPlan.CommPool[1].PartitionName[0x1f] = '\0';
    strcpy((char*)tPoolFloorPlan.CommPool[1].PartitionName,"anonymous");
    s32Ret = AX_POOL_SetConfig(&tPoolFloorPlan);
    if (s32Ret == 0) {
      s32Ret = AX_POOL_Init();
      if (s32Ret == 0) {
        AXV_LOGD(
                     "Create common pool success! Pool[0] BlkSize:%lld, BlkCnt:%d, Pool[1] BlkSize:%lld,  BlkCnt:%d",
                     tPoolFloorPlan.CommPool[0].BlkSize,tPoolFloorPlan.CommPool[0].BlkCnt,
                     tPoolFloorPlan.CommPool[1].BlkSize,tPoolFloorPlan.CommPool[1].BlkCnt);
        s32Ret = AX_VIN_Init();
        if (s32Ret == 0) {
          bRet = true;
          goto done;
        }
        AXV_LOGE("AX_VIN_Init failed, ret=0x%x", s32Ret);
      }
      else {
        AXV_LOGE("AX_POOL_Init fail! ret=0x%x", s32Ret);
      }
    }
    else {
      AXV_LOGI(
                   "Create common pool err! Pool[0] BlkSize:%lld, BlkCnt:%d, Pool[1] BlkSize:%lld, BlkCn t:%d",
                   tPoolFloorPlan.CommPool[0].BlkSize,tPoolFloorPlan.CommPool[0].BlkCnt,
                   tPoolFloorPlan.CommPool[1].BlkSize,tPoolFloorPlan.CommPool[1].BlkCnt);
    }
  }
  else {
    AXV_LOGE("AX_POOL_Exit failed, ret=0x%x", s32Ret);
    {
      AXV_LOGI("Please reboot system due to AX_POOL_Exit failure...");
      sync();
      sleep(1);
      //system("reboot");
      sleep(5);
    }
  }
  bRet = false;
done:
  return bRet;
}

static bool AX_CAP_VIN_Deinit(us_ax_capture_s *ax_cap)
{
  bool bRet;
  AX_S32 s32Ret;

  s32Ret = AX_VIN_Deinit();
  if (s32Ret == 0) {
    s32Ret = AX_POOL_Exit();
    if (s32Ret == 0) {
      bRet = true;
      goto done;
    }
    AXV_LOGE("AX_POOL_Exit failed, ret=0x%x", s32Ret);
  }
  else {
    AXV_LOGE("AX_VIN_DeInit failed, ret=0x%x", s32Ret);
  }
  bRet = false;
done:
  return bRet;
}

static bool AX_CAP_SYS_Init(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;

  if (ax_cap->sys_run == 0) {
    s32Ret = AX_SYS_Init();
    if (s32Ret != 0) {
      AXV_LOGE("AX_SYS_Init failed, ret=0x%x", s32Ret);
      ax_cap->sys_run = 0;
      return false;
    }
    ax_cap->sys_run = 1;
  }
  return true;
}

static void AX_CAP_SYS_Deinit(us_ax_capture_s *ax_cap)
{
  bool bRet;
  AX_S32 s32Ret;

  if (ax_cap->sys_run != 0) {
    bRet = AX_CAP_VIN_Deinit(ax_cap);
    if (!bRet) {
      AXV_LOGE("Pool deinit failed");
    }
    s32Ret = AX_SYS_Deinit();
    if (s32Ret == 0) {
      ax_cap->sys_run = 0;
    }
    else {
      AXV_LOGE("AX_SYS_Deinit failed, ret=0x%x", s32Ret);
    }
  }
  return;
}

static bool AX_CAP_HDMI_Enable(us_ax_capture_s *ax_cap)
{
  uint uAddr;
  bool bRet;
  int __fd;
  int *pError;
  void *__addr;
  uint *pData;
  long lCntr;
  ulong __offset;
  char *sError;
  uint32_t pin_data[2] = { 0x0230000C, 0x00020043 };

  __fd = open("/dev/mem",0x101002);
  if (__fd < 0) {
    pError = __errno_location();
    sError = strerror(*pError);
    AXV_LOGE("Failed to open /dev/mem, error: %s", sError);
    bRet = false;
  }
  else {
    pData = &pin_data[0];
    lCntr = 0xc;
    while (lCntr = lCntr + -1, lCntr != 0) {
      uAddr = *pData;
      __offset = (ulong)uAddr & 0xfffff000;
      __addr = mmap((void *)0x0,0x1000,3,1,__fd,__offset);
      if (__addr == (void *)0xffffffffffffffff) {
        pError = __errno_location();
        sError = strerror(*pError);
        AXV_LOGE("mmap failed for address 0x%x, error: %s", *pData, sError);
      }
      else {
        *(uint *)((long)__addr + (uAddr - __offset)) = pData[1];
        munmap(__addr,0x1000);
      }
      pData = pData + 2;
    }
    close(__fd);
    bRet = true;
  }
  return bRet;
}

static bool AX_CAP_MIPI_RX_Init(us_ax_capture_s *ax_cap)
{
  bool bRet;
  AX_S32 s32Ret;
  AX_MIPI_RX_DEV_T tMipiDev;

  s32Ret = AX_MIPI_RX_Init();
  if (s32Ret != 0) {
    AXV_LOGE("AX_MIPI_RX_Init failed, ret=0x%x", s32Ret);
    return '\0';
  }
  AX_MIPI_RX_SetLaneCombo(AX_LANE_COMBO_MODE_0);
  ax_cap->vin_chn = 0;
  memset(&tMipiDev, 0, sizeof(tMipiDev));
  tMipiDev.eInputMode = AX_INPUT_MODE_MIPI;
  tMipiDev.tMipiAttr.ePhyMode = AX_MIPI_PHY_TYPE_DPHY;
  tMipiDev.tMipiAttr.eLaneNum = AX_MIPI_DATA_LANE_4;
  tMipiDev.tMipiAttr.nDataRate = 600;
  tMipiDev.tMipiAttr.nDataLaneMap[0] = 0;
  tMipiDev.tMipiAttr.nDataLaneMap[1] = 1;
  tMipiDev.tMipiAttr.nDataLaneMap[2] = 3;
  tMipiDev.tMipiAttr.nDataLaneMap[3] = 4;
  tMipiDev.tMipiAttr.nClkLane[0] = 2;
  tMipiDev.tMipiAttr.nClkLane[1] = 5;
  s32Ret = AX_MIPI_RX_SetAttr(0,&tMipiDev);
  if (s32Ret == 0) {
    s32Ret = AX_MIPI_RX_Reset(ax_cap->vin_chn);
    if (s32Ret == 0) {
      s32Ret = AX_MIPI_RX_Start(ax_cap->vin_chn);
      if (s32Ret == 0) {
        bRet = AX_CAP_HDMI_Enable(ax_cap);
        if (bRet) {
          return bRet;
        }
        AXV_LOGE("Set pinmux failed");
        goto fail;
      }
      AXV_LOGE("AX_MIPI_RX_Start failed, ret=0x%x", s32Ret);
    }
    else {
      AXV_LOGE("AX_MIPI_RX_Reset, ret=0x%x", s32Ret);
    }
  }
  else {
    AXV_LOGE("AX_MIPI_RX_SetAttr failed, ret=0x%x", s32Ret);
  }
fail:
  ax_cap->vin_chn = 0xffffffff;
  return false;
}

static bool AX_CAP_MIPI_RX_Deinit(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;

  s32Ret = AX_MIPI_RX_Stop(ax_cap->vin_chn);
  if (s32Ret == 0) {
    s32Ret = AX_MIPI_RX_DeInit();
    if (s32Ret == 0) {
      ax_cap->vin_chn = 0xffffffff;
      return true;
    }
    AXV_LOGE("AX_MIPI_RX_DeInit failed, ret=0x%x", s32Ret);
  }
  else {
    AXV_LOGE("AX_MIPI_RX_Stop failed, ret=0x%x", s32Ret);
  }
  return false;
}

static bool AX_CAP_ISP_StreamOn(us_ax_capture_s *ax_cap)
{
  AX_U8 nDevId;
  bool bRet;
  void *pHandler;
  AX_SENSOR_REGISTER_FUNC_T *ptSnsRegister;
  int imgRgn;
  AX_S32 s32Ret;
  AX_VIN_CHN_ATTR_T tChnAttr;
  AX_SNS_ATTR_T tSnsAttr;
  AX_VIN_DEV_BIND_PIPE_T tDevBindPipe;
  AX_VIN_PIPE_ATTR_T tPipeAttr;
  AX_VIN_DEV_ATTR_T tDevAttr;

  bRet = AX_CAP_MIPI_RX_Init(ax_cap);
  s32Ret = (AX_S32)bRet;
  if (!bRet) {
    AXV_LOGE("axera_mipi_init failed, ret=0x%x", s32Ret);
  }
  else {
    memset(&tDevAttr, 0, sizeof(tDevAttr));
    tDevAttr.bImgDataEnable = AX_TRUE;
    tDevAttr.eSnsMode = AX_SNS_LINEAR_ONLY_MODE;
    tDevAttr.eBayerPattern = AX_BP_BGGR;
    tDevAttr.ePixelFmt = AX_FORMAT_BAYER_RAW_16BPP;

    tDevAttr.tFrameRateCtrl.fSrcFrameRate = 1.0;
    tDevAttr.tFrameRateCtrl.fDstFrameRate = 1.0;
    tDevAttr.tMipiIntfAttr.szImgDt[0] = 0x1e;

    tDevAttr.tDevImgRgn[0].nWidth = ax_cap->dst_width;
    tDevAttr.tDevImgRgn[0].nHeight = ax_cap->dst_height;
    tDevAttr.tDevImgRgn[1].nWidth = tDevAttr.tDevImgRgn[0].nWidth;
    tDevAttr.tDevImgRgn[1].nHeight = tDevAttr.tDevImgRgn[0].nHeight;
    tDevAttr.tDevImgRgn[2].nWidth = tDevAttr.tDevImgRgn[0].nWidth;
    tDevAttr.tDevImgRgn[2].nHeight = tDevAttr.tDevImgRgn[0].nHeight;
    tDevAttr.tDevImgRgn[3].nWidth = tDevAttr.tDevImgRgn[0].nWidth;
    tDevAttr.tDevImgRgn[3].nHeight = tDevAttr.tDevImgRgn[0].nHeight;

    AXV_LOGD(
                 "eDevMode: %d, bImgDataEnable: %d, bNonImgDataEnable: %d, eSnsIntfType: %d, eSnsMode: %d, eBayerPattern: %d, ePixelFmt: %d",
                 tDevAttr.eDevMode,tDevAttr.bImgDataEnable,tDevAttr.bNonImgDataEnable,
                 tDevAttr.eSnsIntfType,tDevAttr.eSnsMode,tDevAttr.eBayerPattern,tDevAttr.ePixelFmt);
    imgRgn = 0;
    while( true ) {
      if (imgRgn == 4) break;
      AXV_LOGD(
                   "tDevImgRgn[%d]: (%d, %d, %d, %d)", imgRgn,
		   tDevAttr.tDevImgRgn[imgRgn].nStartX,
		   tDevAttr.tDevImgRgn[imgRgn].nStartY,
		   tDevAttr.tDevImgRgn[imgRgn].nWidth,
		   tDevAttr.tDevImgRgn[imgRgn].nHeight);
      imgRgn = imgRgn + 1;
    }
    AXV_LOGD(
                 "tFrameRateCtrl.fSrcFrameRate: %.2f, tFrameRateCtrl.fDstFrameRate: %.2f",
                 tDevAttr.tFrameRateCtrl.fSrcFrameRate,tDevAttr.tFrameRateCtrl.fDstFrameRate);
    AXV_LOGD(
                 "tCompressInfo.enCompressMode: %d, tCompressInfo.u32CompressLevel: %d",
                 tDevAttr.tCompressInfo.enCompressMode,tDevAttr.tCompressInfo.u32CompressLevel);
    AXV_LOGD(
                 "szImgVc[0]: %d, szImgDt[0]: 0x%.2x",
		 tDevAttr.tMipiIntfAttr.szImgVc[0],
		 tDevAttr.tMipiIntfAttr.szImgDt[0]);
    s32Ret = AX_VIN_CreateDev((AX_U8)ax_cap->vin_chn,&tDevAttr);
    if (s32Ret == 0) {
      s32Ret = AX_VIN_SetDevAttr((AX_U8)ax_cap->vin_chn,&tDevAttr);
      if (s32Ret == 0) {
        tDevBindPipe.nPipeId[3] = 0;
        tDevBindPipe.nPipeId[4] = 0;
        tDevBindPipe.nPipeId[1] = 0;
        tDevBindPipe.nPipeId[2] = 0;
        tDevBindPipe.nHDRSel[1] = 0;
        tDevBindPipe.nHDRSel[4] = 0;
        tDevBindPipe.nHDRSel[5] = 0;
        tDevBindPipe.nHDRSel[2] = 0;
        tDevBindPipe.nHDRSel[3] = 0;
        tDevBindPipe.nPipeId[0] = ax_cap->vin_chn;
        tDevBindPipe.nNum = 1;
        tDevBindPipe.nPipeId[5] = 0;
        tDevBindPipe.nHDRSel[0] = 1;
        s32Ret = AX_VIN_SetDevBindPipe((AX_U8)tDevBindPipe.nPipeId[0],&tDevBindPipe);
        if (s32Ret == 0) {
          nDevId = (AX_U8)ax_cap->vin_chn;
          s32Ret = AX_VIN_SetDevBindMipi(nDevId,nDevId);
          if (s32Ret == 0) {
            memset(&tPipeAttr, 0, sizeof(tPipeAttr));
            tPipeAttr.ePipeWorkMode = AX_VIN_PIPE_ISP_BYPASS_MODE;
            tPipeAttr.eBayerPattern = AX_BP_BGGR;
            tPipeAttr.ePixelFmt = AX_FORMAT_BAYER_RAW_16BPP;
            tPipeAttr.eSnsMode = AX_SNS_LINEAR_ONLY_MODE;
            tPipeAttr.tPipeImgRgn.nWidth = ax_cap->dst_width;
            tPipeAttr.nWidthStride = tPipeAttr.tPipeImgRgn.nWidth;
            tPipeAttr.tPipeImgRgn.nHeight = ax_cap->dst_height;
            s32Ret = AX_VIN_CreatePipe((AX_U8)ax_cap->vin_chn,&tPipeAttr);
            if (s32Ret == 0) {
              s32Ret = AX_VIN_SetPipeAttr((AX_U8)ax_cap->vin_chn,&tPipeAttr);
              if (s32Ret == 0) {
                pHandler = (void *)dlopen("/opt/lib/libsns_dummy.so",1);
                ax_cap->libsns_handler = pHandler;
                if (pHandler == NULL) {
                  AXV_LOGE("ISP dlopen failed: %s","libsns_dummy.so");
                }
                else {
                  AXV_LOGD("dlopen %s success","libsns_dummy.so");
                  ptSnsRegister =
                       (AX_SENSOR_REGISTER_FUNC_T *)dlsym(ax_cap->libsns_handler,"gSnsdummyObj");
                  s32Ret = AX_ISP_RegisterSensor((AX_U8)ax_cap->vin_chn,ptSnsRegister);
                  if (s32Ret == 0) {
                    tSnsAttr.fFrameRate = 60.0;
                    tSnsAttr.eSnsMode = AX_SNS_LINEAR_ONLY_MODE;
                    tSnsAttr.eRawType = AX_RT_RAW16;
                    tSnsAttr.eBayerPattern = AX_BP_BGGR;
                    tSnsAttr.bTestPatternEnable = AX_FALSE;
                    tSnsAttr.eMasterSlaveSel = AX_SNS_MASTER;
                    tSnsAttr.nSettingIndex = 0;
                    tSnsAttr.eSnsOutputMode = AX_SNS_NORMAL;
                    tSnsAttr.nWidth = ax_cap->dst_width;
                    tSnsAttr.nHeight = ax_cap->dst_height;
                    s32Ret = AX_ISP_SetSnsAttr((AX_U8)ax_cap->vin_chn,&tSnsAttr);
                    if (s32Ret == 0) {
                      s32Ret = AX_ISP_Create((AX_U8)ax_cap->vin_chn);
                      if (s32Ret == 0) {
                        s32Ret = AX_ISP_Open((AX_U8)ax_cap->vin_chn);
                        if (s32Ret == 0) {
                          tChnAttr.eImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
                          tChnAttr.nDepth = 1;
                          tChnAttr.nWidth = ax_cap->dst_width;
                          tChnAttr.nHeight = ax_cap->dst_height;
                          tChnAttr.tCompressInfo.enCompressMode = AX_COMPRESS_MODE_NONE;
                          tChnAttr.tCompressInfo.u32CompressLevel = 0;
                          tChnAttr.tFrameRateCtrl.fSrcFrameRate = 0.0;
                          tChnAttr.tFrameRateCtrl.fDstFrameRate = 0.0;
                          tChnAttr.nWidthStride = ax_cap->dst_width;
                          s32Ret = AX_VIN_SetChnAttr((AX_U8)ax_cap->vin_chn,ax_cap->vin_chn,
                                                        &tChnAttr);
                          if (s32Ret == 0) {
                            s32Ret = AX_VIN_SetChnFrameMode
                                                  ((AX_U8)ax_cap->vin_chn,ax_cap->vin_chn,
                                                   AX_VIN_FRAME_MODE_OFF);
                            if (s32Ret == 0) {
                              s32Ret = AX_VIN_EnableChn((AX_U8)ax_cap->vin_chn,ax_cap->vin_chn)
                              ;
                              if (s32Ret == 0) {
                                s32Ret = AX_VIN_StartPipe((AX_U8)ax_cap->vin_chn);
                                if (s32Ret == 0) {
                                  s32Ret = AX_ISP_Start((AX_U8)ax_cap->vin_chn);
                                  if (s32Ret == 0) {
                                    s32Ret = AX_VIN_EnableDev((AX_U8)ax_cap->vin_chn);
                                    if (s32Ret == 0) {
                                      s32Ret = AX_ISP_StreamOn((AX_U8)ax_cap->vin_chn);
                                      if (s32Ret == 0) {
                                        return bRet;
                                      }
                                      AXV_LOGE("AX_ISP_StreamOn failed, ret=0x%x", s32Ret);
                                    }
                                    else {
                                      AXV_LOGE("AX_VIN_EnableDev failed, ret=0x%x", s32Ret);
                                    }
                                  }
                                  else {
                                    AXV_LOGE("AX_ISP_Start failed, ret=0x%x", s32Ret);
                                  }
                                }
                                else {
                                  AXV_LOGE("AX_VIN_StartPipe failed, ret=0x%x", s32Ret);
                                }
                              }
                              else {
                                AXV_LOGE("AX_VIN_EnableChn failed, ret=0x%x", s32Ret);
                              }
                            }
                            else {
                              AXV_LOGE("AX_VIN_SetChnFrameMode failed, ret=0x%x", s32Ret);
                            }
                          }
                          else {
                            AXV_LOGE("AX_VIN_SetChnAttr failed, ret=0x%x", s32Ret);
                          }
                          goto fail;
                        }
                        AXV_LOGE("AX_ISP_Open failed, ret=0x%x", s32Ret);
                      }
                      else {
                        AXV_LOGE("AX_ISP_Create failed, ret=0x%x", s32Ret);
                      }
                    }
                    else {
                      AXV_LOGE("AX_ISP_SetSnsAttr failed, ret=0x%x", s32Ret);
                    }
                    goto fail;
                  }
                  AXV_LOGE("AX_ISP Register Sensor Failed, ret=0x%x", s32Ret);
                }
              }
              else {
                AXV_LOGE("AX_VI_SetPipeAttr failed, ret=0x%x", s32Ret);
              }
            }
            else {
              AXV_LOGE("AX_VIN_CreatePipe failed, ret=0x%x", s32Ret);
            }
            goto fail;
          }
          AXV_LOGE("AX_VIN_SetDevBindMipi failed, ret=0x%x", s32Ret);
        }
        else {
          AXV_LOGE("AX_VIN_SetDevBindPipe failed, ret=0x%x", s32Ret);
        }
      }
      else {
        AXV_LOGE("AX_VIN_CreateDev failed, ret=0x%x", s32Ret);
      }
    }
    else {
      AXV_LOGE("AX_VIN_CreateDev failed, ret=0x%x", s32Ret);
    }
  }
fail:
  return false;
}

static bool AX_CAP_ISP_StreamOff(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;

  s32Ret = AX_ISP_StreamOff((AX_U8)ax_cap->vin_chn);
  if (s32Ret == 0) {
    s32Ret = AX_VIN_DisableDev((AX_U8)ax_cap->vin_chn);
    if (s32Ret == 0) {
      s32Ret = AX_ISP_Stop((AX_U8)ax_cap->vin_chn);
      if (s32Ret == 0) {
        s32Ret = AX_VIN_StopPipe((AX_U8)ax_cap->vin_chn);
        if (s32Ret == 0) {
          s32Ret = AX_VIN_DisableChn((AX_U8)ax_cap->vin_chn,ax_cap->vin_chn);
          if (s32Ret == 0) {
            s32Ret = AX_ISP_Close((AX_U8)ax_cap->vin_chn);
            if (s32Ret == 0) {
              s32Ret = AX_ISP_Destroy((AX_U8)ax_cap->vin_chn);
              if (s32Ret == 0) {
                s32Ret = AX_ISP_UnRegisterSensor((AX_U8)ax_cap->vin_chn);
                if (s32Ret == 0) {
                  if (ax_cap->libsns_handler != NULL) {
                    dlclose(ax_cap->libsns_handler);
                    AXV_LOGD("dlclose %s", "libsns_dummy.so");
                    ax_cap->libsns_handler = NULL;
                  }
                  s32Ret = AX_VIN_DestroyPipe((AX_U8)ax_cap->vin_chn);
                  if (s32Ret == 0) {
                    s32Ret = AX_VIN_DestroyDev((AX_U8)ax_cap->vin_chn);
                    if (s32Ret == 0) {
                      if ((-1 < (int)ax_cap->vin_chn) &&
                         (!AX_CAP_MIPI_RX_Deinit(ax_cap))) {
                        AXV_LOGE("MIPI deinit failed");
                      }
                      return true;
                    }
                    AXV_LOGE("AX_VIN_DestroyDev failed, ret=0x%x", s32Ret);
                  }
                  else {
                    AXV_LOGE("AX_VIN_DestroyPipe failed, ret=0x%x", s32Ret);
                  }
                }
                else {
                  AXV_LOGE("AX_ISP_UnRegisterSensor failed, ret=0x%x", s32Ret);
                }
              }
              else {
                AXV_LOGE("AX_ISP_Destroy failed, ret=0x%x", s32Ret);
              }
            }
            else {
              AXV_LOGE("AX_ISP_Close failed, ret=0x%x", s32Ret);
            }
          }
          else {
            AXV_LOGE("AX_VIN_DisableChn failed, ret=0x%x", s32Ret);
          }
        }
        else {
          AXV_LOGE("AX_VIN_StopPipe failed, ret=0x%x", s32Ret);
        }
      }
      else {
        AXV_LOGE("AX_ISP_Stop failed, ret=0x%x", s32Ret);
      }
    }
    else {
      AXV_LOGE("AX_VIN_DisableDev failed, ret=0x%x", s32Ret);
    }
  }
  else {
    AXV_LOGE("AX_ISP_StreamOff failed, ret=0x%x", s32Ret);
  }
  return false;
}

static bool AX_CAP_IVPS_Init(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;
  AX_IVPS_GRP_ATTR_T stGrpAttr;
  AX_IVPS_PIPELINE_ATTR_T stPipelineAttr;

  s32Ret = AX_IVPS_Init();
  if (s32Ret == 0) {
    ax_cap->ivps_grp = 0;
    stGrpAttr.nInFifoDepth = '\x01';
    stGrpAttr.ePipeline = AX_IVPS_PIPELINE_DEFAULT;
    s32Ret = AX_IVPS_CreateGrp(0,&stGrpAttr);
    if (s32Ret == 0) {
      memset(&stPipelineAttr, 0, sizeof(stPipelineAttr));
      stPipelineAttr.tFilter[0][0].nDstPicWidth = (AX_U16)ax_cap->dst_width;
      stPipelineAttr.tFilter[0][0].nDstPicHeight = (AX_U16)ax_cap->dst_height;
      stPipelineAttr.tFilter[0][0].nDstPicStride =
           (stPipelineAttr.tFilter[0][0].nDstPicWidth + 0xf) & 0xfff0;
      stPipelineAttr.tFilter[1][1].nDstPicWidth = 0x140;
      stPipelineAttr.tFilter[1][1].nDstPicHeight = 0xac;
      stPipelineAttr.tFilter[1][1].nDstPicStride = 0x140;
      stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate = (AX_F32)(int)ax_cap->src_fps;
      stPipelineAttr.tFilter[1][1].eDstPicFormat = AX_FORMAT_RGB565;
      stPipelineAttr.nOutChnNum = '\x01';
      stPipelineAttr.nInDebugFifoDepth = 0;
      stPipelineAttr.nOutFifoDepth[0] = '\x01';
      stPipelineAttr.nOutFifoDepth[1] = '\0';
      stPipelineAttr.nOutFifoDepth[2] = '\0';
      stPipelineAttr.nOutFifoDepth[3] = '\0';
      stPipelineAttr.tFilter[0][0].eEngine = AX_IVPS_ENGINE_TDP;
      stPipelineAttr.tFilter[0][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR;
      stPipelineAttr.tFilter[1][0].bEngage = AX_TRUE;
      stPipelineAttr.tFilter[1][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR;
      stPipelineAttr.tFilter[1][1].bEngage = AX_TRUE;
      stPipelineAttr.tFilter[1][1].eEngine = AX_IVPS_ENGINE_TDP;
      stPipelineAttr.tFilter[0][0].tFRC.fDstFrameRate =
           stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate;
      stPipelineAttr.tFilter[1][0].tFRC.fSrcFrameRate =
           stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate;
      stPipelineAttr.tFilter[1][0].tFRC.fDstFrameRate =
           stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate;
      stPipelineAttr.tFilter[1][0].nDstPicWidth = stPipelineAttr.tFilter[0][0].nDstPicWidth;
      stPipelineAttr.tFilter[1][0].nDstPicHeight = stPipelineAttr.tFilter[0][0].nDstPicHeight;
      stPipelineAttr.tFilter[1][0].nDstPicStride = stPipelineAttr.tFilter[0][0].nDstPicStride;
      stPipelineAttr.tFilter[1][1].tFRC.fSrcFrameRate =
           stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate;
      stPipelineAttr.tFilter[1][1].tFRC.fDstFrameRate =
           stPipelineAttr.tFilter[0][0].tFRC.fSrcFrameRate;
      s32Ret = AX_IVPS_SetPipelineAttr(ax_cap->ivps_grp,&stPipelineAttr);
      if (s32Ret == 0) {
        s32Ret = AX_IVPS_EnableChn(ax_cap->ivps_grp,0);
        if (s32Ret == 0) {
          s32Ret = AX_IVPS_StartGrp(ax_cap->ivps_grp);
          if (s32Ret == 0) {
            return true;
          }
          AXV_LOGE("AX_IVPS_StartGrp failed, ret=0x%x", s32Ret);
        }
        else {
          AXV_LOGE("AX_IVPS_EnableChn failed, ret=0x%x", s32Ret);
        }
      }
      else {
        AXV_LOGE("AX_IVPS_SetPipelineAttr failed, ret=0x%x", s32Ret);
      }
      goto fail;
    }
    AXV_LOGE("AX_IVPS_CreateGrp failed, ret=0x%x", s32Ret);
  }
  else {
    AXV_LOGE("AX_IVPS_Init failed, ret=0x%x", s32Ret);
  }
fail:
  return false;
}

static bool AX_CAP_IVPS_Deinit(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;

  s32Ret = AX_IVPS_StopGrp(ax_cap->ivps_grp);
  if (s32Ret == 0) {
    s32Ret = AX_IVPS_DisableChn(ax_cap->ivps_grp,1);
    if (s32Ret == 0) {
      s32Ret = AX_IVPS_DisableChn(ax_cap->ivps_grp,0);
      if (s32Ret == 0) {
        s32Ret = AX_IVPS_DestoryGrp(ax_cap->ivps_grp);
        if (s32Ret == 0) {
          s32Ret = AX_IVPS_Deinit();
          if (s32Ret == 0) {
            return true;
          }
          AXV_LOGE("AX_IVPS_Deinit failed, ret=0x%x", s32Ret);
        }
        else {
          AXV_LOGE("AX_IVPS_DestoryGrp failed, ret=0x%x", s32Ret);
        }
      }
      else {
        AXV_LOGE("AX_IVPS_DisableChn failed, ret=0x%x", s32Ret);
      }
    }
    else {
      AXV_LOGE("AX_IVPS_DisableChn failed, ret=0x%x", s32Ret);
    }
  }
  else {
    AXV_LOGE("AX_IVPS_StopGrp failed, ret=0x%x", s32Ret);
  }
  return false;
}

static bool AX_CAP_SYS_Link(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;
  AX_MOD_INFO_T srcMod;
  AX_MOD_INFO_T dstMod;
  bool bRet;

  srcMod.enModId = AX_ID_VIN;
  srcMod.s32GrpId = 0;
  dstMod.s32ChnId = 0;
  srcMod.s32ChnId = ax_cap->vin_chn;
  dstMod.enModId = AX_ID_IVPS;
  dstMod.s32GrpId = ax_cap->ivps_grp;
  s32Ret = AX_SYS_Link(&srcMod,&dstMod);
  bRet = s32Ret == 0;
  if (!bRet) {
    AXV_LOGE("AX_SYS_Link failed (VIN->IVPS), ret=0x%x", s32Ret);
  }
  return bRet;
}

static bool AX_CAP_SYS_Unlink(us_ax_capture_s *ax_cap)
{
  AX_S32 s32Ret;
  AX_MOD_INFO_T srcMod;
  AX_MOD_INFO_T dstMod;
  bool bRet;

  srcMod.enModId = AX_ID_VIN;
  srcMod.s32GrpId = 0;
  dstMod.s32ChnId = 0;
  srcMod.s32ChnId = ax_cap->vin_chn;
  dstMod.enModId = AX_ID_IVPS;
  dstMod.s32GrpId = ax_cap->ivps_grp;
  s32Ret = AX_SYS_UnLink(&srcMod,&dstMod);
  bRet = s32Ret == 0;
  if (!bRet) {
    AXV_LOGE("AX_SYS_UnLink failed (VIN->IVPS), ret=0x%x", s32Ret);
  }
  return bRet;
}

static bool AX_CAP_Init(us_ax_capture_s *ax_cap)
{
  bool bRet = false;

  AXV_LOGI("Axera vin init...");
  bRet = AX_CAP_SYS_Init(ax_cap);
  if (!bRet) {
    AXV_LOGE("Axera util init failed");
  }
  else {
    bRet = AX_CAP_VIN_Init(ax_cap,3840,2400);
    if (!bRet) {
      AXV_LOGE("Axera common pool init failed");
    }
    else {
      bRet = AX_CAP_ISP_StreamOn(ax_cap);
      if (!bRet) {
        AXV_LOGE("Axera vin init failed");
      }
      else {
        bRet = AX_CAP_IVPS_Init(ax_cap);
        if (!bRet) {
          AXV_LOGE("Axera IPVS init failed");
        }
        else {
          bRet = AX_CAP_SYS_Link(ax_cap);
          if (!bRet) {
            AXV_LOGE("Axera Link init failed");
          }
	}
      }
    }
  }

  return bRet;
}

static void AX_CAP_Deinit(us_ax_capture_s *ax_cap)
{
  bool bRet = false;

  bRet = AX_CAP_SYS_Unlink(ax_cap);
  if (!bRet) {
    AXV_LOGE("Axera Link deinit failed");
  }
  bRet = AX_CAP_IVPS_Deinit(ax_cap);
  if (!bRet) {
    AXV_LOGI("IPVS deinit failed");
  }
  bRet = AX_CAP_ISP_StreamOff(ax_cap);
  if (!bRet) {
    AXV_LOGE("VIN deinit failed");
  }
  AX_CAP_SYS_Deinit(ax_cap);
  AXV_LOGI("Axera vin exiting...");
  return;
}

static bool file_exists(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1) {
		return false;
	}

	return (sb.st_mode & S_IFMT) == S_IFREG;
}

static bool socket_exists(const char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1) {
		return false;
	}

	return (sb.st_mode & S_IFMT) == S_IFSOCK;
}

static void us_ax_get_lt_info(us_ax_capture_s *ax_cap)
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
		ax_cap->dst_width = width;
		ax_cap->dst_height = height;
	}
	else {
		ax_cap->dst_width = 1920;
		ax_cap->dst_height = 1080;
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
	ax_cap->src_fps = fps;
	AXV_LOGI("Using %dx%d %d fps", ax_cap->dst_width, ax_cap->dst_height, ax_cap->src_fps);
}

us_ax_capture_s *us_ax_capture_init(int width, int height, uint32_t fps)
{
	bool res;
	int i;

	us_ax_capture_s *ax_cap = (us_ax_capture_s *)malloc(sizeof(us_ax_capture_s));
	if (ax_cap == NULL) return NULL;

	AXV_LOGI("Open capture...");

	memset(ax_cap, 0, sizeof(us_ax_capture_s));

	ax_cap->dst_width  = width;
	ax_cap->dst_height = height;
	ax_cap->src_fps    = fps;

	i = 0;
	while (!file_exists("/proc/lt6911_info/status") && i < 5) {
		sleep(1);
		i += 1;
	}

	i = 0;
	while (!socket_exists("/run/kvm/vin_sock") && i < 5) {
		sleep(1);
		i += 1;
	}

	us_ax_get_lt_info(ax_cap);

	if (socket_exists("/run/kvm/vin_sock")) {
		AXV_LOGI("Capture opened by kvm_vin");
		return ax_cap;
	}

	res = AX_CAP_Init(ax_cap);
	if (!res) {
		goto ErrorHandle;
	}

	ax_cap->cap_run = 1;

	AXV_LOGI("Capture open success");
	return ax_cap;

ErrorHandle:
	AXV_LOGE("Capture open meet error, now handle it");
	free(ax_cap);
	return NULL;
}

int us_ax_capture_destroy(us_ax_capture_s *ax_cap)
{
	if (ax_cap == NULL) return -1;

	if (ax_cap->cap_run) {
		AX_CAP_Deinit(ax_cap);
	}

	free(ax_cap);

	AXV_LOGI("Capture closed");

	return 0;
}

int us_ax_get_yuv_frame(us_ax_capture_s *ax_cap, us_frame_s *frame)
{
  AX_S32 timeout = 1000;
  AX_U32 size;
  AX_VOID *pviraddr;
  AX_S32 s32Ret;
  AX_IMG_INFO_T capture_img_info;

  if (ax_cap == NULL) return -1;
  memset(&capture_img_info, 0, sizeof(capture_img_info));
  s32Ret = AX_VIN_GetYuvFrame((AX_U8)ax_cap->vin_chn,AX_VIN_CHN_ID_MAIN,&capture_img_info,timeout);
  if (s32Ret != 0) {
    AXV_LOGE("AX_VIN_GetYuvFrame failed, ret=0x%x", s32Ret);
    return -1;
  }
  AXV_LOGD(
               "Got YUYV422 frame: SeqNum=%lld, FrameSize=%d, Width=%d, Height=%d, Stride=%d, Format=0x%.2x",
               capture_img_info.tFrameInfo.stVFrame.u64SeqNum,
               capture_img_info.tFrameInfo.stVFrame.u32FrameSize,
               capture_img_info.tFrameInfo.stVFrame.u32Width,
               capture_img_info.tFrameInfo.stVFrame.u32Height,
               capture_img_info.tFrameInfo.stVFrame.u32PicStride[0],
	       capture_img_info.tFrameInfo.stVFrame.enImgFormat);
  {
    size = capture_img_info.tFrameInfo.stVFrame.u32PicStride[0] *
           capture_img_info.tFrameInfo.stVFrame.u32Height * 2;
    pviraddr = AX_SYS_Mmap(capture_img_info.tFrameInfo.stVFrame.u64PhyAddr[0],size);
    if (pviraddr != NULL) {
      AXV_LOGD(
                   "AX_SYS_Mmap success: PhyAddr=0x%.16llx, VirtAddr=0x%.8llx, size=%d",
		   (AX_U64)capture_img_info.tFrameInfo.stVFrame.u64PhyAddr[0],
		   (AX_U64)pviraddr, size);
      us_frame_set_data(frame, pviraddr, size);
      s32Ret = AX_SYS_Munmap(pviraddr,size);
      if (s32Ret != 0) {
        AXV_LOGE("AX_SYS_Munmap failed for addr=0x%.16llx, ret=0x%x",
                     capture_img_info.tFrameInfo.stVFrame.u64PhyAddr[0], s32Ret);
      }
      AX_VIN_ReleaseYuvFrame((AX_U8)ax_cap->vin_chn,AX_VIN_CHN_ID_MAIN,&capture_img_info);
      return 0;
    }
    AXV_LOGE("AX_SYS_Mmap failed for YUYV422 frame, addr=0x%.16llx, size=%d",
                 capture_img_info.tFrameInfo.stVFrame.u64PhyAddr[0], size);
  }
  AX_VIN_ReleaseYuvFrame((AX_U8)ax_cap->vin_chn,AX_VIN_CHN_ID_MAIN,&capture_img_info);
  return -1;
}
