/*
**                Copyright 2014, MARVELL SEMICONDUCTOR, LTD.
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

#ifndef OMX_AUDIO_PORT_H
#define OMX_AUDIO_PORT_H

#include "OmxPortImpl.h"

using namespace std;

namespace mmp {


class OmxAudioPort : public OmxPortImpl {
  public :
    OmxAudioPort() {
      InitOmxHeader(&mVideoParam);
    };
    OmxAudioPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAudioPort();
    void updateDomainParameter() {
    };
    void updateDomainDefinition() {
    }
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
        memcpy(&mCodecParam.pcmout, codec_param, codec_param->nSize);
        mCodecParam.pcmout.nSize = nSize;
        if (mCodecParam.pcmout.nChannels == 1 && isTunneled() == OMX_FALSE) {
          mCodecParam.pcmout.nChannels = 2;
        }
      } else {
        mCodecParam.pcmout = *codec_param;
      }
    }
    void SetOutputParameter(OMX_U32 mSampleRate = kDefaultOutputSampleRate,
        OMX_U32 mChannel = kDefaultOutputChannels);

  protected:
    union {
      OMX_AUDIO_PARAM_MP3TYPE mp3;
      OMX_AUDIO_PARAM_AACPROFILETYPE aac;
      OMX_AUDIO_PARAM_VORBISTYPE vorbis;
      OMX_AUDIO_PARAM_PCMMODETYPE pcm;
      OMX_AUDIO_PARAM_PCMMODETYPE pcmout;
    } mCodecParam;

  private:
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8 kMaxOutPutChannels = 8;
    static const OMX_U8 kDefaultOutputChannels = 2;
    static const OMX_U32 kDefaultOutputSampleRate = 44100;
};

class OmxMp3Port : public OmxAudioPort {
  public :
    OmxMp3Port(OMX_DIRTYPE dir);
    OmxMp3Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxMp3Port();
    inline OMX_AUDIO_PARAM_MP3TYPE& getCodecParam() {return mCodecParam.mp3;};
    inline void setCodecParam(OMX_AUDIO_PARAM_MP3TYPE *codec_param) {
      mCodecParam.mp3 = *codec_param;
    }
  private:
    static const OMX_U32 kNumBuffers = 4;
    static const OMX_U32 kOutputBufferSize = 4608 * 2;
};

class OmxAACPort : public OmxAudioPort {
  public :
    OmxAACPort(OMX_DIRTYPE dir);
    OmxAACPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAACPort();
    inline OMX_AUDIO_PARAM_AACPROFILETYPE& getCodecParam() {return mCodecParam.aac;};
    inline void setCodecParam(OMX_AUDIO_PARAM_AACPROFILETYPE *codec_param) {
      mCodecParam.aac = *codec_param;
    }
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8  kMaxOutPutChannels = 8;
};

class OmxVorbisPort : public OmxAudioPort {
  public :
    OmxVorbisPort(OMX_DIRTYPE dir);
    OmxVorbisPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxVorbisPort() {}
    inline OMX_AUDIO_PARAM_VORBISTYPE& getCodecParam() {return mCodecParam.vorbis;};
    inline void setCodecParam(OMX_AUDIO_PARAM_VORBISTYPE *codec_param) {
      mCodecParam.vorbis = *codec_param;
    }
  private:
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
    static const OMX_U8  kMaxOutPutChannels = 8;
};

class OmxPcmPort : public OmxAudioPort {
  public:
    explicit OmxPcmPort(OMX_DIRTYPE dir);
    OmxPcmPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxPcmPort() {}
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
        memcpy(&mCodecParam.pcm, codec_param, mCodecParam.pcm.nSize);
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

}

#endif
