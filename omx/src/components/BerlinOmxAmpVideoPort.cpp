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


#define LOG_TAG "OmxAmpVideoPort"
#include <BerlinOmxAmpVideoPort.h>

namespace berlin {

static const unsigned int FramePaddingSize = (1024);
static const unsigned int kVP8PaddingLen = (64 * 1024);
static const unsigned int kDivx3PaddingLen = (64 * 1024);
static const unsigned int kInvalidFrameRate = 0;

static const unsigned int kPlayReadyType = 1;
static const unsigned int kWideVineType = 2;
static const unsigned int kWMDRMType = 3;
static const unsigned int kMarLinType = 4;
static const unsigned int drm_PlayReadyPayloadCtrl = 0x11;
static const unsigned int drm_WidevinePayloadCtrl = 0x13;
static const unsigned int es_buf_size = 8 * 1024 * 1024;
static const unsigned int es_buf_pad_size = 4 * 1024;
static const unsigned int es_bd_num = 160;
static const unsigned int ctrl_buf_num = 2000;
static const unsigned int ctrl_buf_size = 2000 * sizeof(AMPDMX_CTRL_UNIT);
//Regarded to the interlaced streams, we just use 0x88 for padding now.
static const unsigned int eof_padding[] =
{
    0xff010000, 0x04000081,
    0x88888801, 0x88888888,
    0x88888888, 0x88888888,
    0x88888888, 0x88888888,
};
static const unsigned int uf_padding[] =
{
    0xff010000, 0x0400008f,
    0x8888880f, 0x88888888,
    0x88888888, 0x88888888,
    0x88888888, 0x88888888,
};
static const unsigned int eos_padding[] =
{
    0xff010000, 0x04000080,
    0x88888800, 0x88888888,
    0x88888888, 0x88888888,
    0x88888888, 0x88888888,
};

OmxAmpVideoPort::OmxAmpVideoPort() :
    mFormatHeadSize(0),
    mCodecDataSize(0),
    mIsFirstFrame(OMX_FALSE),
    mIsDrm(OMX_FALSE),
    mEsLock(NULL),
    mCtrlLock(NULL),
    mPadHandle(NULL),
    mPadVirtualAddr(NULL),
    mPadPhysicalAddr(NULL),
    mDrmType(0),
    mPaddingSize(0),
    mPrePaddingSize(0),
    mPushedBytes(0),
    mPushedOffset(0),
    mPushedCtrls(0),
    mPushedCtrlIndex(0),
    mPayloadOffset(0) {
  InitOmxHeader(&mVideoParam);
}

OmxAmpVideoPort::OmxAmpVideoPort(OMX_U32 index, OMX_DIRTYPE dir) :
    mFormatHeadSize(0),
    mCodecDataSize(0),
    mIsFirstFrame(OMX_FALSE),
    mIsDrm(OMX_FALSE),
    mEsLock(NULL),
    mCtrlLock(NULL),
    mPadHandle(NULL),
    mPadVirtualAddr(NULL),
    mPadPhysicalAddr(NULL),
    mDrmType(0),
    mPaddingSize(0),
    mPrePaddingSize(0),
    mPushedBytes(0),
    mPushedOffset(0),
    mPushedCtrls(0),
    mPushedCtrlIndex(0),
    mPayloadOffset(0) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  mDefinition.format.video.pNativeRender = NULL;
  mDefinition.format.video.nFrameWidth = 720;
  mDefinition.format.video.nFrameHeight = 480;
  mDefinition.format.video.nStride =  720;
  mDefinition.format.video.nSliceHeight =  480;
  mDefinition.format.video.nBitrate = 64000;
  mDefinition.format.video.xFramerate = kInvalidFrameRate;
  mDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  mDefinition.format.video.eColorFormat = OMX_COLOR_FormatCbYCrY;
  mDefinition.format.video.pNativeWindow = NULL;
  updateDomainParameter();
  InitOmxHeader(&mVideoParam);
}

OmxAmpVideoPort::~OmxAmpVideoPort() {
}

OMX_U32 OmxAmpVideoPort::getFormatHeadSize() {
  return mFormatHeadSize;
}

void OmxAmpVideoPort::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
  OMX_U8 *buf = header->pBuffer;
  OMX_U32 inoffset = header->nOffset;
  OMX_U32 lenth = header->nFilledLen;
  if (!isPadding) {
    return;
  }
  if (lenth < FramePaddingSize) {
    mPaddingSize = FramePaddingSize - lenth;
  } else if (lenth & 31) {
    mPaddingSize = 32 - (lenth & 31);
  } else {
    mPaddingSize = 0;
  }
  if (!isSecure) {
    if (0 < mPaddingSize && mPaddingSize < 32) {
      mPaddingSize += 32;
      kdMemcpy(buf + inoffset + lenth, uf_padding, 32);
      kdMemset(buf + inoffset + lenth + 32, 0x88, mPaddingSize - 32);
      //kdMemset(buf + inoffset + lenth, 0x88, padding_size);
    } else if (32 <= mPaddingSize && mPaddingSize < FramePaddingSize) {
      kdMemcpy(buf + inoffset + lenth, uf_padding, 32);
      kdMemset(buf + inoffset + lenth + 32, 0x88, mPaddingSize - 32);
    } else if (mPaddingSize == FramePaddingSize) {
      OMX_LOGD("eos padding.");
      kdMemcpy(buf + inoffset + lenth, eos_padding, 32);
      kdMemset(buf + inoffset + lenth + 32, 0x88, mPaddingSize - 32);
    }
  } else {
      if (0 < mPaddingSize && mPaddingSize < 32) {
        mPaddingSize += 32;
        kdMemcpy(mPadVirtualAddr, uf_padding, 32);
        kdMemset(mPadVirtualAddr + 32, 0x88, mPaddingSize - 32);
      } else if (32 <= mPaddingSize && mPaddingSize < FramePaddingSize) {
        kdMemcpy(mPadVirtualAddr, uf_padding, 32);
        kdMemset(mPadVirtualAddr + 32, 0x88, mPaddingSize - 32);
      } else if (mPaddingSize == FramePaddingSize) {
        OMX_LOGD("eos padding.");
        kdMemcpy(mPadVirtualAddr, eos_padding, 32);
        kdMemset(mPadVirtualAddr + 32, 0x88, mPaddingSize - 32);
      }
      void *dst = static_cast<AmpBuffer *>(header->pPlatformPrivate)->getData();
      if (TEEC_FastMemMove(pContext, dst + header->nOffset + header->nFilledLen,
          mPadPhysicalAddr, mPaddingSize)) {
        OMX_LOGE("TEEU_MemCopy error.");
      }
  }
  header->nFilledLen = lenth + mPaddingSize;
  header->nOffset = inoffset;
}

