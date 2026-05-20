#include <sys/mman.h>
#include <linux/fb.h>

#include "ax_middleware.hpp"
#include "ax_middleware_priv.h"

#ifndef ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))
#endif

namespace maix::middleware::maixcam2
{

// __sample_fb_config(unsigned int, axSAMPLE_FB_CONFIG_S*)

err::Err __sample_fb_config(SAMPLE_FB_CONFIG_S *pstFbConfig)

{
  int __fd;
  int iRet;
  int *piError;
  char *pcError;
  void *__s;
  __u32 red_length;
  __u32 transp_length;
  __u32 transp_offset;
  __u32 red_offset;
  AX_FB_COLORKEY_T stColorKey;
  char caFbDevPath [32];
  fb_fix_screeninfo stFbFixInfo;
  fb_var_screeninfo stFbVarInfo;
  AX_U32 fbFmt;
  AX_U32 fbHeight;
  AX_U32 fbWidth;
  uint32_t fbindex;

  stColorKey.nKeyLow = pstFbConfig->u32ColorKey;
  fbHeight = pstFbConfig->u32ResoH;
  fbFmt = pstFbConfig->u32Fmt;
  fbindex = pstFbConfig->u32Index;
  fbWidth = pstFbConfig->u32ResoW;
  stColorKey.bEnable = (AX_BOOL)pstFbConfig->u32ColorKeyEn;
  stColorKey.bInv = (AX_BOOL)pstFbConfig->u32ColorKeyInv;
  if (fbFmt == AX_FORMAT_ARGB1555) {
    transp_length = 1;
    transp_offset = 15;
    red_length = 5;
    red_offset = 10;
  }
  else {
    transp_length = 8;
    transp_offset = 24;
    red_length = 8;
    red_offset = 16;
  }
  stColorKey.nKeyHigh = stColorKey.nKeyLow;
  snprintf(caFbDevPath,sizeof(caFbDevPath),"/dev/fb%d",fbindex);
  __fd = open(caFbDevPath,O_RDWR);
  if (__fd < 0) {
    piError = __errno_location();
    pcError = strerror(*piError);
    maix::log::error("open %s failed, err:%s\n",caFbDevPath,pcError);
    return err::ERR_RUNTIME;
  }
  iRet = ioctl(__fd,FBIOGET_VSCREENINFO,&stFbVarInfo);
  if (iRet < 0) {
    pcError = "get variable screen info from fb%d failed\n";
  }
  else {
    stFbVarInfo.bits_per_pixel = 16;
    if (fbFmt != AX_FORMAT_ARGB1555) {
      stFbVarInfo.bits_per_pixel = 32;
    }
    stFbVarInfo.yres_virtual = fbHeight << 1;
    stFbVarInfo.blue.msb_right = 0;
    stFbVarInfo.green.msb_right = 0;
    stFbVarInfo.transp.msb_right = 0;
    stFbVarInfo.red.msb_right = 0;
    stFbVarInfo.green.offset = 0;
    stFbVarInfo.xres = fbWidth;
    stFbVarInfo.yres = fbHeight;
    stFbVarInfo.xres_virtual = fbWidth;
    stFbVarInfo.red.offset = red_length;
    stFbVarInfo.red.length = red_length;
    stFbVarInfo.green.length = red_length;
    stFbVarInfo.blue.offset = transp_offset;
    stFbVarInfo.blue.length = transp_length;
    stFbVarInfo.transp.offset = red_offset;
    stFbVarInfo.transp.length = red_length;
    iRet = ioctl(__fd,FBIOPUT_VSCREENINFO,&stFbVarInfo);
    if (iRet < 0) {
      pcError = "put variable screen info to fb%d failed\n";
    }
    else {
      iRet = ioctl(__fd,FBIOGET_FSCREENINFO,&stFbFixInfo);
      if (iRet < 0) {
        pcError = "get fix screen info from fb%d failed\n";
      }
      else {
        __s = mmap(NULL, stFbFixInfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED ,__fd,0);
        if (__s == (void *)-1) {
          pcError = "map fb%d failed\n";
        }
        else {
          memset(__s,0,stFbFixInfo.smem_len);
          munmap(__s,stFbFixInfo.smem_len);
          iRet = ioctl(__fd,AX_FBIOPUT_COLORKEY,&stColorKey);
          if (-1 < iRet) {
            maix::log::info("init fb%d done\n",fbindex);
            goto done;
          }
          pcError = "set fb%d colorkey failed!\n";
        }
      }
    }
  }
  maix::log::error(pcError,fbindex);
done:
  close(__fd);
  return err::ERR_NONE;
}



// SAMPLE_CALC_IMAGE_SIZE(unsigned int, unsigned int, AX_IMG_FORMAT_E,
// unsigned int)

AX_U32 SAMPLE_CALC_IMAGE_SIZE
                 (AX_U32 u32Width,AX_U32 u32Height,uint32_t eImgType,AX_U32 u32Stride)

{
  AX_U32 u32Bpp = 0;
  uint64_t uTmp;

  if (u32Width == 0 || u32Height == 0) {
    printf("\x1b[1;30;31mERROR  :[%s:%d] Invalid width %d or height %d!\x1b[0m\n",
           "SAMPLE_CALC_IMAGE_SIZE",__LINE__,u32Width,u32Height);
    return 0;
  }
  if (u32Stride == 0) {
    u32Stride = u32Width;
  }
  if (eImgType < AX_FORMAT_YUV422_INTERLEAVED_VYUY) {
    switch(eImgType) {
    case AX_FORMAT_YUV400:
      u32Bpp = 8;
      break;
    case AX_FORMAT_YUV420_PLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR:
    case AX_FORMAT_YUV420_SEMIPLANAR_VU:
      u32Bpp = 12;
      break;
    default:
      goto default_bpp;
    case AX_FORMAT_YUV422_INTERLEAVED_YUYV:
    case AX_FORMAT_YUV422_INTERLEAVED_UYVY:
      u32Bpp = 16;
    }
  }
  else {
    if (eImgType == AX_FORMAT_YUV444_PACKED) {
      u32Bpp = 24;
      goto done;
    }
    if (eImgType - AX_FORMAT_RGB888 < 0x31) {
      uTmp = 1L << ((ulong)(eImgType - AX_FORMAT_RGB888) & 0x3f);
      if ((uTmp & 0x1414000000000) == 0) {
        u32Bpp = 0;
        if ((uTmp & 0x11) != 0) {
          u32Bpp = 24;
        }
      }
      else {
        u32Bpp = 32;
      }
      goto done;
    }
default_bpp:
    u32Bpp = 0;
  }
done:
  return u32Stride * u32Height * u32Bpp >> 3;
}



// __ax_ivps_csc_tdp(axVIDEO_FRAME_T const*, axVIDEO_FRAME_T*,
// AX_IMG_FORMAT_E)

AX_S32 __ax_ivps_csc_tdp
                 (AX_VIDEO_FRAME_T *pstSrcFrame,AX_VIDEO_FRAME_T *pDstFrame,
                 AX_IMG_FORMAT_E eImgFormat)

{
  AX_U32 size;
  AX_S32 s32Ret;
  AX_U64 uPhyAddr;
  AX_VOID *pVirAddr;
  AX_VIDEO_FRAME_T stCscTdpFrame;

  uPhyAddr = 0;
  pVirAddr = (AX_VOID *)0x0;
  memcpy(&stCscTdpFrame,pDstFrame,sizeof(stCscTdpFrame));
  size = SAMPLE_CALC_IMAGE_SIZE
                   (pstSrcFrame->u32Width,pstSrcFrame->u32Height,eImgFormat,
                    pstSrcFrame->u32PicStride[0]);
  s32Ret = AX_SYS_MemAllocCached(&uPhyAddr,&pVirAddr,size,0x1000,(AX_S8 *)"tdp csc");
  if (s32Ret == 0) {
    AX_SYS_MflushCache(pstSrcFrame->u64PhyAddr[0],(AX_VOID *)pstSrcFrame->u64VirAddr[0],
                       pstSrcFrame->u32FrameSize);
    stCscTdpFrame.u32Width = pstSrcFrame->u32Width;
    stCscTdpFrame.u32Height = pstSrcFrame->u32Height;
    stCscTdpFrame.u32PicStride[0] = pstSrcFrame->u32PicStride[0];
    stCscTdpFrame.u64PhyAddr[0] = uPhyAddr;
    stCscTdpFrame.u64VirAddr[0] = (AX_U64)pVirAddr;
    stCscTdpFrame.enImgFormat = eImgFormat;
    stCscTdpFrame.u32FrameSize = size;
    s32Ret = AX_IVPS_CscTdp(pstSrcFrame,&stCscTdpFrame);
    if (s32Ret == 0) {
      AX_SYS_MinvalidateCache
                (stCscTdpFrame.u64PhyAddr[0],(AX_VOID *)stCscTdpFrame.u64VirAddr[0],
                 stCscTdpFrame.u32FrameSize);
      memcpy(pDstFrame,&stCscTdpFrame,sizeof(*pDstFrame));
    }
    else {
      AX_SYS_MemFree(uPhyAddr,pVirAddr);
    }
  }
  return s32Ret;
}



#ifdef __cplusplus
extern "C" {
#endif

static bool LT6911_HDMI_Enable()
{
  uint32_t uAddr;
  bool bRet;
  int __fd;
  int *pError;
  void *__addr;
  uint32_t *pData;
  long lCntr;
  uint64_t __offset;
  char *sError;
  uint32_t pin_data[22] = {
    0x0230000C, 0x00020043,
    0x02300018, 0x00040003,
    0x02300024, 0x00000003,
    0x02300030, 0x00040003,
    0x0230003C, 0x00040003,
    0x02300048, 0x00060003,
    0x02300054, 0x00060003,
    0x02300060, 0x00060003,
    0x0230006C, 0x00030083,
    0x02300078, 0x00030083,
    0x02300084, 0x00040003,
  };

  __fd = open("/dev/mem", O_SYNC | O_RDWR);
  if (__fd < 0) {
    pError = __errno_location();
    sError = strerror(*pError);
    printf("Failed to open /dev/mem, error: %s\n", sError);
    bRet = false;
  }
  else {
    pData = &pin_data[0];
    lCntr = 0xc;
    while (lCntr = lCntr - 1, lCntr != 0) {
      uAddr = *pData;
      __offset = (ulong)uAddr & 0xfffff000;
      __addr = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED ,__fd,__offset);
      if (__addr == (void *)-1) {
        pError = __errno_location();
        sError = strerror(*pError);
        printf("mmap failed for address 0x%x, error: %s\n", *pData, sError);
      }
      else {
        *(uint32_t *)((long)__addr + (uAddr - __offset)) = pData[1];
        munmap(__addr,0x1000);
      }
      pData = pData + 2;
    }
    close(__fd);
    bRet = true;
  }
  return bRet;
}

#ifdef __cplusplus
}
#endif



// Frame::Frame(int, int, axVIDEO_FRAME_T*,
// frame_from_e, AX_IMG_FORMAT_E)

Frame::Frame
          (IVPS_GRP IvpsGrp,IVPS_CHN IvpsChn,AX_VIDEO_FRAME_T *ptFrame,frame_from_e from,
          AX_IMG_FORMAT_E invert_fmt)

