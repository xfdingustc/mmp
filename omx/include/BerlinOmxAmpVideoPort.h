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


#ifndef BERLIN_OMX_AMP_VIDEO_PORT_H_
#define BERLIN_OMX_AMP_VIDEO_PORT_H_

extern "C" {
#include <amp_buf_desc.h>
#include <amp_client_support.h>
#include <amp_component.h>
#include <OSAL_api.h>
}
#include <BerlinOmxAmpPort.h>

using namespace std;

namespace berlin {

#define SWAPSHORT(x) ((((x) & 0xff) << 8) + ((x) >> 8))

#pragma pack(1)
typedef struct _MRVL_COMMON_PACKAGE_Hdr {
  UINT16 start_code_1;
  UINT8  start_code_2;
  UINT8  packet_type;
  UINT16 reserved;
  UINT8  marker_byte_1;
  UINT16 packet_payload_len_msb;
  UINT8  marker_byte_2;
  UINT16 packet_payload_len_lsb;
  UINT8  marker_byte_3;
  UINT16 packet_padding_len;
  UINT8  marker_byte_4;
} MRVL_COMMON_PACKAGE_Hdr;

typedef struct _MRVL_WMV3_FRAME_Hdr {
  UINT8  frame_size_0;
  UINT8  frame_size_1;
  UINT8  frame_size_2;
  UINT8  key;
  UINT32 time_stamp;
} MRVL_WMV3_FRAME_Hdr;

typedef struct _MRVL_WMV3_SEQUENCE_Hdr {
  UINT32 start_code;
  UINT32 mark_code_04;
  UINT32 struct_c;
  UINT32 struct_a_1;
  UINT32 struct_a_2;
  UINT32 mark_code_0c;
  UINT32 struct_b_1;
  UINT32 struct_b_2;
  UINT32 struct_b_3;
} MRVL_WMV3_SEQUENCE_Hdr;
#pragma  pack()

typedef struct  _MRVL_DIVX_SEQUENCE_Hdr {
  uint32_t    start_code: 24;
  uint32_t    packet_type: 8;
  uint16_t    reserved;
  uint8_t     marker_byte_0;
  uint16_t    packet_payload_len_msb;
  uint8_t     marker_byte_1;
  uint16_t    packet_payload_len_lsb;
  uint8_t     marker_byte_2;
  uint16_t    packet_padding_len;
  uint8_t     marker_byte_3;
  uint16_t    pic_dis_width;
  uint16_t    pic_dis_height;
}__attribute__ ((packed)) MRVL_DIVX_SEQUENCE_Hdr;

typedef struct  _MRVL_DIVX_FRAME_Hdr {
  uint32_t    start_code: 24;
  uint32_t    packet_type: 8;
  uint16_t    reserved;
  uint8_t     marker_byte_0;
  uint16_t    packet_payload_len_msb;
  uint8_t     marker_byte_1;
  uint16_t    packet_payload_len_lsb;
  uint8_t     marker_byte_2;
  uint16_t    packet_padding_len;
  uint8_t     marker_byte_3;
}__attribute__ ((packed)) MRVL_DIVX_FRAME_Hdr;

class OmxAmpVideoPort : public OmxAmpPort{
public:
  OmxAmpVideoPort();
  OmxAmpVideoPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpVideoPort();
  void updateDomainParameter() {
    mVideoParam.eColorFormat = getVideoDefinition().eColorFormat;
    mVideoParam.eCompressionFormat = getVideoDefinition().eCompressionFormat;
    mVideoParam.xFramerate = getVideoDefinition().xFramerate;
    mVideoParam.nPortIndex = getPortIndex();
  };
  void updateDomainDefinition() {
    mDefinition.format.video.eColorFormat = mVideoParam.eColorFormat;
    mDefinition.format.video.eCompressionFormat = mVideoParam.eCompressionFormat;
    mDefinition.format.video.xFramerate = mVideoParam.xFramerate;
  }
  virtual OMX_U32 getFormatHeadSize();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);
  inline void setCodecDataSize(OMX_U32 size) {mCodecDataSize = size;
      mIsFirstFrame = OMX_TRUE;};
  void clearMemCtrlIndex();