OMX_ERRORTYPE OmxAmpVideoPort::configDrm(OMX_U32 drm_type,
    AMP_SHM_HANDLE *es_handle) {
  OMX_LOGD("Secure configDrm");
  HRESULT err = SUCCESS;
  err = AMP_SHM_Allocate(AMP_SHM_DYNAMIC, es_buf_size + es_buf_pad_size, 32, &mEs);
  CHECKAMPERR(err);
  err = AMP_SHM_GetVirtualAddress(mEs, 0, (VOID **)&mVirtEsAddr);
  CHECKAMPERR(err);
  mEsLock= kdThreadMutexCreate(KD_NULL);
  if (mAmpDisableTvp == OMX_FALSE) {
    err = AMP_SHM_Allocate(AMP_SHM_DYNAMIC, ctrl_buf_size, 1, &mCtrl);
    CHECKAMPERR(err);
    err = AMP_SHM_GetVirtualAddress(mCtrl, 0, (VOID **)&mVirtCtrlAddr);
    CHECKAMPERR(err);
    mCtrlLock = kdThreadMutexCreate(KD_NULL);
  }
  *es_handle = mEs;
  err = AMP_BDCHAIN_Create(true, &mBDChain);
  CHECKAMPERR(err);
  OMX_U32 i;
  AMP_BD_HANDLE hBD;
  for (i = 0; i < es_bd_num; i++) {
    err = AMP_BD_Allocate(&hBD);
    CHECKAMPERR(err);
    err = AMP_BDCHAIN_PushItem(mBDChain, hBD);
    CHECKAMPERR(err);
  }
  mIsDrm = OMX_TRUE;
  mDrmType = drm_type;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoPort::releaseDrm() {
  OMX_LOGD("Secure releaseDrm");
  HRESULT err = SUCCESS;
  if (mIsDrm) {
    err = AMP_SHM_Release(mEs);
    if (SUCCESS != err) {
        OMX_LOGE("Secure release mEs err.");
    }
    if (mEsLock) {
        kdThreadMutexFree(mEsLock);
        mEsLock = NULL;
    }
    if (mAmpDisableTvp == OMX_FALSE) {
      err = AMP_SHM_Release(mCtrl);
      if (SUCCESS != err) {
          OMX_LOGE("Secure release mCtrl err.");
      }
      if (mCtrlLock) {
          kdThreadMutexFree(mCtrlLock);
          mCtrlLock = NULL;
      }
    }
    OMX_U32 freed_bd = 0;
    AMP_BD_HANDLE hBD;
    while (AMP_BDCHAIN_PopItem(mBDChain, &hBD) == SUCCESS) {
        if (AMP_BD_Free(hBD) != SUCCESS) {
            OMX_LOGE("Secure failed to release BD %p\n", hBD);
        }
        freed_bd++;
    }
    if (freed_bd != es_bd_num) {
      OMX_LOGE("Secure there are %d bds not be freed.", es_bd_num - freed_bd);
    }
    err = AMP_BDCHAIN_Destroy(mBDChain);
    if (SUCCESS != err) {
       OMX_LOGE("Secure destory mBDChain err.");
    }
  } else {
    OMX_LOGE("Secure fail to release drm, mIsDrm is NULL.");
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpVideoPort::updateCtrlInfo(OMX_U32 bytes,
    OMX_BOOL isEncrypted, OMX_U64 sampleId, OMX_U64 offset) {
  kdThreadMutexLock(mCtrlLock);
  if (ctrl_buf_num == mPushedCtrls) {
    OMX_LOGW("Secure wait for ctrl bufs, can not be here...");
    kdThreadMutexUnlock(mCtrlLock);
    return OMX_ErrorNoMore;
  }
  mCtrlUnit = mVirtCtrlAddr + mPushedCtrlIndex;
  mCtrlUnit->m_ControlInfoType = drm_PlayReadyPayloadCtrl;
  mCtrlUnit->m_Rsvd = 0;
  mCtrlUnit->m_DataCount = mPayloadOffset;
  mCtrlUnit->m_BufPosition = 0;
  mCtrlUnit->m_UserData = 0xffff;
  mCtrlUnit->m_Union.m_PrPayloadInfo.m_IsEncrypted = isEncrypted;
  mCtrlUnit->m_Union.m_PrPayloadInfo.m_PayloadSize = bytes;
  mCtrlUnit->m_Union.m_PrPayloadInfo.SampleID = sampleId;
  mCtrlUnit->m_Union.m_PrPayloadInfo.BlockOffset = offset>>4;
  mCtrlUnit->m_Union.m_PrPayloadInfo.ByteOffset = offset & 0x0f;
  mPayloadOffset += bytes;
  mPushedCtrls++;
  mPushedCtrlIndex++;
  if (ctrl_buf_num == mPushedCtrlIndex) {
    mPushedCtrlIndex = 0;
  }
  kdThreadMutexUnlock(mCtrlLock);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpVideoPort::updateCtrlInfo(OMX_U32 bytes,
    OMX_BOOL isEncrypted, OMX_U8 *ivKey) {
  kdThreadMutexLock(mCtrlLock);
  if (ctrl_buf_num == mPushedCtrls) {
    OMX_LOGW("Secure wait for ctrl bufs, can not be here...");
    kdThreadMutexUnlock(mCtrlLock);
    return OMX_ErrorNoMore;
  }
  mCtrlUnit = mVirtCtrlAddr + mPushedCtrlIndex;
  mCtrlUnit->m_ControlInfoType = drm_WidevinePayloadCtrl;
  mCtrlUnit->m_Rsvd = 0;
  mCtrlUnit->m_DataCount = mPayloadOffset;
  mCtrlUnit->m_BufPosition = 0;
  mCtrlUnit->m_UserData = 0xffff;
  mCtrlUnit->m_Union.m_WvPayloadInfo.m_IsEncrypted = isEncrypted;
  mCtrlUnit->m_Union.m_WvPayloadInfo.m_PayloadSize = bytes;
  if (ivKey) {
    kdMemcpy(mCtrlUnit->m_Union.m_WvPayloadInfo.m_IVKey, ivKey, 16);
  } else {
    kdMemset(mCtrlUnit->m_Union.m_WvPayloadInfo.m_IVKey, 0x00, 16);
  }
  mPayloadOffset += bytes;
  mPushedCtrls++;
  mPushedCtrlIndex++;
  if (ctrl_buf_num == mPushedCtrlIndex) {
    mPushedCtrlIndex = 0;
  }
  kdThreadMutexUnlock(mCtrlLock);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpVideoPort::updateMemInfo(
    OMX_BUFFERHEADERTYPE *header) {
  kdThreadMutexLock(mEsLock);
  if (header->nFilledLen < FramePaddingSize) {
    mPaddingSize = FramePaddingSize - header->nFilledLen;
  } else if (header->nFilledLen & 63) {
    mPaddingSize = 64 - (header->nFilledLen & 63);
  } else {
    mPaddingSize = 0;
  }
  OMX_U32 frame_size = header->nFilledLen + mPaddingSize;
  if (es_buf_size < mPushedBytes + frame_size) {
    OMX_LOGV("Secure wait for mem bufs...");
    kdThreadMutexUnlock(mEsLock);
    return OMX_ErrorNoMore;
  }
  mEsUnit = mVirtEsAddr + mPushedOffset;
  kdMemcpy(mEsUnit, header->pBuffer + header->nOffset,
      header->nFilledLen);
  mPushedBytes += header->nFilledLen;
  mPushedOffset += header->nFilledLen;
  if (mPaddingSize) {
    kdMemset(mEsUnit + header->nFilledLen, 0x88, mPaddingSize);
    mPushedBytes += mPaddingSize;
    mPushedOffset += mPaddingSize;
  }
  if (es_buf_size < mPushedOffset + mDefinition.nBufferSize) {
    kdMemset(mEsUnit + header->nFilledLen + mPaddingSize, 0x88,
        es_buf_size - mPushedOffset);
    mPaddingSize += es_buf_size - mPushedOffset;
    mPushedBytes += es_buf_size - mPushedOffset;
    mPushedOffset = 0;
  }
  header->nFilledLen += mPaddingSize;
  kdThreadMutexUnlock(mEsLock);
  return OMX_ErrorNone;
}

void OmxAmpVideoPort::updatePushedBytes(OMX_U32 size) {
  kdThreadMutexLock(mEsLock);
  mPushedBytes -= size;
  kdThreadMutexUnlock(mEsLock);
}

void OmxAmpVideoPort::updatePushedCtrls(OMX_U32 size) {
  kdThreadMutexLock(mCtrlLock);
  mPushedCtrls -= size;
  kdThreadMutexUnlock(mCtrlLock);
}

HRESULT OmxAmpVideoPort::clearBdTag(AMP_BD_ST *bd) {
  HRESULT err = SUCCESS;
  if (NULL != bd) {
    err = AMP_BDTAG_Clear(bd);
  }
  if (err != SUCCESS) {
    OMX_LOGE("Clear Bd Tag error %d", err);
  }
  return err;
}

HRESULT OmxAmpVideoPort::addDmxPtsTag(AMP_BD_ST *bd,
    OMX_BUFFERHEADERTYPE *header, OMX_U32 stream_pos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_DMX_CTRL_INFO dmxCtrlTag;
  OMX_LOGV("addDmxPtsTag  datacount %d", stream_pos);
  kdMemset(&dmxCtrlTag, 0, sizeof(AMP_BDTAG_DMX_CTRL_INFO));
  dmxCtrlTag.Header = {AMP_BDTAG_BS_UNITSTART_CTRL, sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
  dmxCtrlTag.m_CtrlUnit.m_ControlInfoType = 0xa0;
  dmxCtrlTag.m_CtrlUnit.m_DataCount = stream_pos;
  convertUsto90K(header->nTimeStamp,
      &dmxCtrlTag.m_CtrlUnit.m_Union.m_UnitStart.m_PtsValue.uiStampHigh,
      &dmxCtrlTag.m_CtrlUnit.m_Union.m_UnitStart.m_PtsValue.uiStampLow);
  err = AMP_BDTAG_Append(bd, (UINT8 *)&dmxCtrlTag, NULL, NULL);
  if (err != SUCCESS) {
    OMX_LOGE("Add Dmx Pts Tag error %d", err);
  }
  return err;
}

HRESULT OmxAmpVideoPort::addMemInfoTag(AMP_BD_ST *bd, OMX_U32 offset,
    OMX_U32 size, OMX_BOOL eos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_MEMINFO mem_tag;
  kdMemset(&mem_tag, 0, sizeof(AMP_BDTAG_MEMINFO));
  mem_tag.Header = {AMP_BDTAG_ASSOCIATE_MEM_INFO, sizeof(AMP_BDTAG_MEMINFO)};
  mem_tag.uMemHandle = mEs;
  mem_tag.uSize = size;
  mem_tag.uMemOffset = offset;
  if (eos) {
    OMX_LOGD("Secure add EOS flag.");
    mem_tag.uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
  }
  OMX_LOGV("Secure addMemInfoTag  uMemOffset = %d  uSize = %d",
      mem_tag.uMemOffset, mem_tag.uSize);
  err = AMP_BDTAG_Append(bd, (UINT8 *)&mem_tag, NULL, NULL);
  if (err != SUCCESS) {
    OMX_LOGE("Add Mem Info Tag error %d", err);
  }
  return err;
}

HRESULT OmxAmpVideoPort::addMemInfoTag(AMP_BD_ST *bd, OMX_BOOL eos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_MEMINFO mem_tag;
  kdMemset(&mem_tag, 0, sizeof(AMP_BDTAG_MEMINFO));
  mem_tag.Header = {AMP_BDTAG_ASSOCIATE_MEM_INFO, sizeof(AMP_BDTAG_MEMINFO)};
  mem_tag.uMemHandle = mEs;
  if (eos) {
    OMX_LOGD("Secure add EOS flag.");
    mem_tag.uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
  }
  err = AMP_BDTAG_Append(bd, (UINT8 *)&mem_tag, NULL, NULL);
  if (err != SUCCESS) {
    OMX_LOGE("add mem info tag error %d", err);
  }
  return err;
}

HRESULT OmxAmpVideoPort::updateMemInfoTag(AMP_BD_ST *bd,
    OMX_U32 offset, OMX_U32 size) {
  HRESULT err = SUCCESS;
  UINT32 i, bd_num;
  void *tag;
  err = AMP_BDTAG_GetNum(bd, &bd_num);
  CHECKAMPERRLOG(err, "get secure bd tag num.");
  for (i = 0; i < bd_num; i++) {
    err = AMP_BDTAG_GetWithIndex(bd, i, &tag);
    if (SUCCESS != err) {
      OMX_LOGE("Secure Failed to get tag %d from bd 0x%x.", i, bd->uiBDId);
      return err;
    }
    if (static_cast<AMP_BDTAG_MEMINFO *>(tag)->Header.eType ==
        AMP_BDTAG_ASSOCIATE_MEM_INFO) {
      static_cast<AMP_BDTAG_MEMINFO *>(tag)->uMemOffset = offset;
      static_cast<AMP_BDTAG_MEMINFO *>(tag)->uSize = size;
      break;
    }
  }
  if (i == bd_num) {
    OMX_LOGW("Secure no meminfo tag in the bd.");
  }
  return err;
}

HRESULT OmxAmpVideoPort::addDmxCryptoCtrlInfoTag(AMP_BD_ST *bd,
    OMX_U32 stream_pos) {
  HRESULT err = SUCCESS;
  AMP_BDTAG_DMX_CTRL_INFO ctrl_tag;
  kdMemset(&ctrl_tag, 0, sizeof(AMP_BDTAG_DMX_CTRL_INFO));
  ctrl_tag.Header = {AMP_BDTAG_BS_CRYPTO_CTRL, sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
  if (mDrmType == kPlayReadyType) {
    ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSchemeType =
        AMP_DRMSCHEME_PLAYREADY;
  } else if (mDrmType == kWideVineType) {
    ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSchemeType =
        AMP_DRMSCHEME_WIDEVINE;
  } else if (mDrmType == kMarLinType) {
    ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSchemeType =
        AMP_DRMSCHEME_MARLIN;
  } else {
    OMX_LOGE("Secure Unsupported drm type %d\n", mDrmType);
  }
  ctrl_tag.m_CtrlUnit.m_DataCount = stream_pos;
  ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiSessionID = 0;
  kdMemset(ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiKeyID, 0, 16);
  ctrl_tag.m_CtrlUnit.m_Union.m_CryptoPayloadInfo.CryptInfo.uiKeyIdLen = 4;
  err = AMP_BDTAG_Append(bd, (UINT8 *)&ctrl_tag, NULL, NULL);
  if (err != SUCCESS) {
    OMX_LOGE("Add Dmx Crypto Ctrl Tag error %d", err);
  }
  return err;
}

HRESULT OmxAmpVideoPort::addDmxCtrlInfoTag(AMP_BD_ST *bd,
    OMX_U64 datacount, OMX_U32 bytes, OMX_BOOL isEncrypted,
    OMX_U64 sampleId, OMX_U64 block_offset, OMX_U64 byte_offset, OMX_U8 *ivKey) {
  HRESULT ret;
  AMP_BDTAG_DMX_CTRL_INFO ctrl_tag;
  OMX_LOGV("addDmxCtrlInfoTag  datacount %lld   datasize %d", datacount, bytes);
  kdMemset(&ctrl_tag, 0, sizeof(AMP_BDTAG_DMX_CTRL_INFO));
  if (mDrmType == kPlayReadyType || mDrmType == kMarLinType) {
    ctrl_tag.Header = {AMP_BDTAG_PAYLOAD_PLAYREADY_CTRL,
        sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
    ctrl_tag.m_CtrlUnit.m_ControlInfoType = drm_PlayReadyPayloadCtrl;
    ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.m_IsEncrypted = isEncrypted;
    ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.m_PayloadSize = bytes;
    ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.SampleID = sampleId;
    ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.BlockOffset = block_offset;
    ctrl_tag.m_CtrlUnit.m_Union.m_PrPayloadInfo.ByteOffset = byte_offset;
  } else if (mDrmType == kWideVineType) {
    ctrl_tag.Header = {AMP_BDTAG_PAYLOAD_WIDEVINE_CTRL,
        sizeof(AMP_BDTAG_DMX_CTRL_INFO)};
    ctrl_tag.m_CtrlUnit.m_ControlInfoType = drm_WidevinePayloadCtrl;
    ctrl_tag.m_CtrlUnit.m_Union.m_WvPayloadInfo.m_IsEncrypted = isEncrypted;
    ctrl_tag.m_CtrlUnit.m_Union.m_WvPayloadInfo.m_PayloadSize = bytes;
    if (ivKey) {
      kdMemcpy(ctrl_tag.m_CtrlUnit.m_Union.m_WvPayloadInfo.m_IVKey, ivKey, 16);
    } else {
      kdMemset(ctrl_tag.m_CtrlUnit.m_Union.m_WvPayloadInfo.m_IVKey, 0x00, 16);
    }
  } else {
    OMX_LOGE("Secure Unsupported drm type %d\n", mDrmType);
  }
  ctrl_tag.m_CtrlUnit.m_Rsvd = 0;
  ctrl_tag.m_CtrlUnit.m_DataCount = datacount;
  ctrl_tag.m_CtrlUnit.m_BufPosition = 0;
  ctrl_tag.m_CtrlUnit.m_UserData = 0;
  ret =  AMP_BDTAG_Append(bd, (UINT8 *)&ctrl_tag, NULL, NULL);
  if (SUCCESS != ret) {
    OMX_LOGW("Secure fail to append ctrl bd tag, ret %d.", ret);
  }
  return ret;
}

OMX_ERRORTYPE OmxAmpVideoPort::popBdFromBdChain(AMP_BD_HANDLE *pbd) {
  UINT32 pNum;
  HRESULT err = SUCCESS;
  err = AMP_BDCHAIN_GetItemNum(mBDChain, &pNum);
  if (0 == pNum) {
    OMX_LOGV("Secure there is no bd in the bd chain.");
    *pbd = NULL;
    return OMX_ErrorNone;
  }
  err = AMP_BDCHAIN_PopItem(mBDChain, pbd);
  if (SUCCESS != err) {
    OMX_LOGE("Secure pop bd from bdchain err %d.", err);
    *pbd = NULL;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpVideoPort::pushBdToBdChain(AMP_BD_HANDLE bd) {
  HRESULT err = SUCCESS;
  err = AMP_BDCHAIN_PushItem(mBDChain, bd);
  if (SUCCESS != err) {
    OMX_LOGE("Secure push bd to bdchain err %d.", err);
  }
  return OMX_ErrorNone;
}

UINT32 OmxAmpVideoPort::getBdChainNum() {
  UINT32 pNum = 0;
  HRESULT err = SUCCESS;
  err = AMP_BDCHAIN_GetItemNum(mBDChain, &pNum);
  if (SUCCESS != err) {
    OMX_LOGE("Secure Get bd Num err %d.", err);
  }
  return pNum;
}

void OmxAmpVideoPort::clearMemCtrlIndex() {
  if (0 != mPushedBytes || 0 != mPushedCtrls) {
    OMX_LOGD("Secure there are %d mem and %d ctrl buffers not returned.",
        mPushedBytes, mPushedCtrls);
    mPushedBytes = 0;
    mPushedCtrls = 0;
  }
  mPushedCtrlIndex = 0;
  mPushedOffset = 0;
  mPayloadOffset = 0;
}

void OmxAmpVideoPort::InitalSecurePadding(TEEC_Context *context) {
  OMX_LOGD("InitalSecurePadding");
  if (context != NULL) {
    pContext = context;
  } else {
    OMX_LOGE("The Tee context is NULL.");
    return;
  }
  if (mPadHandle == NULL) {
    if (AMP_SHM_Allocate(AMP_SHM_DYNAMIC, FramePaddingSize, 32, &mPadHandle)) {
      OMX_LOGE("allocate padding SHM error.");
      return;
    }
    if (AMP_SHM_GetVirtualAddress(mPadHandle, 0, &mPadVirtualAddr)) {
      OMX_LOGE("get virtual addr of the padding SHM error.");
      return;
    }
    if (AMP_SHM_GetPhysicalAddress(mPadHandle, 0, &mPadPhysicalAddr)) {
      OMX_LOGE("get physical addr of the padding SHM error.");
      return;
    }
  } else {
    OMX_LOGD("already has allocated the padding SHM. ");
  }
}

void OmxAmpVideoPort::FinalizeSecurePadding() {
  OMX_LOGD("FinalizeSecurePadding");
  if (mPadHandle != NULL) {
    if (AMP_SHM_Release(mPadHandle)) {
      OMX_LOGE("release the padding SHM error.");
      return;
    }
    mPadHandle = NULL;
  } else {
    OMX_LOGD("already has released the padding SHM.");
  }
}

/************************
*  OmxAmpAvcPort
************************/

OmxAmpAvcPort::OmxAmpAvcPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingAVC;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.avc);
  mCodecParam.avc.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
  mCodecParam.avc.eLevel = OMX_VIDEO_AVCLevel1;
}

OmxAmpAvcPort::~OmxAmpAvcPort() {
}

/************************
*  OmxAmpH263Port
************************/
OmxAmpH263Port::OmxAmpH263Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingH263;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.h263);
  mCodecParam.h263.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
  mCodecParam.h263.eLevel = OMX_VIDEO_H263Level10;
  mCodecParam.h263.bPLUSPTYPEAllowed = OMX_FALSE;
  mCodecParam.h263.bForceRoundingTypeToZero = OMX_TRUE;
}

OmxAmpH263Port::~OmxAmpH263Port() {
}

 /************************
*  OmxAmpMpeg2Port
************************/
OmxAmpMpeg2Port::OmxAmpMpeg2Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingMPEG2;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
}

OmxAmpMpeg2Port::~OmxAmpMpeg2Port() {
}

/************************
*  OmxAmpMpeg4Port
************************/
OmxAmpMpeg4Port::OmxAmpMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mpeg4);
  mCodecParam.mpeg4.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.mpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
  mCodecParam.mpeg4.eLevel = OMX_VIDEO_MPEG4Level1;
}

OmxAmpMpeg4Port::~OmxAmpMpeg4Port() {
}

/************************
*  OmxAmpMsMpeg4Port
************************/
OmxAmpMsMpeg4Port::OmxAmpMsMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat =
      static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingMSMPEG4);
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  mFormatHeadSize = sizeof(MRVL_DIVX_SEQUENCE_Hdr) + sizeof(MRVL_DIVX_FRAME_Hdr);
  pMsmpeg4Hdr = reinterpret_cast<OMX_U8 *>(kdMalloc(mFormatHeadSize));
  if (pMsmpeg4Hdr != NULL) {
    pSeqHeader = reinterpret_cast<MRVL_DIVX_SEQUENCE_Hdr *>(pMsmpeg4Hdr);
    pSeqHeader->start_code = 0x010000;
    pSeqHeader->packet_type = 0xB0;
    pSeqHeader->reserved = 0x00;
    pSeqHeader->marker_byte_0 = 0x88;
    pSeqHeader->marker_byte_1 = 0x88;
    pSeqHeader->marker_byte_2 = 0x88;
    pSeqHeader->marker_byte_3 = 0x88;
    pSeqHeader->packet_payload_len_msb = 0x00;
    pSeqHeader->packet_payload_len_lsb = static_cast<uint16_t> (4 << 8);
    pSeqHeader->packet_padding_len = 0;
    pSeqHeader->pic_dis_width = 0;
    pSeqHeader->pic_dis_height = 0;
    pFrameHdr = reinterpret_cast<MRVL_DIVX_FRAME_Hdr *>(pMsmpeg4Hdr +
        sizeof(MRVL_DIVX_SEQUENCE_Hdr));
    pFrameHdr->start_code = 0x010000;
    pFrameHdr->packet_type = 0xB6;
    pFrameHdr->reserved = 0x00;
    pFrameHdr->marker_byte_0 = 0x88;
    pFrameHdr->marker_byte_1 = 0x88;
    pFrameHdr->marker_byte_2 = 0x88;
    pFrameHdr->marker_byte_3 = 0x88;
    pFrameHdr->packet_payload_len_msb = 0;
    pFrameHdr->packet_payload_len_lsb = 0;
    pFrameHdr->packet_padding_len = 0;
  } else {
    OMX_LOGE("kdMalloc size %d fail", mFormatHeadSize);
    pSeqHeader = NULL;
  }

}

OmxAmpMsMpeg4Port::~OmxAmpMsMpeg4Port() {
  if (NULL != pMsmpeg4Hdr) {
    kdFree(pMsmpeg4Hdr);
    pMsmpeg4Hdr = NULL;
  }
}

void OmxAmpMsMpeg4Port::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
  OMX_U8 *buf = header->pBuffer + header->nOffset;
  OMX_U32 len = header->nFilledLen - mCodecDataSize;
  OMX_U32 copy_size;
  OMX_U8 *copy_header;
  if (0 == len || pSeqHeader == NULL) {
    OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
    return;
  }
  if (mCodecDataSize || mIsFirstFrame) {
    copy_size = mFormatHeadSize;
    kdMemmove(buf + copy_size, buf + mCodecDataSize, len);
    pSeqHeader->pic_dis_width = SWAPSHORT(getVideoDefinition().nFrameWidth & 0xffff);
    pSeqHeader->pic_dis_height = SWAPSHORT(getVideoDefinition().nFrameHeight & 0xffff);
    copy_header = reinterpret_cast<OMX_U8 *>(pSeqHeader);
    mPrePaddingSize = (OMX_S32)copy_size - mCodecDataSize;
    mCodecDataSize = 0;
    mIsFirstFrame = OMX_FALSE;
    OMX_LOGD("msmpeg4 add sequence header, height = %d  width = %d.",
        pSeqHeader->pic_dis_height, pSeqHeader->pic_dis_width);
  } else {
    copy_size = sizeof(MRVL_DIVX_FRAME_Hdr);
    kdMemmove(buf + copy_size, buf, len);
    mPrePaddingSize = copy_size;
    copy_header = reinterpret_cast<OMX_U8 *>(pFrameHdr);
  }
  if (isPadding) {
    header->nFilledLen = (len + copy_size + kDivx3PaddingLen -1) & (~(kDivx3PaddingLen -1));
  } else {
    header->nFilledLen = len + copy_size;
  }
  pFrameHdr->packet_payload_len_msb = SWAPSHORT(len >> 16);
  pFrameHdr->packet_payload_len_lsb = SWAPSHORT(len & 0xffff);
  mPaddingSize = header->nFilledLen - len - copy_size;
  pFrameHdr->packet_padding_len = SWAPSHORT(mPaddingSize & 0xffff);
  kdMemcpy(buf, copy_header, copy_size);
  if (0 < mPaddingSize && mPaddingSize < 32) {
    kdMemset(buf + copy_size + len, 0x88, mPaddingSize);
  } else if (32 <= mPaddingSize && mPaddingSize < kDivx3PaddingLen) {
    kdMemcpy(buf + copy_size + len, uf_padding, 32);
    kdMemset(buf + copy_size + len + 32, 0x88, mPaddingSize - 32);
  } else if (mPaddingSize >= kDivx3PaddingLen) {
    OMX_LOGE("can not be here.");
  }
}

