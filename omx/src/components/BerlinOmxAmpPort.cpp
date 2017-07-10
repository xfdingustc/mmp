/*
**                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
** THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
** NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
** OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
** DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
** THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
** IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
**
** MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
** MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
** SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
** (MJKK), MARVELL ISRAEL LTD. (MSIL).
*/

#define LOG_TAG "BerlinOmxAmpPort"
#include <BerlinOmxAmpPort.h>

#define SHMALIGN 32

#ifndef STRINGIFY
#define STRINGIFY(x) case x: return #x
#endif

OMX_STRING AmpState2String(AMP_STATE state) {
  switch(state) {
    STRINGIFY(AMP_LOADED);
    STRINGIFY(AMP_IDLE);
    STRINGIFY(AMP_EXECUTING);
    STRINGIFY(AMP_PAUSED);
    default: return "STATE_UNKOWN";
  }
}

OMX_STRING AmpError2String(HRESULT error) {
  switch(error) {
    STRINGIFY(SUCCESS);
    STRINGIFY(ERR_NOTIMPL);
    STRINGIFY(ERR_ERRPARAM);
    STRINGIFY(ERR_NOMEM);
    STRINGIFY(ERR_NOSHM);
    STRINGIFY(ERR_TIMEOUT);
    STRINGIFY(ERR_ERRSYSCALL);
    STRINGIFY(ERR_IOFAIL);
    STRINGIFY(ERR_EVENTFULL);
    STRINGIFY(ERR_HWBUSY);
    STRINGIFY(ERR_HWFAIL);
    STRINGIFY(ERR_OSALFAIL);
    STRINGIFY(ERR_NOSWRSC);
    STRINGIFY(ERR_NOHWRSC);
    STRINGIFY(ERR_SWSTATEWRONG);
    STRINGIFY(ERR_HWSTATEWRONG);
    STRINGIFY(ERR_RCPERROR);
    STRINGIFY(ERR_SWMODEWRONG);
    STRINGIFY(ERR_HWMODEWRONG);
    STRINGIFY(ERR_CONNCLEARED);
    STRINGIFY(ERR_PRIVATE_BASE);
    default: return "ERROR_UNKOWN";
  }
}

OMX_STRING AmpVideoCodec2String(AMP_MEDIA_TYPE codec) {
  switch(codec) {
    STRINGIFY(MEDIA_VES_MPEG1);
    STRINGIFY(MEDIA_VES_MPEG2);
    STRINGIFY(MEDIA_VES_H263);
    STRINGIFY(MEDIA_VES_MPEG4);
    STRINGIFY(MEDIA_VES_WMV);
    STRINGIFY(MEDIA_VES_RV);
    STRINGIFY(MEDIA_VES_AVC);
    STRINGIFY(MEDIA_VES_MJPEG);
    STRINGIFY(MEDIA_VES_VC1);
    STRINGIFY(MEDIA_VES_VP8);
    STRINGIFY(MEDIA_VES_DIVX);
    STRINGIFY(MEDIA_VES_XVID);
    STRINGIFY(MEDIA_VES_AVS);
    STRINGIFY(MEDIA_VES_SORENSON);
    STRINGIFY(MEDIA_VES_DIV50);
    STRINGIFY(MEDIA_VES_VP6);
    STRINGIFY(MEDIA_VES_RV30);
    STRINGIFY(MEDIA_VES_RV40);
    STRINGIFY(MEDIA_VES_DIV3);
    STRINGIFY(MEDIA_VES_DIV4);
    default: return "VIDEO_CODEC_UNKOWN";
  }
}

