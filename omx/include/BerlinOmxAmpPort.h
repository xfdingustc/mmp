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


#ifndef BERLIN_OMX_AMP_PORT_H_
#define BERLIN_OMX_AMP_PORT_H_

extern "C" {
#include <amp_buf_desc.h>
#include <amp_cbuf_helper.h>
#include <amp_client_support.h>
#include <amp_component.h>
#include <amp_dmx_bdtag.h>
#ifndef DISABLE_PLAYREADY
#include <drmmanager.h>
#endif
#include <OSAL_api.h>
#include <tee_client_api.h>
}
#include <BerlinOmxPortImpl.h>

#ifdef _ANDROID_
#include <ui/GraphicBuffer.h>
#endif

#ifdef _ANDROID_
using namespace android;
#endif

//#define VDEC_USE_FRAME_IN_MODE
//#define VDEC_USE_CBUF_STREAM_IN_MODE

#define CHECKAMPERR(err) \
  do { \
    if (SUCCESS != err) { \
      OMX_LOGE("error number %s(0x%x)", AmpError2String(err), err); \
      return static_cast<OMX_ERRORTYPE>(err); \
    } \
  } while(0)

#define CHECKAMPERRNOTRETURN(err) \
  do { \
    if (SUCCESS != err) { \
      OMX_LOGE("error number %s(0x%x)", AmpError2String(err), err); \
    } \
  } while(0)


#define CHECKAMPERRLOG(err, errlog) \
  do { \
    if (SUCCESS != err) { \
      OMX_LOGE("Failed to %s error number %s(0x%x)", errlog, AmpError2String(err), err); \
      return static_cast<OMX_ERRORTYPE>(err); \
    } \
  } while(0)

#define CHECKAMPERRLOGNOTRETURN(err, errlog) \
  do { \
    if (SUCCESS != err) { \
      OMX_LOGE("Failed to %s error number %s(0x%x)", errlog, AmpError2String(err), err); \
    } \
  } while(0)

#define CHECK_AMP_RETURN_VAL(err, errlog) \
  do { \
    if (SUCCESS != err) { \
      OMX_LOGE("Failed to %s error number %s(0x%x)", errlog, AmpError2String(err), err); \
      return static_cast<OMX_ERRORTYPE>(err); \
    } else { \
      OMX_LOGD("Successfully %s", errlog); \
    } \
  } while(0)

OMX_STRING AmpState2String(AMP_STATE state);
OMX_STRING AmpError2String(HRESULT error);
OMX_STRING AmpVideoCodec2String(AMP_MEDIA_TYPE codec);

namespace berlin {

AMP_MEDIA_TYPE getAmpVideoCodecType(OMX_VIDEO_CODINGTYPE type);
void convertUsto90K(OMX_TICKS in_pts, UINT32 *pts_high, UINT32 *pts_low);
OMX_TICKS convert90KtoUs(UINT32 pts_high, UINT32 pts_low);

typedef struct CodecExtraDataType {
  void * data;
  unsigned int size;
} CodecExtraDataType;

class CodecExtraData {
  vector<CodecExtraDataType> mDataVector;
  unsigned int mDataSize;

public:
  bool mMerged;
  CodecExtraData() {
    mDataSize = 0;
    mMerged = false;
  }

  virtual ~CodecExtraData() {
    clear();
  }

  virtual bool canUpdate(void *data, unsigned int size) {
    return true;
  }

  virtual bool canMerge() {
    return true;
  }

  void add(void *data, unsigned int size) {
    if (!canUpdate(data, size)) {
      return;
    }
    if (mMerged) {
      // mMerged will be set to false in clear()
      clear();
    }
    OMX_LOGD("add extra data %p, size %d", data, size);
    CodecExtraDataType extra_data;
    extra_data.data = kdMalloc(size);
    extra_data.size = size;
    kdMemcpy(extra_data.data, data, size);
    mDataVector.push_back(extra_data);
    mDataSize += size;
  }