/************************
*  OmxAmpMjpegPort
************************/
OmxAmpMjpegPort::OmxAmpMjpegPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingMJPEG;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
}

OmxAmpMjpegPort::~OmxAmpMjpegPort() {
}

/************************
*  OmxAmpVP8Port
************************/
OmxAmpVP8Port::OmxAmpVP8Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat =
      static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVP8);
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  mFormatHeadSize = sizeof(MRVL_COMMON_PACKAGE_Hdr);
  pVP8Hdr = static_cast<MRVL_COMMON_PACKAGE_Hdr *>(kdMalloc(mFormatHeadSize));
  if (pVP8Hdr) {
    pVP8Hdr->start_code_1 = 0;
    pVP8Hdr->start_code_2 = 1;
    pVP8Hdr->packet_type = 0x12;
    pVP8Hdr->reserved = 0;
    pVP8Hdr->marker_byte_1 = 0x88;
    pVP8Hdr->marker_byte_2 = 0x88;
    pVP8Hdr->marker_byte_3 = 0x88;
    pVP8Hdr->marker_byte_4 = 0x88;
  } else {
    OMX_LOGE("kdMalloc size %d fail", mFormatHeadSize);
    pVP8Hdr = NULL;
  }
}

OmxAmpVP8Port::~OmxAmpVP8Port() {
  if (NULL != pVP8Hdr) {
    kdFree(pVP8Hdr);
    pVP8Hdr = NULL;
  }
}