namespace berlin {
AMP_MEDIA_TYPE getAmpVideoCodecType(OMX_VIDEO_CODINGTYPE type) {
  AMP_MEDIA_TYPE amp_type = MEDIA_INVALIDATE;
  switch (type) {
    case OMX_VIDEO_CodingH263:
      amp_type = MEDIA_VES_H263;
      break;
    case OMX_VIDEO_CodingMPEG2:
      amp_type = MEDIA_VES_MPEG2;
      break;
    case OMX_VIDEO_CodingMPEG4:
      amp_type = MEDIA_VES_MPEG4;
      break;
    case OMX_VIDEO_CodingAVC:
      amp_type = MEDIA_VES_AVC;
      break;
#ifdef _ANDROID_
     case OMX_VIDEO_CodingVPX:
       amp_type = MEDIA_VES_VP8;
       break;
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_VIDEO_CodingVP8:
      amp_type = MEDIA_VES_VP8;
      break;
    case OMX_VIDEO_CodingVC1:
      amp_type = MEDIA_VES_VC1;
      break;
    case OMX_VIDEO_CodingMSMPEG4:
      amp_type = MEDIA_VES_DIV3;
      break;
    case OMX_VIDEO_CodingHEVC:
      amp_type = MEDIA_VES_HEVC;
      break;
#endif
    case OMX_VIDEO_CodingWMV:
      amp_type = MEDIA_VES_WMV;
      break;
    case OMX_VIDEO_CodingMJPEG:
      amp_type = MEDIA_VES_MJPEG;
      break;
    case OMX_VIDEO_CodingRV:
      // Default set RV30.
      amp_type = MEDIA_VES_RV30;
      break;
    default :
      OMX_LOGE("amp video codec %d not supported", type);
  }
  return amp_type;
}

void convertUsto90K(OMX_TICKS in_pts, UINT32 *pts_high, UINT32 *pts_low) {
  if (in_pts >= 0) {
    OMX_TICKS pts90k = in_pts * 9 / 100;
    *pts_high = static_cast<UINT32>(0xFFFFFFFF & (pts90k >> 32)) | TIME_STAMP_VALID_MASK;
    *pts_low = static_cast<UINT32>(0xFFFFFFFF & pts90k);
  } else {
    *pts_high = *pts_low = 0;
  }
}

OMX_TICKS convert90KtoUs(UINT32 pts_high, UINT32 pts_low) {
    return ((pts_high & 0x7FFFFFFFLL) << 32 | pts_low) * 100 / 9;
}

AmpBuffer::AmpBuffer() :
  mBD(NULL),
  mShm(NULL),
  mCtrl(NULL),
  mGfx(NULL),
  mIsAllocator(false),
  mHasBackup(false) {
}

AmpBuffer::AmpBuffer(AMP_SHM_TYPE type, int size, int align, bool hasBackup,
    AMP_SHM_HANDLE es, AMP_SHM_HANDLE ctrl, OMX_BOOL needRef) :
  mData(NULL),
  mIsAllocator(false),
  mBD(NULL),
  mShm(es),
  mCtrl(ctrl),
  mGfx(NULL),
  mHasBackup(hasBackup),
  mMemInfoIndex(0),
  mPtsIndex(0),
  mFrameInfoIndex(0),
  mCtrlInfoIndex(0),
  mStreamInfoIndex(0),
  mIsBdRef(needRef) {
  HRESULT err = SUCCESS;
  if (!mCtrl) {
    err = AMP_SHM_Allocate(type, size, align, &mShm);
    if (err == SUCCESS) {
      if (type == AMP_SHM_SECURE_DYNAMIC) {
        err = AMP_SHM_GetPhysicalAddress(mShm, 0, &mData);
      } else {
        err = AMP_SHM_GetVirtualAddress(mShm, 0, &mData);
      }
      mIsAllocator = true;
    } else {
      OMX_LOGE("AMP_SHM_Allocate type %d size %d fail %d", type, size, err);
    }
    err = AMP_BD_Allocate(&mBD);
    if (err == SUCCESS) {
      // For input bd, ref is not necessary.
      if (mIsBdRef) {
        err = AMP_BD_Ref(mBD);
      }
    } else {
      OMX_LOGE("Fail to allocate bd.");
    }
  }
}

AmpBuffer::AmpBuffer(void *data, int size, OMX_BOOL needRef) {
  HRESULT err = SUCCESS;
  mIsAllocator = false;
  mCtrl = NULL;
  mGfx = NULL;
  mHasBackup = false;
  mIsBdRef = needRef;
  if (data != NULL) {
    // Need to add AMP_SHM_FLAG_SECURE when use secure memory.
    err = AMP_SHM_Import((int)data, AMP_SHM_TYPE_GEN(AMP_SHM_CLASS_GRAPHICS,
        AMP_SHM_FLAG_CACHEABLE | AMP_SHM_FLAG_DYNAMIC | AMP_SHM_FLAG_SECURE),
        SHMALIGN, &mShm);
    /*if (err == SUCCESS) {
      err = AMP_SHM_GetVirtualAddress(mShm, 0, &mData);
    }*/
  }
  err = AMP_BD_Allocate(&mBD);
  if (err == SUCCESS) {
   // For input bd, ref is not necessary.
   if (mIsBdRef) {
     err = AMP_BD_Ref(mBD);
   }
  } else {
   OMX_LOGE("Fail to allocate bd.");
  }
}

AmpBuffer::~AmpBuffer() {
  HRESULT err = SUCCESS;
  if (NULL != mBD && !mCtrl) {
    err = AMP_BDTAG_Clear(mBD);
    if (mIsBdRef) {
      err = AMP_BD_Unref(mBD);
    }
    err = AMP_BD_Free(mBD);
  }
  if (!mCtrl) {
    if (mIsAllocator) {
      err = AMP_SHM_Release(mShm);
    } else {
      err = AMP_SHM_Discard(mShm);
    }
  }
}

HRESULT AmpBuffer::addPtsTag() {
  UINT32 space = 0;
  AMP_BDTAG_UNITSTART pts_tag;
  kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_UNITSTART));
  pts_tag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_UNITSTART)};
  return AMP_BDTAG_Append(mBD, (UINT8 *)&pts_tag, &mPtsIndex, &space);
}