  OMX_ERRORTYPE configDrm(OMX_U32 drm_type, AMP_SHM_HANDLE *es_handle);
  OMX_ERRORTYPE releaseDrm();
  OMX_ERRORTYPE updateCtrlInfo(OMX_U32 bytes, OMX_BOOL isEncrypted,
      OMX_U64 sampleId, OMX_U64 offset);
  OMX_ERRORTYPE updateCtrlInfo(OMX_U32 bytes, OMX_BOOL isEncrypted,
      OMX_U8 *ivKey);
  OMX_ERRORTYPE updateMemInfo(OMX_BUFFERHEADERTYPE *header);
  HRESULT clearBdTag(AMP_BD_ST *bd);
  HRESULT addDmxPtsTag(AMP_BD_ST *bd,
      OMX_BUFFERHEADERTYPE *header, OMX_U32 stream_pos);
  HRESULT addMemInfoTag(AMP_BD_ST *bd, OMX_U32 offset,
    OMX_U32 size, OMX_BOOL eos);
  HRESULT addMemInfoTag(AMP_BD_ST *bd, OMX_BOOL eos);
  HRESULT updateMemInfoTag(AMP_BD_ST *bd, OMX_U32 offset, OMX_U32 size);
  HRESULT addDmxCryptoCtrlInfoTag(AMP_BD_ST *bd, OMX_U32 stream_pos);
  HRESULT addDmxCtrlInfoTag(AMP_BD_ST *bd,
      OMX_U64 datacount, OMX_U32 bytes, OMX_BOOL isEncrypted,
      OMX_U64 sampleId, OMX_U64 block_offset, OMX_U64 byte_offset, OMX_U8 *ivKey);
  OMX_ERRORTYPE popBdFromBdChain(AMP_BD_HANDLE *pbd);
  OMX_ERRORTYPE pushBdToBdChain(AMP_BD_HANDLE bd);
  UINT32 getBdChainNum();
  void updatePushedBytes(OMX_U32 size);
  void updatePushedCtrls(OMX_U32 size);
  inline OMX_U32 getCtrlIndex() {return mPushedCtrlIndex;};
  inline OMX_U32 getEsOffset() {return mPushedOffset;};

  void InitalSecurePadding(TEEC_Context *context); //only be used when do padding for secure buffer
  void FinalizeSecurePadding();

public:
  OMX_U32 mFormatHeadSize;
  OMX_U32 mCodecDataSize;
  OMX_BOOL mIsFirstFrame;
  OMX_BOOL mIsDrm;
  OMX_U32 mDrmType;
  OMX_U32 mPaddingSize;
  OMX_S32 mPrePaddingSize;

protected:
  union {
    OMX_VIDEO_PARAM_AVCTYPE avc;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4;
    OMX_VIDEO_PARAM_H263TYPE h263;
    OMX_VIDEO_WMVFORMATTYPE wmv;
    OMX_VIDEO_PARAM_MPEG2TYPE mpeg2;
    OMX_VIDEO_PARAM_RVTYPE rv;
  } mCodecParam;

private:
  OMX_U32 mPushedBytes;
  OMX_U32 mPushedOffset;
  OMX_U32 mPushedCtrls;
  OMX_U32 mPushedCtrlIndex;
  OMX_U32 mPayloadOffset;
  OMX_U8 *mEsUnit, *mVirtEsAddr;
  AMPDMX_CTRL_UNIT *mCtrlUnit, *mVirtCtrlAddr;
  AMP_BDCHAIN *mBDChain;
  KDThreadMutex *mEsLock;
  KDThreadMutex *mCtrlLock;

  TEEC_Context *pContext; //only be used when do padding for secure buffer
  AMP_SHM_HANDLE mPadHandle;
  void *mPadVirtualAddr;
  void *mPadPhysicalAddr;
};

class OmxAmpAvcPort : public OmxAmpVideoPort {
public :
   OmxAmpAvcPort(OMX_U32 index, OMX_DIRTYPE dir);
   virtual ~OmxAmpAvcPort();
   inline OMX_VIDEO_PARAM_AVCTYPE& getCodecParam() {return mCodecParam.avc;};
   inline void setCodecParam(OMX_VIDEO_PARAM_AVCTYPE *codec_param) {
     mCodecParam.avc = *codec_param;
   }
 };