void OmxAmpVP8Port::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
  OMX_U32 length = header->nFilledLen;
  if (0 == length || pVP8Hdr == NULL) {
    OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
    return;
  }
  pVP8Hdr->packet_payload_len_msb = SWAPSHORT(length >> 16);
  pVP8Hdr->packet_payload_len_lsb = SWAPSHORT(length & 0xffff);
  OMX_U8 *src = header->pBuffer + header->nOffset;
  kdMemmove(src + mFormatHeadSize, src, length);
  length += mFormatHeadSize;
  if (isPadding) {
    header->nFilledLen = (length + kVP8PaddingLen - 1) & (~(kVP8PaddingLen - 1));
  } else {
    header->nFilledLen = length;
  }
  mPrePaddingSize = mFormatHeadSize;
  mPaddingSize = header->nFilledLen - length;
  pVP8Hdr->packet_padding_len = SWAPSHORT(mPaddingSize & 0xffff);
  kdMemcpy(src, reinterpret_cast<OMX_U8 *>(pVP8Hdr), mFormatHeadSize);
  if (0 < mPaddingSize && mPaddingSize < 32) {
     kdMemset(src + length, 0x88, mPaddingSize);
  } else if (32 <= mPaddingSize && mPaddingSize < kVP8PaddingLen) {
    kdMemcpy(src + length, uf_padding, 32);
    kdMemset(src + length + 32, 0x88, mPaddingSize - 32);
  } else if (mPaddingSize >= kVP8PaddingLen) {
    OMX_LOGE("can not be here.");
  }
  OMX_LOGV("filled %ld, padding %u", header->nFilledLen, mPaddingSize);
}