  unsigned int getExtraDataSize() {
    return mDataSize;
  }

  void merge(void *outData, unsigned int size) {
    if (!canMerge()) {
      OMX_LOGW("Invaild codec extra data");
      return;
    }
    if (size < mDataSize) {
      OMX_LOGW("Not enough space for all extra data");
      return;
    }
    vector<CodecExtraDataType>::iterator it;
    unsigned int offset = 0;
    for (it = mDataVector.begin(); it != mDataVector.end(); it++) {
      OMX_LOGD("Merge extra data %p, size %d", it->data, it->size);
      kdMemcpy(outData + offset, it->data, it->size);
      offset += it->size;
    }
    mMerged = true;
  }

  void clear() {
    OMX_LOGD("clear extra data");
    vector<CodecExtraDataType>::iterator it;
    while (!mDataVector.empty()) {
      it = mDataVector.begin();
      if (it->data != NULL) {
        kdFree(it->data);
      }
      mDataVector.erase(it);
    }
    mDataSize = 0;
    mMerged = false;
  }

};  // class CodecExtraData

const char kSpsType = 0x7;
const char kPpsType = 0x8;

class AvcExtraData : public CodecExtraData {
  bool mGotSps;
  bool mGotPps;
public:
  AvcExtraData() {
    mGotSps = false;
    mGotPps = false;
  }

  virtual ~AvcExtraData() {
  }

  virtual bool canUpdate(void *data, unsigned int size) {
    OMX_LOGD("%s", __func__);
    if (data == NULL || size <= 4) {
      return false;
    }
    char nal_type = *(static_cast<char *>(data) + 4) & 0xf;
    OMX_LOGD("nal type %x", nal_type);
    if (nal_type == kSpsType) {
      mGotSps = true;
    } else if (nal_type == kPpsType ) {
      if (mGotSps == false) {
        return false;
      }
      mGotPps = true;
    }
    return true;
  }
  virtual bool canMerge() {
    OMX_LOGD("%s", __func__);
    if (mMerged) {
      return true;
    }
    if (mGotSps && mGotPps) {
      mGotSps = false;
      mGotPps = false;
      return true;
    }
    //return false;
    return true;
  }
};

class SecureCodecExtraData {
  vector<CodecExtraDataType> mDataVector;
  unsigned int mDataSize;
  TEEC_Context *pContext;

public:
  bool mMerged;
  SecureCodecExtraData(TEEC_Context *context) {
    mDataSize = 0;
    mMerged = false;
    pContext = context;
  }

  virtual ~SecureCodecExtraData() {
    clear();
  }

  void add(void *handle, unsigned int size, unsigned int offset) {
    if (mMerged) {
      // mMerged will be set to false in clear()
      clear();
    }
    OMX_LOGD("add extra handle %p, size %d, offset %d", handle, size, offset);
    CodecExtraDataType extra_data;
    AMP_SHM_HANDLE shm;
    void *srcPhyAddr;
    void *dstPhyAddr;
    if (AMP_SHM_Allocate(AMP_SHM_SECURE_DYNAMIC, size, 32, &shm)) {
      OMX_LOGE("add extra data AMP_SHM_Allocate error.");
      return;
    }
    if (AMP_SHM_GetPhysicalAddress((AMP_SHM_HANDLE)handle,  offset, &srcPhyAddr)) {
      OMX_LOGE("add extra data Get src PhysicalAddress error.");
      return;
    }
    if (AMP_SHM_GetPhysicalAddress(shm,  0, &dstPhyAddr)) {
      OMX_LOGE("add extra data Get dst PhysicalAddress error.");
      return;
    }
    if (TEEC_FastMemMove(pContext, dstPhyAddr, srcPhyAddr, size)) {
      OMX_LOGE("add extra data TEEU_MemCopy error.");
      return;
    }
    extra_data.data = (void *)shm;
    extra_data.size = size;
    mDataVector.push_back(extra_data);
    mDataSize += size;
  }