HRESULT AmpBuffer::addAVSPtsTag() {
  UINT32 space = 0;
  AMP_BDTAG_AVS_PTS pts_tag;
  kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_AVS_PTS));
  pts_tag.Header = {AMP_BDTAG_SYNC_PTS_META, sizeof(AMP_BDTAG_AVS_PTS)};
  return AMP_BDTAG_Append(mBD, (UINT8 *)&pts_tag, &mPtsIndex, &space);
}

HRESULT AmpBuffer::addPtsTag(OMX_BUFFERHEADERTYPE *header,
    OMX_U32 stream_pos) {
  UINT32 space = 0;
  AMP_BDTAG_UNITSTART pts_tag;
  kdMemset(&pts_tag, 0, sizeof(AMP_BDTAG_UNITSTART));
  pts_tag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_UNITSTART)};
  convertUsto90K(header->nTimeStamp, &pts_tag.uPtsHigh, &pts_tag.uPtsLow);
  pts_tag.uStrmPos = stream_pos;
  return AMP_BDTAG_Append(mBD, (UINT8 *)&pts_tag, &mPtsIndex, &space);
}

HRESULT AmpBuffer::addMemInfoTag() {
  UINT32 space = 0;
  AMP_BDTAG_MEMINFO mem_tag;
  kdMemset(&mem_tag, 0, sizeof(AMP_BDTAG_MEMINFO));
  mem_tag.Header = {AMP_BDTAG_ASSOCIATE_MEM_INFO, sizeof(AMP_BDTAG_MEMINFO)};
  mem_tag.uMemHandle = mShm;
  mem_tag.uMemOffset = 0;
  return AMP_BDTAG_Append(mBD, (UINT8 *)&mem_tag, &mMemInfoIndex, &space);
}

HRESULT AmpBuffer::addMemInfoTag(OMX_BUFFERHEADERTYPE *header,
    OMX_U32 offset) {
  UINT32 space = 0;
  AMP_BDTAG_MEMINFO mem_tag;
  kdMemset(&mem_tag, 0, sizeof(AMP_BDTAG_MEMINFO));
  mem_tag.Header = {AMP_BDTAG_ASSOCIATE_MEM_INFO, sizeof(AMP_BDTAG_MEMINFO)};
  mem_tag.uMemHandle = mShm;
  mem_tag.uSize = header->nFilledLen;
  mem_tag.uMemOffset = offset;
  if (header->nFlags & OMX_BUFFERFLAG_EOS) {
    mem_tag.uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
  }
  OMX_LOGV("addMemInfoTag  uMemOffset = %d  uSize = %d",
      mem_tag.uMemOffset, mem_tag.uSize);
  return AMP_BDTAG_Append(mBD, (UINT8 *)&mem_tag, &mMemInfoIndex, &space);
}

HRESULT AmpBuffer::addFrameInfoTag() {
  UINT32 space = 0;
  AMP_BGTAG_FRAME_INFO frame_tag;
  kdMemset(&frame_tag, 0, sizeof(AMP_BGTAG_FRAME_INFO));
  frame_tag.Header = {AMP_BGTAG_FRAME_INFO_META, sizeof(AMP_BGTAG_FRAME_INFO)};
  return AMP_BDTAG_Append(mBD, (UINT8 *)&frame_tag, &mFrameInfoIndex, &space);
}

HRESULT AmpBuffer::addAudioFrameInfoTag(OMX_U32 channelnum, OMX_U32 bitdepth,
    OMX_U32 samplerate) {
  UINT32 space = 0;
  AMP_BDTAG_AUD_FRAME_INFO audio_frame_tag;
  kdMemset(&audio_frame_tag, 0, sizeof(AMP_BDTAG_AUD_FRAME_INFO));
  audio_frame_tag.Header = {AMP_BDTAG_AUD_FRAME_CTRL,
      sizeof(AMP_BDTAG_AUD_FRAME_INFO)};
  audio_frame_tag.uChanNr = channelnum;
  audio_frame_tag.uBitDepth = bitdepth;
  audio_frame_tag.uSampleRate = samplerate;
  audio_frame_tag.uMemHandle = mShm;
  audio_frame_tag.uDataLength = 0;
  audio_frame_tag.uSize = AMP_AUD_MAX_BUF_NR * 4096;
  audio_frame_tag.uMemOffset[0] = 0 * 4096;
  audio_frame_tag.uMemOffset[1] = 1 * 4096;
  audio_frame_tag.uMemOffset[2] = 2 * 4096;
  audio_frame_tag.uMemOffset[3] = 3 * 4096;
  audio_frame_tag.uMemOffset[4] = 4 * 4096;
  audio_frame_tag.uMemOffset[5] = 5 * 4096;
  audio_frame_tag.uMemOffset[6] = 6 * 4096;
  audio_frame_tag.uMemOffset[7] = 7 * 4096;
  OMX_LOGD("Channel num = %d, uBitDepth=%d uSampleRate=%d",
    channelnum, bitdepth, samplerate);
  return AMP_BDTAG_Append(mBD, (UINT8 *)&audio_frame_tag,
      &mFrameInfoIndex, &space);
}