/************************
*  OmxAmpVC1Port
************************/
OmxAmpVC1Port::OmxAmpVC1Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingVC1;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  mFormatHeadSize = 4;
  pVc1Hdr = static_cast<OMX_U8 *>(kdMalloc(mFormatHeadSize));
  if (pVc1Hdr) {
    pVc1Hdr[0] = 0x00;
    pVc1Hdr[1] = 0x00;
    pVc1Hdr[2] = 0x01;
    pVc1Hdr[3] = 0x0D;
  } else {
    OMX_LOGE("kdMalloc size %d fail", mFormatHeadSize);
    pVc1Hdr = NULL;
  }
}

OmxAmpVC1Port::~OmxAmpVC1Port() {
  if (NULL != pVc1Hdr) {
    kdFree(pVc1Hdr);
    pVc1Hdr = NULL;
  }
}

void OmxAmpVC1Port::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
  OMX_U8 *buf = header->pBuffer + header->nOffset + mCodecDataSize;
  OMX_U32 len = header->nFilledLen - mCodecDataSize;
  OMX_U32 copy_size;
  if (0 == len || pVc1Hdr == NULL) {
    OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
    return;
  }
  if ((mDrmType == kWMDRMType) || (buf[0]!=0) || (buf[1] != 0) || (buf[2] != 1) ||
      (buf[3] != 0x0D && buf[3] != 0x0F && buf[3] != 0x0B && buf[3] != 0x0C)) {
    kdMemmove(buf + 4, buf, len);
    kdMemcpy(buf, pVc1Hdr, 4);
    copy_size = 4;
  } else {
    copy_size = 0;
  }
  mPrePaddingSize = copy_size;
  if (mCodecDataSize) {
    OMX_LOGD("Vc1 first decoder frame.");
    mCodecDataSize = 0;
  }
  header->nFilledLen += copy_size;
  OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
}