  unsigned int getExtraDataSize() {
    return mDataSize;
  }

  void merge(void *handle, unsigned int size, unsigned int offset) {
    if (size < mDataSize) {
      OMX_LOGW("Not enough space for all extra data");
      return;
    }
    void *srcPhyAddr;
    void *dstPhyAddr;
    vector<CodecExtraDataType>::iterator it;
    unsigned int mergedsize = 0;
    if (AMP_SHM_GetPhysicalAddress((AMP_SHM_HANDLE)handle, offset, &dstPhyAddr)) {
      OMX_LOGE("Merge extra data Get dst PhysicalAddress error.");
      return;
    }
    for (it = mDataVector.begin(); it != mDataVector.end(); it++) {
      OMX_LOGD("Merge extra data %p, size %d", it->data, it->size);
      if (AMP_SHM_GetPhysicalAddress((AMP_SHM_HANDLE)(it->data),  0, &srcPhyAddr)) {
        OMX_LOGE("Merge extra data Get src PhysicalAddress error.");
        return;
      }
      if (TEEC_FastMemMove(pContext, dstPhyAddr + mergedsize, srcPhyAddr, it->size)) {
        OMX_LOGE("Merge extra data TEEU_MemCopy error.");
        return;
      }
      mergedsize += it->size;
    }
    mMerged = true;
  }

  void clear() {
    OMX_LOGD("clear extra data");
    vector<CodecExtraDataType>::iterator it;
    while (!mDataVector.empty()) {
      it = mDataVector.begin();
      if (it->data != NULL) {
        AMP_SHM_Release((AMP_SHM_HANDLE)(it->data));
      }
      mDataVector.erase(it);
    }
    mDataSize = 0;
    mMerged = false;
  }

};  // class CodecExtraData

class AmpBuffer {
public:
  AmpBuffer();
  AmpBuffer(AMP_SHM_TYPE type, int size, int align, bool hasBackup,
      AMP_SHM_HANDLE es, AMP_SHM_HANDLE ctrl, OMX_BOOL needRef);
  AmpBuffer(void *data, int size, OMX_BOOL needRef);
  ~AmpBuffer();
  HRESULT addPtsTag() ;
  HRESULT addAVSPtsTag();
  HRESULT addPtsTag(OMX_BUFFERHEADERTYPE *header, OMX_U32 stream_pos);
  HRESULT addMemInfoTag();
  HRESULT addMemInfoTag(OMX_BUFFERHEADERTYPE *header, OMX_U32 offset);
  HRESULT addFrameInfoTag();
  HRESULT addAudioFrameInfoTag(OMX_U32 channelnum = 2,
      OMX_U32 bitdepth = 16, OMX_U32 samplerate = 44100);
  HRESULT addCtrlInfoTag(OMX_U32 total, OMX_U32 start, OMX_U32 size);
  HRESULT addDmxCtrlInfoTag(OMX_U32 datacount, OMX_U32 bytes,
      OMX_BOOL isEncrypted, OMX_U64 sampleId, OMX_U64 block_offset,
      OMX_U64 byte_offset, OMX_U8 drm_type);
  HRESULT addDmxUnitStartTag(OMX_BUFFERHEADERTYPE *header,
      OMX_U32 stream_pos);
  HRESULT addCryptoCtrlInfoTag(OMX_U8 type, OMX_U32 stream_pos);
  HRESULT addStreamInfoTag();
  HRESULT clearBdTag();
  HRESULT updatePts(OMX_BUFFERHEADERTYPE *header, OMX_U32 stream_pos);
  HRESULT updateAVSPts(OMX_BUFFERHEADERTYPE *header, OMX_U32 stream_pos);
  HRESULT updateMemInfo(OMX_BUFFERHEADERTYPE *header);
  HRESULT updateMemInfo(AMP_SHM_HANDLE shm);
  HRESULT updateAudioFrameInfo(OMX_BUFFERHEADERTYPE *header);
  HRESULT updateFrameInfo(OMX_BUFFERHEADERTYPE *header);
  HRESULT getAudioFrameInfo(AMP_BDTAG_AUD_FRAME_INFO **audio_frame_info);
  HRESULT getStreamInfo(AMP_BDTAG_STREAM_INFO **stream_info);
  HRESULT getPts(AMP_BDTAG_UNITSTART **pts_tag );
  HRESULT getAVSPts(AMP_BDTAG_AVS_PTS **pts_tag);
  HRESULT getOutputPts(AMP_BDTAG_AVS_PTS **pts_tag);
  HRESULT getMemInfo(AMP_BDTAG_MEMINFO **mem_info);
  HRESULT getCtrlInfo(AMP_BDTAG_TVPCTRLBUF **ctrl_info);
  HRESULT getFrameInfo(AMP_BGTAG_FRAME_INFO **frame_info);
  inline void *getData() {return mData;};
  inline void setShm(AMP_SHM_HANDLE shm) {mShm = shm;};
  inline AMP_SHM_HANDLE getShm() {return mShm;};
  inline AMP_BD_HANDLE getBD() {return mBD;};
  inline void setBD(AMP_BD_HANDLE bd) {mBD = bd;};
  inline void setGfx(void *gfx) {mGfx = gfx;};
  inline void *getGfx() {return mGfx;};

private:
  void *mData;
  bool mIsAllocator;
  bool mHasBackup;
  AMP_SHM_HANDLE mShm, mCtrl;
  AMP_BD_HANDLE mBD;
  UINT8 mMemInfoIndex;
  UINT8 mPtsIndex;
  UINT8 mFrameInfoIndex;
  UINT8 mCtrlInfoIndex;
  UINT8 mStreamInfoIndex;
  void *mGfx;
  OMX_BOOL mIsBdRef;
};

class OmxAmpPort : public OmxPortImpl {
public :
  OmxAmpPort();
  virtual ~OmxAmpPort();
  virtual OMX_ERRORTYPE allocateBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_PTR appPrivate,
      OMX_U32 size);
  virtual OMX_ERRORTYPE freeBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE useBuffer(OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_PTR appPrivate,
      OMX_U32 size,
      OMX_U8 *buffer);