{
  AX_VIDEO_FRAME_T *ptSrc;
  AX_U32 u32Height;
  AX_U32 u32Stride;
  AX_U32 u32FrameSize;
  AX_S32 s32Ret;
  frame_video_param_t *__s;
  AX_VOID *pVirAddr;
  AX_U64 uTdpPhyAddr;
  AX_VOID *pTdpVirAddr;
  AX_VIDEO_FRAME_T stVideoFrame;
  AX_IMG_FORMAT_E srcImgFormat;
  uint32_t srcIvpsChn;
  uint32_t srcIvpsGrp;

  __s = (frame_video_param_t*)malloc(sizeof(*__s));
  if (__s == (frame_video_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->ivps_grp = IvpsGrp;
  __s->ivps_chn = IvpsChn;
  ptSrc = &__s->stFrame;
  memcpy(ptSrc,ptFrame,sizeof(*ptSrc));
  __s->from = from;
  if (from == FRAME_FROM_IVPS_CHN) {
    srcImgFormat = (__s->stFrame).enImgFormat;
    if (srcImgFormat < (AX_FORMAT_YUV420_SEMIPLANAR_VU|AX_FORMAT_YUV420_PLANAR)) {
      if (srcImgFormat < AX_FORMAT_YUV420_SEMIPLANAR) {
        if (srcImgFormat != AX_FORMAT_YUV400) goto not_impl;
        goto do_map;
      }
      pVirAddr = AX_SYS_MmapCache((__s->stFrame).u64PhyAddr[0],(__s->stFrame).u32FrameSize);
      u32Height = (__s->stFrame).u32Height;
      u32Stride = (__s->stFrame).u32PicStride[0];
      (__s->stFrame).u64VirAddr[0] = (AX_U64)pVirAddr;
      (__s->stFrame).u64VirAddr[1] = (ulong)(u32Stride * u32Height) + (long)pVirAddr;
      if (invert_fmt != AX_FORMAT_INVALID) {
        uTdpPhyAddr = 0;
        pTdpVirAddr = (AX_VOID *)0x0;
        memset(&stVideoFrame,0,sizeof(stVideoFrame));
        u32FrameSize = SAMPLE_CALC_IMAGE_SIZE((__s->stFrame).u32Width,u32Height,invert_fmt,u32Stride);
        s32Ret = AX_SYS_MemAllocCached(&uTdpPhyAddr,&pTdpVirAddr,u32FrameSize,0x1000,(AX_S8 *)"tdp used");
        if (s32Ret != 0) {
          maix::err::check_raise(err::ERR_RUNTIME,"AX_SYS_MemAllocCached failed");
        }
        AX_SYS_MflushCache((__s->stFrame).u64PhyAddr[0],(AX_VOID *)(__s->stFrame).u64VirAddr[0],
                           (__s->stFrame).u32FrameSize);
        stVideoFrame.u32Width = (__s->stFrame).u32Width;
        stVideoFrame.u32Height = (__s->stFrame).u32Height;
        stVideoFrame.u32PicStride[0] = (__s->stFrame).u32PicStride[0];
        stVideoFrame.u64PhyAddr[0] = uTdpPhyAddr;
        stVideoFrame.u64VirAddr[0] = (AX_U64)pTdpVirAddr;
        stVideoFrame.enImgFormat = invert_fmt;
        stVideoFrame.u32FrameSize = u32FrameSize;
        AX_IVPS_CscTdp(ptSrc,&stVideoFrame);
        AX_SYS_MinvalidateCache(uTdpPhyAddr,pTdpVirAddr,stVideoFrame.u32FrameSize);
        AX_SYS_Munmap((AX_VOID *)(__s->stFrame).u64VirAddr[0],(__s->stFrame).u32FrameSize);
        srcIvpsGrp = __s->ivps_grp;
        srcIvpsChn = __s->ivps_chn;
        (__s->stFrame).u64VirAddr[0] = 0;
        (__s->stFrame).u64VirAddr[1] = 0;
        AX_IVPS_ReleaseChnFrame(srcIvpsGrp,srcIvpsChn,ptSrc);
        memcpy(ptSrc,&stVideoFrame,sizeof(*ptSrc));
        __s->from = FRAME_FROM_SYS_MEM_ALLOC;
        pVirAddr = (void *)(__s->stFrame).u64VirAddr[0];
      }
    }
    else {
      if ((srcImgFormat & ~AX_FORMAT_YUV420_SEMIPLANAR_VU) != AX_FORMAT_RGB888) {
not_impl:
        maix::err::check_raise(err::ERR_NOT_IMPL,"frame format not implemented");
        goto done;
      }
do_map:
      pVirAddr = AX_SYS_MmapCache((__s->stFrame).u64PhyAddr[0],(__s->stFrame).u32FrameSize);
      (__s->stFrame).u64VirAddr[0] = (AX_U64)pVirAddr;
    }
set_data:
    this->data = pVirAddr;
  }
  else {
    if (from == FRAME_FROM_SYS_MEM_ALLOC) {
      srcImgFormat = (__s->stFrame).enImgFormat;
      if (srcImgFormat < (AX_FORMAT_YUV420_SEMIPLANAR_VU|AX_FORMAT_YUV420_PLANAR)) {
        if (AX_FORMAT_YUV420_PLANAR_VU < srcImgFormat) {
          pVirAddr = AX_SYS_MmapCache((__s->stFrame).u64PhyAddr[0],(__s->stFrame).u32FrameSize);
          (__s->stFrame).u64VirAddr[0] = (AX_U64)pVirAddr;
          (__s->stFrame).u64VirAddr[1] =
               (ulong)((__s->stFrame).u32PicStride[0] * (__s->stFrame).u32Height) + (long)pVirAddr;
          goto set_data;
        }
        if (srcImgFormat == AX_FORMAT_YUV400) goto do_map;
      }
      else if (((uint)(srcImgFormat + ~AX_FORMAT_RGB565) < 0x2f) &&
              ((0x404000000011U >> ((ulong)(uint)(srcImgFormat + ~AX_FORMAT_RGB565) & 0x3f) & 1) !=
               0)) goto do_map;
      maix::err::check_raise(err::ERR_NOT_IMPL,"frame format not implemented");
    }
    else {
      maix::log::error("[%s][%d] frame from %d not implemented","Frame",__LINE__,from);
      maix::err::check_raise(err::ERR_NOT_IMPL,"frame from not implemented");
    }
  }
done:
  this->w = (__s->stFrame).u32Width;
  this->h = (__s->stFrame).u32Height;
  this->fmt = (__s->stFrame).enImgFormat;
  this->__param = (frame_param_t *)__s;
  this->len = (__s->stFrame).u32FrameSize;
  return;
}



// Frame::Frame(int, axVENC_STREAM_T*,
// frame_from_e)

Frame::Frame(int venc_ch,axVENC_STREAM_T *frame,frame_from_e from)

{
  frame_param_t *__s;
  AX_U8 *pData;

  __s = (frame_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  memcpy(__s->data,frame,sizeof(*frame));
  pData = (frame->stPack).pu8Addr;
  __s->venc_chn = (uint16_t)venc_ch;
  __s->from = from;
  this->data = pData;
  this->__param = __s;
  this->len = (frame->stPack).u32Len;
  return;
}



// Frame::Frame(int, axVIDEO_FRAME_INFO_T*,
// frame_from_e)

Frame::Frame
          (int vdec_ch,AX_VIDEO_FRAME_INFO_T *frame,frame_from_e from)

{
  AX_U32 u32FrameSize;
  frame_param_t *__s;
  AX_VOID *pVirAddr;
  AX_U64 uPhyAddr;
  AX_IMG_FORMAT_E srcImgFormat;

  __s = (frame_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  srcImgFormat = (frame->stVFrame).enImgFormat;
  u32FrameSize = SAMPLE_CALC_IMAGE_SIZE
                     ((frame->stVFrame).u32Width,(frame->stVFrame).u32Height,srcImgFormat,
                      (frame->stVFrame).u32PicStride[0]);
  if ((uint)((frame->stVFrame).enImgFormat + ~AX_FORMAT_YUV420_PLANAR_VU) < 2) {
    pVirAddr = AX_SYS_MmapCache((frame->stVFrame).u64PhyAddr[0],u32FrameSize);
    (frame->stVFrame).u32FrameSize = u32FrameSize;
    (frame->stVFrame).u64VirAddr[0] = (AX_U64)pVirAddr;
    (frame->stVFrame).u64VirAddr[1] =
         (ulong)((frame->stVFrame).u32PicStride[0] * (frame->stVFrame).u32Height) + (long)pVirAddr;
  }
  else {
    maix::err::check_raise(err::ERR_NOT_IMPL,"frame format not implemented");
  }
  memset(__s,0,sizeof(*__s));
  memcpy(__s->data,frame,sizeof(*frame));
  uPhyAddr = (frame->stVFrame).u64VirAddr[0];
  __s->vdec_chn = (uint16_t)vdec_ch;
  __s->from = from;
  this->data = (void*)uPhyAddr;
  this->len = (frame->stVFrame).u32FrameSize;
  this->w = (frame->stVFrame).u32Width;
  this->h = (frame->stVFrame).u32Height;
  srcImgFormat = (frame->stVFrame).enImgFormat;
  this->__param = __s;
  this->fmt = srcImgFormat;
  return;
}



// Frame::Frame(int, int, void*, int, AX_IMG_FORMAT_E)

Frame::Frame(int w,int h,void *data,int data_size,AX_IMG_FORMAT_E fmt)

{
  AX_S32 s32Ret;
  AX_U32 u32FrameSize;
  frame_video_param_t *__s;
  AX_U64 uPhyAddr;
  AX_VOID *pVirAddr;
  AX_VIDEO_FRAME_T stVideoFrame;

  __s = (frame_video_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_video_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->from = FRAME_FROM_SYS_MEM_ALLOC;
  uPhyAddr = 0;
  pVirAddr = (AX_VOID *)0x0;
  memset(&stVideoFrame,0,sizeof(stVideoFrame));
  u32FrameSize = SAMPLE_CALC_IMAGE_SIZE(w,h,fmt,w);
  s32Ret = AX_SYS_MemAllocCached(&uPhyAddr,&pVirAddr,u32FrameSize,0x1000,(const AX_S8 *)"ax alloc frame");
  if (s32Ret == 0) {
    if (fmt - 3 < 2) {
      stVideoFrame.u64PhyAddr[1] = uPhyAddr + (uint)(w * h);
      stVideoFrame.u64VirAddr[1] = (long)pVirAddr + (ulong)(uint)(w * h);
      stVideoFrame.u32PicStride[1] = w;
    }
    else {
      stVideoFrame.u64VirAddr[1] = 0;
      stVideoFrame.u64PhyAddr[1] = 0;
      stVideoFrame.u32PicStride[1] = 0;
    }
    stVideoFrame.u64PhyAddr[0] = uPhyAddr;
    stVideoFrame.u64VirAddr[0] = (AX_U64)pVirAddr;
    stVideoFrame.u32Width = w;
    stVideoFrame.u32Height = h;
    stVideoFrame.enImgFormat = fmt;
    stVideoFrame.u32PicStride[0] = w;
    stVideoFrame.u32FrameSize = u32FrameSize;
    memcpy(&__s->stFrame,&stVideoFrame,sizeof(__s->stFrame));
  }
  else if ((s32Ret & 0xffff) != 0) {
    maix::err::check_raise(err::ERR_RUNTIME,"ax malloc frame failed!");
  }
  if ((__s->stFrame).u32FrameSize != (uint32_t)data_size) {
    maix::log::error("input size not correctly, input size:%d, need size:%d",(__s->stFrame).u32FrameSize,data_size);
    maix::err::check_raise(err::ERR_RUNTIME,"input size not correctly");
  }
  memcpy((void *)(__s->stFrame).u64VirAddr[0],data,(long)(int)data_size);
  AX_SYS_MflushCache((__s->stFrame).u64PhyAddr[0],(AX_VOID *)(__s->stFrame).u64VirAddr[0],
                     (__s->stFrame).u32FrameSize);
  this->data = (void *)(__s->stFrame).u64VirAddr[0];
  this->len = (__s->stFrame).u32FrameSize;
  this->w = (__s->stFrame).u32Width;
  this->h = (__s->stFrame).u32Height;
  this->__param = (frame_param_t *)__s;
  this->fmt = (__s->stFrame).enImgFormat;
  return;
}



// Frame::Frame(int, int, int, void*, int, AX_IMG_FORMAT_E)

Frame::Frame
          (int pool_id,int w,int h,void *data,int data_size,AX_IMG_FORMAT_E fmt)

{
  AX_U32 u32FrameSize;
  AX_BLK BlockId;
  frame_video_param_t *__s;
  AX_U64 uPhyAddr;
  AX_VOID *pVirAddr;

  __s = (frame_video_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_video_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->from = FRAME_FROM_GET_BLOCK;
  u32FrameSize = SAMPLE_CALC_IMAGE_SIZE(w,h,fmt,w);
  BlockId = AX_POOL_GetBlock(pool_id,u32FrameSize,(AX_S8 *)0x0);
  if (BlockId == 0) {
    maix::err::check_raise(err::ERR_RUNTIME,"Frame AX_POOL_GetBlock failed!");
  }
  (__s->stFrame).u32FrameSize = u32FrameSize;
  uPhyAddr = AX_POOL_Handle2PhysAddr(BlockId);
  (__s->stFrame).u64PhyAddr[0] = uPhyAddr;
  pVirAddr = AX_POOL_GetBlockVirAddr(BlockId);
  (__s->stFrame).u32Width = w;
  (__s->stFrame).u32Height = h;
  (__s->stFrame).enImgFormat = fmt;
  (__s->stFrame).u32PicStride[0] = w;
  (__s->stFrame).u64VirAddr[0] = (AX_U64)pVirAddr;
  (__s->stFrame).u32BlkId[0] = BlockId;
  (__s->stFrame).u32BlkId[1] = 0;
  (__s->stFrame).u32BlkId[2] = 0;
  if (fmt - 3 < 2) {
    (__s->stFrame).u32PicStride[1] = w;
  }
  memcpy(pVirAddr,data,(long)data_size);
  this->data = (void *)(__s->stFrame).u64VirAddr[0];
  this->len = (__s->stFrame).u32FrameSize;
  this->w = (__s->stFrame).u32Width;
  this->h = (__s->stFrame).u32Height;
  this->__param = (frame_param_t *)__s;
  this->fmt = (__s->stFrame).enImgFormat;
  return;
}



// Frame::Frame(void*, int, frame_from_e)

Frame::Frame(void *data,int data_size,frame_from_e from)

{
  AX_S32 s32Ret;
  frame_param_t *__s;
  void *pData;
  AX_U64 uPhyAddr;
  AX_VOID *pVirAddr;

  __s = (frame_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->from = from;
  if (from == FRAME_FROM_MALLOC) {
    *(uint16_t *)&__s->par1 = 0x101;
    pData = malloc((long)data_size);
    this->data = pData;
    if (pData == (void *)0x0) {
      maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
    }
    memcpy(this->data,data,(long)data_size);
    this->len = data_size;
  }
  else if (from == FRAME_FROM_AX_MALLOC) {
    uPhyAddr = 0;
    pVirAddr = (void *)0x0;
    s32Ret = AX_SYS_MemAllocCached(&uPhyAddr,&pVirAddr,data_size,0x1000,(const AX_S8 *)"ax alloc frame");
    if (s32Ret != 0) {
      maix::err::check_raise(err::ERR_RUNTIME,"ax sys malloc failed");
    }
    this->data = pVirAddr;
    this->phy_addr = uPhyAddr;
    this->len = data_size;
    memcpy(pVirAddr,data,(long)data_size);
    AX_SYS_MflushCache(uPhyAddr,pVirAddr,data_size);
  }
  else {
    maix::log::error("[%s][%d] frame from %d not implemented","Frame",__LINE__,from);
    maix::err::check_raise(err::ERR_NOT_IMPL,"frame from not implemented");
  }
  this->__param = __s;
  return;
}



// Frame::Frame(int, int, axAUDIO_FRAME_T*,
// frame_from_e)

Frame::Frame
          (int card,int device,AX_AUDIO_FRAME_T *frame,frame_from_e from)

{
  frame_audio_param_t *__s;

  maix::err::check_bool_raise(from == FRAME_FROM_AUDIO_GET_FRAME,"Create this frame is only support from FRAME_FROM_AUDIO_GET_FRAME");
  __s = (frame_audio_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_audio_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->card = card;
  __s->device = device;
  __s->from = from;
  __s->vir_addr = frame->u64VirAddr;
  __s->bit_width = frame->enBitwidth;
  __s->sound_mode = frame->enSoundmode;
  __s->timestamp = frame->u64TimeStamp;
  __s->phy_addr = (void*)frame->u64PhyAddr;
  *(uint64_t *)__s->pool_id = *(uint64_t *)frame->u32PoolId;
  __s->seq = frame->u32Seq;
  __s->len = frame->u32Len;
  __s->eof = frame->bEof;
  __s->blk_id = frame->u32BlkId;
  this->data = frame->u64VirAddr;
  this->__param = (frame_param_t *)__s;
  this->len = frame->u32Len;
  return;
}



// Frame::Frame(int, int, void*, int, axAUDIO_BIT_WIDTH_E,
// axAUDIO_SOUND_MODE_E, frame_from_e)

Frame::Frame
          (int card,int device,void *data,int data_size,axAUDIO_BIT_WIDTH_E bit_width,
          axAUDIO_SOUND_MODE_E sound_mode, frame_from_e from)

{
  AX_BLK BlockId;
  frame_audio_param_t *__s;
  AX_VOID *pVirAddr;

  maix::err::check_bool_raise(from == FRAME_FROM_AUDIO_FRAME,"Create this frame is only support from FRAME_FROM_AUDIO_FRAME");
  __s = (frame_audio_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_audio_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  __s->card = card;
  __s->device = device;
  __s->from = from;
  BlockId = AX_POOL_GetBlock(0xffffffff,(long)data_size,(AX_S8 *)0x0);
  pVirAddr = AX_POOL_GetBlockVirAddr(BlockId);
  this->data = pVirAddr;
  this->len = data_size;
  memcpy(pVirAddr,data,(long)data_size);
  __s->bit_width = bit_width;
  __s->sound_mode = sound_mode;
  __s->vir_addr = pVirAddr;
  __s->len = data_size;
  __s->blk_id = BlockId;
  this->__param = (frame_param_t *)__s;
  return;
}



// Frame::Frame(unsigned char, _AX_VIN_PIPE_DUMP_NODE_E_,
// AX_SNS_HDR_FRAME_E, _AX_IMG_INFO_T_*, frame_from_e)

Frame::Frame
          (AX_U8 nPipeId,AX_VIN_PIPE_DUMP_NODE_E eRawId,AX_SNS_HDR_FRAME_E eSnsFrame,AX_IMG_INFO_T *pImgInfo,frame_from_e from)

{
  frame_param_t *__s;
  AX_VOID *pVirAddr;

  __s = (frame_param_t *)malloc(sizeof(*__s));
  if (__s == (frame_param_t *)0x0) {
    maix::err::check_raise(err::ERR_RUNTIME,"malloc failed");
  }
  memset(__s,0,sizeof(*__s));
  memcpy(__s->data,pImgInfo,sizeof(*pImgInfo));
  *(AX_U8 *)&__s->pipe_id = nPipeId;
  pVirAddr = (AX_VOID *)(pImgInfo->tFrameInfo).stVFrame.u64VirAddr[0];
  __s->raw_id = eRawId;
  __s->sns_frame = eSnsFrame;
  __s->from = from;
  this->data = pVirAddr;
  this->len = (pImgInfo->tFrameInfo).stVFrame.u32FrameSize;
  this->w = (pImgInfo->tFrameInfo).stVFrame.u32Width;
  this->h = (pImgInfo->tFrameInfo).stVFrame.u32Height;
  this->__param = __s;
  this->fmt = (pImgInfo->tFrameInfo).stVFrame.enImgFormat;
  return;
}



// Frame::~Frame()

Frame::~Frame()

{
  AX_BLK BlockId;
  frame_param_t *this_param;

  this_param = (frame_param_t *)this->__param;
  if (this_param == (frame_param_t *)0x0) {
    return;
  }
  switch(this_param->from) {
  case FRAME_FROM_IVPS_CHN:
    AX_SYS_Munmap(*(AX_VOID **)(this_param->data + 0x50),*(AX_U32 *)(this_param->data + 0xe4));
    this_param->data[0x50] = '\0';
    this_param->data[0x51] = '\0';
    this_param->data[0x52] = '\0';
    this_param->data[0x53] = '\0';
    this_param->data[0x54] = '\0';
    this_param->data[0x55] = '\0';
    this_param->data[0x56] = '\0';
    this_param->data[0x57] = '\0';
    this_param->data[0x58] = '\0';
    this_param->data[0x59] = '\0';
    this_param->data[0x5a] = '\0';
    this_param->data[0x5b] = '\0';
    this_param->data[0x5c] = '\0';
    this_param->data[0x5d] = '\0';
    this_param->data[0x5e] = '\0';
    this_param->data[0x5f] = '\0';
    AX_IVPS_ReleaseChnFrame(this_param->ivps_grp,this_param->ivps_chn,(AX_VIDEO_FRAME_T *)this_param->data);
  case FRAME_FROM_MALLOC:
malloc_type:
    if (((*(char *)((long)&this_param->par1 + 1) != 0) && ((char)this_param->par1 != 0)) &&
       (this->data != (void *)0x0)) {
      free(this->data);
    }
    break;
  case FRAME_FROM_SYS_MEM_ALLOC:
    AX_SYS_MemFree(*(AX_U64 *)(this_param->data + 0x38),*(AX_VOID **)(this_param->data + 0x50));
    goto malloc_type;
  case FRAME_FROM_VENC_GET_STREAM:
    AX_VENC_ReleaseStream((int)(short)this_param->venc_chn,(AX_VENC_STREAM_T *)this_param->data);
    break;
  case FRAME_FROM_GET_BLOCK:
    BlockId = *(AX_BLK *)(this_param->data + 0xa4);
    goto release_block;
  case FRAME_FROM_VDEC_GET_STREAM:
    AX_SYS_Munmap(*(AX_VOID **)(this_param->data + 0x50),*(AX_U32 *)(this_param->data + 0xe4));
    AX_VDEC_ReleaseFrame((int)(short)this_param->vdec_chn,(AX_VIDEO_FRAME_INFO_T *)this_param->data);
    break;
  case FRAME_FROM_AX_MALLOC:
    AX_SYS_MemFree(this->phy_addr,this->data);
    break;
  case FRAME_FROM_AUDIO_GET_FRAME:
    AX_AI_ReleaseFrame(this_param->card,this_param->device,(AX_AUDIO_FRAME_T *)this_param->data);
    break;
  case FRAME_FROM_AUDIO_FRAME:
    BlockId = *(AX_BLK *)(this_param->data + 0x34);
release_block:
    AX_POOL_ReleaseBlock(BlockId);
    break;
  case FRAME_FROM_GET_RAW_FRAME:
    AX_VIN_ReleaseRawFrame
              ((AX_U8)this_param->pipe_id,(AX_VIN_PIPE_DUMP_NODE_E)this_param->raw_id,(AX_SNS_HDR_FRAME_E)this_param->sns_frame,(AX_IMG_INFO_T *)this_param->data);
    break;
  default:
    maix::log::error("[%s][%d] frame from %d not implemented","~Frame",__LINE__,this_param->from);
    maix::err::check_raise(err::ERR_NOT_IMPL,"frame from not implemented");
  }
  free(this_param);
  return;
}



// Frame::from()

frame_from_e Frame::from()

{
  frame_param_t *this_param = (frame_param_t *)this->__param;
  return (frame_from_e)this_param->from;
}



// Frame::get_video_frame(axVIDEO_FRAME_T*)

err::Err Frame::get_video_frame(AX_VIDEO_FRAME_T * frame)

{
  err::Err uRet;
  frame_from_e eFrom;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  eFrom = (frame_from_e)this_param->from;
  if ((eFrom < FRAME_FROM_AX_MALLOC) && ((-0x2cL >> ((ulong)eFrom & 0x3f) & 1U) == 0)) {
    memcpy(frame,this_param->data,sizeof(*frame));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",eFrom);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// Frame::set_video_frame(axVIDEO_FRAME_T*)

err::Err Frame::set_video_frame(AX_VIDEO_FRAME_T * frame)

{
  err::Err uRet;
  frame_from_e eFrom;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  eFrom = (frame_from_e)this_param->from;
  if ((eFrom < FRAME_FROM_AX_MALLOC) && ((-0x2cL >> ((ulong)eFrom & 0x3f) & 1U) == 0)) {
    memcpy(this_param->data,frame,sizeof(*frame));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",eFrom);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// Frame::get_audio_frame(axAUDIO_FRAME_T*)

err::Err Frame::get_audio_frame(AX_AUDIO_FRAME_T * frame)

{
  frame_audio_param_t *this_param;

  this_param = (frame_audio_param_t *)this->__param;
  if (this_param->from != FRAME_FROM_AUDIO_FRAME) {
    maix::log::error("get audio frame failed! frame from %d not implemented",this_param->from);
    return err::ERR_RUNTIME;
  }
  frame->u64VirAddr = (AX_U8 *)this_param->vir_addr;
  frame->enBitwidth = (AX_AUDIO_BIT_WIDTH_E)this_param->bit_width;
  frame->enSoundmode = (AX_AUDIO_SOUND_MODE_E)this_param->sound_mode;
  frame->u64TimeStamp = this_param->timestamp;
  frame->u64PhyAddr = (AX_U64)this_param->phy_addr;
  *(uint64_t *)frame->u32PoolId = *(uint64_t *)this_param->pool_id;
  frame->u32Seq = this_param->seq;
  frame->u32Len = this_param->len;
  frame->bEof = (AX_BOOL)this_param->eof;
  frame->u32BlkId = this_param->blk_id;
  return err::ERR_NONE;
}



// Frame::set_audio_frame(axAUDIO_FRAME_T*)

err::Err Frame::set_audio_frame(AX_AUDIO_FRAME_T * frame)

{
  frame_audio_param_t *this_param;

  this_param = (frame_audio_param_t *)this->__param;
  if (this_param->from != FRAME_FROM_AUDIO_FRAME) {
    maix::log::error("get audio frame failed! frame from %d not implemented",this_param->from);
    return err::ERR_RUNTIME;
  }
  this_param->vir_addr = frame->u64VirAddr;
  this_param->bit_width = frame->enBitwidth;
  this_param->sound_mode = frame->enSoundmode;
  this_param->timestamp = frame->u64TimeStamp;
  this_param->phy_addr = (void *)frame->u64PhyAddr;
  *(uint64_t *)this_param->pool_id = *(uint64_t *)frame->u32PoolId;
  this_param->seq = frame->u32Seq;
  this_param->len = frame->u32Len;
  this_param->eof = frame->bEof;
  this_param->blk_id = frame->u32BlkId;
  return err::ERR_NONE;
}



// Frame::get_video_frame_info(axVIDEO_FRAME_INFO_T*)

err::Err Frame::get_video_frame_info(AX_VIDEO_FRAME_INFO_T * stream)

{
  err::Err uRet;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  if (this_param->from == FRAME_FROM_VDEC_GET_STREAM) {
    memcpy(stream,this_param->data,sizeof(*stream));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",this_param->from);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// Frame::set_video_frame_info(axVIDEO_FRAME_INFO_T*)

err::Err Frame::set_video_frame_info(AX_VIDEO_FRAME_INFO_T * stream)

{
  err::Err uRet;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  if (this_param->from == FRAME_FROM_VDEC_GET_STREAM) {
    memcpy(this_param->data,stream,sizeof(*stream));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",this_param->from);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// Frame::get_venc_stream(axVENC_STREAM_T*)

err::Err Frame::get_venc_stream(AX_VENC_STREAM_T * stream)

{
  err::Err uRet;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  if (this_param->from == FRAME_FROM_VENC_GET_STREAM) {
    memcpy(stream,this_param->data,sizeof(*stream));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",this_param->from);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// Frame::set_venc_stream(axVENC_STREAM_T*)

err::Err Frame::set_venc_stream(AX_VENC_STREAM_T * stream)

{
  err::Err uRet;

  frame_param_t *this_param = (frame_param_t *)this->__param;
  if (this_param->from == FRAME_FROM_VENC_GET_STREAM) {
    memcpy(this_param->data,stream,sizeof(*stream));
    uRet = err::ERR_NONE;
  }
  else {
    maix::log::error("get video frame failed! frame from %d not implemented",this_param->from);
    uRet = err::ERR_RUNTIME;
  }
  return uRet;
}



// ENGINE::ENGINE(AX_ENGINE_NPU_MODE_T)

ENGINE::ENGINE(AX_ENGINE_NPU_MODE_T mode)

{
  this->__is_inited = false;
  this->__mode = mode;
  return;
}



// ENGINE::init()

err::Err ENGINE::init()

{
  AX_S32 s32Ret;
  AX_ENGINE_NPU_ATTR_T stNpuAttr;
  int initCount;

  if (this->__is_inited == false) {
    AxModuleParam &axMod = AxModuleParam::getInstance();
    ax_sys_mod_t *sysMod = (ax_sys_mod_t *)axMod.get_param(AX_MOD_SYS);
    axMod.lock(AX_MOD_SYS);
    initCount = sysMod->init_count;
    axMod.unlock(AX_MOD_SYS);
    if (initCount < 1) {
      maix::log::error("ax sys not init");
      return err::ERR_RUNTIME;
    }
    ax_engine_mod_t *engineMod = (ax_engine_mod_t *)axMod.get_param(AX_MOD_ENGINE);
    axMod.lock(AX_MOD_ENGINE);
    initCount = engineMod->init_count;
    if (initCount < 1) {
      memset(&stNpuAttr,0,sizeof(stNpuAttr));
      stNpuAttr.eHardMode = this->__mode;
      s32Ret = AX_ENGINE_Init(&stNpuAttr);
      if (s32Ret != 0) {
        axMod.unlock(AX_MOD_ENGINE);
        maix::log::error("ax engine init failed! ret:%#x",s32Ret);
        return err::ERR_RUNTIME;
      }
      engineMod->init_count = 1;
      engineMod->mode = this->__mode;
    }
    else {
      engineMod->init_count = initCount + 1;
    }
    axMod.unlock(AX_MOD_ENGINE);
    maix::log::info("maix npu driver used count: %d",
                    engineMod->init_count);
    this->__is_inited = true;
  }
  return err::ERR_NONE;
}



// ENGINE::deinit()

void ENGINE::deinit()

{
  int initCount;
  uint32_t newInitCount;
  char *pcLog;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_engine_mod_t *engineMod = (ax_engine_mod_t *)axMod.get_param(AX_MOD_ENGINE);
  axMod.lock(AX_MOD_ENGINE);
  initCount = engineMod->init_count;
  if (initCount < 2) {
    AX_ENGINE_Deinit();
    initCount = 0;
  }
  else {
    initCount = initCount - 1;
  }
  engineMod->init_count = initCount;
  axMod.unlock(AX_MOD_ENGINE);
  newInitCount = engineMod->init_count;
  pcLog = ", driver released.";
  if (newInitCount != 0) {
    pcLog = "";
  }
  maix::log::info("maix npu driver used count: %d%s",newInitCount,pcLog);
  this->__is_inited = false;
  return;
}



// ENGINE::~ENGINE()

ENGINE::~ENGINE()

{
  if (this->__is_inited != false) {
    deinit();
    return;
  }
  return;
}



// VI::VI()

VI::VI()

{
  err::Err uErr;
  ENGINE *engineCtx;
  AX_IMG_FORMAT_E dstImageFormat;
  int initCount;
  AX_U8 nCommCamCnt;
  AX_U8 nPrivCamCnt;
  AX_U32 nPrivPoolCfgCnt;
  COMMON_SYS_POOL_CFG_T *pCommPoolCfg;
  COMMON_SYS_POOL_CFG_T *pPrivPoolCfg;
  AX_U16 u16Width;
  AX_U32 uCommPoolCfgCnt;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_sys_mod_t *sysMod = (ax_sys_mod_t *)axMod.get_param(AX_MOD_SYS);
  axMod.lock(AX_MOD_SYS);
  initCount = sysMod->init_count;
  nCommCamCnt = sysMod->tCommonArgs.nCamCnt;
  uCommPoolCfgCnt = sysMod->tCommonArgs.nPoolCfgCnt;
  pCommPoolCfg = sysMod->tCommonArgs.pPoolCfg;
  nPrivCamCnt = sysMod->tPrivArgs.nCamCnt;
  nPrivPoolCfgCnt = sysMod->tPrivArgs.nPoolCfgCnt;
  pPrivPoolCfg = sysMod->tPrivArgs.pPoolCfg;
  axMod.unlock(AX_MOD_SYS);
  if (initCount < 1) {
    maix::err::check_raise(err::ERR_RUNTIME,"ax sys not init");
  }
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  initCount = viMod->init_count;
  if (initCount < 1) {
    if (viMod->cams[0].tPipeInfo[0].ePipeMode == SAMPLE_PIPE_MODE_VIDEO) {
      engineCtx = (ENGINE *)0x0;
    }
    else {
      engineCtx = new ENGINE(AX_ENGINE_VIRTUAL_NPU_ENABLE);
      maix::err::check_null_raise(engineCtx,"create engine failed");
      uErr = engineCtx->init();
      maix::err::check_raise(uErr,"engine init failed");
    }
    viMod->tCommonArgs.nCamCnt = nCommCamCnt;
    viMod->tCommonArgs.nPoolCfgCnt = uCommPoolCfgCnt;
    viMod->tCommonArgs.pPoolCfg = pCommPoolCfg;
    viMod->tPrivArgs.nCamCnt = nPrivCamCnt;
    viMod->tPrivArgs.nPoolCfgCnt = nPrivPoolCfgCnt;
    viMod->tPrivArgs.pPoolCfg = pPrivPoolCfg;
    dstImageFormat = pCommPoolCfg->nFmt;
    viMod->VinId = 0;
    viMod->IvpsId = 0;
    viMod->nGrpId = 0;
    viMod->nChnNum = 0;
    viMod->nGroupInputWidth = pCommPoolCfg->nWidth;
    viMod->nGroupInputHeight = pCommPoolCfg->nHeight;
    viMod->nGroupInputFormat = dstImageFormat;
    memset(&viMod->stGrpAttr,0,sizeof(viMod->stGrpAttr));
    viMod->stGrpAttr.nInFifoDepth = 2;
    u16Width = (AX_U16)pCommPoolCfg->nWidth;
    viMod->stPipelineAttr.tFilter[0][0].nDstPicWidth = u16Width;
    viMod->stPipelineAttr.tFilter[0][0].nDstPicHeight = (AX_U16)pCommPoolCfg->nHeight;
    viMod->stPipelineAttr.tFilter[0][0].nDstPicStride = (u16Width + 0xf) & 0xfff0;
    viMod->stPipelineAttr.tFilter[0][0].eDstPicFormat = dstImageFormat;
    viMod->engine = engineCtx;
    viMod->stPipelineAttr.tFilter[0][0].bEngage = AX_TRUE;
    viMod->stPipelineAttr.tFilter[0][0].eEngine = AX_IVPS_ENGINE_TDP;
    initCount = 1;
  }
  else {
    initCount = initCount + 1;
  }
  viMod->init_count = initCount;
  axMod.unlock(AX_MOD_VI);
  return;
}



// VI::~VI()

VI::~VI()

{
  int initCount;
  ENGINE *engineCtx;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  initCount = viMod->init_count;
  if (initCount < 2) {
    engineCtx = viMod->engine;
    if (engineCtx != (ENGINE *)0x0) {
      delete engineCtx;
      viMod->engine = (ENGINE *)0x0;
    }
    initCount = 0;
  }
  else {
    initCount = initCount - 1;
  }
  viMod->init_count = initCount;
  axMod.unlock(AX_MOD_VI);
  return;
}



// VI::init()

err::Err VI::init()

{
  AX_U8 nPipeId;
  int initCount2;
  AX_S32 s32PipeRet;
  AX_S32 s32SnsRet;
  AX_S32 s32Ret;
  char *pcError;
  uint64_t uError;
  uint64_t uCurPipeIdx;
  SAMPLE_PIPE_INFO_T *ptPipeInfo;
  AX_VIN_STITCH_GRP_ATTR_T stStitchAttr;
  AX_MOD_INFO_T tSrcMod;
  AX_MOD_INFO_T tDstMod;
  AX_CAMERA_T stCam;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  memcpy(&stCam,viMod->cams,sizeof(stCam));
  memset(&stStitchAttr,0,sizeof(stStitchAttr));
  initCount2 = viMod->init_count2;
  if (0 < initCount2) {
    initCount2 = initCount2 + 1;
done:
    viMod->init_count2 = initCount2;
    axMod.unlock(AX_MOD_VI);
    return err::ERR_NONE;
  }
  AX_SYS_SetVINIVPSMode(viMod->VinId,viMod->IvpsId,AX_ITP_OFFLINE_VPP)
  ;
  tSrcMod.s32ChnId = 0;
  tSrcMod.enModId = AX_ID_VIN;
  tSrcMod.s32GrpId = viMod->VinId;
  tDstMod.enModId = AX_ID_IVPS;
  tDstMod.s32GrpId = viMod->IvpsId;
  tDstMod.s32ChnId = 0;
  s32Ret = AX_SYS_Link(&tSrcMod,&tDstMod);
  if (s32Ret == 0) {
    s32Ret = AX_VIN_Init();
    if (s32Ret == 0) {
      s32Ret = AX_VIN_GetStitchGrpAttr(0,&stStitchAttr);
      uError = s32Ret;
      if (s32Ret == 0) {
        s32Ret = AX_VIN_SetStitchGrpAttr(0,&stStitchAttr);
        uError = s32Ret;
        if (s32Ret != 0) {
          printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
          pcError = "AX_VIN_Init failed, ret=0x%x.\n";
          goto vin_stitchgrp_failed;
        }
        s32Ret = COMMON_CAM_PrivPoolInit(&viMod->tPrivArgs);
        uError = s32Ret;
        if (s32Ret != 0) {
          printf("[COMM_ISP][%s][%5d] ","init",__LINE__);
          pcError = "COMMON_CAM_PrivPoolInit fail, ret:0x%x";
          goto vin_stitchgrp_failed;
        }
        s32Ret = AX_MIPI_RX_Init();
        uError = s32Ret;
        if (s32Ret != 0) {
          printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
          pcError = "AX_MIPI_RX_Init failed, ret=0x%x.\n";
          goto vin_stitchgrp_failed;
        }
        s32Ret = AX_ISP_OpenSnsClk(stCam.tSnsClkAttr.nSnsClkIdx,stCam.tSnsClkAttr.eSnsClkRate);
        uError = s32Ret;
        if (s32Ret == 0) {
          s32Ret = COMMON_ISP_ResetSnsObj(0,stCam.nDevId,stCam.ptSnsHdl[stCam.nPipeId]);
          uError = s32Ret;
          if (s32Ret != 0) {
            printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
            pcError = "COMMON_ISP_ResetSnsObj failed, ret=0x%x.\n";
            goto isp_snsclk_failed;
          }
          s32Ret = COMMON_VIN_StartMipi
                             (stCam.nRxDev & 0xff,stCam.eInputMode,&stCam.tMipiAttr,
                              stCam.eLaneComboMode);
          uError = s32Ret;
          if (s32Ret != 0) {
            printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
            pcError = "COMMON_VIN_StartMipi failed, r-et=0x%x.\n";
            goto isp_snsclk_failed;
          }
          if (viMod->cams[0].eSnsType == SAMPLE_SNS_LT6911) {
            LT6911_HDMI_Enable();
          }
          s32Ret = COMMON_VIN_CreateDev
                             (stCam.nDevId,(AX_U8)stCam.nRxDev,&stCam.tDevAttr,&stCam.tDevBindPipe);
          if (s32Ret == 0) {
            ptPipeInfo = &stCam.tPipeInfo[0];
            for (uCurPipeIdx = 0; uCurPipeIdx < stCam.tDevBindPipe.nNum; uCurPipeIdx = uCurPipeIdx + 1) {
              nPipeId = (AX_U8)stCam.tDevBindPipe.nPipeId[uCurPipeIdx];
              stCam.tPipeAttr[stCam.nPipeId].bAiIspEnable = ptPipeInfo->bAiispEnable;
              s32PipeRet = COMMON_VIN_SetPipeAttr
                                (stCam.eSysMode,stCam.eLoadRawNode,nPipeId,
                                 stCam.tPipeAttr + stCam.nPipeId);
              if (s32PipeRet != 0) {
                printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                printf("COMMON_ISP_SetPipeAttr failed, ret=0x%x.\n",s32PipeRet);
                uCurPipeIdx = stCam.nPipeId;
                puts(" ===================== vin pipe info ===================== \r");
                printf("attr->ePipeWorkMode:%d\r\n",(uint32_t)stCam.tPipeAttr[uCurPipeIdx].ePipeWorkMode);
                printf("attr->tPipeImgRgn.nStartX:%d\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tPipeImgRgn.nStartX);
                printf("attr->tPipeImgRgn.nStartY:%d\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tPipeImgRgn.nStartY);
                printf("attr->tPipeImgRgn.nWidth:%d\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tPipeImgRgn.nWidth);
                printf("attr->tPipeImgRgn.nHeight:%d\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tPipeImgRgn.nHeight);
                printf("attr->nWidthStride:%d\r\n",stCam.tPipeAttr[uCurPipeIdx].nWidthStride);
                printf("attr->eBayerPattern:%d\r\n",stCam.tPipeAttr[uCurPipeIdx].eBayerPattern);
                printf("attr->ePixelFmt:%d\r\n",(uint32_t)stCam.tPipeAttr[uCurPipeIdx].ePixelFmt);
                printf("attr->eSnsMode:%d\r\n",stCam.tPipeAttr[uCurPipeIdx].eSnsMode);
                printf("attr->eFusionMode:%d\r\n",stCam.tPipeAttr[uCurPipeIdx].eFusionMode);
                printf("attr->bAiIspEnable:%d\r\n",stCam.tPipeAttr[uCurPipeIdx].bAiIspEnable);
                printf("attr->tCompressInfo.enCompressMode:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tCompressInfo.enCompressMode);
                printf("attr->tCompressInfo.u32CompressLevel:%d\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tCompressInfo.u32CompressLevel);
                printf("attr->eCombMode:%d\r\n",(uint32_t)stCam.tPipeAttr[uCurPipeIdx].eCombMode);
                printf("attr->tNrAttr.t3DnrAttr.bPwlEnable:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tNrAttr.t3DnrAttr.bPwlEnable);
                if (stCam.tPipeAttr[uCurPipeIdx].tNrAttr.t3DnrAttr.bPwlEnable != AX_FALSE) {
                  printf("attr->tNrAttr.t3DnrAttr.tCompressInfo.enCompressMode:%d\r\n",
                         (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tNrAttr.t3DnrAttr.tCompressInfo.
                                enCompressMode);
                  printf("attr->tNrAttr.t3DnrAttr.tCompressInfo.u32CompressLevel:%d\r\n",
                         stCam.tPipeAttr[uCurPipeIdx].tNrAttr.t3DnrAttr.tCompressInfo.
                                u32CompressLevel);
                }
                printf("attr->tNrAttr.tAinrAttr.bPwlEnable:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tNrAttr.tAinrAttr.bPwlEnable);
                if (stCam.tPipeAttr[uCurPipeIdx].tNrAttr.tAinrAttr.bPwlEnable != AX_FALSE) {
                  printf("attr->tNrAttr.tAinrAttr.tCompressInfo.enCompressMode:%d\r\n",
                         (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tNrAttr.tAinrAttr.tCompressInfo.
                                enCompressMode);
                  printf("attr->tNrAttr.tAinrAttr.tCompressInfo.u32CompressLevel:%d\r\n",
                         stCam.tPipeAttr[uCurPipeIdx].tNrAttr.tAinrAttr.tCompressInfo.
                                u32CompressLevel);
                }
                printf("attr->tFrameRateCtrl.fDstFrameRate:%f\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tFrameRateCtrl.fDstFrameRate);
                printf("attr->tFrameRateCtrl.fSrcFrameRate:%f\r\n",
                       stCam.tPipeAttr[uCurPipeIdx].tFrameRateCtrl.fSrcFrameRate);
                printf("attr->tMotionAttr.bMotionComp:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tMotionAttr.bMotionComp);
                printf("attr->tMotionAttr.bMotionEst:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tMotionAttr.bMotionEst);
                printf("attr->tMotionAttr.bMotionShare:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tMotionAttr.bMotionShare);
                printf("attr->tWarpAttr.eWarpEngine:%d\r\n",
                       (uint32_t)stCam.tPipeAttr[uCurPipeIdx].tWarpAttr.eWarpEngine);
                s32SnsRet = (AX_GDC_MODE_E)stCam.tPipeAttr[uCurPipeIdx].tWarpAttr.uWarpMode.eGdcMode;
                pcError = "attr->tWarpAttr.uWarpMode:%d\r\n";
vin_pipe_failed:
                printf(pcError,(uint32_t)s32SnsRet);
                goto vin_dev_destroy;
              }
              if (stCam.bRegisterSns != AX_FALSE) {
                s32SnsRet = COMMON_ISP_RegisterSns
                                  (nPipeId,stCam.nDevId,stCam.eBusType,stCam.ptSnsHdl[stCam.nPipeId],
                                   stCam.nI2cAddr,stCam.nI2cNode);
                if (s32SnsRet != AX_GDC_MODE_CORE0) {
                  printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                  pcError = "COMMON_ISP_RegisterSns failed, ret=0x%x.\n";
                  goto vin_pipe_failed;
                }
                s32Ret = COMMON_ISP_SetSnsAttr(nPipeId,&stCam.tSnsAttr,&stCam.tSnsClkAttr);
                if (s32Ret == 0) goto isp_pipe_init;
                printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                pcError = "COMMON_ISP_SetSnsAttr failed, ret=0x%x.\n";
isp_sns_failed:
                printf(pcError,s32Ret);
isp_sns_unreg:
                COMMON_ISP_UnRegisterSns(nPipeId);
                goto vin_dev_destroy;
              }
isp_pipe_init:
              s32Ret = COMMON_ISP_Init(nPipeId,stCam.ptSnsHdl[stCam.nPipeId],stCam.bRegisterSns,
                                       stCam.bUser3a,&stCam.tAeFuncs,&stCam.tAwbFuncs,
                                       &stCam.tAfFuncs,&stCam.tLscFuncs,ptPipeInfo->szBinPath);
              if (s32Ret != 0) {
                printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                pcError = "COMMON_ISP_StartIsp failed, axRet = 0x%x.\n";
                goto isp_sns_failed;
              }
              s32Ret = AX_VIN_SetChnAttr(nPipeId,AX_VIN_CHN_ID_MAIN,stCam.tChnAttr);
              if (s32Ret != 0) {
                printf("[COMM_VIN][%s][%5d] ","init",__LINE__);
                pcError = "AX_VIN_SetChnAttr failed, nRet=0x%x.\n";
vin_chn_failed:
                printf(pcError,s32Ret);
isp_pipe_deinit:
                COMMON_ISP_DeInit(nPipeId,stCam.bRegisterSns);
                goto isp_sns_unreg;
              }
              if ((stCam.bChnEn[0] == AX_TRUE) &&
                 (s32Ret = AX_VIN_EnableChn(nPipeId,AX_VIN_CHN_ID_MAIN), s32Ret != 0)) {
                printf("[COMM_VIN][%s][%5d] ","init",__LINE__);
                pcError = "AX_VIN_EnableChn failed, nRet=0x%x.\n";
                goto vin_chn_failed;
              }
              s32PipeRet = AX_VIN_StartPipe(nPipeId);
              if (s32PipeRet != 0) {
                printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                printf("AX_VIN_StartPipe failed, ret=0x%x\n",s32PipeRet);
vin_stop_chn:
                COMMON_VIN_StopChn(nPipeId);
                goto isp_pipe_deinit;
              }
              ptPipeInfo = ptPipeInfo + 1;
              s32PipeRet = AX_ISP_Start(nPipeId);
              if (s32PipeRet != 0) {
                printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                printf("AX_ISP_Open failed, ret=0x%x\n",s32PipeRet);
                AX_VIN_StopPipe(nPipeId);
                goto vin_stop_chn;
              }
            }
            s32Ret = COMMON_VIN_StartDev(stCam.nDevId,stCam.bEnableDev,&stCam.tDevAttr);
            if (s32Ret == 0) {
              if ((stCam.bRegisterSns != AX_FALSE) && (stCam.bEnableDev != AX_FALSE)) {
                for (uCurPipeIdx = 0; uCurPipeIdx < stCam.tDevBindPipe.nNum; uCurPipeIdx = uCurPipeIdx + 1) {
                  s32Ret = AX_ISP_StreamOn((AX_U8)stCam.tDevBindPipe.nPipeId[uCurPipeIdx]);
                  if (s32Ret != 0) {
                    printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
                    pcError = " failed, ret=0x%x.\n";
                    goto vin_dev_failed;
                  }
                }
              }
              s32Ret = AX_IVPS_Init();
              if (s32Ret == 0) {
                s32Ret = AX_IVPS_CreateGrp(viMod->nGrpId,
                                           &viMod->stGrpAttr);
                if (s32Ret == 0) {
                  initCount2 = 1;
                  goto done;
                }
                printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_CreateGrp failed,nGrp %d,s32Ret:0x%x\x1 b[0m\n"
                       ,"init", __LINE__, (uint32_t)viMod->nGrpId,s32Ret);
                AX_IVPS_Deinit();
              }
              else {
                printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_Init failed,s32Ret:0x%x\x1b[0m\n",
                       "init",__LINE__,s32Ret);
              }
              if ((stCam.bRegisterSns != AX_FALSE) && (stCam.bEnableDev != AX_FALSE)) {
                for (uCurPipeIdx = 0; uCurPipeIdx < stCam.tDevBindPipe.nNum; uCurPipeIdx = uCurPipeIdx + 1) {
                  AX_ISP_StreamOff((AX_U8)stCam.tDevBindPipe.nPipeId[uCurPipeIdx]);
                }
              }
              COMMON_VIN_StopDev(stCam.nDevId,stCam.bEnableDev);
            }
            else {
              printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
              pcError = "COMMON_VIN_StartDev failed, ret=0x%x.\n";
vin_dev_failed:
              printf(pcError,s32Ret);
            }
            for (uCurPipeIdx = 0; uCurPipeIdx < stCam.tDevBindPipe.nNum; uCurPipeIdx = uCurPipeIdx + 1) {
              nPipeId = (AX_U8)stCam.tDevBindPipe.nPipeId[uCurPipeIdx];
              AX_ISP_Stop(nPipeId);
              AX_VIN_StopPipe(nPipeId);
              COMMON_VIN_StopChn(nPipeId);
              COMMON_ISP_DeInit(nPipeId,stCam.bRegisterSns);
            }
vin_dev_destroy:
            COMMON_VIN_DestroyDev(stCam.nDevId);
          }
          else {
            printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
            printf("COMMON_VIN_CreateDev failed, ret=0x%x.\n",s32Ret);
          }
          COMMON_VIN_StopMipi(stCam.nRxDev & 0xff);
        }
        else {
          printf("[COMM_ISP][%s][%5d] ","init",__LINE__);
          pcError = "AX_ISP_OpenSnsClk failed, nRet=0x%x.\n";
isp_snsclk_failed:
          printf(pcError,uError);
        }
        AX_MIPI_RX_DeInit();
      }
      else {
        printf("[COMM_ISP][%s][%5d] ","init",__LINE__);
        pcError = "AX_VIN_GetStitchGrpAttr failed, nRet=0x%x.\n";
vin_stitchgrp_failed:
        printf(pcError,uError);
      }
      AX_VIN_Deinit();
      goto failed;
    }
    printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
    pcError = "AX_VIN_Init failed, ret=0x%x.\n";
  }
  else {
    printf("[COMM_CAM][%s][%5d] ","init",__LINE__);
    pcError = "AX_SYS_Link failed, ret:0x%x\n";
  }
  printf(pcError,s32Ret);
failed:
  axMod.unlock(AX_MOD_VI);
  maix::err::check_raise(err::ERR_RUNTIME,"vi init failed");
  return err::ERR_RUNTIME;
}


// VI::deinit()

err::Err VI::deinit()

{
  AX_U8 nPipeId;
  uint64_t uCurPipeIdx;
  AX_MOD_INFO_T tSrcMod;
  AX_MOD_INFO_T tDstMod;
  AX_U8 nDevId;
  int initCount2;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  nDevId = viMod->cams[0].nDevId;
  initCount2 = viMod->init_count2;
  if (initCount2 < 2) {
    if (initCount2 == 1) {
      AX_IVPS_DestoryGrp(viMod->nGrpId);
      AX_IVPS_Deinit();
      for (uCurPipeIdx = 0; uCurPipeIdx < viMod->cams[0].tDevBindPipe.nNum;
          uCurPipeIdx = uCurPipeIdx + 1) {
        AX_ISP_Stop((AX_U8)viMod->cams[0].tDevBindPipe.nPipeId[uCurPipeIdx]);
      }
      AX_VIN_DisableDev(nDevId);
      if ((viMod->cams[0].bRegisterSns != AX_FALSE) &&
         (viMod->cams[0].bEnableDev != AX_FALSE)) {
        for (uCurPipeIdx = 0; uCurPipeIdx < viMod->cams[0].tDevBindPipe.nNum;
            uCurPipeIdx = uCurPipeIdx + 1) {
          AX_ISP_StreamOff((AX_U8)viMod->cams[0].tDevBindPipe.nPipeId[uCurPipeIdx]);
        }
      }
      for (uCurPipeIdx = 0; uCurPipeIdx < viMod->cams[0].tDevBindPipe.nNum;
          uCurPipeIdx = uCurPipeIdx + 1) {
        nPipeId = (AX_U8)viMod->cams[0].tDevBindPipe.nPipeId[uCurPipeIdx];
        AX_ISP_CloseSnsClk(nPipeId);
        AX_VIN_StopPipe(nPipeId);
        AX_VIN_DisableChn(nPipeId,AX_VIN_CHN_ID_MAIN);
        AX_ISP_Close(nPipeId);
        AX_ISP_Destroy(nPipeId);
        AX_ISP_UnRegisterSensor(nPipeId);
        AX_VIN_DestroyPipe(nPipeId);
      }
      AX_MIPI_RX_Stop(nDevId);
      AX_VIN_DestroyDev(nDevId);
      AX_MIPI_RX_DeInit();
      AX_VIN_Deinit();
      tSrcMod.s32ChnId = 0;
      tSrcMod.enModId = AX_ID_VIN;
      tSrcMod.s32GrpId = viMod->VinId;
      tDstMod.enModId = AX_ID_IVPS;
      tDstMod.s32GrpId = viMod->IvpsId;
      tDstMod.s32ChnId = 0;
      AX_SYS_UnLink(&tSrcMod,&tDstMod);
      viMod->init_count2 = 0;
    }
  }
  else {
    viMod->init_count2 = initCount2 - 1;
  }
  axMod.unlock(AX_MOD_VI);
  return err::ERR_NONE;
}


// VI::get_unused_channel()

int VI::get_unused_channel()

{
  int iRet;
  long curIvpsChn;
  long nextIvpsChn;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  curIvpsChn = 0;
  do {
    nextIvpsChn = curIvpsChn + 1;
    if (viMod->stPipelineAttr.tFilter[curIvpsChn + 1][0].bEngage == AX_FALSE) {
      iRet = (int)curIvpsChn;
      goto done;
    }
    curIvpsChn = nextIvpsChn;
  } while (nextIvpsChn != 5);
  iRet = -1;
done:
  axMod.unlock(AX_MOD_VI);
  return iRet;
}



// VI::add_channel(int, int, int, AX_IMG_FORMAT_E, int, int, bool, bool,
// int)

err::Err VI::add_channel(int ch, int width, int height, AX_IMG_FORMAT_E format, int fps, int depth, bool mirror, bool vflip, int fit)

{
  int FilterChn;
  uint16_t u16VppTmp;
  uint32_t u32DstWidth;
  uint32_t u32DstHeight;
  int NumIvpsChn;
  IVPS_GRP IvpsGrp;
  AX_S32 s32GrpRet;
  AX_S32 s32Ret;
  char *pcError;
  uint64_t uLine;
  uint32_t u32AdjWidth;
  uint32_t u32AdjHeight;
  uint16_t u16VppWidth;
  uint16_t u16VppHeight;
  AX_U16 u16DstHeight;
  uint32_t nGrpId;
  AX_U16 u16DstWidth;
  uint64_t uCurIvpsChn;
  IVPS_CHN CurIvpsChn;
  double dInpRatio;
  double dOutWidth;
  double dOutHeight;
  double dInpHeight;
  double dInpWidth;
  AX_IMG_FORMAT_E dstImgFormat;

#if 0
  if ((width != 320 || height != 240) || ((format & 0xfffffffb) != 0xa1)) {
    auth_func();
  }
#endif
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  if (4 < viMod->nChnNum) {
    maix::log::error("channel num exceed max");
    goto failed;
  }
  FilterChn = ch + 1;
  if ((viMod->stPipelineAttr).tFilter[FilterChn][0].bEngage != AX_FALSE) {
    maix::log::warn("channel already exist");
    axMod.unlock(AX_MOD_VI);
    return err::ERR_NOT_OPEN;
  }
  if ((width & 0xfU) == 0 && (height & 1U) == 0) {
    if ((0x1f < width && height != 0x1f) && (width < 0x20 || 0x1e < height)) {
      s32GrpRet = AX_IVPS_StopGrp(viMod->nGrpId);
      if ((s32GrpRet != 0) && (s32GrpRet != AX_ERR_IVPS_NOT_PERM)) {
        printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x\x1b[0m\n",
               "add_channel",__LINE__,(uint32_t)viMod->nGrpId,s32GrpRet);
      }
      CurIvpsChn = 0;
      while( true ) {
        NumIvpsChn = viMod->nChnNum;
        IvpsGrp = viMod->nGrpId;
        if (NumIvpsChn <= CurIvpsChn) break;
        s32Ret = AX_IVPS_DisableChn(IvpsGrp,CurIvpsChn);
        if (s32Ret != 0) {
          printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_DisableChn failed,nGrp %d,nChn %d,s32Ret:0x%x\x1b[0m\n"
                 ,"add_channel",__LINE__,(uint32_t)viMod->nGrpId,
                 (uint32_t)CurIvpsChn,s32Ret);
        }
        CurIvpsChn = CurIvpsChn + 1;
      }
      NumIvpsChn = NumIvpsChn + 1;
      viMod->chn_out[ch].w = width;
      viMod->chn_out[ch].h = height;
      viMod->chn_out[ch].fmt = format;
      viMod->chn_out[ch].fps = fps;
      viMod->chn_out[ch].depth = depth;
      viMod->chn_out[ch].mirror = (AX_BOOL)mirror;
      viMod->chn_out[ch].flip = (AX_BOOL)vflip;
      viMod->chn_out[ch].fit = fit;
      viMod->nChnNum = NumIvpsChn;
      viMod->stPipelineAttr.nOutChnNum = (AX_U8)NumIvpsChn;
      if (fit == 0) {
no_fit:
        (viMod->stPipelineAttr).tFilter[FilterChn][0].bEngage = AX_TRUE;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eEngine = AX_IVPS_ENGINE_TDP;
        u16DstHeight = (AX_U16)height;
        u16DstWidth = (AX_U16)width;
        if ((viMod->eRotAngle & ~AX_IVPS_ROTATION_180) == AX_IVPS_ROTATION_90) {
          u16DstHeight = (AX_U16)width;
          u16DstWidth = (AX_U16)height;
        }
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eSclType = AX_IVPS_SCL_TYPE_BILINEAR;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].tTdpCfg.bFlip = (AX_BOOL)vflip;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicWidth = u16DstWidth;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicHeight = u16DstHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicStride = (u16DstWidth + 0xf) & 0xfff0;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].tTdpCfg.bMirror = (AX_BOOL)mirror;
        if ((format & 0xfffffff7) == AX_FORMAT_ARGB8888) {
          format = AX_FORMAT_YUV420_SEMIPLANAR;
        }
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eDstPicFormat = format;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].bEngage = AX_FALSE;
      }
      else {
        dInpHeight = (double)viMod->nGroupInputHeight;
        dInpWidth = (double)viMod->nGroupInputWidth;
        dOutWidth = (double)width;
        dOutHeight = (double)height;
        dInpRatio = dInpWidth / dInpHeight;
        if (ABS(dInpRatio - dOutWidth / dOutHeight) < 0.0001) goto no_fit;
        if (dInpWidth / dOutWidth < dInpHeight / dOutHeight) {
          height = (int)(dOutWidth / dInpRatio) & 0xffff;
        }
        else {
          width = (int)(dInpRatio * dOutHeight) & 0xffff;
        }
        u32AdjWidth = (width + 0xfU) & 0xfffffff0;
        u32AdjHeight = (height + 1U) & 0xfffffffe;
        u16VppTmp = (uint16_t)(height + 1U) & 0xfffe;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].bEngage = AX_TRUE;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eEngine = AX_IVPS_ENGINE_VPP;
        u16VppWidth = (uint16_t)(width + 0xfU) & 0xfff0;
        u16VppHeight = u16VppTmp;
        if ((viMod->eRotAngle & ~AX_IVPS_ROTATION_180) == AX_IVPS_ROTATION_90) {
          u16VppHeight = u16VppWidth;
          u16VppWidth = u16VppTmp;
        }
        dInpRatio = (double)(int)u32AdjWidth / dOutWidth;
        dInpHeight = (double)(int)u32AdjHeight / dOutHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicWidth = u16VppWidth;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicHeight = u16VppHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].nDstPicStride = (u16VppWidth + 0xf) & 0xfff0;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].bEngage = AX_TRUE;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].eEngine = AX_IVPS_ENGINE_TDP;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].bCrop = AX_TRUE;
        if (dInpHeight <= dInpRatio) {
          dInpRatio = dInpHeight;
        }
        dstImgFormat = AX_FORMAT_YUV420_SEMIPLANAR;
        if ((format & 0xfffffff7) != AX_FORMAT_ARGB8888) {
          dstImgFormat = format;
        }
        (viMod->stPipelineAttr).tFilter[FilterChn][1].eDstPicFormat = dstImgFormat;
        u32DstWidth = ((int)(dInpRatio * dOutWidth) + 0xfU) & 0xfff0;
        u32DstHeight = ((int)(dInpRatio * dOutHeight) + 1U) & 0xfffe;
        u16DstWidth = (AX_U16)u32DstWidth;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].tCropRect.nW = u16DstWidth;
        u16DstHeight = (AX_U16)u32DstHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].tCropRect.nH = u16DstHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].nDstPicWidth = u16DstWidth;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].nDstPicHeight = u16DstHeight;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].nDstPicStride = u16DstWidth;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].tCropRect.nX =
             (short)(((int)(u32AdjWidth - u32DstWidth) / 2) + 1U) & 0xfffe;
        (viMod->stPipelineAttr).tFilter[FilterChn][1].tCropRect.nY =
             (short)(((int)(u32AdjHeight - u32DstHeight) / 2) + 1U) & 0xfffe;
      }
      uCurIvpsChn = 0;
      (viMod->stPipelineAttr).nOutFifoDepth[ch] = 1;
      if (FilterChn == 1 && viMod->cams[0].eSnsType == SAMPLE_SNS_LT6911) {
        (viMod->stPipelineAttr).tFilter[0][0].bEngage = AX_FALSE;

        (viMod->stPipelineAttr).tFilter[FilterChn][0].eEngine = AX_IVPS_ENGINE_SCL;
        (viMod->stPipelineAttr).tFilter[FilterChn][0].eSclType = AX_IVPS_SCL_TYPE_AUTO;
      }
      s32Ret = AX_IVPS_SetPipelineAttr(IvpsGrp,&viMod->stPipelineAttr);
      if (s32Ret == 0) {
        do {
          if (viMod->stPipelineAttr.tFilter[uCurIvpsChn + 1][0].bEngage != AX_FALSE) {
            s32Ret = AX_IVPS_EnableChn(viMod->nGrpId,(IVPS_CHN)uCurIvpsChn);
            if (s32Ret != 0) {
              printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_EnableChn failed,nGrp %d,nChn %d,s32Ret:0x%x\x1b[0m\n"
                     ,"add_channel",__LINE__,(uint32_t)viMod->nGrpId,
                     (uint32_t)uCurIvpsChn,s32Ret);
              goto failed;
            }
          }
          uCurIvpsChn = uCurIvpsChn + 1;
        } while (uCurIvpsChn != 5);
        s32Ret = AX_IVPS_StartGrp(viMod->nGrpId);
        if (s32Ret == 0) {
          axMod.unlock(AX_MOD_VI);
          return err::ERR_NONE;
        }
        nGrpId = viMod->nGrpId;
        pcError =
          "\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_StartGrp failed,nGrp %d,s32Ret:0x%x\x1b[0m\n";
        uLine = __LINE__;
      }
      else {
        uLine = __LINE__;
        nGrpId = viMod->nGrpId;
        pcError =
          "\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_SetPipelineAttr failed,nGrp %d,s32Ret:0x%x\x1b[0m\n";
      }
      printf(pcError,"add_channel",uLine,nGrpId,s32Ret);
      goto failed;
    }
    pcError = "width and height must be greater than 32, current the width is %d, the height is %d";
  }
  else {
    pcError =
      "width must be multiple of 16, height must be multiple of 2, current the width is %d, the height is %d"
    ;
  }
  maix::log::error(pcError,width,height);
failed:
  axMod.unlock(AX_MOD_VI);
  return err::ERR_RUNTIME;
}


// VI::del_channel(int)

err::Err VI::del_channel(int ch)

{
  AX_S32 s32Ret;
  int viChnNum;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  s32Ret = AX_IVPS_StopGrp(viMod->nGrpId);
  if ((s32Ret != 0) && (s32Ret != AX_ERR_IVPS_NOT_PERM)) {
    printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x\x1b[0m\n",
           "del_channel",__LINE__,(uint32_t)viMod->nGrpId,s32Ret);
  }
  if ((viMod->stPipelineAttr).tFilter[ch + 1][0].bEngage != AX_FALSE) {
    s32Ret = AX_IVPS_DisableChn(viMod->nGrpId,ch);
    if (s32Ret != 0) {
      printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_DisableChn failed,nGrp %d,nChn %d,s32Ret:0x%x\x1b[0m\n"
             ,"del_channel",__LINE__,(uint32_t)viMod->nGrpId,(uint32_t)ch,
             s32Ret);
      axMod.unlock(AX_MOD_VI);
      return err::ERR_RUNTIME;
    }
    viChnNum = viMod->nChnNum;
    (viMod->stPipelineAttr).tFilter[ch + 1][0].bEngage = AX_FALSE;
    viMod->nChnNum = viChnNum - 1;
  }
  axMod.unlock(AX_MOD_VI);
  return err::ERR_NONE;
}


// VI::del_channel_all()

err::Err VI::del_channel_all()

{
  AX_S32 s32Ret;
  uint64_t curIvpsChn;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  s32Ret = AX_IVPS_StopGrp(viMod->nGrpId);
  if ((s32Ret != 0) && (s32Ret != AX_ERR_IVPS_NOT_PERM)) {
    printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x\x1b[0m\n",
           "del_channel_all",__LINE__,(uint32_t)viMod->nGrpId,s32Ret);
  }
  curIvpsChn = 0;
  do {
    if (viMod->stPipelineAttr.tFilter[curIvpsChn + 1][0].bEngage != AX_FALSE) {
      s32Ret = AX_IVPS_DisableChn(viMod->nGrpId,(IVPS_CHN)curIvpsChn);
      if (s32Ret != 0) {
        axMod.unlock(AX_MOD_VI);
        printf("\x1b[1;30;31mERROR  :[%s:%d] AX_IVPS_DisableChn failed,nGrp %d,nChn %d,s32Ret:0x%x\x1b[0m\n"
               ,"del_channel_all",__LINE__,(uint32_t)viMod->nGrpId,
               (uint32_t)curIvpsChn,s32Ret);
        maix::err::check_raise(err::ERR_RUNTIME,"AX_IVPS_DisableChn failed");
      }
    }
    curIvpsChn = curIvpsChn + 1;
  } while (curIvpsChn != 5);
  viMod->nChnNum = 0;
  axMod.unlock(AX_MOD_VI);
  return err::ERR_NONE;
}



// VI::pop(int, int)

maixcam2::Frame *VI::pop(int ch, int32_t timeout_ms)

{
  AX_IMG_FORMAT_E eImgFormat;
  IVPS_GRP IvpsGrp;
  AX_S32 s32Ret;
  frame_from_e eFrom;
  AX_VIDEO_FRAME_T *ptFrame;
  Frame *frame;
  AX_VIDEO_FRAME_T stIvpsFrame;
  AX_VIDEO_FRAME_T stCscTdpFrame;

  ptFrame = &stIvpsFrame;
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  memset(ptFrame,0,sizeof(*ptFrame));
  s32Ret = AX_IVPS_GetChnFrame(viMod->nGrpId,ch,ptFrame,timeout_ms);
  if (s32Ret == 0) {
    eImgFormat = viMod->chn_out[ch].fmt;
    if (eImgFormat == stIvpsFrame.enImgFormat) {
      eFrom = FRAME_FROM_IVPS_CHN;
    }
    else {
      memset(&stCscTdpFrame,0,sizeof(stCscTdpFrame));
      s32Ret = __ax_ivps_csc_tdp(ptFrame,&stCscTdpFrame,eImgFormat);
      IvpsGrp = viMod->nGrpId;
      if (s32Ret != 0) {
        AX_IVPS_ReleaseChnFrame(IvpsGrp,ch,ptFrame);
        axMod.unlock(AX_MOD_VI);
        maix::log::info(" ivps invert format failed, ret:%#x",s32Ret);
        goto failed;
      }
      eFrom = FRAME_FROM_SYS_MEM_ALLOC;
      AX_IVPS_ReleaseChnFrame(IvpsGrp,ch,ptFrame);
      ptFrame = &stCscTdpFrame;
    }
    frame = new Frame(viMod->nGrpId,ch,ptFrame,eFrom,AX_FORMAT_INVALID);
    axMod.unlock(AX_MOD_VI);
  }
  else {
    axMod.unlock(AX_MOD_VI);
failed:
    frame = (Frame *)0x0;
  }
  return frame;
}



// VI::pop_raw(int, int)

maixcam2::Frame *VI::pop_raw(int ch, int32_t timeout_ms)

{
  AX_S32 s32Ret;
  Frame *frame;
  AX_IMG_INFO_T stImgInfo;

  memset(&stImgInfo,0,sizeof(stImgInfo));
  s32Ret = AX_VIN_GetRawFrame(0,AX_VIN_PIPE_DUMP_NODE_IFE,AX_SNS_HDR_FRAME_L,&stImgInfo,
                              timeout_ms);
  if ((s32Ret != 0) &&
     (maix::log::error("AX_VIN_GetRawFrame failed, ret:0x%x",s32Ret), s32Ret == AX_ERR_VIN_RES_EMPTY))
  {
    maix::err::check_raise(err::ERR_REOPEN,"Raw buffer empty");
  }
  frame = new Frame(0,AX_VIN_PIPE_DUMP_NODE_IFE,AX_SNS_HDR_FRAME_L,&stImgInfo,FRAME_FROM_GET_RAW_FRAME);
  return frame;
}



// VI::set_windowing(int, int, int, int, int)

err::Err VI::set_windowing(int ch, int x, int y, int w, int h)

{
  err::Err uErr;
  ax_vi_mod_t stViMod;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vi_mod_t *viMod = (ax_vi_mod_t *)axMod.get_param(AX_MOD_VI);
  axMod.lock(AX_MOD_VI);
  memcpy(&stViMod,viMod,sizeof(stViMod));
  axMod.unlock(AX_MOD_VI);
  viMod->stPipelineAttr.tFilter[0][0].bCrop = AX_TRUE;
  viMod->stPipelineAttr.tFilter[0][0].tCropRect.nX = (AX_S16)x;
  viMod->stPipelineAttr.tFilter[0][0].tCropRect.nY = (AX_S16)y;
  viMod->stPipelineAttr.tFilter[0][0].tCropRect.nW = (AX_U16)w;
  viMod->stPipelineAttr.tFilter[0][0].tCropRect.nH = (AX_U16)h;
  uErr = del_channel(ch);
  if (uErr == 0) {
    uErr = add_channel(ch,stViMod.chn_out[ch].w,stViMod.chn_out[ch].h,stViMod.chn_out[ch].fmt,
                        stViMod.chn_out[ch].fps,stViMod.chn_out[ch].depth,
                        stViMod.chn_out[ch].mirror != AX_FALSE,stViMod.chn_out[ch].flip != AX_FALSE,
                        stViMod.chn_out[ch].fit);
  }
  return uErr;
}



// VI::set_and_get_exposure(int)

int VI::set_and_get_exposure(int value)

{
  return value;
}



// VI::set_and_get_gain(int)

int VI::set_and_get_gain(int value)

{
  return value;
}



// VI::set_and_get_luma(int)

int VI::set_and_get_luma(int value)

{
  return value;
}



// VI::set_and_get_saturation(int)

int VI::set_and_get_saturation(int value)

{
  return value;
}



// VI::set_and_get_constrast(int)

int VI::set_and_get_constrast(int value)

{
  return value;
}



// VI::set_and_get_hue(int)

int VI::set_and_get_hue(int value)

{
  return value;
}



// VI::set_and_get_mirror(int, int)

int VI::set_and_get_mirror(int ch,int value)

{
  return value;
}



// VI::set_and_get_flip(int, int)

int VI::set_and_get_flip(int ch,int value)

{
  return value;
}



// VI::set_and_get_awb_mode(int)

int VI::set_and_get_awb_mode(int value)

{
  return value;
}



// VI::set_and_get_exp_mode(int)

int VI::set_and_get_exp_mode(int value)

{
  return value;
}



// VO::VO()

VO::VO()

{
  int initCount;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_sys_mod_t *sysMod = (ax_sys_mod_t *)axMod.get_param(AX_MOD_SYS);
  axMod.lock(AX_MOD_SYS);
  initCount = sysMod->init_count;
  axMod.unlock(AX_MOD_SYS);
  if (initCount < 1) {
    maix::err::check_raise(err::ERR_RUNTIME,"ax sys not init");
  }
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  initCount = voMod->init_count;
  if (initCount < 1) {
    voMod->used_channels[0][0] = false;
    voMod->used_channels[0][1] = false;
    voMod->used_channels[0][2] = false;
    voMod->used_channels[1][0] = false;
    voMod->used_channels[1][1] = false;
    voMod->used_channels[1][2] = false;
    voMod->used_channels[2][0] = false;
    voMod->used_channels[2][1] = false;
    voMod->used_channels[2][2] = false;
    memset(voMod->channel_param,0,sizeof(voMod->channel_param));
    initCount = 1;
  }
  else {
    initCount = initCount + 1;
  }
  voMod->init_count = initCount;
  axMod.unlock(AX_MOD_VO);
}



// VO::init(ax_vo_param_t*)

err::Err VO::init(ax_vo_param_t *param)

{
  ax_vo_param_t *pstVoConfig;
  AX_U32 u32LayerWidth;
  AX_U32 u32LayerHeight;
  int initCount2;
  IVPS_GRP IvpsGrp;
  AX_U32 u32LayerNr;
  AX_U16 u16DstWidth;
  AX_U16 u16DstHeight;
  AX_IVPS_PIPELINE_ATTR_T *ptPipelineAttr;
  AX_S32 s32Ret;
  uint64_t uVoLayIndex;
  AX_POOL nPoolId;
  int64_t lTmpSize;
  uint64_t uTmpSize;
  char *pcError;
  uint64_t uError;
  uint64_t uBlkDiv;
  SAMPLE_VO_DEV_CONFIG_S *pTmpVoDev;
  err::Err uErr;
  AX_U32 u32BlkCnt;
  uint64_t uErrIdx;
  uint64_t uVoDevIndex;
  SAMPLE_VO_DEV_CONFIG_S *pCurVoDev;
  AX_BOOL bVoDevWbcEn;
  SAMPLE_VO_GRAPHIC_CONFIG_S *pVoGraphic;
  SAMPLE_VO_LAYER_CONFIG_S *pCurVoLayer;
  uint64_t uBlkSize;
  AX_U32 winRow;
  AX_U32 winCol;
  AX_U32 winWidth;
  AX_U32 winHeight;
  AX_POOL_CONFIG_T stPoolConfig;
  SAMPLE_VO_DEV_CONFIG_S *pVoDev;
  SAMPLE_VO_DEV_CONFIG_S *pNextVoDev;
  AX_BOOL *pbWbcEn;
  AX_IMG_FORMAT_E voLayerPixFmt;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  initCount2 = voMod->init_count2;
  if (0 < initCount2) {
    voMod->init_count2 = initCount2 + 1;
success:
    uErr = err::ERR_NONE;
    goto done;
  }
  pstVoConfig = &voMod->vo_param;
  memcpy(pstVoConfig,param,sizeof(*pstVoConfig));
  voMod->nGrpId = 1;
  s32Ret = AX_VO_Init();
  uError = s32Ret;
  if (s32Ret == 0) {
    pVoDev = &voMod->vo_param.vo_cfg.stVoDev[0];
    u32LayerNr = voMod->vo_param.vo_cfg.u32VDevNr;
    if (voMod->vo_param.vo_cfg.u32BindMode != 0) {
      u32LayerNr = 1;
    }
    bVoDevWbcEn = AX_FALSE;
    voMod->vo_param.vo_cfg.u32LayerNr = u32LayerNr;
    pCurVoDev = pVoDev;
    pCurVoLayer = &voMod->vo_param.vo_cfg.stVoLayer[0];
    for (uVoLayIndex = 0; uVoLayIndex < voMod->vo_param.vo_cfg.u32LayerNr; uVoLayIndex = uVoLayIndex + 1) {
      pCurVoLayer->u64KeepChnPrevFrameBitmap0 = 0xffffffffffffffff;
      pCurVoLayer->u64KeepChnPrevFrameBitmap1 = 0xffffffffffffffff;
      if (voMod->vo_param.vo_cfg.u32BindMode == 0) {
        pCurVoLayer->bindVoDev[0] = pCurVoDev->u32VoDev;
        bVoDevWbcEn = pCurVoDev->bWbcEn;
      }
      else {
        uVoDevIndex = 0;
        pTmpVoDev = pVoDev;
        while (uVoDevIndex < voMod->vo_param.vo_cfg.u32VDevNr) {
          pNextVoDev = pTmpVoDev + 1;
          pCurVoLayer->bindVoDev[uVoDevIndex] = pTmpVoDev->u32VoDev;
          uVoDevIndex = uVoDevIndex + 1;
          pbWbcEn = &pTmpVoDev->bWbcEn;
          pTmpVoDev = pNextVoDev;
          if (*pbWbcEn != AX_FALSE) {
            bVoDevWbcEn = *pbWbcEn;
          }
        }
      }
      u32LayerWidth = (pCurVoLayer->stVoLayerAttr).stImageSize.u32Width;
      u32LayerHeight = (pCurVoLayer->stVoLayerAttr).stImageSize.u32Height;
      SAMPLE_VO_WIN_INFO(u32LayerWidth,u32LayerHeight,pCurVoLayer->enVoMode,&winRow,&winCol,&winWidth,
                         &winHeight);
      lTmpSize = (ulong)((u32LayerWidth + 7) & 0xfffffff8) * (ulong)((u32LayerHeight + 1) & 0xfffffffe);
      voLayerPixFmt = (pCurVoLayer->stVoLayerAttr).enPixFmt;
      if (AX_FORMAT_BGRA5658 < voLayerPixFmt) {
format_failed:
        maix::log::info("not support fromat %d",voLayerPixFmt);
        uErrIdx = uVoLayIndex;
        uError = (uint32_t)voMod->vo_param.vo_cfg.stVoLayer[uVoLayIndex].stVoLayerAttr.enPixFmt;
        pcError = "SAMPLE_VO_FMT2ImgStoreInfo failed, i:%d, enPixFmt:0x%x\n";
        goto vo_start_failed;
      }
      if (voLayerPixFmt < AX_FORMAT_RGB565) {
        if (voLayerPixFmt < (AX_FORMAT_YUV420_SEMIPLANAR_VU|AX_FORMAT_YUV420_PLANAR)) {
          if (voLayerPixFmt < AX_FORMAT_YUV420_SEMIPLANAR) goto format_failed;
        }
        else if (voLayerPixFmt != AX_FORMAT_YUV422_SEMIPLANAR) goto format_failed;
        uBlkDiv = 2;
      }
      else {
        if ((0x1fffe000000063U >>
             ((ulong)(uint)(voLayerPixFmt +
                           ~(AX_FORMAT_BAYER_RAW_8BPP|AX_FORMAT_YUV444_PACKED|
                             AX_FORMAT_YUV420_SEMIPLANAR_VU|AX_FORMAT_YUV420_SEMIPLANAR)) & 0x3f) &
            1) == 0) goto format_failed;
        uBlkDiv = 1;
      }
      if (voLayerPixFmt == AX_FORMAT_YUV422_SEMIPLANAR) {
        uTmpSize = lTmpSize * 4;
      }
      else {
        uTmpSize = lTmpSize * 3;
      }
      uBlkSize = 0;
      if (uBlkDiv != 0) {
        uBlkSize = uTmpSize / uBlkDiv;
      }
      u32BlkCnt = 8;
      if (bVoDevWbcEn == AX_FALSE) {
        u32BlkCnt = 4;
      }
      stPoolConfig.PoolName[0x1c] = '\0';
      stPoolConfig.PoolName[0x1d] = '\0';
      stPoolConfig.PoolName[0x1e] = '\0';
      stPoolConfig.PoolName[0x1f] = '\0';
      stPoolConfig.PoolName[0x14] = '\0';
      stPoolConfig.PoolName[0x15] = '\0';
      stPoolConfig.PoolName[0x16] = '\0';
      stPoolConfig.PoolName[0x17] = '\0';
      stPoolConfig.PoolName[0x18] = '\0';
      stPoolConfig.PoolName[0x19] = '\0';
      stPoolConfig.PoolName[0x1a] = '\0';
      stPoolConfig.PoolName[0x1b] = '\0';
      pCurVoDev = pCurVoDev + 1;
      stPoolConfig.CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
      stPoolConfig.PartitionName[0] = '\0';
      stPoolConfig.PartitionName[1] = '\0';
      stPoolConfig.PartitionName[2] = '\0';
      stPoolConfig.PartitionName[3] = '\0';
      stPoolConfig.PartitionName[0xc] = '\0';
      stPoolConfig.PartitionName[0xd] = '\0';
      stPoolConfig.PartitionName[0xe] = '\0';
      stPoolConfig.PartitionName[0xf] = '\0';
      stPoolConfig.PartitionName[0x10] = '\0';
      stPoolConfig.PartitionName[0x11] = '\0';
      stPoolConfig.PartitionName[0x12] = '\0';
      stPoolConfig.PartitionName[0x13] = '\0';
      stPoolConfig.PartitionName[4] = '\0';
      stPoolConfig.PartitionName[5] = '\0';
      stPoolConfig.PartitionName[6] = '\0';
      stPoolConfig.PartitionName[7] = '\0';
      stPoolConfig.PartitionName[8] = '\0';
      stPoolConfig.PartitionName[9] = '\0';
      stPoolConfig.PartitionName[10] = '\0';
      stPoolConfig.PartitionName[0xb] = '\0';
      stPoolConfig.PartitionName[0x1c] = '\0';
      stPoolConfig.PartitionName[0x1d] = '\0';
      stPoolConfig.PartitionName[0x1e] = '\0';
      stPoolConfig.PartitionName[0x1f] = '\0';
      stPoolConfig.PoolName[0] = '\0';
      stPoolConfig.PoolName[1] = '\0';
      stPoolConfig.PoolName[2] = '\0';
      stPoolConfig.PoolName[3] = '\0';
      stPoolConfig.PartitionName[0x14] = '\0';
      stPoolConfig.PartitionName[0x15] = '\0';
      stPoolConfig.PartitionName[0x16] = '\0';
      stPoolConfig.PartitionName[0x17] = '\0';
      stPoolConfig.PartitionName[0x18] = '\0';
      stPoolConfig.PartitionName[0x19] = '\0';
      stPoolConfig.PartitionName[0x1a] = '\0';
      stPoolConfig.PartitionName[0x1b] = '\0';
      stPoolConfig.PoolName[0xc] = '\0';
      stPoolConfig.PoolName[0xd] = '\0';
      stPoolConfig.PoolName[0xe] = '\0';
      stPoolConfig.PoolName[0xf] = '\0';
      stPoolConfig.PoolName[0x10] = '\0';
      stPoolConfig.PoolName[0x11] = '\0';
      stPoolConfig.PoolName[0x12] = '\0';
      stPoolConfig.PoolName[0x13] = '\0';
      stPoolConfig.PoolName[4] = '\0';
      stPoolConfig.PoolName[5] = '\0';
      stPoolConfig.PoolName[6] = '\0';
      stPoolConfig.PoolName[7] = '\0';
      stPoolConfig.PoolName[8] = '\0';
      stPoolConfig.PoolName[9] = '\0';
      stPoolConfig.PoolName[10] = '\0';
      stPoolConfig.PoolName[0xb] = '\0';
      stPoolConfig.MetaSize = 0x200;
      stPoolConfig.BlkSize = uBlkSize;
      stPoolConfig.BlkCnt = u32BlkCnt;
      strcpy((char*)stPoolConfig.PartitionName,"anonymous");
      nPoolId = AX_POOL_CreatePool(&stPoolConfig);
      pCurVoLayer->u32LayerPoolId = nPoolId;
      if (nPoolId == 0xffffffff) {
        maix::log::info("AX_POOL_CreatePool failed, u32BlkCnt = %d, u64BlkSize = 0x%llx, u64MetaSize = 0x%llx\n"
                        ,u32BlkCnt,uBlkSize,0x200);
        uErrIdx = uVoLayIndex;
        pcError = "SAMPLE_VO_CREATE_POOL failed, i:%d, s32Ret:0x%x\n";
        uError = 0xffffffff;
        goto vo_start_failed;
      }
      maix::log::info("u32BlkCnt = %d, u64BlkSize = 0x%llx, pPoolID = %d\n",u32BlkCnt,uBlkSize,
                      nPoolId);
      (pCurVoLayer->stVoLayerAttr).u32FifoDepth = pCurVoLayer->u32FifoDepth;
      (pCurVoLayer->stVoLayerAttr).u32PoolId = pCurVoLayer->u32LayerPoolId;
      pCurVoLayer = pCurVoLayer + 1;
    }
    pVoGraphic = &voMod->vo_param.vo_cfg.stGraphicLayer[0];
    for (uVoDevIndex = 0; uVoDevIndex < voMod->vo_param.vo_cfg.u32VDevNr; uVoDevIndex = uVoDevIndex + 1
        ) {
      if (pVoGraphic->u32FbNum != 0) {
        uint64_t uFbIndex = 0;
        pVoGraphic->bindVoDev = pVoDev[uVoDevIndex].u32VoDev;
        do {
          s32Ret = (AX_S32)__sample_fb_config((pstVoConfig->vo_cfg).stGraphicLayer[uVoDevIndex].stFbConf + uFbIndex);
          uError = s32Ret;
          if (s32Ret != 0) {
            pcError = "SAMPLE_VO_FB_INIT failed, s32Ret:0x%x\n";
            goto failed;
          }
          uFbIndex = uFbIndex + 1;
        } while (uFbIndex < pVoGraphic->u32FbNum);
      }
      pVoGraphic = pVoGraphic + 1;
    }
    s32Ret = SAMPLE_COMM_VO_StartVO(&pstVoConfig->vo_cfg);
    uError = s32Ret;
    if (s32Ret == 0) {
      s32Ret = AX_IVPS_Init();
      uError = s32Ret;
      if (s32Ret == 0) {
        memset(&voMod->stGrpAttr,0,sizeof(voMod->stGrpAttr));
        voMod->stGrpAttr.nInFifoDepth = 2;
        IvpsGrp = voMod->nGrpId;
        voMod->stGrpAttr.ePipeline = AX_IVPS_PIPELINE_DEFAULT;
        s32Ret = AX_IVPS_CreateGrp(IvpsGrp,&voMod->stGrpAttr);
        uError = s32Ret;
        if (s32Ret == 0) {
          voMod->nChnNum = 1;
          memset(&voMod->stPipelineAttr,0,sizeof(voMod->stPipelineAttr));
          ptPipelineAttr = &voMod->stPipelineAttr;
          ptPipelineAttr->tFilter[1][0].bEngage = AX_TRUE;
          ptPipelineAttr->tFilter[1][0].eEngine = AX_IVPS_ENGINE_TDP;
          u16DstWidth = (AX_U16)(param->vo_cfg).stVoLayer[0].stVoLayerAttr.stImageSize.u32Width;
          u16DstHeight = (AX_U16)(param->vo_cfg).stVoLayer[0].stVoLayerAttr.stImageSize.u32Height;
          voMod->stPipelineAttr.tFilter[1][0].nDstPicWidth = u16DstWidth;
          voMod->init_count2 = 1;
          voMod->stPipelineAttr.tFilter[1][0].nDstPicHeight = u16DstHeight;
          voMod->stPipelineAttr.tFilter[1][0].nDstPicStride = (u16DstWidth + 0xf) & 0xfff0;
          voMod->stPipelineAttr.tFilter[1][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR
          ;
          goto success;
        }
        pcError = "AX_IVPS_CreateGrp failed, s32Ret = 0x%x\n";
      }
      else {
        pcError = "AX_IVPS_Init failed, s32Ret = 0x%x\n";
      }
      goto failed;
    }
    uErrIdx = uVoDevIndex & 0xffffffff;
    pcError = "SAMPLE_COMM_VO_StartVO failed, i:%d, s32Ret:0x%x\n";
vo_start_failed:
    maix::log::error(pcError,uErrIdx,uError);
  }
  else {
    pcError = "AX_VO_Init failed, s32Ret = 0x%x\n";
failed:
    maix::log::error(pcError,uError);
  }
  uErr = err::ERR_RUNTIME;
done:
  axMod.unlock(AX_MOD_VO);
  return uErr;
}


// VO::get_unused_channel(int)

int VO::get_unused_channel(int layer)

{
  int iRet;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  if (voMod->used_channels[layer][0] == false) {
    iRet = 0;
  }
  else if (voMod->used_channels[layer][1] == false) {
    iRet = 1;
  }
  else {
    iRet = 2;
    if (voMod->used_channels[layer][2] != false) {
      iRet = -1;
    }
  }
  axMod.unlock(AX_MOD_VO);
  return iRet;
}



// VO::get_channel_param(int, int,
// ax_vo_channel_param_t*)

err::Err VO::get_channel_param(int layer, int ch, ax_vo_channel_param_t *param)

{
  ax_vo_channel_param_t *voParam;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  voParam = voMod->channel_param[layer] + ch;
  param->format_in = voParam->format_in;
  param->format_out = voParam->format_out;
  param->width = voParam->width;
  param->height = voParam->height;
  param->mirror = voParam->mirror;
  param->vflip = voParam->vflip;
  param->fps = voParam->fps;
  param->depth = voParam->depth;
  param->pool_num_in = voParam->pool_num_in;
  param->pool_num_out = voParam->pool_num_out;
  param->fit = voParam->fit;
  param->rotate = voParam->rotate;
  axMod.unlock(AX_MOD_VO);
  return err::ERR_NONE;
}


// VO::add_channel(int, int,
// ax_vo_channel_param_t*)

err::Err VO::add_channel(int layer, int ch, ax_vo_channel_param_t *param)

{
  int FilterChn;
  ax_vo_channel_param_t *pChnParam;
  AX_U16 u16DstWidth;
  AX_U16 u16DstHeight;
  IVPS_GRP nGrpId;
  uint32_t uCurIvpsGrp;
  AX_U16 u16Tmp;
  int voMirror;
  int voFlip;
  int voFps;
  int voDepth;
  bool bParamFlip;
  bool bGlobalMirror;
  AX_S32 s32Ret;
  AX_S32 nChnNum;
  AX_S32 s32Rotate;
  bool bGlobalFlip;
  char *pcError;
  uint64_t uError;
  err::Err uErr;
  IVPS_CHN IvpsChn;
  uint64_t uCurIvpsChn;
  AX_MOD_INFO_T tModSrc;
  AX_MOD_INFO_T tModDst;
  char caFbDevPath [32];

  uCurIvpsChn = layer;
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  if (ch < 3) {
    if (voMod->used_channels[layer][ch] == false) {
      voMirror = param->mirror;
      voFlip = param->vflip;
      voFps = param->fps;
      voDepth = param->depth;
      pChnParam = voMod->channel_param[layer] + ch;
      pChnParam->format_in = param->format_in;
      pChnParam->format_out =  param->format_out;
      pChnParam->width = param->width;
      pChnParam->height = param->height;
      pChnParam->mirror = voMirror;
      pChnParam->vflip = voFlip;
      pChnParam->fps = voFps;
      pChnParam->depth = voDepth;
      pChnParam->pool_num_in = param->pool_num_in;
      pChnParam->pool_num_out = param->pool_num_out;
      pChnParam->fit = param->fit;
      pChnParam->rotate = param->rotate;
      if (layer == 1) {
        int fbFd;
        snprintf(caFbDevPath,sizeof(caFbDevPath),"/dev/fb%d",(uint32_t)ch);
        fbFd = open(caFbDevPath, O_NONBLOCK | O_RDWR);
        if (-1 < fbFd) {
          voMod->fb_fd[ch] = fbFd;
          goto success;
        }
        maix::log::error("open %s failed",caFbDevPath);
      }
      else {
        if (layer == 2) {
success:
          voMod->used_channels[layer][ch] = true;
          uErr = err::ERR_NONE;
          goto done;
        }
        if (layer != 0) {
          pcError = "invalid layer %d";
          goto failed;
        }
        bParamFlip = param->vflip != 0;
        bGlobalMirror = param->mirror != 0;
        bGlobalFlip = bParamFlip;
        if (param->rotate == 0x5a || param->rotate == 0x10e) {
          bGlobalFlip = bGlobalMirror;
          bGlobalMirror = bParamFlip;
        }
        voMod->global_mirror = bGlobalMirror;
        voMod->global_flip = bGlobalFlip;
        s32Ret = AX_IVPS_StopGrp(voMod->nGrpId);
        uCurIvpsChn = s32Ret;
        if (s32Ret == 0) {
ivps_loop:
          while( true ) {
            nGrpId = voMod->nGrpId;
            nChnNum = voMod->nChnNum;
            IvpsChn = (IVPS_CHN)uCurIvpsChn;
            if (nChnNum <= IvpsChn) break;
            s32Ret = AX_IVPS_DisableChn(nGrpId,IvpsChn);
            if (s32Ret != 0) {
              uCurIvpsGrp = voMod->nGrpId;
              pcError = "AX_IVPS_DisableChn failed,nGrp %d,nChn %d,s32Ret:0x%x";
              goto ivps_chn_error;
            }
            uCurIvpsChn = (IvpsChn + 1);
          }
          voMod->stPipelineAttr.nOutChnNum = (AX_U8)nChnNum;
          voMod->stPipelineAttr.tFilter[0][0].bEngage = AX_FALSE;
          voMod->stPipelineAttr.tFilter[0][0].eEngine = AX_IVPS_ENGINE_TDP;
          s32Rotate = param->rotate;
          u16DstWidth = (AX_U16)param->width;
          u16Tmp = (AX_U16)param->height;
          if (s32Rotate != 90 && s32Rotate != 270) {
            voMod->stPipelineAttr.tFilter[0][0].nDstPicWidth = (AX_U16)param->width;
            u16DstHeight = u16Tmp;
          }
          else {
            voMod->stPipelineAttr.tFilter[0][0].nDstPicWidth = (AX_U16)param->height;
            u16DstHeight = u16DstWidth;
          }
          uCurIvpsChn = 0;
          if (s32Rotate == 90 || s32Rotate == 270) {
            u16DstWidth = u16Tmp;
          }
          u16Tmp = voMod->stPipelineAttr.tFilter[0][0].nDstPicWidth;
          voMod->stPipelineAttr.tFilter[0][0].nDstPicHeight = u16DstHeight;
          voMod->stPipelineAttr.tFilter[0][0].nDstPicStride = (u16Tmp + 0xf) & 0xfff0;
          voMod->stPipelineAttr.tFilter[0][0].eDstPicFormat = AX_FORMAT_YUV420_SEMIPLANAR
          ;
          FilterChn = ch + 1;
          voMod->stPipelineAttr.tFilter[FilterChn][0].nDstPicWidth = u16DstWidth;
          voMod->stPipelineAttr.tFilter[FilterChn][0].nDstPicHeight = u16DstHeight;
          voMod->stPipelineAttr.tFilter[FilterChn][0].nDstPicStride =
               (voMod->stPipelineAttr.tFilter[1][0].nDstPicWidth + 0xf) & 0xfff0;
          voMod->stPipelineAttr.tFilter[FilterChn][0].tTdpCfg.eRotation =
               (AX_IVPS_ROTATION_E)(s32Rotate / 90);
          bGlobalFlip = voMod->global_flip;
          voMod->stPipelineAttr.tFilter[FilterChn][0].tTdpCfg.bMirror =
               (AX_BOOL)voMod->global_mirror;
          voMod->stPipelineAttr.tFilter[FilterChn][0].tTdpCfg.bFlip =
               (AX_BOOL)bGlobalFlip;
          s32Ret = AX_IVPS_SetPipelineAttr(nGrpId,&voMod->stPipelineAttr);
          uError = s32Ret;
          if (s32Ret == 0) {
ivps_chn_enable:
            nGrpId = voMod->nGrpId;
            if ((int)uCurIvpsChn < voMod->nChnNum) {
              if ((voMod->stPipelineAttr.tFilter[uCurIvpsChn + 1][0].bEngage == AX_FALSE)
                 || (s32Ret = AX_IVPS_EnableChn(nGrpId,(int)uCurIvpsChn), s32Ret == 0))
              goto ivps_chn_next;
              uCurIvpsGrp = voMod->nGrpId;
              uCurIvpsChn = uCurIvpsChn & 0xffffffff;
              pcError = "AX_IVPS_EnableChn failed,nGrp %d,nChn %d,s32Ret:0x%x";
ivps_chn_error:
              maix::log::error(pcError,uCurIvpsGrp,uCurIvpsChn,s32Ret);
              goto ivps_failed;
            }
            s32Ret = AX_IVPS_StartGrp(nGrpId);
            uError = s32Ret;
            if (s32Ret == 0) {
              tModSrc.enModId = AX_ID_IVPS;
              tModSrc.s32GrpId = voMod->nGrpId;
              tModDst.enModId = AX_ID_VO;
              tModDst.s32GrpId = 0;
              tModSrc.s32ChnId = ch;
              tModDst.s32ChnId = ch;
              s32Ret = AX_SYS_Link(&tModSrc,&tModDst);
              if (s32Ret != 0) {
                maix::log::error("AX_SYS_Link failed, ret:0x%x\n",s32Ret);
                goto ivps_failed;
              }
              goto success;
            }
            uCurIvpsChn = voMod->nGrpId;
            pcError = "AX_IVPS_StartGrp failed,nGrp %d,s32Ret:0x%x";
          }
          else {
            uCurIvpsChn = voMod->nGrpId;
            pcError = "AX_IVPS_SetPipelineAttr failed,nGrp %d,s32Ret:0x%x";
          }
        }
        else {
          uCurIvpsChn = voMod->nGrpId;
          if (s32Ret == AX_ERR_IVPS_NOT_PERM) {
            uCurIvpsChn = 0;
            goto ivps_loop;
          }
          uError = s32Ret;
          pcError = "AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x";
        }
        maix::log::error(pcError,uCurIvpsChn,uError);
      }
ivps_failed:
      uErr = err::ERR_RUNTIME;
      goto done;
    }
    uCurIvpsChn = ch;
    pcError = "channel %d already exist";
  }
  else {
    pcError = "channel %d is invalid";
    uCurIvpsChn = ch;
  }
failed:
  uErr = err::ERR_ARGS;
  maix::log::error(pcError,uCurIvpsChn);
done:
  axMod.unlock(AX_MOD_VO);
  return uErr;
ivps_chn_next:
  uCurIvpsChn = uCurIvpsChn + 1;
  goto ivps_chn_enable;
}


// VO::del_channel(int, int)

err::Err VO::del_channel(int layer, int ch)

{
  AX_S32 s32Ret;
  int fbFd;
  void *__s;
  err::Err uErr;
  uint64_t uCurIvpsChn;
  AX_MOD_INFO_T tSrcMod;
  AX_MOD_INFO_T tDstMod;
  fb_fix_screeninfo stFbFixInfo;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  if (layer == 1) {
    s32Ret = ioctl(voMod->fb_fd[ch],FBIOGET_FSCREENINFO,&stFbFixInfo);
    if (s32Ret < 0) {
      uErr = err::ERR_RUNTIME;
      maix::log::error("get fix screen info from fb%d failed\n",
                       voMod->fb_fd[ch]);
    }
    else {
      uErr = err::ERR_NONE;
    }
    __s = mmap(NULL, stFbFixInfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED ,voMod->fb_fd[ch],0);
    if (__s == (void *)0xffffffffffffffff) {
      uErr = err::ERR_RUNTIME;
      maix::log::error("map fb%d failed\n",voMod->fb_fd[ch]);
    }
    memset(__s,0,stFbFixInfo.smem_len);
    munmap(__s,stFbFixInfo.smem_len);
    fbFd = voMod->fb_fd[ch];
    if (2 < fbFd) {
      close(fbFd);
      voMod->fb_fd[ch] = -1;
    }
  }
  else {
    if (layer != 2) {
      if (layer != 0) {
        uErr = err::ERR_ARGS;
        maix::log::error("invalid layer %d",(uint32_t)layer);
        goto done;
      }
      tSrcMod.enModId = AX_ID_IVPS;
      tSrcMod.s32GrpId = voMod->nGrpId;
      tDstMod.enModId = AX_ID_VO;
      tDstMod.s32GrpId = 0;
      tSrcMod.s32ChnId = ch;
      tDstMod.s32ChnId = ch;
      s32Ret = AX_SYS_UnLink(&tSrcMod,&tDstMod);
      if (s32Ret != 0) {
        maix::log::error("AX_SYS_UnLink failed, ret:0x%x\n",s32Ret);
      }
      s32Ret = AX_IVPS_StopGrp(voMod->nGrpId);
      if (s32Ret == 0) {
        for (uCurIvpsChn = 0; (int)uCurIvpsChn < voMod->nChnNum; uCurIvpsChn = uCurIvpsChn + 1) {
          if (voMod->stPipelineAttr.tFilter[uCurIvpsChn + 1][0].bEngage != AX_FALSE) {
            s32Ret = AX_IVPS_DisableChn(voMod->nGrpId,(int)uCurIvpsChn);
            if (s32Ret != 0) {
              maix::log::error("AX_IVPS_EnableChn failed,nGrp %d,nChn %d,s32Ret:0x%x",
                               voMod->nGrpId,uCurIvpsChn & 0xffffffff,s32Ret
                              );
            }
          }
        }
      }
      else {
        maix::log::error("AX_IVPS_StopGrp failed,nGrp %d,s32Ret:0x%x",
                         voMod->nGrpId,s32Ret);
      }
      s32Ret = AX_VO_ClearChnBuf(0,ch,AX_TRUE);
      if (s32Ret != 0) {
        maix::log::error("AX_VO_ClearChnBuf failed,nLayer %d,nChn %d,s32Ret:0x%x",0,ch,
                         s32Ret);
      }
    }
    uErr = err::ERR_NONE;
  }
done:
  voMod->used_channels[layer][ch] = false;
  axMod.unlock(AX_MOD_VO);
  return uErr;
}


// VO::del_channel_all()

err::Err VO::del_channel_all()

{
  int iRet;
  uint64_t uCurChn;
  uint32_t layer;
  err::Err uErr;

  layer = 0;
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  uErr = err::ERR_NONE;
  do {
    uCurChn = 0;
    do {
      if ((voMod->used_channels[layer][uCurChn] != false) && (iRet = del_channel(layer,(int)uCurChn), iRet != 0))
      {
        uErr = err::ERR_RUNTIME;
        maix::log::error("delete layer %d channel %d failed!",layer,uCurChn & 0xffffffff);
      }
      uCurChn = uCurChn + 1;
    } while (uCurChn != 3);
    layer = layer + 1;
  } while (layer != 3);
  axMod.unlock(AX_MOD_VO);
  return uErr;
}


// VO::deinit()

void VO::deinit()

{
  int initCount2;
  AX_S32 s32Ret;
  uint64_t uCurLayer;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  initCount2 = voMod->init_count2;
  if (initCount2 < 2) {
    if (initCount2 == 1) {
      axMod.unlock(AX_MOD_VO);
      del_channel_all();
      axMod.lock(AX_MOD_VO);
      s32Ret = AX_IVPS_DestoryGrp(voMod->nGrpId);
      if (s32Ret != 0) {
        maix::log::error("AX_VO_DestoryGrp failed, s32Ret:0x%x\n",s32Ret);
      }
      s32Ret = AX_IVPS_Deinit();
      if (s32Ret != 0) {
        maix::log::error("AX_IVPS_Deinit failed, s32Ret:0x%x\n",s32Ret);
      }
      s32Ret = SAMPLE_COMM_VO_StopVO(&voMod->vo_param.vo_cfg);
      if (s32Ret != 0) {
        maix::log::error("SAMPLE_COMM_VO_StopVO failed, s32Ret:0x%x\n",s32Ret);
      }
      for (uCurLayer = 0; uCurLayer < voMod->vo_param.vo_cfg.u32LayerNr; uCurLayer = uCurLayer + 1)
      {
        AX_POOL_DestroyPool(voMod->vo_param.vo_cfg.stVoLayer[uCurLayer].u32LayerPoolId);
      }
      s32Ret = AX_VO_Deinit();
      if (s32Ret != 0) {
        maix::log::error("AX_VO_Deinit failed, s32Ret:0x%x\n",s32Ret);
      }
      voMod->init_count2 = 0;
    }
  }
  else {
    voMod->init_count2 = initCount2 - 1;
  }
  axMod.unlock(AX_MOD_VO);
}


// VO::~VO()

VO::~VO()

{
  int initCount;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  initCount = voMod->init_count;
  if (initCount < 2) {
    axMod.unlock(AX_MOD_VO);
    deinit();
    axMod.lock(AX_MOD_VO);
    initCount = 0;
  }
  else {
    initCount = initCount - 1;
  }
  voMod->init_count = initCount;
  axMod.unlock(AX_MOD_VO);
}


// VO::push(int, int, Frame*)

err::Err VO::push(int layer, int ch, maixcam2::Frame *frame)

{
  AX_IVPS_ROTATION_E eRotation;
  uint32_t u32Tmp;
  int iRet;
  char *pcError;
  void *pFbVirAddr;
  AX_IVPS_CHN_FLIP_MODE_E eFlipMode;
  err::Err uErr;
  AX_U64 uPhyAddr;
  AX_VOID *pVirAddr;
  fb_fix_screeninfo stFbFixInfo;
  AX_VIDEO_FRAME_T stVideoFrame;
  axVIDEO_FRAME_T stEmptyFrame;
  axVIDEO_FRAME_T stDstVideoFrame;
  AX_U64 pDstVirAddr;
  AX_U32 u32FrameSize;
  AX_U64 uDstPhyAddr;

  if (frame == (Frame *)0x0) {
    return err::ERR_ARGS;
  }
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vo_mod_t *voMod = (ax_vo_mod_t *)axMod.get_param(AX_MOD_VO);
  axMod.lock(AX_MOD_VO);
  if (layer == 1) {
    memset(&stVideoFrame,0,sizeof(stVideoFrame));
    memset(&stEmptyFrame,0,sizeof(stEmptyFrame));
    iRet = ioctl(voMod->fb_fd[ch],FBIOGET_FSCREENINFO,&stFbFixInfo);
    if (iRet < 0) {
      uErr = err::ERR_RUNTIME;
      maix::log::error("get fix screen info from fb%d failed\n",
                       voMod->fb_fd[ch]);
    }
    else {
      uErr = err::ERR_NONE;
    }
    frame->get_video_frame(&stVideoFrame);
    u32Tmp = (uint32_t)voMod->global_flip;
    if (voMod->global_mirror == false) {
      eFlipMode = (AX_IVPS_CHN_FLIP_MODE_E)(u32Tmp << 1);
    }
    else {
      eFlipMode = AX_IVPS_CHN_FLIP_AND_MIRROR;
      if (u32Tmp == 0) {
        eFlipMode = AX_IVPS_CHN_FLIP;
      }
    }
    uPhyAddr = 0;
    pVirAddr = (AX_VOID *)0x0;
    eRotation = (AX_IVPS_ROTATION_E)(voMod->channel_param[1][ch].rotate / 0x5a);
    memcpy(&stDstVideoFrame,&stEmptyFrame,sizeof(stEmptyFrame));
    u32FrameSize = stVideoFrame.u32Width;
    if ((eRotation & ~AX_IVPS_ROTATION_180) != AX_IVPS_ROTATION_90) {
      u32FrameSize = stVideoFrame.u32Height;
      stVideoFrame.u32Height = stVideoFrame.u32Width;
    }
    u32FrameSize = SAMPLE_CALC_IMAGE_SIZE
                             (stVideoFrame.u32Height,u32FrameSize,stVideoFrame.enImgFormat,
                              stVideoFrame.u32Height);
    u32Tmp = AX_SYS_MemAllocCached(&uPhyAddr,&pVirAddr,u32FrameSize,0x1000,(AX_S8 *)"vpp crop reisze");
    if (u32Tmp == 0) {
      AX_SYS_MflushCache(stVideoFrame.u64PhyAddr[0],(AX_VOID *)stVideoFrame.u64VirAddr[0],
                         stVideoFrame.u32FrameSize);
      stDstVideoFrame.enImgFormat = stVideoFrame.enImgFormat;
      stDstVideoFrame.u64PhyAddr[0] = uPhyAddr;
      stDstVideoFrame.u64VirAddr[0] = (AX_U64)pVirAddr;
      stDstVideoFrame.u32FrameSize = u32FrameSize;
      u32Tmp = AX_IVPS_FlipAndRotationTdp(&stVideoFrame,eFlipMode,eRotation,&stDstVideoFrame);
      if (u32Tmp == 0) {
        AX_SYS_MinvalidateCache(uPhyAddr,pVirAddr,stDstVideoFrame.u32FrameSize);
        u32FrameSize = stDstVideoFrame.u32FrameSize;
        pDstVirAddr = stDstVideoFrame.u64VirAddr[0];
        uDstPhyAddr = stDstVideoFrame.u64PhyAddr[0];
        pFbVirAddr = mmap(NULL, stFbFixInfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED ,voMod->fb_fd[ch],0)
        ;
        if (pFbVirAddr == (void *)0xffffffffffffffff) {
          uErr = err::ERR_RUNTIME;
          maix::log::error("map fb%d failed\n",voMod->fb_fd[ch]);
        }
        memcpy(pFbVirAddr,(void *)pDstVirAddr,(ulong)u32FrameSize);
        AX_SYS_MemFree(uDstPhyAddr,(AX_VOID *)pDstVirAddr);
        munmap(pFbVirAddr,stFbFixInfo.smem_len);
        goto done;
      }
      AX_SYS_MemFree(uPhyAddr,pVirAddr);
      pcError = "AX_IVPS_FlipAndRotationTdp failed, ret = %#x";
    }
    else {
      pcError = "AX_SYS_MemAllocCached failed, ret = %#x";
    }
    maix::log::error(pcError,u32Tmp);
    maix::log::error("__ax_ivps_flip_rotation_tdp failed\n");
failed:
    uErr = err::ERR_RUNTIME;
  }
  else {
    if (layer != 2) {
      if (layer != 0) {
        uErr = err::ERR_ARGS;
        maix::log::error("invalid layer %d",(uint32_t)layer);
        goto done;
      }
      memset(&stDstVideoFrame,0,sizeof(stDstVideoFrame));
      frame->get_video_frame(&stDstVideoFrame);
      u32Tmp = AX_IVPS_SendFrame(voMod->nGrpId,&stDstVideoFrame,-1);
      if (u32Tmp != 0) {
        maix::log::error("layer%d-chn%d AX_IVPS_SendFrame failed, s32Ret = 0x%x\n",0,ch
                         ,u32Tmp);
        goto failed;
      }
    }
    uErr = err::ERR_NONE;
  }
done:
  axMod.unlock(AX_MOD_VO);
  return uErr;
}


// VENC::VENC(ax_venc_param_t*)

VENC::VENC(ax_venc_param_t *cfg)

{
  bool bEn;
  int initCount;
  int s32Tmp;
  AX_S32 s32Ret;
  long lCurChn;
  AX_VENC_RECV_PIC_PARAM_T stVencRecvParam [2];
  AX_VENC_MOD_ATTR_T stVencModAttr;
  AX_U32 u32Gop;
  AX_U32 u32BitRate;
  AX_U32 u32MaxQp;
  AX_U32 u32MinQp;
  AX_U32 u32MaxIQp;
  AX_U32 u32MinIQp;
  AX_U32 u32MaxIprop;
  AX_U32 u32MinIprop;
  AX_U32 u32IdrQpDeltaRange;
  AX_VENC_QPMAP_META_T stQpmapInfo;
  AX_VENC_CHN_ATTR_T stVencChnAttr;
  ax_venc_param_t *pVencParam;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_venc_mod_t *vencMod = (ax_venc_mod_t *)axMod.get_param(AX_MOD_VENC);
  axMod.lock(AX_MOD_VENC);
  initCount = vencMod->init_count;
  if (initCount < 1) {
    stVencModAttr.stModThdAttr.enSchedPolicy = AX_VENC_SCHED_OTHER;
    stVencModAttr.stModThdAttr.u32SchedPriority = 0;
    stVencModAttr.enVencType = AX_VENC_MULTI_ENCODER;
    stVencModAttr.stModThdAttr.bExplicitSched = AX_FALSE;
    stVencModAttr.stModThdAttr.u32TotalThreadNum = 1;
    s32Ret = AX_VENC_Init(&stVencModAttr);
    if (s32Ret != 0) {
      axMod.unlock(AX_MOD_VENC);
      maix::log::info(" venc init failed, ret:%#x",s32Ret);
      maix::err::check_raise(err::ERR_RUNTIME,"venc init failed");
    }
    initCount = 1;
  }
  else {
    initCount = initCount + 1;
  }
  vencMod->init_count = initCount;
  if (cfg == (ax_venc_param_t *)0x0) {
    this->_ch = -1;
  }
  else {
    lCurChn = 0;
    do {
      bEn = vencMod->venc[lCurChn].en;
      if (bEn == false) goto venc_init;
      lCurChn = lCurChn + 1;
    } while (lCurChn != 0x10);
    axMod.unlock(AX_MOD_VENC);
    maix::err::check_raise(err::ERR_RUNTIME,"venc channel is full");
    lCurChn = 0xffffffff;
venc_init:
    this->_ch = (int)lCurChn;
    memset(&stVencChnAttr,0,sizeof(stVencChnAttr));
    stVencChnAttr.stVencAttr.u32PicWidthSrc = cfg->w;
    stVencChnAttr.stVencAttr.u32PicHeightSrc = cfg->h;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = cfg->w;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = cfg->h;
    stVencChnAttr.stVencAttr.u8InFifoDepth = 4;
    stVencChnAttr.stVencAttr.u8OutFifoDepth = 4;
    stVencChnAttr.stVencAttr.u32BufSize =
         (int)(stVencChnAttr.stVencAttr.u32PicWidthSrc * stVencChnAttr.stVencAttr.u32PicHeightSrc *
              3) / 2;
    switch(cfg->type) {
    case AX_VENC_TYPE_JPG:
      stVencChnAttr.stVencAttr.enType = PT_JPEG;
      stVencChnAttr.stVencAttr.u32BufSize = 0;
      stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = cfg->jpg.input_fps;
      stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = cfg->jpg.output_fps;
      break;
    case AX_VENC_TYPE_H264:
      stVencChnAttr.stVencAttr.enType = PT_H264;
      stVencChnAttr.stVencAttr.enProfile = AX_VENC_H264_MAIN_PROFILE;
      stVencChnAttr.stVencAttr.enLevel = AX_VENC_H264_LEVEL_5_2;
      stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = cfg->h264.input_fps;
      stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_H264CBR;
      stVencChnAttr.stRcAttr.s32FirstFrameStartQp = cfg->h264.first_frame_start_qp;
      u32BitRate = cfg->h264.bitrate;
      u32MinQp = cfg->h264.min_qp;
      stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = cfg->h264.output_fps;
      u32MaxQp = cfg->h264.max_qp;
      u32MinIQp = cfg->h264.min_iqp;
      u32MinIprop = cfg->h264.min_iprop;
      u32MaxIprop = cfg->h264.max_iprop;
      u32MaxIQp = cfg->h264.max_iqp;
      u32IdrQpDeltaRange = 0;
      stQpmapInfo.enQpmapBlockType = AX_VENC_QPMAP_BLOCK_DISABLE;
      stQpmapInfo.enQpmapBlockUnit = AX_VENC_QPMAP_BLOCK_UNIT_64x64;
      stQpmapInfo.enCtbRcMode = AX_VENC_RC_CTBRC_DISABLE;
      stQpmapInfo.enQpmapQpType = AX_VENC_QPMAP_QP_DISABLE;
      u32Gop = cfg->h264.gop;
      stVencChnAttr.stRcAttr.stH264Cbr.u32BitRate = u32BitRate;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxQp = u32MaxQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32Gop = u32Gop;
      stVencChnAttr.stRcAttr.stH264Cbr.u32StatTime = 0;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinIQp = u32MinIQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIprop = u32MaxIprop;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinQp = u32MinQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIQp = u32MaxIQp;
      stVencChnAttr.stRcAttr.stH264Cbr.s32DeBreathQpDelta = cfg->h264.de_breath_qp_delta;
      stVencChnAttr.stRcAttr.stH264Cbr.s32IntraQpDelta = cfg->h264.intra_qp_delta;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinIprop = u32MinIprop;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockType = AX_VENC_QPMAP_BLOCK_DISABLE;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockUnit = AX_VENC_QPMAP_BLOCK_UNIT_64x64
      ;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enCtbRcMode = AX_VENC_RC_CTBRC_DISABLE;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapQpType = AX_VENC_QPMAP_QP_DISABLE;
      break;
    case AX_VENC_TYPE_H265:
      stVencChnAttr.stVencAttr.enType = PT_H265;
      stVencChnAttr.stVencAttr.enLevel = AX_VENC_HEVC_LEVEL_5_1;
      stVencChnAttr.stRcAttr.stFrameRate.fSrcFrameRate = cfg->h265.input_fps;
      u32BitRate = cfg->h265.bitrate;
      u32MinQp = cfg->h264.min_qp;
      stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_H265CBR;
      u32MaxQp = cfg->h264.max_qp;
      u32MinIQp = cfg->h264.min_iqp;
      u32MinIprop = cfg->h264.min_iprop;
      u32MaxIprop = cfg->h264.max_iprop;
      u32Gop = cfg->h265.gop;
      stVencChnAttr.stRcAttr.stFrameRate.fDstFrameRate = cfg->h265.output_fps;
      stVencChnAttr.stRcAttr.s32FirstFrameStartQp = cfg->h265.first_frame_start_qp;
      u32MaxIQp = cfg->h264.max_iqp;
      u32IdrQpDeltaRange = cfg->h265.qp_delta_rgn;
      stQpmapInfo.enQpmapQpType = cfg->h265.qp_map_type;
      stQpmapInfo.enCtbRcMode = cfg->h265.ctb_rc_mode;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinIQp = u32MinIQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIprop = u32MaxIprop;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinQp = u32MinQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxIQp = u32MaxIQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32BitRate = u32BitRate;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MaxQp = u32MaxQp;
      stVencChnAttr.stRcAttr.stH264Cbr.u32Gop = u32Gop;
      stVencChnAttr.stRcAttr.stH264Cbr.u32StatTime = 0;
      stVencChnAttr.stRcAttr.stH264Cbr.u32IdrQpDeltaRange = u32IdrQpDeltaRange;
      stVencChnAttr.stRcAttr.stH264Cbr.s32DeBreathQpDelta = cfg->h265.de_breath_qp_delta;
      stVencChnAttr.stRcAttr.stH264Cbr.s32IntraQpDelta = cfg->h265.intra_qp_delta;
      stVencChnAttr.stRcAttr.stH264Cbr.u32MinIprop = u32MinIprop;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enCtbRcMode = stQpmapInfo.enCtbRcMode;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapQpType = stQpmapInfo.enQpmapQpType;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockType = cfg->h265.qp_map_blk_type;
      stVencChnAttr.stRcAttr.stH264Cbr.stQpmapInfo.enQpmapBlockUnit = cfg->h265.qp_map_block_unit;
      break;
    case AX_VENC_TYPE_MJPG:
      stVencChnAttr.stRcAttr.enRcMode = AX_VENC_RC_MODE_MJPEGCBR;
      stVencChnAttr.stRcAttr.s32FirstFrameStartQp = cfg->mjpg.first_frame_start_qp;
      stVencChnAttr.stRcAttr.stMjpegCbr.u32StatTime = cfg->mjpg.stat_time;
      stVencChnAttr.stRcAttr.stMjpegCbr.u32BitRate = cfg->mjpg.bitrate;
      stVencChnAttr.stRcAttr.stMjpegCbr.u32MinQp = cfg->mjpg.qp_min;
      stVencChnAttr.stRcAttr.stMjpegCbr.u32MaxQp = cfg->mjpg.qp_max;
      break;
    default:
      axMod.unlock(AX_MOD_VENC);
      maix::err::check_raise(err::ERR_RUNTIME,"venc type error");
    }
    s32Ret = AX_VENC_CreateChn(this->_ch,&stVencChnAttr);
    if (s32Ret != 0) {
      axMod.unlock(AX_MOD_VENC);
      maix::log::info("VencChn %d: AX_VENC_CreateChn failed, s32Ret:0x%x",this->_ch);
      maix::err::check_raise(err::ERR_RUNTIME,"venc create channel failed");
    }
    s32Ret = AX_VENC_StartRecvFrame(this->_ch,stVencRecvParam);
    if (s32Ret != 0) {
      axMod.unlock(AX_MOD_VENC);
      maix::log::error("AX_VENC_StartRecvFrame failed, ch:%d s32Ret:0x%x",this->_ch);
      maix::err::check_raise(err::ERR_RUNTIME,"start recv frame failed");
    }
    s32Tmp = this->_ch;
    pVencParam = vencMod->venc + s32Tmp;
    vencMod->venc[s32Tmp].w = cfg->w;
    vencMod->venc[s32Tmp].h = cfg->h;
    pVencParam->en = cfg->en;
    pVencParam->type = cfg->type;
    vencMod->venc[s32Tmp].h265.gop = (cfg->h265).gop;
    vencMod->venc[s32Tmp].h265.input_fps = (cfg->h265).input_fps;
    vencMod->venc[s32Tmp].fmt = cfg->fmt;
    vencMod->venc[s32Tmp].h265.first_frame_start_qp = (cfg->h265).first_frame_start_qp;
    vencMod->venc[s32Tmp].h265.min_qp = (cfg->h265).min_qp;
    vencMod->venc[s32Tmp].h265.max_qp = (cfg->h265).max_qp;
    vencMod->venc[s32Tmp].h265.output_fps = (cfg->h265).output_fps;
    vencMod->venc[s32Tmp].h265.bitrate = (cfg->h265).bitrate;
    vencMod->venc[s32Tmp].h265.intra_qp_delta = (cfg->h265).intra_qp_delta;
    vencMod->venc[s32Tmp].h265.de_breath_qp_delta = (cfg->h265).de_breath_qp_delta;
    vencMod->venc[s32Tmp].h265.min_iqp = (cfg->h265).min_iqp;
    vencMod->venc[s32Tmp].h265.max_iqp = (cfg->h265).max_iqp;
    vencMod->venc[s32Tmp].h265.qp_delta_rgn = (cfg->h265).qp_delta_rgn;
    vencMod->venc[s32Tmp].h265.qp_map_type = (cfg->h265).qp_map_type;
    vencMod->venc[s32Tmp].h265.min_iprop = (cfg->h265).min_iprop;
    vencMod->venc[s32Tmp].h265.max_iprop = (cfg->h265).max_iprop;
    vencMod->venc[s32Tmp].h265.qp_map_block_unit = (cfg->h265).qp_map_block_unit;
    vencMod->venc[s32Tmp].h265.ctb_rc_mode = (cfg->h265).ctb_rc_mode;
    vencMod->venc[s32Tmp].h265.qp_map_type = (cfg->h265).qp_map_type;
    vencMod->venc[s32Tmp].h265.qp_map_blk_type = (cfg->h265).qp_map_blk_type;
    vencMod->venc[this->_ch].en = true;
  }
  axMod.unlock(AX_MOD_VENC);
}



// VENC::~VENC()

VENC::~VENC()

{
  AX_S32 s32Ret;
  ax_venc_param_t *pVencParam;
  int initCount;
  int s32Tmp;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_venc_mod_t *vencMod = (ax_venc_mod_t *)axMod.get_param(AX_MOD_VENC);
  axMod.lock(AX_MOD_VENC);
  s32Tmp = this->_ch;
  if (-1 < s32Tmp) {
    if (vencMod->venc[s32Tmp].en != false) {
      s32Ret = AX_VENC_StopRecvFrame(s32Tmp);
      if (s32Ret != 0) {
        maix::log::error("AX_VENC_StopRecvFrame(%d) failed, ret=%#x",this->_ch,
                         s32Ret);
      }
      s32Ret = AX_VENC_DestroyChn(this->_ch);
      if (s32Ret != 0) {
        maix::log::error("AX_VENC_DestroyChn(%d) failed, ret=%#x\n",this->_ch,
                         s32Ret);
      }
    }
    s32Tmp = this->_ch;
    pVencParam = vencMod->venc + s32Tmp;
    vencMod->venc[s32Tmp].h265.qp_delta_rgn = 0;
    vencMod->venc[s32Tmp].h265.qp_map_type = AX_VENC_QPMAP_QP_DISABLE;
    vencMod->venc[s32Tmp].h265.min_iprop = 0;
    vencMod->venc[s32Tmp].h265.max_iprop = 0;
    vencMod->venc[s32Tmp].w = 0;
    vencMod->venc[s32Tmp].h = 0;
    pVencParam->en = false;
    pVencParam->type = AX_VENC_TYPE_JPG;
    vencMod->venc[s32Tmp].h265.gop = 0;
    vencMod->venc[s32Tmp].h265.input_fps = 0;
    vencMod->venc[s32Tmp].fmt = AX_FORMAT_YUV400;
    vencMod->venc[s32Tmp].h265.first_frame_start_qp = 0;
    vencMod->venc[s32Tmp].h265.min_qp = 0;
    vencMod->venc[s32Tmp].h265.max_qp = 0;
    vencMod->venc[s32Tmp].h265.output_fps = 0;
    vencMod->venc[s32Tmp].h265.bitrate = 0;
    vencMod->venc[s32Tmp].h265.intra_qp_delta = 0;
    vencMod->venc[s32Tmp].h265.de_breath_qp_delta = 0;
    vencMod->venc[s32Tmp].h265.min_iqp = 0;
    vencMod->venc[s32Tmp].h265.max_iqp = 0;
    vencMod->venc[s32Tmp].h265.qp_map_block_unit = AX_VENC_QPMAP_BLOCK_UNIT_64x64;
    vencMod->venc[s32Tmp].h265.ctb_rc_mode = AX_VENC_RC_CTBRC_DISABLE;
    vencMod->venc[s32Tmp].h265.qp_map_type = AX_VENC_QPMAP_QP_DISABLE;
    vencMod->venc[s32Tmp].h265.qp_map_blk_type = AX_VENC_QPMAP_BLOCK_DISABLE;
  }
  initCount = vencMod->init_count;
  if (initCount < 2) {
    s32Ret = AX_VENC_Deinit();
    if (s32Ret != 0) {
      maix::log::error("AX_VENC_Deinit failed, ret=%d\n",this->_ch,s32Ret);
    }
    initCount = 0;
  }
  else {
    initCount = initCount - 1;
  }
  vencMod->init_count = initCount;
  axMod.unlock(AX_MOD_VENC);
}



// ax_jpg_enc_deinit()

err::Err ax_jpg_enc_deinit(void)

{
  int initCount;
  VENC *vencMod;
  SYS *sysMod;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_jpg_mod_t *jpgMod = (ax_jpg_mod_t *)axMod.get_param(AX_MOD_JPG);
  axMod.lock(AX_MOD_JPG);
  initCount = jpgMod->init_count;
  axMod.unlock(AX_MOD_JPG);
  if (0 < initCount) {
    vencMod = jpgMod->venc;
    if (vencMod != (VENC *)0x0) {
      delete vencMod;
    }
    sysMod = jpgMod->sys;
    if (sysMod != (SYS *)0x0) {
      delete sysMod;
    }
    initCount = 0;
  }
  axMod.lock(AX_MOD_JPG);
  jpgMod->init_count = initCount;
  axMod.unlock(AX_MOD_JPG);
  return err::ERR_NONE;
}



// VENC::push(Frame*, int)

err::Err VENC::push(maixcam2::Frame *frame, int32_t timeout_ms)

{
  AX_S32 s32Ret;
  AX_VIDEO_FRAME_INFO_T *pstFrame;
  err::Err iErr;
  axVIDEO_FRAME_T stVideoFrame;
  axVIDEO_FRAME_T stOutFrame;

  if (frame == (Frame *)0x0) {
    iErr = err::ERR_ARGS;
  }
  else {
    memset(&stVideoFrame,0,sizeof(stVideoFrame));
    iErr = frame->get_video_frame(&stVideoFrame);
    if (iErr == err::ERR_NONE) {
      stVideoFrame.u64SeqNum = 1;
      stVideoFrame.u64PTS = 1000;
      pstFrame = (AX_VIDEO_FRAME_INFO_T *)memcpy(&stOutFrame,&stVideoFrame,sizeof(stOutFrame));
      s32Ret = AX_VENC_SendFrame(this->_ch,pstFrame,timeout_ms);
      if ((short)s32Ret != 0) {
        iErr = err::ERR_RUNTIME;
        maix::log::error("AX_VENC_SendFrame failed! ch:%d ret:%#x",this->_ch);
      }
    }
    else {
      maix::log::error("frame is invalid!");
    }
  }
  return iErr;
}



// VENC::pop(int)

maixcam2::Frame * VENC::pop(int32_t timeout_ms)

{
  AX_S32 s32Ret;
  Frame *pFrame;
  AX_VENC_STREAM_T stStream;

  memset(&stStream,0,sizeof(stStream));
  s32Ret = AX_VENC_GetStream(this->_ch,&stStream,timeout_ms);
  if (s32Ret == 0) {
    pFrame = new Frame(this->_ch,&stStream,FRAME_FROM_VENC_GET_STREAM);
  }
  else {
    pFrame = (Frame *)0x0;
  }
  return pFrame;
}



// VENC::get_config(ax_venc_param_t*)

err::Err VENC::get_config(ax_venc_param_t *cfg)

{
  ax_venc_param_t *pVencParam;
  int vencChn;

  if (cfg != (ax_venc_param_t *)0x0) {
    AxModuleParam &axMod = AxModuleParam::getInstance();
    ax_venc_mod_t *vencMod = (ax_venc_mod_t *)axMod.get_param(AX_MOD_VENC);
    axMod.lock(AX_MOD_VENC);
    vencChn = this->_ch;
    pVencParam = vencMod->venc + vencChn;
    cfg->w = vencMod->venc[vencChn].w;
    cfg->h = vencMod->venc[vencChn].h;
    cfg->en = pVencParam->en;
    cfg->type = pVencParam->type;
    (cfg->h265).gop = vencMod->venc[vencChn].h265.gop;
    (cfg->h265).input_fps = vencMod->venc[vencChn].h265.input_fps;
    cfg->fmt = vencMod->venc[vencChn].fmt;
    (cfg->h265).first_frame_start_qp = vencMod->venc[vencChn].h265.first_frame_start_qp;
    (cfg->h265).min_qp = vencMod->venc[vencChn].h265.min_qp;
    (cfg->h265).max_qp = vencMod->venc[vencChn].h265.max_qp;
    (cfg->h265).output_fps = vencMod->venc[vencChn].h265.output_fps;
    (cfg->h265).bitrate = vencMod->venc[vencChn].h265.bitrate;
    (cfg->h265).intra_qp_delta = vencMod->venc[vencChn].h265.intra_qp_delta;
    (cfg->h265).de_breath_qp_delta = vencMod->venc[vencChn].h265.de_breath_qp_delta;
    (cfg->h265).min_iqp = vencMod->venc[vencChn].h265.min_iqp;
    (cfg->h265).max_iqp = vencMod->venc[vencChn].h265.max_iqp;
    (cfg->h265).qp_delta_rgn = vencMod->venc[vencChn].h265.qp_delta_rgn;
    (cfg->h265).qp_map_type = vencMod->venc[vencChn].h265.qp_map_type;
    (cfg->h265).min_iprop = vencMod->venc[vencChn].h265.min_iprop;
    (cfg->h265).max_iprop = vencMod->venc[vencChn].h265.max_iprop;
    (cfg->h265).qp_map_block_unit = vencMod->venc[vencChn].h265.qp_map_block_unit;
    (cfg->h265).ctb_rc_mode = vencMod->venc[vencChn].h265.ctb_rc_mode;
    (cfg->h265).qp_map_type = vencMod->venc[vencChn].h265.qp_map_type;
    (cfg->h265).qp_map_blk_type = vencMod->venc[vencChn].h265.qp_map_blk_type;
    axMod.unlock(AX_MOD_VENC);
    return err::ERR_NONE;
  }
  return err::ERR_ARGS;
}



// VDEC::VDEC(ax_vdec_param_t*)

VDEC::VDEC(ax_vdec_param_t *cfg)

{
  uint32_t u32Width;
  uint32_t u32TmpSize;
  uint32_t u32BlkSize;
  AX_S32 s32Ret;
  char *pcLog;
  long curVdecChn;
  uint64_t uTmp;
  AX_PAYLOAD_TYPE_E vdecPayloadType;
  AX_VDEC_MOD_ATTR_T stVdecModAttr [2];
  AX_VDEC_RECV_PIC_PARAM_T stVdecRecvParam;
  AX_VDEC_GRP vdecGrp;
  AX_VDEC_GRP_ATTR_T stVdecGrpAttr;
  AX_POOL_CONFIG_T stPoolConfig;
  bool bEn;
  int blkCnt;
  int blkSize;
  int iHeight;
  AX_IMG_FORMAT_E imgFormat;
  ax_vdec_param_t *pVdecParam;
  AX_POOL poolId;
  int u32Tmp;
  ax_vdec_type_e vdecType;

  vdecGrp = -1;
  stPoolConfig.BlkSize = 0;
  stPoolConfig.MetaSize = 0;
  stPoolConfig.CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
  stPoolConfig.PartitionName[0] = '\0';
  stPoolConfig.PartitionName[1] = '\0';
  stPoolConfig.PartitionName[2] = '\0';
  stPoolConfig.PartitionName[3] = '\0';
  stPoolConfig.BlkCnt = 0;
  stPoolConfig.IsMergeMode = AX_FALSE;
  stPoolConfig.PartitionName[0xc] = '\0';
  stPoolConfig.PartitionName[0xd] = '\0';
  stPoolConfig.PartitionName[0xe] = '\0';
  stPoolConfig.PartitionName[0xf] = '\0';
  stPoolConfig.PartitionName[0x10] = '\0';
  stPoolConfig.PartitionName[0x11] = '\0';
  stPoolConfig.PartitionName[0x12] = '\0';
  stPoolConfig.PartitionName[0x13] = '\0';
  stPoolConfig.PartitionName[4] = '\0';
  stPoolConfig.PartitionName[5] = '\0';
  stPoolConfig.PartitionName[6] = '\0';
  stPoolConfig.PartitionName[7] = '\0';
  stPoolConfig.PartitionName[8] = '\0';
  stPoolConfig.PartitionName[9] = '\0';
  stPoolConfig.PartitionName[10] = '\0';
  stPoolConfig.PartitionName[0xb] = '\0';
  stPoolConfig.PartitionName[0x1c] = '\0';
  stPoolConfig.PartitionName[0x1d] = '\0';
  stPoolConfig.PartitionName[0x1e] = '\0';
  stPoolConfig.PartitionName[0x1f] = '\0';
  stPoolConfig.PoolName[0] = '\0';
  stPoolConfig.PoolName[1] = '\0';
  stPoolConfig.PoolName[2] = '\0';
  stPoolConfig.PoolName[3] = '\0';
  stPoolConfig.PartitionName[0x14] = '\0';
  stPoolConfig.PartitionName[0x15] = '\0';
  stPoolConfig.PartitionName[0x16] = '\0';
  stPoolConfig.PartitionName[0x17] = '\0';
  stPoolConfig.PartitionName[0x18] = '\0';
  stPoolConfig.PartitionName[0x19] = '\0';
  stPoolConfig.PartitionName[0x1a] = '\0';
  stPoolConfig.PartitionName[0x1b] = '\0';
  stPoolConfig.PoolName[0xc] = '\0';
  stPoolConfig.PoolName[0xd] = '\0';
  stPoolConfig.PoolName[0xe] = '\0';
  stPoolConfig.PoolName[0xf] = '\0';
  stPoolConfig.PoolName[0x10] = '\0';
  stPoolConfig.PoolName[0x11] = '\0';
  stPoolConfig.PoolName[0x12] = '\0';
  stPoolConfig.PoolName[0x13] = '\0';
  stPoolConfig.PoolName[4] = '\0';
  stPoolConfig.PoolName[5] = '\0';
  stPoolConfig.PoolName[6] = '\0';
  stPoolConfig.PoolName[7] = '\0';
  stPoolConfig.PoolName[8] = '\0';
  stPoolConfig.PoolName[9] = '\0';
  stPoolConfig.PoolName[10] = '\0';
  stPoolConfig.PoolName[0xb] = '\0';
  stPoolConfig.PoolName[0x1c] = '\0';
  stPoolConfig.PoolName[0x1d] = '\0';
  stPoolConfig.PoolName[0x1e] = '\0';
  stPoolConfig.PoolName[0x1f] = '\0';
  stPoolConfig.PoolName[0x14] = '\0';
  stPoolConfig.PoolName[0x15] = '\0';
  stPoolConfig.PoolName[0x16] = '\0';
  stPoolConfig.PoolName[0x17] = '\0';
  stPoolConfig.PoolName[0x18] = '\0';
  stPoolConfig.PoolName[0x19] = '\0';
  stPoolConfig.PoolName[0x1a] = '\0';
  stPoolConfig.PoolName[0x1b] = '\0';
  if ((0x780 < cfg->w) || (0x780 < cfg->h)) {
    maix::err::check_raise(err::ERR_ARGS,"vdec size is too large");
  }
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vdec_mod_t *vdecMod = (ax_vdec_mod_t *)axMod.get_param(AX_MOD_VDEC);
  axMod.lock(AX_MOD_VDEC);
  cfg->pool_id = 0xffffffff;
  curVdecChn = 0;
  do {
    bEn = vdecMod->vdec[curVdecChn].en;
    if (bEn == false) {
      this->_ch = (int)curVdecChn;
      u32Tmp = vdecMod->init_count;
      if (u32Tmp < 1) {
        stVdecModAttr[0].u32MaxGroupCount = 16;
        s32Ret = AX_VDEC_Init(stVdecModAttr);
        uTmp = s32Ret;
        if (s32Ret == 0) {
          u32Tmp = 1;
          goto vdec_init;
        }
        pcLog = " vdec init failed, ret:%#x";
      }
      else {
        u32Tmp = u32Tmp + 1;
vdec_init:
        vdecType = cfg->type;
        vdecMod->init_count = u32Tmp;
        if (vdecType == AX_VDEC_TYPE_JPG) {
          vdecPayloadType = PT_JPEG;
vdec_setup:
          stVdecGrpAttr.enLinkMode = AX_UNLINK_MODE;
          stVdecGrpAttr.enOutOrder = AX_VDEC_OUTPUT_ORDER_DISP;
          stVdecGrpAttr.u32PicWidth = cfg->w;
          stVdecGrpAttr.u32PicHeight = cfg->h;
          stVdecGrpAttr.u32FrameHeight = (cfg->h + 0xfU) & 0xfffffff0;
          stVdecGrpAttr.s32DestroyTimeout = 0;
          stVdecGrpAttr.u32StreamBufSize = ((cfg->w * cfg->h * 3) / 2 + 0xfU) & 0xfffffff0;
          stVdecGrpAttr.enInputMode = AX_VDEC_INPUT_MODE_FRAME;
          stVdecGrpAttr.enVdecVbSource = AX_POOL_SOURCE_USER;
          stVdecGrpAttr.u32FrameBufCnt = 8;
          stVdecGrpAttr.enCodecType = vdecPayloadType;
          s32Ret = AX_VDEC_CreateGrpEx(&vdecGrp,&stVdecGrpAttr);
          if (s32Ret == 0) {
            this->_ch = vdecGrp;
            maix::log::info("ax vdec create group successfully, ch:%d");
            stPoolConfig.MetaSize = 0x200;
            u32Tmp = cfg->blk_cnt;
            if (u32Tmp == 0) {
              u32Tmp = 8;
            }
            stPoolConfig.BlkCnt = u32Tmp;
            if (cfg->blk_size == 0) {
              u32BlkSize = cfg->h + 0xf;
              u32Width = cfg->w + 0xf;
              u32TmpSize = (u32BlkSize & 0xfffffff0) * (u32Width & 0xfffffff0) * 3;
              u32BlkSize = (u32BlkSize >> 4) * (u32Width >> 4) * 0x40 + 0x20 + (u32TmpSize >> 1);
              if (vdecPayloadType == PT_JPEG) {
                u32BlkSize = u32TmpSize;
              }
              stPoolConfig.BlkSize = (AX_U64)u32BlkSize;
            }
            else {
              stPoolConfig.BlkSize = (AX_U64)cfg->blk_size;
            }
            strcpy((char*)stPoolConfig.PartitionName,"anonymous");
            poolId = AX_POOL_CreatePool(&stPoolConfig);
            cfg->pool_id = poolId;
            if (poolId == 0xffffffff) {
              maix::log::info(" vdec create pool failed, BlkCnt:%d, BlkSize:0x%llx",
                              stPoolConfig.BlkCnt, stPoolConfig.BlkSize);
            }
            else {
              s32Ret = AX_VDEC_AttachPool(this->_ch,poolId);
              if (s32Ret == 0) {
                stVdecRecvParam.s32RecvPicNum = 0;
                s32Ret = AX_VDEC_StartRecvStream(this->_ch,&stVdecRecvParam);
                if (s32Ret == 0) {
                  u32Tmp = this->_ch;
                  iHeight = cfg->h;
                  bEn = cfg->en;
                  vdecType = cfg->type;
                  blkSize = cfg->blk_size;
                  poolId = cfg->pool_id;
                  imgFormat = cfg->fmt;
                  blkCnt = cfg->blk_cnt;
                  pVdecParam = vdecMod->vdec + u32Tmp;
                  vdecMod->vdec[u32Tmp].w = cfg->w;
                  vdecMod->vdec[u32Tmp].h = iHeight;
                  pVdecParam->en = bEn;
                  pVdecParam->type = vdecType;
                  vdecMod->vdec[u32Tmp].blk_size = blkSize;
                  vdecMod->vdec[u32Tmp].pool_id = poolId;
                  vdecMod->vdec[u32Tmp].fmt = imgFormat;
                  vdecMod->vdec[u32Tmp].blk_cnt = blkCnt;
                  vdecMod->vdec[this->_ch].en = true;
                  axMod.unlock(AX_MOD_VDEC);
                  return;
                }
                maix::log::error("VdGrp=%d, AX_VDEC_StartRecvFrame FAILED! ret:0x%x\n",
                                 this->_ch);
                if (cfg->pool_id != 0xffffffff) {
                  AX_VDEC_DetachPool(this->_ch);
                }
              }
              else {
                maix::log::info(" VdGrp=%d, AX_VDEC_AttachPool FAILED! PoolId:%d ret:0x%x",
                                this->_ch,cfg->pool_id,s32Ret);
              }
            }
          }
          else {
            maix::log::error("VdGrp=%d, AX_VDEC_CreateGrp FAILED! ret:0x%x\n",(uint32_t)vdecGrp);
            if (s32Ret == AX_ERR_VDEC_RUN_ERROR) {
              maix::log::error("VdGrp=%d, u32PicWidth:%d u32PicHeight:%d u32FrameBufCnt:%d ret:0x%x\n"
                               ,(uint32_t)vdecGrp,stVdecGrpAttr.u32PicHeight,
                               stVdecGrpAttr.u32FrameBufCnt);
            }
            else if (s32Ret == AX_ERR_VDEC_EXIST) {
              s32Ret = AX_VDEC_DestroyGrp(vdecGrp);
              if (s32Ret != 0) {
                maix::log::error("VdGrp=%d, AX_VDEC_DestroyGrp FAILED! ret:%#x",(uint32_t)vdecGrp
                                );
              }
            }
          }
        }
        else {
          if (vdecType == AX_VDEC_TYPE_H264) {
            vdecPayloadType = PT_H264;
            goto vdec_setup;
          }
          maix::log::error("not support vdec type %d",vdecType);
        }
        AX_VDEC_DestroyGrp(this->_ch);
        if (cfg->pool_id != 0xffffffff) {
          AX_POOL_DestroyPool(cfg->pool_id);
        }
        u32Tmp = vdecMod->init_count;
        if (u32Tmp < 2) {
          AX_VDEC_Deinit();
          uTmp = 0;
        }
        else {
          uTmp = (u32Tmp - 1);
        }
        pcLog = " vdec init count:%d";
        vdecMod->init_count = (int)uTmp;
      }
      maix::log::info(pcLog,uTmp);
      goto failed;
    }
    curVdecChn = curVdecChn + 1;
  } while (curVdecChn != 16);
  maix::log::error("vdec channel is full");
failed:
  axMod.unlock(AX_MOD_VDEC);
  maix::err::check_raise(err::ERR_RUNTIME,"vdec init failed");
}


// VDEC::~VDEC()

VDEC::~VDEC()

{
  AX_POOL PoolId;
  AX_S32 s32Ret;
  ax_vdec_param_t *pVdecParam;
  int initCount;
  int s32Tmp;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_vdec_mod_t *vdecMod = (ax_vdec_mod_t *)axMod.get_param(AX_MOD_VDEC);
  axMod.lock(AX_MOD_VDEC);
  s32Ret = this->_ch;
  if (vdecMod->vdec[s32Ret].en != false) {
    if (vdecMod->vdec[(int)s32Ret].pool_id != 0xffffffff) {
      s32Ret = AX_VDEC_DetachPool(s32Ret);
      if (s32Ret != 0) {
        maix::log::error("VdGrp=%d, AX_VDEC_DetachPool FAILED! PoolId:%d ret:0x%x",
                         this->_ch,
                         vdecMod->vdec[this->_ch].pool_id,s32Ret);
      }
    }
    s32Ret = AX_VDEC_StopRecvStream(this->_ch);
    if (s32Ret != 0) {
      maix::log::error("VdGrp=%d, AX_VDEC_StopRecvStream FAILED! ret:0x%x",this->_ch,
                       s32Ret);
    }
    s32Ret = AX_VDEC_DestroyGrp(this->_ch);
    if (s32Ret != 0) {
      maix::log::error("VdGrp=%d, AX_VDEC_DestroyGrp FAILED! ret:0x%x",this->_ch,
                       s32Ret);
    }
    PoolId = vdecMod->vdec[this->_ch].pool_id;
    if (PoolId != 0xffffffff) {
      s32Ret = AX_POOL_DestroyPool(PoolId);
      if (s32Ret != 0) {
        maix::log::error("VdGrp=%d, AX_POOL_DestroyPool FAILED! PoolId:%d ret:0x%x",
                         this->_ch,
                         vdecMod->vdec[this->_ch].pool_id,s32Ret);
      }
    }
  }
  initCount = vdecMod->init_count;
  if (initCount < 2) {
    s32Ret = AX_VDEC_Deinit();
    if (s32Ret != 0) {
      maix::log::error("AX_VDEC_Deinit FAILED! ret:0x%x",s32Ret);
    }
    vdecMod->init_count = 0;
    maix::log::info(" vdec init count:%d",0);
  }
  else {
    vdecMod->init_count = initCount - 1;
  }
  s32Tmp = this->_ch;
  pVdecParam = vdecMod->vdec + s32Tmp;
  pVdecParam->en = false;
  pVdecParam->type = AX_VDEC_TYPE_JPG;
  vdecMod->vdec[s32Tmp].w = 0;
  vdecMod->vdec[s32Tmp].h = 0;
  vdecMod->vdec[s32Tmp].fmt = AX_FORMAT_YUV400;
  vdecMod->vdec[s32Tmp].blk_cnt = 0;
  vdecMod->vdec[s32Tmp].blk_size = 0;
  vdecMod->vdec[s32Tmp].pool_id = 0;
  axMod.unlock(AX_MOD_VDEC);
}



// VDEC::push(Frame*, int)

err::Err VDEC::push(maixcam2::Frame *frame, int32_t timeout_ms)

{
  AX_S32 s32Ret;
  frame_from_e from;
  AX_VDEC_STREAM_T pstStream;

  if (frame == (Frame *)0x0) {
    return err::ERR_ARGS;
  }
  pstStream.u64PTS = 0;
  pstStream.u64PrivateData = 0;
  pstStream.bEndOfFrame = AX_FALSE;
  pstStream.bEndOfStream = AX_FALSE;
  pstStream.bSkipDisplay = AX_FALSE;
  pstStream.u32StreamPackLen = 0;
  pstStream.pu8Addr = (AX_U8 *)0x0;
  pstStream.u64PhyAddr = 0;
  from = frame->from();
  if (from == FRAME_FROM_AX_MALLOC) {
    pstStream.bEndOfFrame = AX_TRUE;
    pstStream.bEndOfStream = AX_FALSE;
    pstStream.pu8Addr = (AX_U8 *)frame->data;
    pstStream.u32StreamPackLen = frame->len;
    s32Ret = AX_VDEC_SendStream(this->_ch,&pstStream,timeout_ms);
    if ((short)s32Ret == 0) {
      return err::ERR_NONE;
    }
    maix::log::error("AX_VDEC_SendFrame failed! ch:%d ret:%#x",this->_ch);
  }
  else {
    from = frame->from();
    maix::log::info("vdec push is not support frame from %d",(uint32_t)from);
  }
  return err::ERR_RUNTIME;
}



// VDEC::pop(int)

maixcam2::Frame * VDEC::pop(int32_t timeout_ms)

{
  AX_S32 s32Ret;
  Frame *frame;
  char *pcError;
  uint64_t uError;
  AX_VIDEO_FRAME_INFO_T stFrameInfo;

  memset(&stFrameInfo,0,sizeof(stFrameInfo));
  s32Ret = AX_VDEC_GetFrame(this->_ch,&stFrameInfo,timeout_ms);
  if (s32Ret == AX_ERR_VDEC_QUEUE_EMPTY) {
    uError = this->_ch;
    pcError = "VdGrp=%d, AX_VDEC_GetFrame AX_ERR_VDEC_QUEUE_EMPTY";
  }
  else if (s32Ret < AX_ERR_VDEC_QUEUE_FULL) {
    if (s32Ret == AX_ERR_VDEC_NOT_PERM) {
      uError = this->_ch;
      pcError = "VdGrp=%d, AX_VDEC_GetFrame AX_ERR_VDEC_NOT_PERM";
    }
    else {
      if (s32Ret != AX_ERR_VDEC_UNEXIST) goto failed;
      uError = this->_ch;
      pcError = "VdGrp=%d, AX_VDEC_GetFrame AX_ERR_VDEC_UNEXIST";
    }
  }
  else {
    if (s32Ret != AX_ERR_VDEC_FLOW_END) {
      if (s32Ret == 0) {
        frame = new Frame(this->_ch,&stFrameInfo,FRAME_FROM_VDEC_GET_STREAM);
        return frame;
      }
failed:
      maix::log::error("VdGrp=%d, AX_VDEC_GetFrame FAILED! ret=0x%x\n",this->_ch);
      return (Frame *)0x0;
    }
    uError = this->_ch;
    pcError = "VdGrp=%d, AX_VDEC_GetFrame AX_ERR_VDEC_FLOW_END";
  }
  maix::log::error(pcError,uError);
  return (Frame *)0x0;
}



// VDEC::get_config(ax_vdec_param_t*)

err::Err VDEC::get_config(ax_vdec_param_t *cfg)

{
  bool bEn;
  int blkCnt;
  int blkSize;
  int iHeight;
  ax_vdec_param_t *pVdecParam;
  AX_POOL poolId;
  int vdecChn;
  AX_IMG_FORMAT_E vdecFmt;
  ax_vdec_type_e vdecType;

  if (cfg != (ax_vdec_param_t *)0x0) {
    AxModuleParam &axMod = AxModuleParam::getInstance();
    ax_vdec_mod_t *vdecMod = (ax_vdec_mod_t *)axMod.get_param(AX_MOD_VDEC);
    axMod.lock(AX_MOD_VDEC);
    vdecChn = this->_ch;
    pVdecParam = vdecMod->vdec + vdecChn;
    iHeight = vdecMod->vdec[vdecChn].h;
    bEn = pVdecParam->en;
    vdecType = pVdecParam->type;
    blkSize = vdecMod->vdec[vdecChn].blk_size;
    poolId = vdecMod->vdec[vdecChn].pool_id;
    vdecFmt = vdecMod->vdec[vdecChn].fmt;
    blkCnt = vdecMod->vdec[vdecChn].blk_cnt;
    cfg->w = vdecMod->vdec[vdecChn].w;
    cfg->h = iHeight;
    cfg->en = bEn;
    cfg->type = vdecType;
    cfg->blk_size = blkSize;
    cfg->pool_id = poolId;
    cfg->fmt = vdecFmt;
    cfg->blk_cnt = blkCnt;
    axMod.unlock(AX_MOD_VDEC);
    return err::ERR_NONE;
  }
  return err::ERR_ARGS;
}



// AudioIn::AudioIn(ax_audio_in_param_t*)

AudioIn::AudioIn(ax_audio_in_param_t *cfg)

{
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  if (0 < aiMod->init_count) {
    axMod.unlock(AX_MOD_AI);
    maix::err::check_raise(err::ERR_BUSY,"ai has been initialized");
  }
  memcpy(&aiMod->param,cfg,sizeof(aiMod->param));
  axMod.unlock(AX_MOD_AI);
}



// AudioIn::deinit()

err::Err AudioIn::deinit()

{
  AX_S32 s32Ret;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  if (aiMod->init_count != 0) {
    if (aiMod->aed_en != false) {
      s32Ret = AX_AI_DisableDev(aiMod->card,aiMod->device);
      if (s32Ret != 0) {
        printf("AX_AI_DisableDev audio_failed! ret= %x\n",s32Ret);
      }
    }
    AX_AI_DisableDev(aiMod->card,aiMod->device);
    if (aiMod->eq_en != false) {
      s32Ret = AX_ACODEC_RxEqDisable(aiMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_RxEqDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    if (aiMod->lpf_en != false) {
      s32Ret = AX_ACODEC_RxLpfDisable(aiMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_RxLpfDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    if (aiMod->hpf_en != false) {
      s32Ret = AX_ACODEC_RxHpfDisable(aiMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_RxHpfDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    s32Ret = AX_AI_DetachPool(aiMod->card,aiMod->device);
    if (s32Ret != 0) {
      printf("AX_AI_DetachPool audio_failed! Error Code:0x%X\n",s32Ret);
    }
    s32Ret = AX_AI_DeInit();
    if (s32Ret != 0) {
      printf("AX_AI_DeInit audio_failed! Error Code:0x%X\n",s32Ret);
    }
    s32Ret = AX_POOL_DestroyPool(aiMod->pool_id);
    if (s32Ret != 0) {
      printf("AX_POOL_DestroyPool audio_failed! Error Code:0x%X\n",s32Ret);
    }
    aiMod->init_count = 0;
  }
  axMod.unlock(AX_MOD_AI);
  return err::ERR_NONE;
}


// AudioIn::~AudioIn()

AudioIn::~AudioIn()

{
  deinit();
  return;
}



// AudioIn::read(int)

maixcam2::Frame *AudioIn::read(int32_t timeout_ms)

{
  char bNeedExit;
  AX_S32 s32Ret;
  long tickStart;
  Frame *frame;
  long tickNow;
  AX_AUDIO_FRAME_T stAiFrame;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  if (aiMod->init_count == 0) {
    axMod.unlock(AX_MOD_AI);
    return (Frame *)0x0;
  }
  if (timeout_ms < 1) {
    s32Ret = AX_AI_GetFrame(aiMod->card,aiMod->device,&stAiFrame,timeout_ms);
    if (s32Ret == 0) {
      frame = new Frame(aiMod->card,aiMod->device,&stAiFrame,FRAME_FROM_AUDIO_GET_FRAME);
      goto done;
    }
  }
  else {
    tickStart = maix::time::ticks_ms();
    while (bNeedExit = maix::app::need_exit(), bNeedExit == 0) {
      s32Ret = AX_AI_GetFrame(aiMod->card,aiMod->device,&stAiFrame,0);
      if (s32Ret == 0) {
        frame = new Frame(aiMod->card,aiMod->device,&stAiFrame,FRAME_FROM_AUDIO_GET_FRAME);
        goto done;
      }
      tickNow = maix::time::ticks_ms();
      if ((ulong)(long)timeout_ms < (ulong)(tickNow - tickStart)) break;
      maix::time::sleep_ms(1);
    }
  }
  frame = (Frame *)0x0;
done:
  axMod.unlock(AX_MOD_AI);
  return frame;
}



// AudioIn::volume(float)

float AudioIn::volume(float volume)

{
  float fRes;
  AX_F64 now_volume;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  if (aiMod->init_count == 0) {
    axMod.unlock(AX_MOD_AI);
    fRes = -1.0;
  }
  else {
    now_volume = -1.0;
    if (0.0 <= volume) {
      AX_AI_SetVqeVolume(aiMod->card,aiMod->device,(double)volume);
    }
    AX_AI_GetVqeVolume(aiMod->card,aiMod->device,&now_volume);
    axMod.unlock(AX_MOD_AI);
    fRes = (float)now_volume;
  }
  return fRes;
}



// AudioIn::period_size(int)

int AudioIn::period_size(int size)

{
  AX_S32 s32Ret;
  uint64_t uRet;
  AX_AI_ATTR_T stAiAttr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  s32Ret = AX_AI_GetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
  if (s32Ret == 0) {
    if (size < 0) {
done:
      axMod.unlock(AX_MOD_AI);
      return stAiAttr.u32PeriodSize;
    }
    stAiAttr.u32PeriodSize = size;
    s32Ret = AX_AI_SetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
    uRet = s32Ret;
    if (s32Ret == 0) {
      s32Ret = AX_AI_GetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
      uRet = s32Ret;
      if (s32Ret == 0) goto done;
    }
  }
  else {
    uRet = s32Ret;
  }
  maix::log::error("AX_AI_GetPubAttr audio_failed! ret= %x",uRet);
  axMod.unlock(AX_MOD_AI);
  return -1;
}



// AudioIn::period_count(int)

int AudioIn::period_count(int count)

{
  AX_S32 s32Ret;
  uint64_t uRet;
  AX_AI_ATTR_T stAiAttr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  s32Ret = AX_AI_GetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
  if (s32Ret == 0) {
    if (count < 0) {
done:
      axMod.unlock(AX_MOD_AI);
      return stAiAttr.u32PeriodCount;
    }
    stAiAttr.u32PeriodCount = count;
    s32Ret = AX_AI_SetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
    uRet = s32Ret;
    if (s32Ret == 0) {
      s32Ret = AX_AI_GetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
      uRet = s32Ret;
      if (s32Ret == 0) goto done;
    }
  }
  else {
    uRet = s32Ret;
  }
  maix::log::error("AX_AI_GetPubAttr audio_failed! ret= %x",uRet);
  axMod.unlock(AX_MOD_AI);
  return -1;
}



// AudioOut::AudioOut(ax_audio_out_param_t*)

AudioOut::AudioOut(ax_audio_out_param_t *cfg)

{
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  if (0 < aoMod->init_count) {
    axMod.unlock(AX_MOD_AO);
    maix::err::check_raise(err::ERR_BUSY,"ao has been initialized");
  }
  memcpy(&aoMod->param,cfg,sizeof(aoMod->param));
  axMod.unlock(AX_MOD_AO);
}



// AudioOut::init()

err::Err AudioOut::init()

{
  AX_POOL PoolId;
  err::Err uErr;
  int iRet;
  AX_S32 s32Ret;
  char *pcError;
  uint64_t uError;
  AX_AUDIO_SAMPLE_RATE_E audioSampleRate;
  uint64_t vqeAggressivenessLevel;
  uint32_t vqeAgcMode;
  AX_S16 vqeTargetLevel;
  AX_S16 vqeGain;
  AX_ACODEC_FREQ_ATTR_T stHpfAttr;
  AX_ACODEC_FREQ_ATTR_T stLpfAttr;
  AX_AP_DNVQE_ATTR_T stVqeAttr;
  AX_ACODEC_EQ_ATTR_T stEqAttr;
  AX_AO_ATTR_T stAoAttr;
  AX_POOL_CONFIG_T stPoolConfig;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  aoMod->card = 0;
  aoMod->device = 1;
  if (3 < (int)aoMod->param.bits) {
    s32Ret = 0xffffffff;
    maix::log::error("Check your audio bit width!");
failed:
    uErr = err::ERR_ARGS;
    goto done;
  }
  stPoolConfig.CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
  stPoolConfig.PartitionName[0] = '\0';
  stPoolConfig.PartitionName[1] = '\0';
  stPoolConfig.PartitionName[2] = '\0';
  stPoolConfig.PartitionName[3] = '\0';
  stPoolConfig.PartitionName[0xc] = '\0';
  stPoolConfig.PartitionName[0xd] = '\0';
  stPoolConfig.PartitionName[0xe] = '\0';
  stPoolConfig.PartitionName[0xf] = '\0';
  stPoolConfig.PartitionName[0x10] = '\0';
  stPoolConfig.PartitionName[0x11] = '\0';
  stPoolConfig.PartitionName[0x12] = '\0';
  stPoolConfig.PartitionName[0x13] = '\0';
  stPoolConfig.PartitionName[4] = '\0';
  stPoolConfig.PartitionName[5] = '\0';
  stPoolConfig.PartitionName[6] = '\0';
  stPoolConfig.PartitionName[7] = '\0';
  stPoolConfig.PartitionName[8] = '\0';
  stPoolConfig.PartitionName[9] = '\0';
  stPoolConfig.PartitionName[10] = '\0';
  stPoolConfig.PartitionName[0xb] = '\0';
  stPoolConfig.PartitionName[0x1c] = '\0';
  stPoolConfig.PartitionName[0x1d] = '\0';
  stPoolConfig.PartitionName[0x1e] = '\0';
  stPoolConfig.PartitionName[0x1f] = '\0';
  stPoolConfig.PoolName[0] = '\0';
  stPoolConfig.PoolName[1] = '\0';
  stPoolConfig.PoolName[2] = '\0';
  stPoolConfig.PoolName[3] = '\0';
  stPoolConfig.PartitionName[0x14] = '\0';
  stPoolConfig.PartitionName[0x15] = '\0';
  stPoolConfig.PartitionName[0x16] = '\0';
  stPoolConfig.PartitionName[0x17] = '\0';
  stPoolConfig.PartitionName[0x18] = '\0';
  stPoolConfig.PartitionName[0x19] = '\0';
  stPoolConfig.PartitionName[0x1a] = '\0';
  stPoolConfig.PartitionName[0x1b] = '\0';
  stPoolConfig.PoolName[0xc] = '\0';
  stPoolConfig.PoolName[0xd] = '\0';
  stPoolConfig.PoolName[0xe] = '\0';
  stPoolConfig.PoolName[0xf] = '\0';
  stPoolConfig.PoolName[0x10] = '\0';
  stPoolConfig.PoolName[0x11] = '\0';
  stPoolConfig.PoolName[0x12] = '\0';
  stPoolConfig.PoolName[0x13] = '\0';
  stPoolConfig.PoolName[4] = '\0';
  stPoolConfig.PoolName[5] = '\0';
  stPoolConfig.PoolName[6] = '\0';
  stPoolConfig.PoolName[7] = '\0';
  stPoolConfig.PoolName[8] = '\0';
  stPoolConfig.PoolName[9] = '\0';
  stPoolConfig.PoolName[10] = '\0';
  stPoolConfig.PoolName[0xb] = '\0';
  stPoolConfig.PoolName[0x1c] = '\0';
  stPoolConfig.PoolName[0x1d] = '\0';
  stPoolConfig.PoolName[0x1e] = '\0';
  stPoolConfig.PoolName[0x1f] = '\0';
  stPoolConfig.PoolName[0x14] = '\0';
  stPoolConfig.PoolName[0x15] = '\0';
  stPoolConfig.PoolName[0x16] = '\0';
  stPoolConfig.PoolName[0x17] = '\0';
  stPoolConfig.PoolName[0x18] = '\0';
  stPoolConfig.PoolName[0x19] = '\0';
  stPoolConfig.PoolName[0x1a] = '\0';
  stPoolConfig.PoolName[0x1b] = '\0';
  stPoolConfig.MetaSize = 0x2000;
  stPoolConfig.BlkSize = 0x8000;
  stPoolConfig.BlkCnt = 0x25;
  stPoolConfig.IsMergeMode = AX_FALSE;
  strcpy((char*)stPoolConfig.PartitionName,"anonymous");
  if (aoMod->param.cfg_pool_en != false) {
    stPoolConfig.BlkSize = aoMod->param.pool_cfg.BlkSize;
    stPoolConfig.MetaSize = aoMod->param.pool_cfg.MetaSize;
    stPoolConfig.CacheMode = aoMod->param.pool_cfg.CacheMode;
    stPoolConfig.PartitionName[0] = aoMod->param.pool_cfg.PartitionName[0];
    stPoolConfig.PartitionName[1] = aoMod->param.pool_cfg.PartitionName[1];
    stPoolConfig.PartitionName[2] = aoMod->param.pool_cfg.PartitionName[2];
    stPoolConfig.PartitionName[3] = aoMod->param.pool_cfg.PartitionName[3];
    stPoolConfig.BlkCnt = aoMod->param.pool_cfg.BlkCnt;
    stPoolConfig.IsMergeMode = aoMod->param.pool_cfg.IsMergeMode;

    stPoolConfig.PartitionName[0xc] = aoMod->param.pool_cfg.PartitionName[0xc];
    stPoolConfig.PartitionName[0xd] = aoMod->param.pool_cfg.PartitionName[0xd];
    stPoolConfig.PartitionName[0xe] = aoMod->param.pool_cfg.PartitionName[0xe];
    stPoolConfig.PartitionName[0xf] = aoMod->param.pool_cfg.PartitionName[0xf];
    stPoolConfig.PartitionName[0x10] = aoMod->param.pool_cfg.PartitionName[0x10];
    stPoolConfig.PartitionName[0x11] = aoMod->param.pool_cfg.PartitionName[0x11];
    stPoolConfig.PartitionName[0x12] = aoMod->param.pool_cfg.PartitionName[0x12];
    stPoolConfig.PartitionName[0x13] = aoMod->param.pool_cfg.PartitionName[0x13];

    stPoolConfig.PartitionName[4] = aoMod->param.pool_cfg.PartitionName[4];
    stPoolConfig.PartitionName[5] = aoMod->param.pool_cfg.PartitionName[5];
    stPoolConfig.PartitionName[6] = aoMod->param.pool_cfg.PartitionName[6];
    stPoolConfig.PartitionName[7] = aoMod->param.pool_cfg.PartitionName[7];
    stPoolConfig.PartitionName[8] = aoMod->param.pool_cfg.PartitionName[8];
    stPoolConfig.PartitionName[9] = aoMod->param.pool_cfg.PartitionName[9];
    stPoolConfig.PartitionName[10] = aoMod->param.pool_cfg.PartitionName[10];
    stPoolConfig.PartitionName[0xb] = aoMod->param.pool_cfg.PartitionName[0xb];

    stPoolConfig.PartitionName[0x1c] = aoMod->param.pool_cfg.PartitionName[0x1c];
    stPoolConfig.PartitionName[0x1d] = aoMod->param.pool_cfg.PartitionName[0x1d];
    stPoolConfig.PartitionName[0x1e] = aoMod->param.pool_cfg.PartitionName[0x1e];
    stPoolConfig.PartitionName[0x1f] = aoMod->param.pool_cfg.PartitionName[0x1f];
    stPoolConfig.PartitionName[0x20] = aoMod->param.pool_cfg.PartitionName[0x20];
    stPoolConfig.PartitionName[0x21] = aoMod->param.pool_cfg.PartitionName[0x21];
    stPoolConfig.PartitionName[0x22] = aoMod->param.pool_cfg.PartitionName[0x22];
    stPoolConfig.PartitionName[0x23] = aoMod->param.pool_cfg.PartitionName[0x23];

    stPoolConfig.PartitionName[0x14] = aoMod->param.pool_cfg.PartitionName[0x14];
    stPoolConfig.PartitionName[0x15] = aoMod->param.pool_cfg.PartitionName[0x15];
    stPoolConfig.PartitionName[0x16] = aoMod->param.pool_cfg.PartitionName[0x16];
    stPoolConfig.PartitionName[0x17] = aoMod->param.pool_cfg.PartitionName[0x17];
    stPoolConfig.PartitionName[0x18] = aoMod->param.pool_cfg.PartitionName[0x18];
    stPoolConfig.PartitionName[0x19] = aoMod->param.pool_cfg.PartitionName[0x19];
    stPoolConfig.PartitionName[0x1a] = aoMod->param.pool_cfg.PartitionName[0x1a];
    stPoolConfig.PartitionName[0x1b] = aoMod->param.pool_cfg.PartitionName[0x1b];

    stPoolConfig.PoolName[0xc] = aoMod->param.pool_cfg.PoolName[0xc];
    stPoolConfig.PoolName[0xd] = aoMod->param.pool_cfg.PoolName[0xd];
    stPoolConfig.PoolName[0xe] = aoMod->param.pool_cfg.PoolName[0xe];
    stPoolConfig.PoolName[0xf] = aoMod->param.pool_cfg.PoolName[0xf];
    stPoolConfig.PoolName[0x10] = aoMod->param.pool_cfg.PoolName[0x10];
    stPoolConfig.PoolName[0x11] = aoMod->param.pool_cfg.PoolName[0x11];
    stPoolConfig.PoolName[0x12] = aoMod->param.pool_cfg.PoolName[0x12];
    stPoolConfig.PoolName[0x13] = aoMod->param.pool_cfg.PoolName[0x13];

    stPoolConfig.PoolName[4] = aoMod->param.pool_cfg.PoolName[4];
    stPoolConfig.PoolName[5] = aoMod->param.pool_cfg.PoolName[5];
    stPoolConfig.PoolName[6] = aoMod->param.pool_cfg.PoolName[6];
    stPoolConfig.PoolName[7] = aoMod->param.pool_cfg.PoolName[7];
    stPoolConfig.PoolName[8] = aoMod->param.pool_cfg.PoolName[8];
    stPoolConfig.PoolName[9] = aoMod->param.pool_cfg.PoolName[9];
    stPoolConfig.PoolName[10] = aoMod->param.pool_cfg.PoolName[10];
    stPoolConfig.PoolName[0xb] = aoMod->param.pool_cfg.PoolName[0xb];

    stPoolConfig.PoolName[0x1c] = aoMod->param.pool_cfg.PoolName[0x1c];
    stPoolConfig.PoolName[0x1d] = aoMod->param.pool_cfg.PoolName[0x1d];
    stPoolConfig.PoolName[0x1e] = aoMod->param.pool_cfg.PoolName[0x1e];
    stPoolConfig.PoolName[0x1f] = aoMod->param.pool_cfg.PoolName[0x1f];
    stPoolConfig.PoolName[0x20] = aoMod->param.pool_cfg.PoolName[0x20];
    stPoolConfig.PoolName[0x21] = aoMod->param.pool_cfg.PoolName[0x21];
    stPoolConfig.PoolName[0x22] = aoMod->param.pool_cfg.PoolName[0x22];
    stPoolConfig.PoolName[0x23] = aoMod->param.pool_cfg.PoolName[0x23];

    stPoolConfig.PoolName[0x14] = aoMod->param.pool_cfg.PoolName[0x14];
    stPoolConfig.PoolName[0x15] = aoMod->param.pool_cfg.PoolName[0x15];
    stPoolConfig.PoolName[0x16] = aoMod->param.pool_cfg.PoolName[0x16];
    stPoolConfig.PoolName[0x17] = aoMod->param.pool_cfg.PoolName[0x17];
    stPoolConfig.PoolName[0x18] = aoMod->param.pool_cfg.PoolName[0x18];
    stPoolConfig.PoolName[0x19] = aoMod->param.pool_cfg.PoolName[0x19];
    stPoolConfig.PoolName[0x1a] = aoMod->param.pool_cfg.PoolName[0x1a];
    stPoolConfig.PoolName[0x1b] = aoMod->param.pool_cfg.PoolName[0x1b];
  }
  PoolId = AX_POOL_CreatePool(&stPoolConfig);
  if (PoolId == 0xffffffff) {
    uErr = err::ERR_RUNTIME;
    maix::log::error("AX_POOL_CreatePool audio_failed!");
    s32Ret = 0xffffffff;
    goto done;
  }
  aoMod->pool_id = PoolId;
  s32Ret = AX_AO_Init();
  if (s32Ret == 0) {
    iRet = aoMod->param.rate;
    if (iRet < 0x1f41) {
      audioSampleRate = AX_AUDIO_SAMPLE_RATE_8000;
    }
    else {
      audioSampleRate = AX_AUDIO_SAMPLE_RATE_16000;
      if ((((16000 < iRet) && (audioSampleRate = AX_AUDIO_SAMPLE_RATE_32000, 32000 < iRet)) &&
          (audioSampleRate = AX_AUDIO_SAMPLE_RATE_48000, 48000 < iRet)) &&
         (audioSampleRate = AX_AUDIO_SAMPLE_RATE_8000, iRet < 0x17701)) {
        audioSampleRate = AX_AUDIO_SAMPLE_RATE_96000;
      }
    }
    stAoAttr.enBitwidth = aoMod->param.bits;
    stAoAttr.enSamplerate = audioSampleRate;
    stAoAttr.U32Depth = 0x1e;
    stAoAttr.enLinkMode = AX_UNLINK_MODE;
    stAoAttr.u32PeriodSize = aoMod->param.period_size;
    stAoAttr.u32PeriodCount = aoMod->param.period_count;
    stAoAttr.u32ChnCnt = 2;
    stAoAttr.enSoundmode = (AX_AUDIO_SOUND_MODE_E)(aoMod->param.channels != 1);
    stAoAttr.bInsertSilence = (AX_BOOL)aoMod->param.insert_silence;
    if (aoMod->param.cfg_pub_attr != false) {
      stAoAttr.enSoundmode = aoMod->param.pub_attr.enSoundmode;
      stAoAttr.u32ChnCnt = aoMod->param.pub_attr.u32ChnCnt;
      stAoAttr.enSamplerate = aoMod->param.pub_attr.enSamplerate;
      stAoAttr.enBitwidth = aoMod->param.pub_attr.enBitwidth;
      stAoAttr.U32Depth = aoMod->param.pub_attr.U32Depth;
      stAoAttr.enLinkMode = aoMod->param.pub_attr.enLinkMode;
      stAoAttr.u32PeriodSize = aoMod->param.pub_attr.u32PeriodSize;
      stAoAttr.u32PeriodCount = aoMod->param.pub_attr.u32PeriodCount;
      stAoAttr.bInsertSilence = aoMod->param.pub_attr.bInsertSilence;
      if (0x1e < stAoAttr.U32Depth) {
        maix::log::error("AX AO audio depth is too large, must be less than 30");
        goto failed;
      }
    }
    s32Ret = AX_AO_SetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
    if (s32Ret == 0) {
      stVqeAttr.u32FrameSamples = aoMod->param.period_size;
      stVqeAttr.s32SampleRate = audioSampleRate;
      if (aoMod->param.vqe_en == false) {
        stVqeAttr.stNsCfg.bNsEnable = AX_FALSE;
        stVqeAttr.stNsCfg.enAggressivenessLevel = AX_AGGRESSIVENESS_LEVEL_HIGH;
        stVqeAttr.stAgcCfg.bAgcEnable = AX_FALSE;
        stVqeAttr.stAgcCfg.enAgcMode = AX_AGC_MODE_FIXED_DIGITAL;
        stVqeAttr.stAgcCfg.s16TargetLevel = -3;
        stVqeAttr.stAgcCfg.s16Gain = 9;
      }
      else {
        stVqeAttr.s32SampleRate = aoMod->param.vqe_attr.s32SampleRate;
        stVqeAttr.u32FrameSamples = aoMod->param.vqe_attr.u32FrameSamples;
        stVqeAttr.stNsCfg.bNsEnable = aoMod->param.vqe_attr.stNsCfg.bNsEnable;
        vqeAgcMode = (aoMod->param).vqe_attr.stAgcCfg.enAgcMode;
        vqeTargetLevel = (aoMod->param).vqe_attr.stAgcCfg.s16TargetLevel;
        vqeGain = (aoMod->param).vqe_attr.stAgcCfg.s16Gain;
        vqeAggressivenessLevel =
             *(uint64_t *)&aoMod->param.vqe_attr.stNsCfg.enAggressivenessLevel;
        stVqeAttr.stNsCfg.enAggressivenessLevel = (AX_AGGRESSIVENESS_LEVEL_E)vqeAggressivenessLevel;
        stVqeAttr.stAgcCfg.bAgcEnable = (AX_BOOL)((ulong)vqeAggressivenessLevel >> 0x20);
        stVqeAttr.stAgcCfg.enAgcMode = (AX_AGC_MODE_E)vqeAgcMode;
        stVqeAttr.stAgcCfg.s16TargetLevel = vqeTargetLevel;
        stVqeAttr.stAgcCfg.s16Gain = vqeGain;
      }
      if ((stVqeAttr.stNsCfg.bNsEnable == AX_FALSE && stVqeAttr.stAgcCfg.bAgcEnable == AX_FALSE) ||
         (s32Ret = AX_AO_SetDnVqeAttr(aoMod->card,aoMod->device,&stVqeAttr),
         s32Ret == 0)) {
        stHpfAttr.s32Freq = 200;
        stHpfAttr.s32Samplerate = audioSampleRate;
        stHpfAttr.s32GainDb = -3;
        if (aoMod->param.hpf_en == false) {
          if (aoMod->hpf_en != false) goto hpf_enable;
lpf_init:
          stLpfAttr.s32Samplerate = audioSampleRate;
          stLpfAttr.s32GainDb = 0;
          stLpfAttr.s32Freq = 3000;
          if (aoMod->param.lpf_en == false) {
            if (aoMod->lpf_en != false) goto lpf_enable;
eq_init:
            stEqAttr.s32Samplerate = audioSampleRate;
            if (aoMod->param.eq_en == false) {
              if (aoMod->eq_en != false) goto eq_enable;
dev_enable:
              s32Ret = AX_AO_EnableDev(aoMod->card,aoMod->device);
              if (s32Ret == 0) {
                audioSampleRate = (AX_AUDIO_SAMPLE_RATE_E)aoMod->param.rate;
                if (((audioSampleRate == AX_AUDIO_SAMPLE_RATE_8000) ||
                    (audioSampleRate == AX_AUDIO_SAMPLE_RATE_16000)) ||
                   ((audioSampleRate == AX_AUDIO_SAMPLE_RATE_32000 ||
                    (((audioSampleRate == AX_AUDIO_SAMPLE_RATE_48000 ||
                      (audioSampleRate == AX_AUDIO_SAMPLE_RATE_96000)) ||
                     (s32Ret = AX_AO_EnableResample
                                         (aoMod->card,aoMod->device,
                                          audioSampleRate), s32Ret == 0)))))) {
                  aoMod->init_count = 1;
                  axMod.unlock(AX_MOD_AO);
                  return err::ERR_NONE;
                }
                uError = s32Ret;
                pcError = "AX_AO_EnableResample audio_failed! ret = %#x";
              }
              else {
                uError = s32Ret;
                pcError = "AX_AO_EnableDev audio_failed! ret = %#x";
              }
            }
            else {
              aoMod->eq_en = true;
              stEqAttr.s32GainDb[0] = aoMod->param.eq_attr.s32GainDb[0];
              stEqAttr.s32GainDb[1] = aoMod->param.eq_attr.s32GainDb[1];
              stEqAttr.s32GainDb[2] = aoMod->param.eq_attr.s32GainDb[2];
              stEqAttr.s32GainDb[3] = aoMod->param.eq_attr.s32GainDb[3];
              stEqAttr.s32GainDb[4] = aoMod->param.eq_attr.s32GainDb[4];
              stEqAttr.s32Samplerate = aoMod->param.eq_attr.s32Samplerate;
              stEqAttr.bEnable = aoMod->param.eq_attr.bEnable;
              stEqAttr.u32Reserved = aoMod->param.eq_attr.u32Reserved;
eq_enable:
              s32Ret = AX_ACODEC_TxEqSetAttr(aoMod->card,&stEqAttr);
              if (s32Ret == 0) {
                s32Ret = AX_ACODEC_TxEqEnable(aoMod->card);
                if (s32Ret == 0) goto dev_enable;
                uError = s32Ret;
                pcError = "AX_ACODEC_TxEqEnable audio_failed! ret = %#x";
              }
              else {
                uError = s32Ret;
                pcError = "AX_ACODEC_TxEqSetAttr audio_failed! ret = %#x";
              }
            }
          }
          else {
            aoMod->lpf_en = true;
            stLpfAttr.s32GainDb = aoMod->param.lpf_attr.s32GainDb;
            stLpfAttr.s32Samplerate = aoMod->param.lpf_attr.s32Samplerate;
            stLpfAttr.s32Freq = aoMod->param.lpf_attr.s32Freq;
            stLpfAttr.bEnable = aoMod->param.lpf_attr.bEnable;
            stLpfAttr.u32Reserved = aoMod->param.lpf_attr.u32Reserved;
lpf_enable:
            s32Ret = AX_ACODEC_TxLpfSetAttr(aoMod->card,&stLpfAttr);
            if (s32Ret == 0) {
              s32Ret = AX_ACODEC_TxLpfEnable(aoMod->card);
              if (s32Ret == 0) goto eq_init;
              uError = s32Ret;
              pcError = "AX_ACODEC_TxLpfEnable audio_failed! ret = %#x";
            }
            else {
              uError = s32Ret;
              pcError = "AX_ACODEC_TxLpfSetAttr audio_failed! ret = %#x";
            }
          }
        }
        else {
          aoMod->hpf_en = true;
          stHpfAttr.s32GainDb = aoMod->param.hpf_attr.s32GainDb;
          stHpfAttr.s32Samplerate = aoMod->param.hpf_attr.s32Samplerate;
          stHpfAttr.s32Freq = aoMod->param.hpf_attr.s32Freq;
          stHpfAttr.bEnable = aoMod->param.hpf_attr.bEnable;
          stHpfAttr.u32Reserved = aoMod->param.hpf_attr.u32Reserved;
hpf_enable:
          s32Ret = AX_ACODEC_TxHpfSetAttr(aoMod->card,&stHpfAttr);
          if (s32Ret == 0) {
            s32Ret = AX_ACODEC_TxHpfEnable(aoMod->card);
            if (s32Ret == 0) goto lpf_init;
            uError = s32Ret;
            pcError = "AX_ACODEC_TxHpfEnable audio_failed! ret = %#x";
          }
          else {
            uError = s32Ret;
            pcError = "AX_ACODEC_TxHpfSetAttr audio_failed! ret = %#x";
          }
        }
      }
      else {
        uError = s32Ret;
        pcError = "AX_AO_SetDnVqeAttr audio_failed! ret = %#x";
      }
    }
    else {
      uError = s32Ret;
      pcError = "AX_AO_SetPubAttr audio_failed! ret = %#x";
    }
  }
  else {
    uError = s32Ret;
    pcError = "AX_AO_Init FAILED! ret:0x%x";
  }
  uErr = err::ERR_NONE;
  maix::log::error(pcError,uError);
done:
  if (uErr == err::ERR_NONE && s32Ret != 0) {
    uErr = err::ERR_RUNTIME;
  }
  if (aoMod->eq_en != false) {
    s32Ret = AX_ACODEC_TxEqDisable(aoMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_TxEqDisable audio_failed! ret= %x",s32Ret);
    }
  }
  if (aoMod->lpf_en != false) {
    s32Ret = AX_ACODEC_TxLpfDisable(aoMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_TxLpfDisable audio_failed! ret= %x\n",s32Ret);
    }
  }
  if (aoMod->hpf_en != false) {
    s32Ret = AX_ACODEC_TxHpfDisable(aoMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_TxHpfDisable audio_failed! ret= %x\n",s32Ret);
    }
  }
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::deinit()

err::Err AudioOut::deinit()

{
  AX_S32 s32Ret;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  if (aoMod->init_count != 0) {
    if (aoMod->eq_en != false) {
      s32Ret = AX_ACODEC_TxEqDisable(aoMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_TxEqDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    if (aoMod->lpf_en != false) {
      s32Ret = AX_ACODEC_TxLpfDisable(aoMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_TxLpfDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    if (aoMod->hpf_en != false) {
      s32Ret = AX_ACODEC_TxHpfDisable(aoMod->card);
      if (s32Ret != 0) {
        printf("AX_ACODEC_TxHpfDisable audio_failed! ret= %x\n",s32Ret);
      }
    }
    AX_AO_DisableDev(aoMod->card,aoMod->device);
    s32Ret = AX_AO_DeInit();
    if (s32Ret != 0) {
      printf("AX_AO_DeInit audio_failed! Error Code:0x%X\n",s32Ret);
    }
    s32Ret = AX_POOL_DestroyPool(aoMod->pool_id);
    if (s32Ret != 0) {
      printf("AX_POOL_DestroyPool audio_failed! Error Code:0x%X\n",s32Ret);
    }
    aoMod->init_count = 0;
  }
  axMod.unlock(AX_MOD_AO);
  return err::ERR_NONE;
}



// AudioOut::~AudioOut()

AudioOut::~AudioOut()

{
  deinit();
  return;
}



// AudioOut::mute(int)

bool AudioOut::mute(int mute)

{
  bool bRet;
  AX_BOOL bEnable;
  AX_AUDIO_FADE_T stAudioFade;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  if (aoMod->init_count == 0) {
    axMod.unlock(AX_MOD_AO);
    bRet = true;
  }
  else {
    bEnable = AX_FALSE;
    stAudioFade.bFade = AX_TRUE;
    stAudioFade.enFadeInRate = AX_AUDIO_FADE_RATE_128;
    stAudioFade.enFadeOutRate = AX_AUDIO_FADE_RATE_128;
    if (-1 < mute) {
      AX_AO_SetVqeMute(aoMod->card,aoMod->device,(AX_BOOL)(mute != 0),
                       &stAudioFade);
    }
    AX_AO_GetVqeMute(aoMod->card,aoMod->device,&bEnable,&stAudioFade);
    axMod.unlock(AX_MOD_AO);
    bRet = bEnable != AX_FALSE;
  }
  return bRet;
}



// AudioOut::volume(float)

float AudioOut::volume(float volume)

{
  float fRet;
  AX_F64 new_volume;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  if (aoMod->init_count == 0) {
    axMod.unlock(AX_MOD_AO);
    fRet = -1.0;
  }
  else {
    new_volume = -1.0;
    if (0.0 <= volume) {
      AX_AO_SetVqeVolume(aoMod->card,aoMod->device,(double)volume);
    }
    AX_AO_GetVqeVolume(aoMod->card,aoMod->device,&new_volume);
    axMod.unlock(AX_MOD_AO);
    mute((uint)(new_volume == 0.0));
    fRet = (float)new_volume;
  }
  return fRet;
}



// AudioOut::pause()

err::Err AudioOut::pause()

{
  AX_S32 s32Ret;
  err::Err uErr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_PauseRecvFrame(aoMod->card,aoMod->device);
  if (s32Ret == 0) {
    uErr = err::ERR_NONE;
  }
  else {
    uErr = err::ERR_RUNTIME;
    maix::log::error("AX_AO_PauseRecvFrame audio_failed! ret = %#x",s32Ret);
  }
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::resume()

err::Err AudioOut::resume()

{
  AX_S32 s32Ret;
  err::Err uErr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_ResumeRecvFrame(aoMod->card,aoMod->device);
  if (s32Ret == 0) {
    uErr = err::ERR_NONE;
  }
  else {
    uErr = err::ERR_RUNTIME;
    maix::log::error("AX_AO_ResumeRecvFrame audio_failed! ret = %#x",s32Ret);
  }
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::write(Frame*, int)

err::Err AudioOut::write(maixcam2::Frame *frame, int32_t timeout_ms)

{
  char bNeedExit;
  AX_BLK BlockId;
  AX_S32 s32Ret;
  err::Err uErr;
  AX_U32 u32LeftLen;
  axAUDIO_FRAME_T stSrcFrame;
  axAUDIO_FRAME_T stDstFrame;
  int iCurPos;
  AX_U32 u32BlkLen;
  AX_U32 u32CurLen;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  uErr = frame->get_audio_frame(&stSrcFrame);
  if (uErr == err::ERR_NONE) {
    stDstFrame.u64VirAddr = stSrcFrame.u64VirAddr;
    stDstFrame.enBitwidth = stSrcFrame.enBitwidth;
    stDstFrame.enSoundmode = stSrcFrame.enSoundmode;
    stDstFrame.u64TimeStamp = stSrcFrame.u64TimeStamp;
    stDstFrame.u64PhyAddr = stSrcFrame.u64PhyAddr;
    stDstFrame.u32Len = stSrcFrame.u32Len;
    stDstFrame.u32Seq = stSrcFrame.u32Seq;
    stDstFrame.bEof = stSrcFrame.bEof;
    stDstFrame.u32BlkId = stSrcFrame.u32BlkId;
    stDstFrame.u32PoolId[0] = stSrcFrame.u32PoolId[0];
    stDstFrame.u32PoolId[1] = stSrcFrame.u32PoolId[1];
    u32BlkLen = aoMod->param.period_size * 10;
    u32LeftLen = stSrcFrame.u32Len;
    while( true ) {
      if (((int)u32LeftLen < 1) || (bNeedExit = maix::app::need_exit(), bNeedExit != 0)) goto done;
      iCurPos = stSrcFrame.u32Len - u32LeftLen;
      u32CurLen = u32LeftLen;
      if ((int)u32BlkLen < (int)u32LeftLen) {
        u32CurLen = u32BlkLen;
      }
      stDstFrame.u64VirAddr = stSrcFrame.u64VirAddr + iCurPos;
      stDstFrame.u32Len = u32CurLen;
      u32LeftLen = u32LeftLen - u32CurLen;
      BlockId = AX_POOL_GetBlock(aoMod->pool_id,(long)(int)u32BlkLen,(AX_S8 *)0x0);
      stDstFrame.u32BlkId = BlockId;
      stDstFrame.u64VirAddr = (AX_U8 *)AX_POOL_GetBlockVirAddr(BlockId);
      memcpy(stDstFrame.u64VirAddr,stSrcFrame.u64VirAddr + iCurPos,(long)(int)u32CurLen);
      s32Ret = AX_AO_SendFrame(aoMod->card,aoMod->device,&stDstFrame,
                               (AX_AUDIO_FRAME_T *)0x0,0.0,timeout_ms);
      if (s32Ret != 0) break;
      AX_POOL_ReleaseBlock(stDstFrame.u32BlkId);
    }
    maix::log::error("AX_AO_SendFrame audio_failed! ret = %#x",s32Ret);
    AX_POOL_ReleaseBlock(stDstFrame.u32BlkId);
  }
  else {
    maix::log::error("audio write get frame failed");
  }
done:
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::clear()

err::Err AudioOut::clear(void)

{
  AX_S32 s32Ret;
  err::Err uErr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_ClearDevBuf(aoMod->card,aoMod->device);
  if (s32Ret == 0) {
    uErr = err::ERR_NONE;
  }
  else {
    uErr = err::ERR_RUNTIME;
    maix::log::error("AX_AO_ClearDevBuf audio_failed! ret = %#x",s32Ret);
  }
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::wait(int)

err::Err AudioOut::wait(int32_t timeout_ms)

{
  char bNeedExit;
  AX_S32 s32Ret;
  long ticksStart;
  long ticksNow;
  AX_AO_DEV_STATE_T stAoState;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  ticksStart = maix::time::ticks_ms();
  while (bNeedExit = maix::app::need_exit(), bNeedExit == 0) {
    s32Ret = AX_AO_QueryDevStat(aoMod->card,aoMod->device,&stAoState);
    if (s32Ret == 0) {
      if (stAoState.u32DevBusyNum == 0) {
        if (aoMod->param.insert_silence != false) break;
      }
      else if (stAoState.longPcmBufDelay < 1) break;
    }
    ticksNow = maix::time::ticks_ms();
    if ((ulong)(long)timeout_ms < (ulong)(ticksNow - ticksStart)) break;
    maix::time::sleep_ms(10);
  }
  axMod.unlock(AX_MOD_AO);
  return err::ERR_NONE;
}



// AudioOut::state(int&, int&, int&, int&)

err::Err AudioOut::state(int &total_num, int &free_num, int &busy_num, int &pcm_delay)

{
  AX_S32 s32Ret;
  err::Err uErr;
  AX_AO_DEV_STATE_T stAoState;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_QueryDevStat(aoMod->card,aoMod->device,&stAoState);
  if (s32Ret == 0) {
    uErr = err::ERR_NONE;
    total_num = stAoState.u32DevTotalNum;
    free_num = stAoState.u32DevFreeNum;
    busy_num = stAoState.u32DevBusyNum;
    pcm_delay = (int)stAoState.longPcmBufDelay;
  }
  else {
    uErr = err::ERR_RUNTIME;
    maix::log::error("AX_AO_QueryDevStat audio_failed! ret = %#x",s32Ret);
  }
  axMod.unlock(AX_MOD_AO);
  return uErr;
}



// AudioOut::period_size(int)

int AudioOut::period_size(int size)

{
  AX_S32 s32Ret;
  char *pcError;
  AX_AO_ATTR_T stAoAttr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_GetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
  if (s32Ret == 0) {
    if (size < 0) {
done:
      axMod.unlock(AX_MOD_AO);
      return stAoAttr.u32PeriodSize;
    }
    stAoAttr.u32PeriodSize = size;
    s32Ret = AX_AO_DisableDev(aoMod->card,aoMod->device);
    if (s32Ret == 0) {
      s32Ret = AX_AO_SetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
      if (s32Ret != 0) {
        pcError = "AX_AO_SetPubAttr audio_failed! ret= %x";
        goto failed;
      }
      s32Ret = AX_AO_EnableDev(aoMod->card,aoMod->device);
      if (s32Ret == 0) {
        s32Ret = AX_AO_GetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
        if (s32Ret == 0) {
          aoMod->param.period_size = stAoAttr.u32PeriodSize;
          goto done;
        }
        goto get_pub_failed;
      }
      pcError = "AX_AO_EnableDev audio_failed! ret = %#x";
    }
    else {
      pcError = "AX_AO_DisableDev audio_failed! ret = %#x";
    }
    maix::log::error(pcError,s32Ret);
  }
  else {
get_pub_failed:
    pcError = "AX_AO_GetPubAttr audio_failed! ret= %x";
failed:
    maix::log::error(pcError,s32Ret);
    axMod.unlock(AX_MOD_AO);
  }
  return -1;
}



// AudioOut::period_count(int)

int AudioOut::period_count(int count)

{
  AX_S32 s32Ret;
  char *pcError;
  AX_AO_ATTR_T stAoAttr;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ao_mod_t *aoMod = (ax_ao_mod_t *)axMod.get_param(AX_MOD_AO);
  axMod.lock(AX_MOD_AO);
  s32Ret = AX_AO_GetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
  if (s32Ret == 0) {
    if (count < 0) {
done:
      axMod.unlock(AX_MOD_AO);
      return stAoAttr.u32PeriodCount;
    }
    stAoAttr.u32PeriodCount = count;
    s32Ret = AX_AO_DisableDev(aoMod->card,aoMod->device);
    if (s32Ret == 0) {
      s32Ret = AX_AO_SetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
      if (s32Ret != 0) {
        pcError = "AX_AO_SetPubAttr audio_failed! ret= %x";
        goto failed;
      }
      s32Ret = AX_AO_EnableDev(aoMod->card,aoMod->device);
      if (s32Ret == 0) {
        s32Ret = AX_AO_GetPubAttr(aoMod->card,aoMod->device,&stAoAttr);
        if (s32Ret == 0) {
          aoMod->param.period_count = stAoAttr.u32PeriodCount;
          goto done;
        }
        goto get_pub_failed;
      }
      pcError = "AX_AO_EnableDev audio_failed! ret = %#x";
    }
    else {
      pcError = "AX_AO_DisableDev audio_failed! ret = %#x";
    }
    maix::log::error(pcError,s32Ret);
  }
  else {
get_pub_failed:
    pcError = "AX_AO_GetPubAttr audio_failed! ret= %x";
failed:
    maix::log::error(pcError,s32Ret);
    axMod.unlock(AX_MOD_AO);
  }
  return -1;
}



// AudioIn::init()

err::Err AudioIn::init(void)

{
  AX_AUDIO_SAMPLE_RATE_E enOutSampleRate;
  AX_POOL PoolId;
  AX_S32 s32Ret;
  err::Err errRet;
  AX_AGGRESSIVENESS_LEVEL_E vqeAggressivenessLevel;
  AX_U32 u32VadLevel;
  AX_AEC_MODE_E aecMode;
  AX_S32 s32Tmp;
  AX_S16 s16Tmp;
  char *pcError;
  uint32_t periodSize;
  uint64_t uError;
  uint32_t periodCount;
  AX_AP_UPTALKVQE_ATTR_T *pstVqeAttr;
  AX_S32 vqeSampleRate;
  AX_U32 vqeFrameSamples;
  AX_U32 pubChnCnt;
  AX_U32 pubPeriodSize;
  AX_U32 pubPeriodCount;
  AX_U32 pubDepth;
  AX_AUDIO_SAMPLE_RATE_E audioSampleRate;
  AX_AED_ATTR_T stAedAttr [2];
  AX_ACODEC_FREQ_ATTR_T stLpfAttr;
  AX_ACODEC_FREQ_ATTR_T stHpfAttr;
  AX_ACODEC_EQ_ATTR_T stEqAttr;
  string sValue;
  AX_AI_ATTR_T stAiAttr;
  AX_AP_UPTALKVQE_ATTR_T stVqeAttr;
  AX_POOL_CONFIG_T stPoolConfig;

  stEqAttr.s32GainDb[0] = -10;
  stEqAttr.s32GainDb[1] = -3;
  stEqAttr.s32GainDb[2] = 3;
  stEqAttr.s32GainDb[3] = 5;
  stEqAttr.s32GainDb[4] = 10;
  stEqAttr.s32Samplerate = 0;
  stEqAttr.bEnable = AX_FALSE;
  stEqAttr.u32Reserved = 0;
  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_ai_mod_t *aiMod = (ax_ai_mod_t *)axMod.get_param(AX_MOD_AI);
  axMod.lock(AX_MOD_AI);
  aiMod->card = 0;
  aiMod->device = 0;
  if ((int)aiMod->param.bits < 4) {
    stPoolConfig.CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
    stPoolConfig.PartitionName[0] = '\0';
    stPoolConfig.PartitionName[1] = '\0';
    stPoolConfig.PartitionName[2] = '\0';
    stPoolConfig.PartitionName[3] = '\0';
    stPoolConfig.PartitionName[0xc] = '\0';
    stPoolConfig.PartitionName[0xd] = '\0';
    stPoolConfig.PartitionName[0xe] = '\0';
    stPoolConfig.PartitionName[0xf] = '\0';
    stPoolConfig.PartitionName[0x10] = '\0';
    stPoolConfig.PartitionName[0x11] = '\0';
    stPoolConfig.PartitionName[0x12] = '\0';
    stPoolConfig.PartitionName[0x13] = '\0';
    stPoolConfig.PartitionName[4] = '\0';
    stPoolConfig.PartitionName[5] = '\0';
    stPoolConfig.PartitionName[6] = '\0';
    stPoolConfig.PartitionName[7] = '\0';
    stPoolConfig.PartitionName[8] = '\0';
    stPoolConfig.PartitionName[9] = '\0';
    stPoolConfig.PartitionName[10] = '\0';
    stPoolConfig.PartitionName[0xb] = '\0';
    stPoolConfig.PartitionName[0x1c] = '\0';
    stPoolConfig.PartitionName[0x1d] = '\0';
    stPoolConfig.PartitionName[0x1e] = '\0';
    stPoolConfig.PartitionName[0x1f] = '\0';
    stPoolConfig.PoolName[0] = '\0';
    stPoolConfig.PoolName[1] = '\0';
    stPoolConfig.PoolName[2] = '\0';
    stPoolConfig.PoolName[3] = '\0';
    stPoolConfig.PartitionName[0x14] = '\0';
    stPoolConfig.PartitionName[0x15] = '\0';
    stPoolConfig.PartitionName[0x16] = '\0';
    stPoolConfig.PartitionName[0x17] = '\0';
    stPoolConfig.PartitionName[0x18] = '\0';
    stPoolConfig.PartitionName[0x19] = '\0';
    stPoolConfig.PartitionName[0x1a] = '\0';
    stPoolConfig.PartitionName[0x1b] = '\0';
    stPoolConfig.PoolName[0xc] = '\0';
    stPoolConfig.PoolName[0xd] = '\0';
    stPoolConfig.PoolName[0xe] = '\0';
    stPoolConfig.PoolName[0xf] = '\0';
    stPoolConfig.PoolName[0x10] = '\0';
    stPoolConfig.PoolName[0x11] = '\0';
    stPoolConfig.PoolName[0x12] = '\0';
    stPoolConfig.PoolName[0x13] = '\0';
    stPoolConfig.PoolName[4] = '\0';
    stPoolConfig.PoolName[5] = '\0';
    stPoolConfig.PoolName[6] = '\0';
    stPoolConfig.PoolName[7] = '\0';
    stPoolConfig.PoolName[8] = '\0';
    stPoolConfig.PoolName[9] = '\0';
    stPoolConfig.PoolName[10] = '\0';
    stPoolConfig.PoolName[0xb] = '\0';
    stPoolConfig.PoolName[0x1c] = '\0';
    stPoolConfig.PoolName[0x1d] = '\0';
    stPoolConfig.PoolName[0x1e] = '\0';
    stPoolConfig.PoolName[0x1f] = '\0';
    stPoolConfig.PoolName[0x14] = '\0';
    stPoolConfig.PoolName[0x15] = '\0';
    stPoolConfig.PoolName[0x16] = '\0';
    stPoolConfig.PoolName[0x17] = '\0';
    stPoolConfig.PoolName[0x18] = '\0';
    stPoolConfig.PoolName[0x19] = '\0';
    stPoolConfig.PoolName[0x1a] = '\0';
    stPoolConfig.PoolName[0x1b] = '\0';
    stPoolConfig.MetaSize = 0x2000;
    stPoolConfig.BlkSize = 0x3c00;
    stPoolConfig.BlkCnt = 0x21;
    stPoolConfig.IsMergeMode = AX_FALSE;
    strcpy((char*)stPoolConfig.PartitionName,"anonymous");
    if (aiMod->param.cfg_pool_en != false) {
      stPoolConfig.BlkSize = aiMod->param.pool_cfg.BlkSize;
      stPoolConfig.MetaSize = aiMod->param.pool_cfg.MetaSize;
      stPoolConfig.CacheMode = aiMod->param.pool_cfg.CacheMode;
      stPoolConfig.PartitionName[0] = aiMod->param.pool_cfg.PartitionName[0];
      stPoolConfig.PartitionName[1] = aiMod->param.pool_cfg.PartitionName[1];
      stPoolConfig.PartitionName[2] = aiMod->param.pool_cfg.PartitionName[2];
      stPoolConfig.PartitionName[3] = aiMod->param.pool_cfg.PartitionName[3];
      stPoolConfig.BlkCnt = aiMod->param.pool_cfg.BlkCnt;
      stPoolConfig.IsMergeMode = aiMod->param.pool_cfg.IsMergeMode;

      stPoolConfig.PartitionName[0xc] = aiMod->param.pool_cfg.PartitionName[0xc];
      stPoolConfig.PartitionName[0xd] = aiMod->param.pool_cfg.PartitionName[0xd];
      stPoolConfig.PartitionName[0xe] = aiMod->param.pool_cfg.PartitionName[0xe];
      stPoolConfig.PartitionName[0xf] = aiMod->param.pool_cfg.PartitionName[0xf];
      stPoolConfig.PartitionName[0x10] = aiMod->param.pool_cfg.PartitionName[0x10];
      stPoolConfig.PartitionName[0x11] = aiMod->param.pool_cfg.PartitionName[0x11];
      stPoolConfig.PartitionName[0x12] = aiMod->param.pool_cfg.PartitionName[0x12];
      stPoolConfig.PartitionName[0x13] = aiMod->param.pool_cfg.PartitionName[0x13];

      stPoolConfig.PartitionName[4] = aiMod->param.pool_cfg.PartitionName[4];
      stPoolConfig.PartitionName[5] = aiMod->param.pool_cfg.PartitionName[5];
      stPoolConfig.PartitionName[6] = aiMod->param.pool_cfg.PartitionName[6];
      stPoolConfig.PartitionName[7] = aiMod->param.pool_cfg.PartitionName[7];
      stPoolConfig.PartitionName[8] = aiMod->param.pool_cfg.PartitionName[8];
      stPoolConfig.PartitionName[9] = aiMod->param.pool_cfg.PartitionName[9];
      stPoolConfig.PartitionName[10] = aiMod->param.pool_cfg.PartitionName[10];
      stPoolConfig.PartitionName[0xb] = aiMod->param.pool_cfg.PartitionName[0xb];

      stPoolConfig.PartitionName[0x1c] = aiMod->param.pool_cfg.PartitionName[0x1c];
      stPoolConfig.PartitionName[0x1d] = aiMod->param.pool_cfg.PartitionName[0x1d];
      stPoolConfig.PartitionName[0x1e] = aiMod->param.pool_cfg.PartitionName[0x1e];
      stPoolConfig.PartitionName[0x1f] = aiMod->param.pool_cfg.PartitionName[0x1f];
      stPoolConfig.PartitionName[0x20] = aiMod->param.pool_cfg.PartitionName[0x20];
      stPoolConfig.PartitionName[0x21] = aiMod->param.pool_cfg.PartitionName[0x21];
      stPoolConfig.PartitionName[0x22] = aiMod->param.pool_cfg.PartitionName[0x22];
      stPoolConfig.PartitionName[0x23] = aiMod->param.pool_cfg.PartitionName[0x23];

      stPoolConfig.PartitionName[0x14] = aiMod->param.pool_cfg.PartitionName[0x14];
      stPoolConfig.PartitionName[0x15] = aiMod->param.pool_cfg.PartitionName[0x15];
      stPoolConfig.PartitionName[0x16] = aiMod->param.pool_cfg.PartitionName[0x16];
      stPoolConfig.PartitionName[0x17] = aiMod->param.pool_cfg.PartitionName[0x17];
      stPoolConfig.PartitionName[0x18] = aiMod->param.pool_cfg.PartitionName[0x18];
      stPoolConfig.PartitionName[0x19] = aiMod->param.pool_cfg.PartitionName[0x19];
      stPoolConfig.PartitionName[0x1a] = aiMod->param.pool_cfg.PartitionName[0x1a];
      stPoolConfig.PartitionName[0x1b] = aiMod->param.pool_cfg.PartitionName[0x1b];

      stPoolConfig.PoolName[0xc] = aiMod->param.pool_cfg.PoolName[0xc];
      stPoolConfig.PoolName[0xd] = aiMod->param.pool_cfg.PoolName[0xd];
      stPoolConfig.PoolName[0xe] = aiMod->param.pool_cfg.PoolName[0xe];
      stPoolConfig.PoolName[0xf] = aiMod->param.pool_cfg.PoolName[0xf];
      stPoolConfig.PoolName[0x10] = aiMod->param.pool_cfg.PoolName[0x10];
      stPoolConfig.PoolName[0x11] = aiMod->param.pool_cfg.PoolName[0x11];
      stPoolConfig.PoolName[0x12] = aiMod->param.pool_cfg.PoolName[0x12];
      stPoolConfig.PoolName[0x13] = aiMod->param.pool_cfg.PoolName[0x13];

      stPoolConfig.PoolName[4] = aiMod->param.pool_cfg.PoolName[4];
      stPoolConfig.PoolName[5] = aiMod->param.pool_cfg.PoolName[5];
      stPoolConfig.PoolName[6] = aiMod->param.pool_cfg.PoolName[6];
      stPoolConfig.PoolName[7] = aiMod->param.pool_cfg.PoolName[7];
      stPoolConfig.PoolName[8] = aiMod->param.pool_cfg.PoolName[8];
      stPoolConfig.PoolName[9] = aiMod->param.pool_cfg.PoolName[9];
      stPoolConfig.PoolName[10] = aiMod->param.pool_cfg.PoolName[10];
      stPoolConfig.PoolName[0xb] = aiMod->param.pool_cfg.PoolName[0xb];

      stPoolConfig.PoolName[0x1c] = aiMod->param.pool_cfg.PoolName[0x1c];
      stPoolConfig.PoolName[0x1d] = aiMod->param.pool_cfg.PoolName[0x1d];
      stPoolConfig.PoolName[0x1e] = aiMod->param.pool_cfg.PoolName[0x1e];
      stPoolConfig.PoolName[0x1f] = aiMod->param.pool_cfg.PoolName[0x1f];
      stPoolConfig.PoolName[0x20] = aiMod->param.pool_cfg.PoolName[0x20];
      stPoolConfig.PoolName[0x21] = aiMod->param.pool_cfg.PoolName[0x21];
      stPoolConfig.PoolName[0x22] = aiMod->param.pool_cfg.PoolName[0x22];
      stPoolConfig.PoolName[0x23] = aiMod->param.pool_cfg.PoolName[0x23];

      stPoolConfig.PoolName[0x14] = aiMod->param.pool_cfg.PoolName[0x14];
      stPoolConfig.PoolName[0x15] = aiMod->param.pool_cfg.PoolName[0x15];
      stPoolConfig.PoolName[0x16] = aiMod->param.pool_cfg.PoolName[0x16];
      stPoolConfig.PoolName[0x17] = aiMod->param.pool_cfg.PoolName[0x17];
      stPoolConfig.PoolName[0x18] = aiMod->param.pool_cfg.PoolName[0x18];
      stPoolConfig.PoolName[0x19] = aiMod->param.pool_cfg.PoolName[0x19];
      stPoolConfig.PoolName[0x1a] = aiMod->param.pool_cfg.PoolName[0x1a];
      stPoolConfig.PoolName[0x1b] = aiMod->param.pool_cfg.PoolName[0x1b];
    }
    PoolId = AX_POOL_CreatePool(&stPoolConfig);
    if (PoolId == 0xffffffff) {
      maix::log::error("AX_POOL_CreatePool audio_failed!");
      s32Ret = 0xffffffff;
      errRet = err::ERR_RUNTIME;
    }
    else {
      aiMod->pool_id = PoolId;
      s32Ret = AX_AI_Init();
      if (s32Ret == 0) {
        int aiRate = aiMod->param.rate;
        if (aiRate < 8001) {
          audioSampleRate = AX_AUDIO_SAMPLE_RATE_8000;
        }
        else {
          audioSampleRate = AX_AUDIO_SAMPLE_RATE_16000;
          if ((((16000 < aiRate) && (audioSampleRate = AX_AUDIO_SAMPLE_RATE_32000, 32000 < aiRate)) &&
              (audioSampleRate = AX_AUDIO_SAMPLE_RATE_48000, 48000 < aiRate)) &&
             (audioSampleRate = AX_AUDIO_SAMPLE_RATE_8000, aiRate < 96001)) {
            audioSampleRate = AX_AUDIO_SAMPLE_RATE_96000;
          }
        }
	stAiAttr.enLinkMode = (AX_LINK_MODE_E)0;
	stAiAttr.enLayoutMode = aiMod->param.layout;
        stAiAttr.U32Depth = 0x1e;
        periodSize = aiMod->param.period_size;
        periodCount = aiMod->param.period_count;
        stAiAttr.u32ChnCnt = aiMod->param.channels;
        stAiAttr.enBitwidth = aiMod->param.bits;
        stAiAttr.enSamplerate = audioSampleRate;
        stAiAttr.f2mic_distance = 0.0;
        stAiAttr.u32PeriodSize = periodSize;
        stAiAttr.u32PeriodCount = periodCount;
        if (aiMod->param.cfg_pub_attr != false) {
          pubChnCnt = aiMod->param.pub_attr.u32ChnCnt;
          pubPeriodSize = aiMod->param.pub_attr.u32PeriodSize;
          stAiAttr.enSamplerate = aiMod->param.pub_attr.enSamplerate;
          stAiAttr.enBitwidth = aiMod->param.pub_attr.enBitwidth;
          stAiAttr.enLinkMode = aiMod->param.pub_attr.enLinkMode;
          stAiAttr.enLayoutMode = aiMod->param.pub_attr.enLayoutMode;
          pubPeriodCount = aiMod->param.pub_attr.u32PeriodCount;
          pubDepth = aiMod->param.pub_attr.U32Depth;
          stAiAttr.f2mic_distance = aiMod->param.pub_attr.f2mic_distance;
          stAiAttr.u32ChnCnt = pubChnCnt;
          stAiAttr.u32PeriodSize = pubPeriodSize;
          stAiAttr.u32PeriodCount = pubPeriodCount;
          stAiAttr.U32Depth = pubDepth;
          if (0x1e < pubDepth) {
            maix::log::error("AX AI audio depth is too large, must be less than 30");
            goto finish;
          }
        }
        s32Ret = AX_AI_SetPubAttr(aiMod->card,aiMod->device,&stAiAttr);
        if (s32Ret == 0) {
          s32Ret = AX_AI_AttachPool(aiMod->card,aiMod->device,PoolId);
          if (s32Ret == 0) {
            pstVqeAttr = &stVqeAttr;
            stVqeAttr.u32FrameSamples = aiMod->param.period_size;
            stVqeAttr.msInSndCardBuf = 0;
            stVqeAttr.stAecCfg.enAecMode = AX_AEC_MODE_DISABLE;
            stVqeAttr.stAecCfg.stAecFloatCfg.enSuppressionLevel = (AX_SUPPRESSION_LEVEL_E)0x0;
            if (aiMod->param.vqe_en == false) {
              stVqeAttr.stAgcCfg.bAgcEnable = AX_FALSE;
              stVqeAttr.stVadCfg.bVadEnable = AX_FALSE;
              stVqeAttr.stVadCfg.u32VadLevel = 2;
              stVqeAttr.stNsCfg.bNsEnable = AX_FALSE;
              stVqeAttr.stNsCfg.enAggressivenessLevel = AX_AGGRESSIVENESS_LEVEL_HIGH;
              stVqeAttr.stAgcCfg.enAgcMode = AX_AGC_MODE_FIXED_DIGITAL;
              stVqeAttr.stAgcCfg.s16TargetLevel = -3;
              stVqeAttr.stAgcCfg.s16Gain = 9;
              stVqeAttr.s32SampleRate = audioSampleRate;
            }
            else {
              stVqeAttr.msInSndCardBuf = aiMod->param.vqe_attr.msInSndCardBuf;
              stVqeAttr.stAecCfg.enAecMode = aiMod->param.vqe_attr.stAecCfg.enAecMode;
              vqeSampleRate = aiMod->param.vqe_attr.s32SampleRate;
              vqeFrameSamples = aiMod->param.vqe_attr.u32FrameSamples;
              stVqeAttr.stVadCfg = aiMod->param.vqe_attr.stVadCfg;
              stVqeAttr.stAgcCfg.enAgcMode = (aiMod->param).vqe_attr.stAgcCfg.enAgcMode;
              stVqeAttr.stAgcCfg.s16TargetLevel = (aiMod->param).vqe_attr.stAgcCfg.s16TargetLevel;
              stVqeAttr.stAgcCfg.s16Gain = (aiMod->param).vqe_attr.stAgcCfg.s16Gain;
              stVqeAttr.s32SampleRate = vqeSampleRate;
              stVqeAttr.u32FrameSamples = vqeFrameSamples;
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio","playback_to_capture_ms",sValue,true);
            if (sValue != "") {
              s16Tmp = std::stoi(sValue,NULL,10);
              stVqeAttr.msInSndCardBuf = s16Tmp;
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio_in","ns_en",sValue,true);
            if (sValue != "") {
              s32Tmp = std::stoi(sValue,NULL,10);
              if (s32Tmp == 1) {
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","ns_level",sValue,true);
                if (sValue != "") {
                  vqeAggressivenessLevel = (AX_AGGRESSIVENESS_LEVEL_E)std::stoi(sValue,NULL,10);
                  stVqeAttr.stNsCfg.enAggressivenessLevel = vqeAggressivenessLevel;
                }
              }
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio_in","vad_en",sValue,true);
            if (sValue != "") {
              s32Tmp = std::stoi(sValue,NULL,10);
              if (s32Tmp == 1) {
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","vad_level",sValue,true);
                if (sValue != "") {
                  u32VadLevel = std::stoi(sValue,NULL,10);
                  stVqeAttr.stVadCfg.u32VadLevel = u32VadLevel;
                }
              }
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio_in","agc_en",sValue,true);
            if (sValue != "") {
              s32Tmp = std::stoi(sValue,NULL,10);
              if (s32Tmp == 1) {
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","agc_target_level",sValue,true);
                if (sValue != "") {
                  s16Tmp = std::stoi(sValue,NULL,10);
                  stVqeAttr.stAgcCfg.s16TargetLevel = s16Tmp;
                }
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","agc_gain",sValue,true);
                if (sValue != "") {
                  s16Tmp = std::stoi(sValue,NULL,10);
                  stVqeAttr.stAgcCfg.s16Gain = s16Tmp;
                }
              }
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio_in","aec_en",sValue,true);
            if (sValue != "") {
              s32Tmp = std::stoi(sValue,NULL,10);
              if (s32Tmp == 1) {
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","aec_mode",sValue,true);
                if (sValue != "") {
                  aecMode = (AX_AEC_MODE_E)std::stoi(sValue,NULL,10);
                  stVqeAttr.stAecCfg.enAecMode = aecMode;
                }
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","aec_float_level",sValue,true);
                if (sValue != "") {
                  s32Tmp = std::stoi(sValue,NULL,10);
                  stVqeAttr.stAecCfg.stAecFloatCfg.enSuppressionLevel = (AX_SUPPRESSION_LEVEL_E)s32Tmp;
                }
                sValue = "";
                maix::app::get_sys_config_kv("audio_in","aec_fixed_level",sValue,true);
                if (sValue != "") {
                  s32Tmp = std::stoi(sValue,NULL,10);
                  stVqeAttr.stAecCfg.stAecFixedCfg.eRoutingMode = (AX_ROUTING_MODE_E)s32Tmp;
                }
              }
            }
            if ((stVqeAttr.stAecCfg.enAecMode != AX_AEC_MODE_DISABLE ||
                stVqeAttr.stNsCfg.bNsEnable != AX_FALSE) ||
                (stVqeAttr.stAgcCfg.bAgcEnable != AX_FALSE ||
                stVqeAttr.stVadCfg.bVadEnable != AX_FALSE)) {
              stVqeAttr.s32SampleRate = AX_AUDIO_SAMPLE_RATE_16000;
              s32Ret = AX_AI_SetUpTalkVqeAttr
                                 (aiMod->card,aiMod->device,pstVqeAttr);
              if (s32Ret != 0) {
                uError = s32Ret;
                pcError = "AX_AI_SetUpTalkVqeAttr audio_failed! ret = %#x";
                goto error;
              }
              audioSampleRate = (AX_AUDIO_SAMPLE_RATE_E)stVqeAttr.s32SampleRate;
            }
            stHpfAttr.s32Freq = 200;
            stHpfAttr.s32Samplerate = audioSampleRate;
            stHpfAttr.s32GainDb = -3;
            if (aiMod->param.hpf_en != false) {
              aiMod->hpf_en = true;
              stHpfAttr.s32Freq = aiMod->param.hpf_attr.s32Freq;
              stHpfAttr.bEnable = aiMod->param.hpf_attr.bEnable;
              stHpfAttr.u32Reserved = aiMod->param.hpf_attr.u32Reserved;
              stHpfAttr.s32GainDb = aiMod->param.hpf_attr.s32GainDb;
              stHpfAttr.s32Samplerate = aiMod->param.hpf_attr.s32Samplerate;
            }
            sValue = "";
            maix::app::get_sys_config_kv("audio","hpf_en",sValue,true);
            if (sValue != "") {
              s32Tmp = std::stoi(sValue,NULL,10);
              if (s32Tmp == 1) {
                aiMod->hpf_en = true;
		sValue = "";
                maix::app::get_sys_config_kv("audio","hpf_freq",sValue,true);
                if (sValue != "") {
                  s32Tmp = std::stoi(sValue,NULL,10);
                  stHpfAttr.s32Freq = s32Tmp;
                }
                sValue = "";
                maix::app::get_sys_config_kv("audio","hpf_gain",sValue,true);
                if (sValue != "") {
                  s32Tmp = std::stoi(sValue,NULL,10);
                  stHpfAttr.s32GainDb = s32Tmp;
                }
                sValue = "";
                maix::app::get_sys_config_kv("audio","hpf_samplerate",sValue,true);
                if (sValue != "") {
                  s32Tmp = std::stoi(sValue,NULL,10);
                  stHpfAttr.s32Samplerate = s32Tmp;
                }
              }
            }
            if (aiMod->hpf_en == false) {
acodec_lpf_init:
              stLpfAttr.s32Freq = 3000;
	      stLpfAttr.s32GainDb = 0;
	      stLpfAttr.s32Samplerate = audioSampleRate;
              if (aiMod->param.lpf_en != false) {
                aiMod->lpf_en = true;
                stLpfAttr.s32GainDb = aiMod->param.lpf_attr.s32GainDb;
                stLpfAttr.s32Samplerate = aiMod->param.lpf_attr.s32Samplerate;
                stLpfAttr.s32Freq = aiMod->param.lpf_attr.s32Freq;
                stLpfAttr.bEnable = aiMod->param.lpf_attr.bEnable;
                stLpfAttr.u32Reserved = aiMod->param.lpf_attr.u32Reserved;
              }
              sValue = "";
              maix::app::get_sys_config_kv("audio","lpf_en",sValue,true);
              if (sValue != "") {
                s32Tmp = std::stoi(sValue,NULL,10);
                if (s32Tmp == 1) {
                  aiMod->lpf_en = true;
                  sValue = "";
                  maix::app::get_sys_config_kv("audio","lpf_freq",sValue,true);
                  if (sValue != "") {
                    s32Tmp = std::stoi(sValue,NULL,10);
                    stLpfAttr.s32Freq = s32Tmp;
                  }
                  sValue = "";
                  maix::app::get_sys_config_kv("audio","lpf_gain",sValue,true);
                  if (sValue != "") {
                    s32Tmp = std::stoi(sValue,NULL,10);
                    stLpfAttr.s32GainDb = s32Tmp;
                  }
                  sValue = "";
                  maix::app::get_sys_config_kv("audio","lpf_samplerate",sValue,true);
                  if (sValue != "") {
                    s32Tmp = std::stoi(sValue,NULL,10);
                    stLpfAttr.s32Samplerate = s32Tmp;
                  }
                }
              }
              if (aiMod->lpf_en != false) {
                s32Ret = AX_ACODEC_RxLpfSetAttr(aiMod->card,&stLpfAttr);
                if (s32Ret != 0) goto hpf_setattr_failed;
                s32Ret = AX_ACODEC_RxLpfEnable(aiMod->card);
                if (s32Ret != 0) goto hpf_en_failed;
              }
              stEqAttr.s32Samplerate = audioSampleRate;
              if (aiMod->param.eq_en != false) {
                aiMod->eq_en = true;
                stEqAttr.s32GainDb[0] = aiMod->param.eq_attr.s32GainDb[0];
                stEqAttr.s32GainDb[1] = aiMod->param.eq_attr.s32GainDb[1];
                stEqAttr.s32GainDb[2] = aiMod->param.eq_attr.s32GainDb[2];
                stEqAttr.s32GainDb[3] = aiMod->param.eq_attr.s32GainDb[3];
                stEqAttr.s32GainDb[4] = aiMod->param.eq_attr.s32GainDb[4];
                stEqAttr.s32Samplerate = aiMod->param.eq_attr.s32Samplerate;
                stEqAttr.bEnable = aiMod->param.eq_attr.bEnable;
                stEqAttr.u32Reserved = aiMod->param.eq_attr.u32Reserved;
              }
              sValue = "";
              maix::app::get_sys_config_kv("audio","eq_en",sValue,true);
              if (sValue == "") {
acodec_eq_check:
                if (aiMod->eq_en != false) goto acodec_eq_enable;
ai_enable_dev:
                s32Ret = AX_AI_EnableDev(aiMod->card,aiMod->device);
                if (s32Ret == 0) {
                  enOutSampleRate = (AX_AUDIO_SAMPLE_RATE_E)aiMod->param.rate;
                  if ((((enOutSampleRate == AX_AUDIO_SAMPLE_RATE_8000) ||
                       (enOutSampleRate == AX_AUDIO_SAMPLE_RATE_16000)) ||
                      ((enOutSampleRate == AX_AUDIO_SAMPLE_RATE_32000 ||
                       ((enOutSampleRate == AX_AUDIO_SAMPLE_RATE_48000 ||
                        (enOutSampleRate == AX_AUDIO_SAMPLE_RATE_96000)))))) ||
                     (s32Ret = AX_AI_EnableResample
                                         (aiMod->card,aiMod->device,
                                          enOutSampleRate), s32Ret == 0)) {
                    if (aiMod->param.aed_en != false) {
                      aiMod->aed_en = true;
                      stAedAttr[0].bDbDetection = aiMod->param.aed_attr.bDbDetection;
                    }
                    sValue = "";
                    maix::app::get_sys_config_kv("audio_in","aed_en",sValue,true);
                    if (sValue == "") {
ai_check_aed:
                      if (aiMod->aed_en != false) goto ai_enable_aed;
ai_enable_ok:
                      aiMod->init_count = 1;
                      errRet = err::ERR_NONE;
		      axMod.unlock(AX_MOD_AI);
                      goto done;
                    }
                    s32Tmp = std::stoi(sValue,NULL,10);
                    if (s32Tmp != 1) goto ai_check_aed;
                    aiMod->aed_en = true;
                    stAedAttr[0].bDbDetection = AX_TRUE;
ai_enable_aed:
                    s32Ret = AX_AI_SetAedAttr(aiMod->card,aiMod->device,
                                              stAedAttr);
                    if (s32Ret == 0) {
                      s32Ret = AX_AI_EnableAed(aiMod->card,aiMod->device);
                      if (s32Ret == 0) goto ai_enable_ok;
                      uError = s32Ret;
                      pcError = "AX_AI_EnableAed audio_failed! ret = %#x";
                    }
                    else {
                      uError = s32Ret;
                      pcError = "AX_AI_SetAedAttr audio_failed! ret = %#x";
                    }
                  }
                  else {
                    uError = s32Ret;
                    pcError = "AX_AI_EnableResample audio_failed! ret = %#x";
                  }
                }
                else {
                  uError = s32Ret;
                  pcError = "AX_AI_EnableDev audio_failed! ret = %#x";
                }
              }
              else {
                s32Tmp = std::stoi(sValue,NULL,10);
                if (s32Tmp != 1) goto acodec_eq_check;
                aiMod->eq_en = true;
acodec_eq_enable:
                s32Ret = AX_ACODEC_RxEqSetAttr(aiMod->card,&stEqAttr);
                if (s32Ret == 0) {
                  s32Ret = AX_ACODEC_RxEqEnable(aiMod->card);
                  if (s32Ret == 0) goto ai_enable_dev;
                  uError = s32Ret;
                  pcError = "AX_ACODEC_RxEqEnable audio_failed! ret = %#x";
                }
                else {
                  uError = s32Ret;
                  pcError = "AX_ACODEC_RxEqSetAttr audio_failed! ret = %#x";
                }
              }
            }
            else {
              s32Ret = AX_ACODEC_RxHpfSetAttr(aiMod->card,&stHpfAttr);
              if (s32Ret == 0) {
                s32Ret = AX_ACODEC_RxHpfEnable(aiMod->card);
                if (s32Ret == 0) goto acodec_lpf_init;
hpf_en_failed:
                uError = s32Ret;
                pcError = "AX_ACODEC_RxHpfEnable audio_failed! ret = %#x";
              }
              else {
hpf_setattr_failed:
                uError = s32Ret;
                pcError = "AX_ACODEC_RxHpfSetAttr audio_failed! ret = %#x";
              }
            }
          }
          else {
            uError = s32Ret;
            pcError = "AX_AI_AttachPool audio_failed! ret = %#x";
          }
        }
        else {
          uError = s32Ret;
          pcError = "AX_AI_SetPubAttr audio_failed! ret = %#x";
        }
      }
      else {
        uError = s32Ret;
        pcError = "AX_AI_Init FAILED! ret:0x%x";
      }
error:
      maix::log::error(pcError,uError);
      errRet = err::ERR_NONE;
    }
  }
  else {
    maix::log::error("Check your audio bit width!");
    s32Ret = 0xffffffff;
finish:
    errRet = err::ERR_ARGS;
  }
  if (errRet == err::ERR_NONE && s32Ret != 0) {
    errRet = err::ERR_RUNTIME;
  }
  if (aiMod->eq_en != false) {
    s32Ret = AX_ACODEC_RxEqDisable(aiMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_RxEqDisable audio_failed! ret= %x",s32Ret);
    }
  }
  if (aiMod->lpf_en != false) {
    s32Ret = AX_ACODEC_RxLpfDisable(aiMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_RxLpfDisable audio_failed! ret= %x\n",s32Ret);
    }
  }
  if (aiMod->hpf_en != false) {
    s32Ret = AX_ACODEC_RxHpfDisable(aiMod->card);
    if (s32Ret != 0) {
      maix::log::error("AX_ACODEC_RxHpfDisable audio_failed! ret= %x\n",s32Ret);
    }
  }
  axMod.unlock(AX_MOD_AI);
done:
  return errRet;
}


// AudioIn::reset()

err::Err AudioIn::reset(void)

{
  deinit();
  init();
  return err::ERR_NONE;
}



// ax_jpg_enc_init()

err::Err ax_jpg_enc_init(void)

{
  uint32_t iRet;
  SYS *sysMod;
  VENC *vencMod;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_jpg_mod_t *jpgMod = (ax_jpg_mod_t *)axMod.get_param(AX_MOD_JPG);
  axMod.lock(AX_MOD_JPG);
  if (jpgMod->init_count < 1) {
    sysMod = new SYS(false);
    jpgMod->sys = sysMod;
    iRet = sysMod->init();
    if (iRet != 0) {
      maix::log::error("jpg sys init failed");
      return err::ERR_RUNTIME;
    }
    vencMod = new VENC((ax_venc_param_t *)0x0);
    jpgMod->init_count = 1;
    jpgMod->venc = vencMod;
  }
  axMod.unlock(AX_MOD_JPG);
  return err::ERR_NONE;
}



// ax_jpg_enc_once(Frame*, int)

Frame * ax_jpg_enc_once(Frame *frame,int quality)

{
  int initRet;
  AX_S32 s32Ret;
  uint32_t u32Ret;
  uint64_t uTmp;
  char *pcError;
  Frame *out_frame;
  AX_VIDEO_FRAME_T *ptFrame;
  uint32_t u32Invert;
  AX_U64 uPhyAddr;
  AX_U8 *pVirAddr;
  AX_VIDEO_FRAME_T stSrcFrame;
  AX_VIDEO_FRAME_T stDstFrame;
  AX_JPEG_ENCODE_ONCE_PARAMS_T stJpegParam;

  AxModuleParam &axMod = AxModuleParam::getInstance();
  ax_jpg_mod_t *jpgMod = (ax_jpg_mod_t *)axMod.get_param(AX_MOD_JPG);
  axMod.lock(AX_MOD_JPG);
  if ((jpgMod->init_count < 1) && (initRet = ax_jpg_enc_init(), initRet != 0)) {
    return (Frame *)0x0;
  }
  axMod.unlock(AX_MOD_JPG);
  memset(&stJpegParam,0,sizeof(stJpegParam));
  if (99 < quality) {
    quality = 99;
  }
  stJpegParam.u32Width = frame->w;
  stJpegParam.u32Height = frame->h;
  stJpegParam.enImgFormat = frame->fmt;
  if (quality < 1) {
    quality = 1;
  }
  stJpegParam.stJpegParam.u32Qfactor = quality;
  uTmp = frame->get_video_frame(&stSrcFrame);
  if ((int)uTmp != 0) {
    maix::log::error("get video frame failed");
    return (Frame *)0x0;
  }
  if ((0xe < (uint)stSrcFrame.enImgFormat) ||
     (uTmp = -0x601bL >> ((ulong)(uint)stSrcFrame.enImgFormat & 0x3f), u32Invert = (uint)uTmp & 1,
     ptFrame = &stSrcFrame, (uTmp & 1) != 0)) {
    ptFrame = &stDstFrame;
    u32Invert = __ax_ivps_csc_tdp(&stSrcFrame,ptFrame,AX_FORMAT_YUV420_SEMIPLANAR);
    uTmp = u32Invert;
    if (u32Invert != 0) {
      pcError = "ivps invert format failed! ret:%#x";
      goto failed;
    }
    u32Invert = 1;
  }
  stJpegParam.u64PhyAddr[0] = ptFrame->u64PhyAddr[0];
  stJpegParam.u64PhyAddr[1] = ptFrame->u64PhyAddr[1];
  stJpegParam.u64PhyAddr[2] = ptFrame->u64PhyAddr[2];
  stJpegParam.u64VirAddr[0] = ptFrame->u64VirAddr[0];
  stJpegParam.u64VirAddr[1] = ptFrame->u64VirAddr[1];
  stJpegParam.u64VirAddr[2] = ptFrame->u64VirAddr[2];
  stJpegParam.u32PicStride[0] = ptFrame->u32PicStride[0];
  stJpegParam.u32PicStride[1] = ptFrame->u32PicStride[1];
  uPhyAddr = 0;
  pVirAddr = (AX_U8 *)0x0;
  stJpegParam.u32PicStride[2] = ptFrame->u32PicStride[2];
  s32Ret = AX_SYS_MemAlloc(&uPhyAddr,(AX_VOID **)&pVirAddr,ptFrame->u32FrameSize,0,(AX_S8 *)"once jpeg");
  if (s32Ret == 0) {
    stJpegParam.ulPhyAddr = uPhyAddr;
    stJpegParam.pu8Addr = pVirAddr;
    stJpegParam.u32OutBufSize = ptFrame->u32FrameSize;
    u32Ret = AX_VENC_JpegEncodeOneFrame(&stJpegParam);
    uTmp = u32Ret;
    if (u32Ret == 0) {
      out_frame = new Frame(stJpegParam.pu8Addr,stJpegParam.u32Len,FRAME_FROM_MALLOC);
      if (u32Invert != 0) {
        AX_SYS_MemFree(ptFrame->u64PhyAddr[0],(AX_VOID *)ptFrame->u64VirAddr[0]);
      }
      if (stJpegParam.ulPhyAddr != 0) {
        if (stJpegParam.pu8Addr != (AX_U8 *)0x0) {
          AX_SYS_MemFree(stJpegParam.ulPhyAddr,stJpegParam.pu8Addr);
          return out_frame;
        }
        return out_frame;
      }
      return out_frame;
    }
    AX_SYS_MemFree(stJpegParam.ulPhyAddr,stJpegParam.pu8Addr);
    pcError = "jpg encode failed, ret:%#x";
  }
  else {
    uTmp = ptFrame->u32FrameSize;
    pcError = "alloc mem err, size(%d).";
  }
failed:
  maix::log::error(pcError,uTmp);
  return (Frame *)0x0;
}


}