/************************
*  OmxAmpWMVPort
************************/
OmxAmpWMVPort::OmxAmpWMVPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingWMV;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  mFormatHeadSize = sizeof(MRVL_COMMON_PACKAGE_Hdr) * 2 +
      sizeof(MRVL_WMV3_SEQUENCE_Hdr) + sizeof(MRVL_WMV3_FRAME_Hdr);
  pWmvHdr = static_cast<OMX_U8 *>(kdMalloc(mFormatHeadSize));
  if (pWmvHdr) {
    pSeqComHdr = reinterpret_cast<MRVL_COMMON_PACKAGE_Hdr *>(pWmvHdr);
    pSeqComHdr->start_code_1 = 0;
    pSeqComHdr->start_code_2 = 1;
    pSeqComHdr->packet_type = 0x10;
    pSeqComHdr->reserved = 0;
    pSeqComHdr->marker_byte_1 = 0x88;
    pSeqComHdr->packet_payload_len_msb = 0;
    pSeqComHdr->marker_byte_2 = 0x88;
    pSeqComHdr->packet_payload_len_lsb = 0x24;
    pSeqComHdr->marker_byte_3 = 0x88;
    pSeqComHdr->packet_padding_len = 0;
    pSeqComHdr->marker_byte_4 = 0x88;
    pSeqHeader = reinterpret_cast<MRVL_WMV3_SEQUENCE_Hdr *>(pWmvHdr +
        sizeof(MRVL_COMMON_PACKAGE_Hdr));
    pSeqHeader->start_code = 0xc5000000;
    pSeqHeader->mark_code_04 = 0x00000004;
    pSeqHeader->mark_code_0c = 0x0000000c;
    pSeqHeader->struct_b_1 = 0;
    pSeqHeader->struct_b_2 = 0;
    pSeqHeader->struct_b_3 = 0xffffffff;
    pFraComHdr = reinterpret_cast<MRVL_COMMON_PACKAGE_Hdr *>(pWmvHdr +
        sizeof(MRVL_COMMON_PACKAGE_Hdr) +
        sizeof(MRVL_WMV3_SEQUENCE_Hdr));
    pFraComHdr->start_code_1 = 0;
    pFraComHdr->start_code_2 = 1;
    pFraComHdr->packet_type = 0x11;
    pFraComHdr->reserved = 0;
    pFraComHdr->marker_byte_1 = 0x88;
    pFraComHdr->marker_byte_2 = 0x88;
    pFraComHdr->marker_byte_3 = 0x88;
    pFraComHdr->packet_padding_len = 0;
    pFraComHdr->marker_byte_4 = 0x88;
    pFrameHdr = reinterpret_cast<MRVL_WMV3_FRAME_Hdr *>(pWmvHdr +
        sizeof(MRVL_COMMON_PACKAGE_Hdr) * 2 +
        sizeof(MRVL_WMV3_SEQUENCE_Hdr));
  } else {
    OMX_LOGE("kdMalloc size %d fail", mFormatHeadSize);
    pSeqComHdr = NULL;
  }
}