HRESULT AmpBuffer::addDmxCtrlInfoTag(OMX_U32 datacount, OMX_U32 bytes,
    OMX_BOOL isEncrypted, OMX_U64 sampleId, OMX_U64 block_offset,
    OMX_U64 byte_offset, OMX_U8 drm_type) {
  UINT32 space = 0;
  AMP_BDTAG_DMX_CTRL_INFO ctrl_tag;
  kdMemset(&ctrl_tag, 0, sizeof(AMP_BDTAG_DMX_CTRL_INFO));
  if (drm_type == AMP_DRMSCHEME_PLAYREADY
      || drm_type == AMP_DRMSCHEME_MARLIN) {
    ctrl_tag.Header = {AMP_BDTAG_PAYLOAD_PLAYREADY_CTRL,
        sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
    ctrl_tag.m_CtrlUnit.m_ControlInfoType = 0x11;  // DRM_PLAYREADY_PAYLOAD_CTRL;
  } else if (drm_type == AMP_DRMSCHEME_WIDEVINE) {
    ctrl_tag.Header = {AMP_BDTAG_PAYLOAD_WIDEVINE_CTRL,
        sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
    ctrl_tag.m_CtrlUnit.m_ControlInfoType = 0x13;  // DRM_WIDEVINE_PAYLOAD_CTRL;
  } else {
    OMX_LOGE("Unsupported drm type %d\n", drm_type);
  }
  ctrl_tag.m_CtrlUnit.m_Rsvd = 0;
  ctrl_tag.m_CtrlUnit.m_DataCount = datacount;
  ctrl_tag.m_CtrlUnit.m_BufPosition = 0;
  ctrl_tag.m_CtrlUnit.m_UserData = 0;
  ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.m_IsEncrypted = isEncrypted;
  ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.m_PayloadSize = bytes;
  ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.SampleID = sampleId;
  ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.BlockOffset = block_offset;
  ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.ByteOffset = byte_offset;
  return AMP_BDTAG_Append(mBD, (UINT8 *)&ctrl_tag, &mCtrlInfoIndex, &space);
}

HRESULT AmpBuffer::addCryptoCtrlInfoTag(OMX_U8 type,
    OMX_U32 stream_pos) {
  UINT32 space = 0;
  AMP_BDTAG_DMX_CTRL_INFO ctrl_tag;

  ctrl_tag.Header = {AMP_BDTAG_BS_CRYPTO_CTRL,
      sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
  // ctrl_tag.m_CtrlUnit.m_ControlInfoType = 0xf0;
  ctrl_tag.m_CtrlUnit.m_DataCount = stream_pos;

  ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSchemeType = type;
  ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSessionID = 0;
  kdMemset(ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiKeyID,
      0, 16);
  ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiKeyIdLen = 4;
  return AMP_BDTAG_Append(mBD, (UINT8 *)&ctrl_tag, NULL, &space);
}

HRESULT AmpBuffer::addDmxUnitStartTag(OMX_BUFFERHEADERTYPE *header,
    OMX_U32 stream_pos) {
  UINT32 space = 0;
  AMP_BDTAG_DMX_CTRL_INFO DmxCtrlTag;
  kdMemset(&DmxCtrlTag, 0, sizeof(AMP_BDTAG_DMX_CTRL_INFO));
  DmxCtrlTag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
  DmxCtrlTag.m_CtrlUnit.m_ControlInfoType = 0xa0;
  DmxCtrlTag.m_CtrlUnit.m_DataCount = stream_pos;
  convertUsto90K(header->nTimeStamp,
      &DmxCtrlTag.m_CtrlUnit.m_Union.m_UnitStart.m_PtsValue.uiStampHigh,
      &DmxCtrlTag.m_CtrlUnit.m_Union.m_UnitStart.m_PtsValue.uiStampLow);
  return AMP_BDTAG_Append(mBD, (UINT8 *)&DmxCtrlTag, &mPtsIndex, &space);
}

HRESULT AmpBuffer::addCtrlInfoTag(OMX_U32 total, OMX_U32 start, OMX_U32 size) {
  UINT32 space = 0;
  AMP_BDTAG_TVPCTRLBUF ctrl_tag;
  kdMemset(&ctrl_tag, 0, sizeof(AMP_BDTAG_TVPCTRLBUF));
  ctrl_tag.Header = {AMP_BDTAG_PAYLOAD_CTRL_BUFFER, sizeof(AMP_BDTAG_TVPCTRLBUF)};
  ctrl_tag.uMemHandle = mCtrl;
  ctrl_tag.uTotal = total;
  ctrl_tag.uStart = start;
  ctrl_tag.uSize = size;
  OMX_LOGV("addCtrlInfoTag   uStart = %d   uSize = %d", ctrl_tag.uStart, ctrl_tag.uSize);
  return AMP_BDTAG_Append(mBD, (UINT8 *)&ctrl_tag, &mCtrlInfoIndex, &space);
}

HRESULT AmpBuffer::addStreamInfoTag() {
  UINT32 space = 0;
  AMP_BDTAG_STREAM_INFO stream_tag;
  kdMemset(&stream_tag, 0, sizeof(AMP_BDTAG_STREAM_INFO));
  stream_tag.Header = {AMP_BDTAG_STREAM_INFO_META, sizeof(AMP_BDTAG_STREAM_INFO)};
  return AMP_BDTAG_Append(mBD, (UINT8 *)&stream_tag, &mStreamInfoIndex, &space);
}

HRESULT AmpBuffer::clearBdTag() {
  HRESULT err = SUCCESS;
  if (NULL != mBD) {
    err = AMP_BDTAG_Clear(mBD);
  }
  return err;
}

HRESULT AmpBuffer::updatePts(OMX_BUFFERHEADERTYPE *header,
    OMX_U32 stream_pos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_UNITSTART *pts_tag = NULL;
  err = getPts(&pts_tag);
  if (pts_tag) {
    convertUsto90K(header->nTimeStamp, &pts_tag->uPtsHigh, &pts_tag->uPtsLow);
    pts_tag->uStrmPos = stream_pos;
  } else {
    OMX_LOGE("getPts() return 0x%x", err);
  }
  return err;
}

HRESULT AmpBuffer::updateAVSPts(OMX_BUFFERHEADERTYPE *header,
    OMX_U32 stream_pos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_AVS_PTS *pts_tag = NULL;
  err = getAVSPts(&pts_tag);
  if (pts_tag) {
    convertUsto90K(header->nTimeStamp, &pts_tag->uPtsHigh, &pts_tag->uPtsLow);
  } else {
    OMX_LOGE("getAVSPts() return 0x%x", err);
  }
  return err;
}

HRESULT AmpBuffer::updateMemInfo(OMX_BUFFERHEADERTYPE *header) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_MEMINFO *mem_info = NULL;
  err = getMemInfo(&mem_info);
  if (mem_info) {
    if (header->nFlags & OMX_BUFFERFLAG_EOS) {
      mem_info->uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
    }
    mem_info->uSize = header->nFilledLen;
    mem_info->uMemOffset = header->nOffset;
    if (mem_info->uSize > 0) {
      if (mHasBackup && mData) {
        kdMemcpy(mData, header->pBuffer + header->nOffset, header->nFilledLen);
      }
      AMP_SHM_CleanCache(mem_info->uMemHandle, 0, mem_info->uSize);
    }
  } else {
    OMX_LOGE("getMemInfo() return 0x%x", err);
  }
  return err;
}

HRESULT AmpBuffer::updateMemInfo(AMP_SHM_HANDLE shm) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_MEMINFO *mem_info = NULL;
  err = getMemInfo(&mem_info);
  if (mem_info && shm) {
    mem_info->uMemHandle = shm;
  } else {
    OMX_LOGE("updateMemInfo fail.");
  }
  return err;
}

HRESULT AmpBuffer::updateAudioFrameInfo(OMX_BUFFERHEADERTYPE *header) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_AUD_FRAME_INFO *audio_frame_info = NULL;
  err = getAudioFrameInfo(&audio_frame_info);
  if (audio_frame_info) {
    if (header->nFlags & OMX_BUFFERFLAG_EOS) {
      audio_frame_info->uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
    }
    audio_frame_info->uDataLength = header->nFilledLen;
    audio_frame_info->uMemOffset[0] = header->nOffset;
    if (mHasBackup && mData) {
      kdMemcpy(mData, header->pBuffer +header->nOffset, header->nFilledLen);
    }
    AMP_SHM_CleanCache(audio_frame_info->uMemHandle, 0, audio_frame_info->uSize);
  } else {
    OMX_LOGE("getAudioFrameInfo() return 0x%x", err);
  }
  return err;
}