  virtual OMX_U32 getFormatHeadSize();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);

  AmpBuffer * getAmpBuffer(OMX_U32 index);
  AMP_BD_ST * getBD(OMX_U32 index);
  AMP_BD_ST * getBD(OMX_BUFFERHEADERTYPE *bufHdr);
  OMX_BUFFERHEADERTYPE * getBufferHeader(AMP_BD_ST *bd);
  inline OMX_ERRORTYPE setAmpHandle(AMP_COMPONENT handle) {
    mAmpHandle = handle;
    return OMX_ErrorNone;
  };
  inline AMP_COMPONENT getAmpHandle() {return mAmpHandle;};

  AMP_COMPONENT mAmpHandle;
  OMX_U32 mAmpPortIndex;
  AMP_SHM_HANDLE mEs, mCtrl;
  OMX_BOOL mAmpDisableTvp;
  OMX_BOOL mOmxDisableTvp;
  OMX_BOOL mStoreMetaDataInBuffers;
};

class TimeStampRecover {
public:
  TimeStampRecover();
  ~TimeStampRecover();
  void clear();
  void insertTimeStamp(OMX_TICKS in_ts);
  void findTimeStamp(UINT32 in_ts_high, UINT32 in_ts_low, OMX_TICKS *out_ts);
  KDThreadMutex *mMutex;
  vector<OMX_TICKS> mVector;
};

};  // namespace berlin

#endif  // BERLIN_OMX_AMP_PORT_H_

