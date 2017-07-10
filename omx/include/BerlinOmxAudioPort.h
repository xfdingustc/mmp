#ifndef BERLIN_OMX_AUDIO_PORT_H
#define BERLIN_OMX_AUDIO_PORT_H

#include "BerlinOmxPortImpl.h"

using namespace std;

namespace berlin {
  class OmxAudioPort : public OmxPortImpl {
 public :
    OmxAudioPort() {
      InitOmxHeader(&mAudioParam);
    };
    OmxAudioPort(OMX_U32 index, OMX_DIRTYPE dir) {
      mDefinition.nPortIndex = index;
      mDefinition.eDir = dir;
      mDefinition.eDomain = OMX_PortDomainAudio;
      mDefinition.format.audio.pNativeRender = NULL;
      mDefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
      mDefinition.format.audio.eEncoding = OMX_AUDIO_CodingAutoDetect;
      InitOmxHeader(&mAudioParam);
    };

    //virtual ~OmxAudioPort();

    void updateDomainParameter() {
      mAudioParam.eEncoding = getAudioDefinition().eEncoding;
    };
    void updateDomainDefinition() {
      getAudioDefinition().eEncoding = mAudioParam.eEncoding;
    }

 protected:
    union {
      OMX_AUDIO_PARAM_AACPROFILETYPE aac;
      OMX_AUDIO_PARAM_MP3TYPE mp3;
      OMX_AUDIO_PARAM_WMATYPE wma;
      OMX_AUDIO_PARAM_VORBISTYPE vorbis;
      OMX_AUDIO_PARAM_SBCTYPE sbc;
      OMX_AUDIO_PARAM_AMRTYPE amr;
#ifdef ENABLE_EXT_RA
      OMX_AUDIO_PARAM_EXT_RATYPE ra;
#endif
      OMX_AUDIO_PARAM_PCMMODETYPE pcm;
      OMX_AUDIO_PARAM_ADPCMTYPE adpcm;
    } mCodecParam;
  };

  class OmxAacPort : public OmxAudioPort {
 public :
    OmxAacPort(OMX_DIRTYPE dir);
    OmxAacPort(OMX_U32 index, OMX_DIRTYPE dir);
    //virtual ~OmxAacPort();
  };

  class OmxMp3Port : public OmxAudioPort {
 public :
    OmxMp3Port(OMX_U32 index, OMX_DIRTYPE dir) {
      mDefinition.nPortIndex = index;
      mDefinition.eDir = dir;
      mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
      updateDomainParameter();
      InitOmxHeader(&mCodecParam.mp3);
      mCodecParam.mp3.nPortIndex = mDefinition.nPortIndex;
    }
    OmxMp3Port(OMX_DIRTYPE dir);
    //virtual ~OmxMp3Port();
 private:
    //OMX_AUDIO_PARAM_MP3TYPE mCodecParam;
  };

} // namespace berlin

#endif // BERLIN_OMX_AUDIO_PORT_H