HRESULT AmpBuffer::updateFrameInfo(OMX_BUFFERHEADERTYPE *header) {
  HRESULT err = SUCCESS;
  AMP_BGTAG_FRAME_INFO *frame_info = NULL;
  err = getFrameInfo(&frame_info);
  if (frame_info) {
    convertUsto90K(header->nTimeStamp, &frame_info->uiPtsHigh,
        &frame_info->uiPtsLow);
  } else {
    OMX_LOGE("getFrameInfo() return 0x%x", err);
  }
  return err;
}

HRESULT AmpBuffer::getPts(AMP_BDTAG_UNITSTART **pts_tag ) {
  HRESULT err = SUCCESS;
  void *amp_pts_tag = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mPtsIndex, &amp_pts_tag);
  *pts_tag = static_cast<AMP_BDTAG_UNITSTART *>(amp_pts_tag);
  return err;
}

HRESULT AmpBuffer::getAVSPts(AMP_BDTAG_AVS_PTS **pts_tag) {
  HRESULT err = SUCCESS;
  void *amp_pts_tag = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mPtsIndex, &amp_pts_tag);
  *pts_tag = static_cast<AMP_BDTAG_AVS_PTS *>(amp_pts_tag);
  return err;
}

HRESULT AmpBuffer::getOutputPts(AMP_BDTAG_AVS_PTS **pts_tag) {
  HRESULT err = SUCCESS;
  void *amp_pts_tag = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mPtsIndex, &amp_pts_tag);
  *pts_tag = static_cast<AMP_BDTAG_AVS_PTS *>(amp_pts_tag);
  return err;
}

