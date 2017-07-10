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

/**
 * @file BerlinOmxAmpAudioPort.h
 * @author  csheng@marvell.com
 * @date 2013.01.17
 * @brief  Define the class OmxAmpAudioPort
 */


#ifndef BERLIN_OMX_AMP_AUDIO_PORT_H_
#define BERLIN_OMX_AMP_AUDIO_PORT_H_

extern "C" {
#include <amp_buf_desc.h>
#include <amp_client_support.h>
#include <amp_component.h>
#include <OSAL_api.h>
}
#include <BerlinOmxPortImpl.h>
#include <BerlinOmxAmpPort.h>

using namespace std;

namespace berlin {
#define DMX_ES_BUF_SIZE             (2*1024*1024)
#define DMX_ES_BUF_PAD_SIZE         (4*1024)

struct AudioFormatType {
  OMX_AUDIO_CODINGTYPE eCompressionFormat;
};

class OmxAmpAudioPort : public OmxAmpPort {
  public:
    OmxAmpAudioPort();
    OmxAmpAudioPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpAudioPort();
    virtual OMX_U8 formatAudEsData(OMX_BUFFERHEADERTYPE *header);
    inline OMX_AUDIO_PARAM_PORTFORMATTYPE getAudioParam() {
      return mAudioParam;
    };
    inline void setAudioParam(OMX_AUDIO_PARAM_PORTFORMATTYPE
        *audio_param) {
      mAudioParam.eEncoding = audio_param->eEncoding;
    };
    inline OMX_AUDIO_PARAM_PCMMODETYPE& getPcmOutParam() {
      return mCodecParam.pcmout;
    };
    inline void setPcmOutParam(
        OMX_AUDIO_PARAM_PCMMODETYPE *codec_param) {
      OMX_LOGD("omx pcm out size %d player pcm size %d struc size %d",
        mCodecParam.pcmout.nSize,
        codec_param->nSize, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if (mCodecParam.pcmout.nSize != codec_param->nSize) {
        OMX_U32 nSize = mCodecParam.pcmout.nSize;
        kdMemcpy(&mCodecParam.pcmout, codec_param, codec_param->nSize);
        mCodecParam.pcmout.nSize = nSize;
        if (mCodecParam.pcmout.nChannels == 1 && isTunneled() == OMX_FALSE) {
          mCodecParam.pcmout.nChannels = 2;
        }
      } else {
        mCodecParam.pcmout = *codec_param;
      }
    }
    void updateDomainParameter() {
      mAudioParam.eEncoding = getAudioDefinition().eEncoding;
      mAudioParam.nPortIndex = getPortIndex();
    };
    void updateDomainDefinition() {
      mDefinition.format.audio.eEncoding = mAudioParam.eEncoding;
    };
    virtual OMX_U32 getFormatHeadSize();
    OMX_ERRORTYPE configMemInfo(AMP_SHM_HANDLE es_handle);
    OMX_ERRORTYPE updateMemInfo(OMX_BUFFERHEADERTYPE *header);
    void SetOutputParameter(OMX_U32 mSampleRate = kDefaultOutputSampleRate,
        OMX_U32 mChannel = kDefaultOutputChannels);
    void GetOutputParameter(OMX_U32 *mSampleRate, OMX_U32 *mChannel);
  public:
    OMX_U32 mFormatHeadSize;
    OMX_U32 mPushedBytes;
    OMX_U32 mPushedOffset;
    OMX_U8 *mEsVirtAddr;
  protected:
    union {
      OMX_AUDIO_PARAM_AACPROFILETYPE aac;
#ifdef OMX_AudioExt_h
      OMX_AUDIO_PARAM_MP1TYPE mp1;
      OMX_AUDIO_PARAM_MP2TYPE mp2;
      OMX_AUDIO_PARAM_AC3TYPE ac3;
      OMX_AUDIO_PARAM_DTSTYPE dts;
#endif
      OMX_AUDIO_PARAM_MP3TYPE mp3;
      OMX_AUDIO_PARAM_VORBISTYPE vorbis;
      OMX_AUDIO_PARAM_PCMMODETYPE pcm;
      OMX_AUDIO_PARAM_WMATYPE wma;
      OMX_AUDIO_PARAM_AMRTYPE amr;
      OMX_AUDIO_PARAM_PCMMODETYPE pcmout;
#ifdef ENABLE_EXT_RA
      OMX_AUDIO_PARAM_EXT_RATYPE ra;
#endif
    } mCodecParam;
  private:
    OMX_AUDIO_PARAM_PORTFORMATTYPE mAudioParam;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
    static const OMX_U8 kDefaultOutputChannels = 2;
    static const OMX_U32 kDefaultOutputSampleRate = 44100;
};

class OmxAmpAacPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpAacPort(OMX_DIRTYPE dir);
    OmxAmpAacPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpAacPort();
    inline OMX_AUDIO_PARAM_AACPROFILETYPE& getCodecParam() {
      return mCodecParam.aac;
    };
    virtual inline void setCodecParam(
        OMX_AUDIO_PARAM_AACPROFILETYPE *codec_param, OMX_U8 *esds = NULL);
    virtual OMX_U8 formatAudEsData(OMX_BUFFERHEADERTYPE *header);
  private:
    OMX_BOOL mIsADTS;
    OMX_BOOL mIsFirst;
    OMX_U32 mInputBufferCount;
    OMX_BOOL mSignalledError;
    OMX_S64 mAnchorTimeUs;
    OMX_S64 mNumSamplesOutput;
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
    static const OMX_U8 kADTSHeaderSize = 7;
    OMX_U8 *mAACHeader;
    enum {
        NONE,
        AWAITING_DISABLED,
        AWAITING_ENABLED
    } mOutputPortSettingsChange;
};

#ifdef OMX_AudioExt_h
class OmxAmpMp1Port : public OmxAmpAudioPort {
  public:
    explicit OmxAmpMp1Port(OMX_DIRTYPE dir);
    OmxAmpMp1Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpMp1Port();
    inline OMX_AUDIO_PARAM_MP1TYPE& getCodecParam() {
      return mCodecParam.mp1;
    };
    inline void setCodecParam(OMX_AUDIO_PARAM_MP1TYPE *codec_param) {
      mCodecParam.mp1 = *codec_param;
    }
  private:
    static const OMX_U32 kNumBuffers = 4;
    static const OMX_U32 kOutputBufferSize = 4608 * 2;
    static const OMX_U32 kPVMP3DecoderDelay = 529;  // frames
};

class OmxAmpMp2Port : public OmxAmpAudioPort {
  public:
    explicit OmxAmpMp2Port(OMX_DIRTYPE dir);
    OmxAmpMp2Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpMp2Port();
    inline OMX_AUDIO_PARAM_MP2TYPE& getCodecParam() {
      return mCodecParam.mp2;
    };
    inline void setCodecParam(OMX_AUDIO_PARAM_MP2TYPE *codec_param) {
      mCodecParam.mp2 = *codec_param;
    }
  private:
    static const OMX_U32 kNumBuffers = 4;
    static const OMX_U32 kOutputBufferSize = 4608 * 2;
    static const OMX_U32 kPVMP3DecoderDelay = 529;  // frames
};

class OmxAmpAc3Port : public OmxAmpAudioPort {
  public:
    explicit OmxAmpAc3Port(OMX_DIRTYPE dir);
    OmxAmpAc3Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpAc3Port();
    inline OMX_AUDIO_PARAM_AC3TYPE& getCodecParam() {
      return mCodecParam.ac3;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_AC3TYPE *codec_param) {
      mCodecParam.ac3 = *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};

class OmxAmpDtsPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpDtsPort(OMX_DIRTYPE dir);
    OmxAmpDtsPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpDtsPort();
    inline OMX_AUDIO_PARAM_DTSTYPE& getCodecParam() {
      return mCodecParam.dts;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_DTSTYPE *codec_param) {
      mCodecParam.dts = *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};
#endif

class OmxAmpMp3Port : public OmxAmpAudioPort {
  public:
    explicit OmxAmpMp3Port(OMX_DIRTYPE dir);
    OmxAmpMp3Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpMp3Port();
    inline OMX_AUDIO_PARAM_MP3TYPE& getCodecParam() {
      return mCodecParam.mp3;
    };
    inline void setCodecParam(OMX_AUDIO_PARAM_MP3TYPE *codec_param) {
      mCodecParam.mp3 = *codec_param;
    }
  private:
    static const OMX_U32 kNumBuffers = 4;
    static const OMX_U32 kOutputBufferSize = 4608 * 2;
    static const OMX_U32 kPVMP3DecoderDelay = 529;   // frames
};

class OmxAmpVorbisPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpVorbisPort(OMX_DIRTYPE dir);
    OmxAmpVorbisPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpVorbisPort();
    inline OMX_AUDIO_PARAM_VORBISTYPE& getCodecParam() {
      return mCodecParam.vorbis;
    };
    virtual inline void setCodecParam(
        OMX_AUDIO_PARAM_VORBISTYPE *codec_param) {
      mCodecParam.vorbis = *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};

class OmxAmpPcmPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpPcmPort(OMX_DIRTYPE dir);
    OmxAmpPcmPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpPcmPort();
    inline OMX_AUDIO_PARAM_PCMMODETYPE& getCodecParam() {
      return mCodecParam.pcm;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_PCMMODETYPE *codec_param) {
      OMX_LOGD("omx pcm in size %d player pcm size %d struc size %d",
        mCodecParam.pcm.nSize,
        codec_param->nSize, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if (mCodecParam.pcm.nSize != codec_param->nSize) {
        OMX_U32 nSize = mCodecParam.pcm.nSize;
        kdMemcpy(&mCodecParam.pcm, codec_param, mCodecParam.pcm.nSize);
        mCodecParam.pcm.nSize = nSize;
      } else {
        mCodecParam.pcm = *codec_param;
      }
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};

class OmxAmpWmaPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpWmaPort(OMX_DIRTYPE dir);
    OmxAmpWmaPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpWmaPort();
    inline OMX_AUDIO_PARAM_WMATYPE& getCodecParam() {
      return mCodecParam.wma;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_WMATYPE *codec_param) {
      mCodecParam.wma = *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};

class OmxAmpAmrPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpAmrPort(OMX_DIRTYPE dir);
    OmxAmpAmrPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpAmrPort();
    inline OMX_AUDIO_PARAM_AMRTYPE& getCodecParam() {
      return mCodecParam.amr;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_AMRTYPE *codec_param) {
      mCodecParam.amr = *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};

#ifdef ENABLE_EXT_RA
class OmxAmpRaPort : public OmxAmpAudioPort {
  public:
    explicit OmxAmpRaPort(OMX_DIRTYPE dir);
    OmxAmpRaPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAmpRaPort();
    inline OMX_AUDIO_PARAM_EXT_RATYPE& getCodecParam() {
      return mCodecParam.ra;
    };
    inline void setCodecParam(
        OMX_AUDIO_PARAM_EXT_RATYPE *codec_param) {
      mCodecParam.ra= *codec_param;
    };
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
};
#endif
}
#endif  // BERLIN_OMX_AMP_AUDIO_PORT_H_