class OmxAmpH263Port : public OmxAmpVideoPort {
public :
  OmxAmpH263Port() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
    InitOmxHeader(&mCodecParam.h263);
    mCodecParam.h263.nPortIndex = mDefinition.nPortIndex;
  }
  OmxAmpH263Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpH263Port();
  inline OMX_VIDEO_PARAM_H263TYPE getCodecParam() {
    return mCodecParam.h263;
  };
  inline void setCodecParam(OMX_VIDEO_PARAM_H263TYPE *codec_param) {
    mCodecParam.h263 = *codec_param;
  }
};

class OmxAmpMpeg2Port : public OmxAmpVideoPort {
public :
  OmxAmpMpeg2Port() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
  }
  OmxAmpMpeg2Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpMpeg2Port();
};

class OmxAmpMpeg4Port : public OmxAmpVideoPort {
public :
  OmxAmpMpeg4Port() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
    InitOmxHeader(&mCodecParam.mpeg4);
    mCodecParam.mpeg4.nPortIndex = mDefinition.nPortIndex;
  }
  OmxAmpMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpMpeg4Port();
  inline OMX_VIDEO_PARAM_MPEG4TYPE getCodecParam() {return mCodecParam.mpeg4;};
  inline void setCodecParam(OMX_VIDEO_PARAM_MPEG4TYPE *codec_param) {
    mCodecParam.mpeg4 = *codec_param;
  }
};

class OmxAmpMsMpeg4Port : public OmxAmpVideoPort {
public :
  OmxAmpMsMpeg4Port() {
    mDefinition.format.video.eCompressionFormat =
        static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingMSMPEG4);
  }
  OmxAmpMsMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpMsMpeg4Port();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);

private:
  OMX_U8 *pMsmpeg4Hdr;
  MRVL_DIVX_SEQUENCE_Hdr *pSeqHeader;
  MRVL_DIVX_FRAME_Hdr *pFrameHdr;
};

class OmxAmpMjpegPort : public OmxAmpVideoPort {
public :
  OmxAmpMjpegPort() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
  }
  OmxAmpMjpegPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpMjpegPort();
};

class OmxAmpVP8Port : public OmxAmpVideoPort {
public :
  OmxAmpVP8Port() {
    mDefinition.format.video.eCompressionFormat =
        static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVP8);
  }
  OmxAmpVP8Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpVP8Port();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);

private:
  MRVL_COMMON_PACKAGE_Hdr *pVP8Hdr;
};

class OmxAmpVC1Port : public OmxAmpVideoPort {
public :
  OmxAmpVC1Port() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingVC1;
  }
  OmxAmpVC1Port(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpVC1Port();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);

private:
  OMX_U8 *pVc1Hdr;
};

class OmxAmpWMVPort : public OmxAmpVideoPort {
public :
  OmxAmpWMVPort() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
  }
  OmxAmpWMVPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpWMVPort();
  virtual void formatEsData(OMX_BUFFERHEADERTYPE *header, OMX_BOOL isPadding,
      OMX_BOOL isSecure);

private:
  OMX_U8 *pWmvHdr;
  MRVL_COMMON_PACKAGE_Hdr *pSeqComHdr;
  MRVL_WMV3_SEQUENCE_Hdr *pSeqHeader;
  MRVL_COMMON_PACKAGE_Hdr *pFraComHdr;
  MRVL_WMV3_FRAME_Hdr *pFrameHdr;
};

class OmxAmpRVPort : public OmxAmpVideoPort {
public :
  OmxAmpRVPort() {
    mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingRV;
  }
  OmxAmpRVPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxAmpRVPort();
  inline OMX_VIDEO_PARAM_RVTYPE& getCodecParam() {return mCodecParam.rv;};
  inline void setCodecParam(OMX_VIDEO_PARAM_RVTYPE *codec_param) {
    mCodecParam.rv = *codec_param;
  }
};

class OmxAmpHevcPort : public OmxAmpVideoPort {
public :
   OmxAmpHevcPort() {
    mDefinition.format.video.eCompressionFormat =
        static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingHEVC);
   }
   OmxAmpHevcPort(OMX_U32 index, OMX_DIRTYPE dir);
   virtual ~OmxAmpHevcPort();
 };


}; // namespace berlin
#endif // BERLIN_OMX_AMP_VIDEO_PORT_H_