HRESULT AmpBuffer::getMemInfo(AMP_BDTAG_MEMINFO **mem_info) {
  HRESULT err = SUCCESS;
  void *amp_mem_info = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mMemInfoIndex, &amp_mem_info);
  *mem_info = static_cast<AMP_BDTAG_MEMINFO *>(amp_mem_info);
  return err;
}

HRESULT AmpBuffer::getCtrlInfo(AMP_BDTAG_TVPCTRLBUF **ctrl_info) {
  HRESULT err = SUCCESS;
  void *amp_ctrl_info = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mCtrlInfoIndex, &amp_ctrl_info);
  *ctrl_info = static_cast<AMP_BDTAG_TVPCTRLBUF *>(amp_ctrl_info);
  return err;
}

HRESULT AmpBuffer::getFrameInfo(AMP_BGTAG_FRAME_INFO **frame_info) {
  HRESULT err = SUCCESS;
  void *amp_frame_info = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mFrameInfoIndex, &amp_frame_info);
  *frame_info = static_cast<AMP_BGTAG_FRAME_INFO *>(amp_frame_info);
  return err;
}

HRESULT AmpBuffer::getAudioFrameInfo(
    AMP_BDTAG_AUD_FRAME_INFO **audio_frame_info) {
  HRESULT err = SUCCESS;
  void *amp_audio_frame_info = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mFrameInfoIndex, &amp_audio_frame_info);
  *audio_frame_info = static_cast<AMP_BDTAG_AUD_FRAME_INFO *>(amp_audio_frame_info);
  return err;
}

HRESULT AmpBuffer::getStreamInfo(AMP_BDTAG_STREAM_INFO **stream_info) {
  HRESULT err = SUCCESS;
  void *amp_stream_info = NULL;
  err = AMP_BDTAG_GetWithIndex(mBD, mStreamInfoIndex, &amp_stream_info);
  *stream_info = static_cast<AMP_BDTAG_STREAM_INFO *>(amp_stream_info);
  return err;
}

OmxAmpPort::OmxAmpPort(void) :
    mEs(NULL),
    mCtrl(NULL),
    mAmpDisableTvp(OMX_FALSE),
    mOmxDisableTvp(OMX_FALSE),
    mStoreMetaDataInBuffers(OMX_FALSE) {
}

OmxAmpPort::~OmxAmpPort(void) {
}