OmxAmpWMVPort::~OmxAmpWMVPort() {
  if (NULL != pWmvHdr) {
    kdFree(pWmvHdr);
    pWmvHdr = NULL;
  }
}

void OmxAmpWMVPort::formatEsData(OMX_BUFFERHEADERTYPE *header,
    OMX_BOOL isPadding, OMX_BOOL isSecure) {
  OMX_U32 len = header->nFilledLen - mCodecDataSize;
  OMX_U8 *buf = header->pBuffer + header->nOffset;
  OMX_U32 copy_size;
  OMX_U8 *copy_header;
  if (0 == len || pSeqComHdr == NULL) {
    OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
    return;
  }
  if (mCodecDataSize) {
    copy_size = mFormatHeadSize;
    kdMemmove(buf + copy_size, buf + mCodecDataSize, len);
    pSeqHeader->struct_c = *(reinterpret_cast<OMX_U32 *>(buf));
    pSeqHeader->struct_a_1 = getVideoDefinition().nFrameHeight;
    pSeqHeader->struct_a_2 = getVideoDefinition().nFrameWidth;
    copy_header = reinterpret_cast<OMX_U8 *>(pSeqComHdr);
    mPrePaddingSize = (OMX_S32)copy_size - mCodecDataSize;
    mCodecDataSize = 0;
    OMX_LOGD("wmv add sequence header, struct_c = %d  height = %d  width = %d.",
        pSeqHeader->struct_c, pSeqHeader->struct_a_1, pSeqHeader->struct_a_2);
  } else {
    copy_size = sizeof(MRVL_COMMON_PACKAGE_Hdr) + sizeof(MRVL_WMV3_FRAME_Hdr);
    kdMemmove(buf + copy_size, buf, len);
    copy_header = reinterpret_cast<OMX_U8 *>(pFraComHdr);
    mPrePaddingSize = copy_size;
  }
  OMX_U32 frame_size = len + sizeof(MRVL_WMV3_FRAME_Hdr);
  pFraComHdr->packet_payload_len_msb = SWAPSHORT(frame_size >> 16);
  pFraComHdr->packet_payload_len_lsb = SWAPSHORT(frame_size & 0xffff);
  *(reinterpret_cast<OMX_U32 *>(pFrameHdr)) = len;
  kdMemcpy(buf, copy_header, copy_size);
  header->nFilledLen = len + copy_size;
  OmxAmpVideoPort::formatEsData(header, isPadding, isSecure);
}

/************************
*  OmxAmpRVPort
************************/
OmxAmpRVPort::OmxAmpRVPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingRV;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.rv);
  mCodecParam.rv.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.rv.eFormat = OMX_VIDEO_RVFormat8;
}

OmxAmpRVPort::~OmxAmpRVPort(){
}

/************************
*  OmxAmpHevcPort
************************/
OmxAmpHevcPort::OmxAmpHevcPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat =
      static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingHEVC);
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = kInvalidFrameRate;
  updateDomainParameter();
}

OmxAmpHevcPort::~OmxAmpHevcPort() {
}


} //namespace berlin