OMX_ERRORTYPE OmxAmpPort::allocateBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_PTR appPrivate,
    OMX_U32 size) {
  OMX_BUFFERHEADERTYPE * buf = new OMX_BUFFERHEADERTYPE;
  InitOmxHeader(buf);
  AMP_SHM_TYPE shm_type = AMP_SHM_DYNAMIC;
  if (getDomain() == OMX_PortDomainVideo &&
      getVideoDefinition().eCompressionFormat == OMX_VIDEO_CodingUnused) {
    shm_type = AMP_SHM_VIDEO_FB;
  }
  AmpBuffer *amp_buffer = NULL;
  OMX_BOOL needRef = (getDir() == OMX_DirOutput) ? OMX_TRUE : OMX_FALSE;
  if (mAmpDisableTvp == OMX_TRUE) {
    if (getDomain() == OMX_PortDomainVideo && mEs != NULL) {
      buf->pBuffer = static_cast<OMX_U8 *>(malloc(size));
    } else {
      amp_buffer = new AmpBuffer(shm_type, size, SHMALIGN, false, mEs, mCtrl, needRef);
      buf->pBuffer = static_cast<OMX_U8 *>(amp_buffer->getData());
    }
  } else if (mOmxDisableTvp == OMX_TRUE) {
    if (getDomain() == OMX_PortDomainAudio && getDir() == OMX_DirOutput) {
      shm_type = AMP_SHM_DYNAMIC;
      amp_buffer = new AmpBuffer(shm_type, size, SHMALIGN, false, mEs, mCtrl, needRef);
      buf->pBuffer = static_cast<OMX_U8 *>(amp_buffer->getData());
    } else {
      shm_type = AMP_SHM_SECURE_DYNAMIC;
      amp_buffer = new AmpBuffer(shm_type, size, SHMALIGN, false, mEs, mCtrl, needRef);
      buf->pBuffer = reinterpret_cast<OMX_U8 *>(amp_buffer->getShm());
    }
  } else {
    amp_buffer = new AmpBuffer(shm_type, size, SHMALIGN, false, mEs, mCtrl, needRef);
    if (getDomain() == OMX_PortDomainVideo && mEs != NULL) {
      buf->pBuffer = static_cast<OMX_U8 *>(malloc(size));
    } else {
      buf->pBuffer = static_cast<OMX_U8 *>(amp_buffer->getData());
    }
  }
  buf->nAllocLen = size;
  kdMemset(buf->pBuffer, 0, buf->nAllocLen);
  buf->pAppPrivate = appPrivate;
  buf->nInputPortIndex = (getDir()== OMX_DirInput)? getPortIndex() : 0;
  buf->nOutputPortIndex = (getDir()== OMX_DirOutput)? getPortIndex() : 0;
  buf->pPlatformPrivate = amp_buffer;
  *bufHdr = buf;
  OMX_LOGV("%s, buffer %p, nAllocLen %lu", __func__, buf, buf->nAllocLen);
  OmxBuffer * omxbuf = new OmxBuffer(buf, OMX_TRUE, OWN_BY_PLAYER);
  mBufferList.push_back(omxbuf);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpPort::freeBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  vector<OmxBuffer *>::iterator it;
  for (it = mBufferList.begin(); it != mBufferList.end(); it++) {
    if(buffer == (*it)->mBuffer) {
      break;
    }
  }
  if (it == mBufferList.end()) {
    OMX_LOGE("Can't find buffer %p",buffer);
    return OMX_ErrorBadParameter;
  }
  if (NULL != buffer->pPlatformPrivate) {
    delete static_cast<AmpBuffer *>(buffer->pPlatformPrivate);
  }
  if((*it)->mIsAllocator && getDomain() == OMX_PortDomainVideo && mEs != NULL) {
    free(buffer->pBuffer);
    buffer->pBuffer = NULL;
  }
  OMX_LOGV("Port free buffer %p", buffer);
  delete buffer;
  delete (*it);
  mBufferList.erase(it);
  setPopulated(OMX_FALSE);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpPort::useBuffer(OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_PTR appPrivate,
    OMX_U32 size,
    OMX_U8 *buffer) {
  OMX_ERRORTYPE err = OmxPortImpl::useBuffer(bufHdr, appPrivate, size, buffer);
  if (OMX_ErrorNone == err) {
    if (isInput()) {
#ifdef VDEC_USE_FRAME_IN_MODE
      if (mAmpDisableTvp == OMX_TRUE) {
        if (getDomain() == OMX_PortDomainAudio || mEs == NULL) {
          (*bufHdr)->pPlatformPrivate =
              new AmpBuffer(AMP_SHM_DYNAMIC, size, SHMALIGN, true, mEs, mCtrl, OMX_FALSE);
        } else {
          (*bufHdr)->pPlatformPrivate = NULL;
        }
      } else {
        (*bufHdr)->pPlatformPrivate =
            new AmpBuffer(AMP_SHM_DYNAMIC, size, SHMALIGN, true, mEs, mCtrl, OMX_FALSE);
      }
#else
      if (mAmpDisableTvp == OMX_TRUE) {
        if (getDomain() == OMX_PortDomainAudio) {
          (*bufHdr)->pPlatformPrivate =
              new AmpBuffer(AMP_SHM_DYNAMIC, size, SHMALIGN, true, mEs, mCtrl, OMX_FALSE);
        } else {
          (*bufHdr)->pPlatformPrivate = NULL;
        }
      } else {
        if (getDomain() == OMX_PortDomainAudio || mCtrl != NULL) {
          (*bufHdr)->pPlatformPrivate =
              new AmpBuffer(AMP_SHM_DYNAMIC, size, SHMALIGN, true, mEs, mCtrl, OMX_FALSE);
        } else {
          (*bufHdr)->pPlatformPrivate = NULL;
        }
      }
#endif
    } else {
      if (mStoreMetaDataInBuffers == OMX_TRUE) {
        (*bufHdr)->pPlatformPrivate = new AmpBuffer(NULL, size, OMX_TRUE);
      } else {
        (*bufHdr)->pPlatformPrivate = new AmpBuffer(buffer, size, OMX_TRUE);
        (*bufHdr)->pBuffer = (OMX_U8 *)static_cast<AmpBuffer *>(
            (*bufHdr)->pPlatformPrivate)->getData();
      }
    }
  }
  return err;
}

OMX_U32 OmxAmpPort::getFormatHeadSize() {
  return 0;
}

void OmxAmpPort::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
}

AMP_BD_ST * OmxAmpPort::getBD(OMX_U32 index) {
  if (index >= mBufferList.size()) {
    return NULL;
  }
  AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
      mBufferList[index]->mBuffer->pPlatformPrivate);
  if (amp_buffer) {
    return amp_buffer->getBD();
  }
  return NULL;
}

AmpBuffer * OmxAmpPort::getAmpBuffer(OMX_U32 index) {
  if (index >= mBufferList.size()) {
    return NULL;
  }
  return static_cast<AmpBuffer *>(
      mBufferList[index]->mBuffer->pPlatformPrivate);
}

AMP_BD_ST * OmxAmpPort::getBD(OMX_BUFFERHEADERTYPE *bufHdr) {
  AmpBuffer *amp_buffer =  static_cast<AmpBuffer *>(
      bufHdr->pPlatformPrivate);
  if (amp_buffer) {
    return amp_buffer->getBD();
  }
  return NULL;
}

OMX_BUFFERHEADERTYPE * OmxAmpPort::getBufferHeader(AMP_BD_ST *bd) {
  vector<OmxBuffer *>::iterator it;
  for (it = mBufferList.begin(); it != mBufferList.end(); it++) {
    AmpBuffer *amp_buffer = static_cast<AmpBuffer *>(
        (*it)->mBuffer->pPlatformPrivate);
    if (amp_buffer && amp_buffer->getBD() == bd) {
      return (*it)->mBuffer;
    }
  }
  if (it == mBufferList.end()) {
    OMX_LOGE("Can't find buffer %p", bd);
  }
  return NULL;
}

TimeStampRecover::TimeStampRecover() {
  mMutex = kdThreadMutexCreate(KD_NULL);
}

TimeStampRecover::~TimeStampRecover() {
  if (mMutex) {
    clear();
    kdThreadMutexFree(mMutex);
    mMutex = NULL;
  }
}

void TimeStampRecover::clear() {
  kdThreadMutexLock(mMutex);
  mVector.clear();
  kdThreadMutexUnlock(mMutex);
}

void TimeStampRecover::insertTimeStamp(OMX_TICKS in_ts) {
  kdThreadMutexLock(mMutex);
  if (!mVector.empty()) {
    vector<OMX_TICKS>::iterator it = mVector.end();
    for(; it != mVector.begin(); it--) {
      if (in_ts >= *(it - 1)) {
        break;
      }
    }
    mVector.insert(it, in_ts);
    kdThreadMutexUnlock(mMutex);
    return;
  }
  mVector.push_back(in_ts);
  kdThreadMutexUnlock(mMutex);
}

void TimeStampRecover::findTimeStamp(
    UINT32 target_ts_high, UINT32 target_ts_low, OMX_TICKS *out_ts) {
  if (target_ts_high & TIME_STAMP_VALID_MASK == 0) {
    OMX_LOGW("Invalid timestamp");
    return;
  }
  OMX_TICKS target_ts_90k = (target_ts_high & 0x7FFFFFFFLL) << 32 | target_ts_low;
  OMX_TICKS target_ts = target_ts_90k * 100 / 9;

  kdThreadMutexLock(mMutex);
  vector<OMX_TICKS>::iterator it = mVector.begin();
  while(it != mVector.end()) {
    if (*it >= target_ts) {
      break;
    }
    it = mVector.erase(it);
  }
  if (it != mVector.end() && *it * 9 / 100 == target_ts_90k) {
    *out_ts = *it;
  } else {
    OMX_LOGD("Can't find target timestamp %lld", target_ts);
    *out_ts = target_ts;
  }
  kdThreadMutexUnlock(mMutex);
}

} ;  // namespace berlin
