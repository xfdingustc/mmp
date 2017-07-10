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

#define LOG_TAG "OmxAudioDecoder"

#include <OmxAudioDecoder.h>
#include <sys/prctl.h>

using namespace android;
namespace mmp {

#define OMX_ROLE_AUDIO_DECODER_MP3  "audio_decoder.mp3"
#define OMX_ROLE_AUDIO_DECODER_AAC  "audio_decoder.aac"
#define OMX_ROLE_AUDIO_DECODER_VORBIS "audio_decoder.vorbis"


OmxAudioDecoder::OmxAudioDecoder(OMX_STRING name) :
    OmxComponentImpl(name),
    mInited(OMX_FALSE),
    mFFmpegDecoder(NULL),
    mCodecSpecficDataSent(OMX_FALSE),
    mEOS(OMX_FALSE) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;

  mFFmpegDecoder = (FFMpegAudioDecoder*)new FFMpegAudioDecoder();
}

OmxAudioDecoder::~OmxAudioDecoder() {
  if (mFFmpegDecoder != NULL) {
    delete mFFmpegDecoder;
    mFFmpegDecoder = NULL;
  }
}

OMX_ERRORTYPE OmxAudioDecoder::initRole() {
  OMX_LOGD("");
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    OMX_LOGD("");
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_MP3, 17)) {
    OMX_LOGD("");
    addRole(OMX_ROLE_AUDIO_DECODER_MP3);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_AAC, 17)) {
    OMX_LOGD("");
    addRole(OMX_ROLE_AUDIO_DECODER_AAC);
  } else if (!strncmp(role_name, OMX_ROLE_AUDIO_DECODER_VORBIS, 20)) {
    OMX_LOGD("");
    addRole(OMX_ROLE_AUDIO_DECODER_VORBIS);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAudioDecoder::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_MP3, 17)) {
    OMX_LOGD("");
    mInputPort = new OmxMp3Port(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_AAC, 17)) {
    OMX_LOGD("");
    mInputPort = new OmxAACPort(kAudioPortStartNumber, OMX_DirInput);
  } else if (!strncmp(mActiveRole, OMX_ROLE_AUDIO_DECODER_VORBIS, 20)) {
    OMX_LOGD("");
    mInputPort = new OmxVorbisPort(kAudioPortStartNumber, OMX_DirInput);
  }
  addPort(mInputPort);

  mOutputPort = new OmxAudioPort(kAudioPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);

  return err;
}

OMX_ERRORTYPE OmxAudioDecoder::getParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (mInputPort->getPortIndex() == audio_param->nPortIndex) {
      } else {
        if (audio_param->nIndex >= 1) {
          err = OMX_ErrorNoMore;
          break;
        }
        OMX_AUDIO_PORTDEFINITIONTYPE &a_def = mOutputPort->getAudioDefinition();
        audio_param->eEncoding = a_def.eEncoding;
      }
      break;
    }
    case OMX_IndexParamAudioMp3: {
      OMX_AUDIO_PARAM_MP3TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP3TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3) {
        *codec_param = static_cast<OmxMp3Port*>(port)->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioAac: {
      OMX_AUDIO_PARAM_AACPROFILETYPE* aacParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AACPROFILETYPE*>(params);
      err = CheckOmxHeader(aacParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(aacParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
        *aacParams = ((OmxAACPort*)(port))->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioVorbis: {
      OMX_AUDIO_PARAM_VORBISTYPE* vorbisParams =
          reinterpret_cast<OMX_AUDIO_PARAM_VORBISTYPE*>(params);
      err = CheckOmxHeader(vorbisParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(vorbisParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingVORBIS) {
        *vorbisParams = ((OmxVorbisPort*)(port))->getCodecParam();
      }
      break;
    }
    case OMX_IndexParamAudioPcm: {
      OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
          reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE *>(params);
      err = CheckOmxHeader(pcmParams);
      if (OMX_ErrorNone != err) {
        OMX_LOGD("error check pcmParams");
        break;
      }
      OmxPortImpl* port = getPort(pcmParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      OMX_LOGD("pcmparam size %d struc size %d", pcmParams->nSize,
          sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
      if (pcmParams->nPortIndex == 0) {
        // input port
        if (port->getDomain() == OMX_PortDomainAudio &&
            (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
            port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
          *pcmParams = static_cast<OmxPcmPort*>(port)->getCodecParam();
          OMX_LOGD("pcmParams->nPortIndex = %d, pcmParams->bInterleaved = %d, "
              "pcmParams->nChannels = %d",
              pcmParams->nPortIndex, pcmParams->bInterleaved, pcmParams->nChannels);
        }
      } else if (pcmParams->nPortIndex == 1) {
        // output port
        if (port->getDomain() == OMX_PortDomainAudio &&
            (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
            port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
          *pcmParams = static_cast<OmxAudioPort*>(port)->getPcmOutParam();
          OMX_LOGD("pcmParams->nPortIndex = %d, pcmParams->bInterleaved = %d, "
              "pcmParams->nChannels = %d",
              pcmParams->nPortIndex, pcmParams->bInterleaved, pcmParams->nChannels);
        }
      } else {
        OMX_LOGE("Error pcm port index %d", pcmParams->nPortIndex);
      }
      break;
    }
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxAudioDecoder::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamAudioPortFormat: {
      OMX_AUDIO_PARAM_PORTFORMATTYPE* audio_param =
          reinterpret_cast<OMX_AUDIO_PARAM_PORTFORMATTYPE*>(params);
      err = CheckOmxHeader(audio_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(audio_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio) {
        port->setAudioParam(audio_param);
        port->updateDomainDefinition();
      }
      break;
    }
    case OMX_IndexParamAudioMp3: {
      OMX_AUDIO_PARAM_MP3TYPE* codec_param =
          reinterpret_cast<OMX_AUDIO_PARAM_MP3TYPE*>(params);
      err = CheckOmxHeader(codec_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(codec_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingMP3) {
        static_cast<OmxMp3Port*>(port)->setCodecParam(codec_param);
        (static_cast<OmxMp3Port *>(mOutputPort))->SetOutputParameter(
            codec_param->nSampleRate, codec_param->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioAac: {
      OMX_AUDIO_PARAM_AACPROFILETYPE* aacParams =
          reinterpret_cast<OMX_AUDIO_PARAM_AACPROFILETYPE*>(params);
      err = CheckOmxHeader(aacParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(aacParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingAAC) {
        static_cast<OmxAACPort*>(port)->setCodecParam(aacParams);
        (static_cast<OmxAACPort *>(mOutputPort))->SetOutputParameter(
            aacParams->nSampleRate, aacParams->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioVorbis: {
      OMX_AUDIO_PARAM_VORBISTYPE* vorbisParams =
          reinterpret_cast<OMX_AUDIO_PARAM_VORBISTYPE*>(params);
      err = CheckOmxHeader(vorbisParams);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(vorbisParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingVORBIS) {
        static_cast<OmxVorbisPort*>(port)->setCodecParam(vorbisParams);
        (static_cast<OmxVorbisPort *>(mOutputPort))->SetOutputParameter(
            vorbisParams->nSampleRate, vorbisParams->nChannels);
      }
      break;
    }
    case OMX_IndexParamAudioPcm: {
      OMX_AUDIO_PARAM_PCMMODETYPE* pcmParams =
          reinterpret_cast<OMX_AUDIO_PARAM_PCMMODETYPE*>(params);
      OMX_LOGD("update pcmParams: nPortIndex = %d, bInterleaved = %d, nBitPerSample = %d, "
          "nChannels = %d, nSamplingRate = %d",
          pcmParams->nPortIndex, pcmParams->bInterleaved, pcmParams->nBitPerSample,
          pcmParams->nChannels, pcmParams->nSamplingRate);
      err = CheckOmxHeader(pcmParams);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("line %d\n", __LINE__);
        break;
      }
      OmxPortImpl* port = getPort(pcmParams->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainAudio &&
          (port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingPCM ||
          port->getAudioDefinition().eEncoding == OMX_AUDIO_CodingG711)) {
        if (pcmParams->nPortIndex == 0) {
          static_cast<OmxPcmPort*>(port)->setCodecParam(pcmParams);
          (static_cast<OmxAudioPort *>(mOutputPort))->SetOutputParameter(
              pcmParams->nSamplingRate);
        } else if (pcmParams->nPortIndex == 1) {
          static_cast<OmxAudioPort*>(port)->setPcmOutParam(pcmParams);
          OMX_LOGD("successfully update PCM out param");
        } else {
          OMX_LOGE("Error pcm port index %d", pcmParams->nPortIndex);
        }
      }
      break;
    }
    default:
      return OmxComponentImpl::setParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxAudioDecoder::processBuffer() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *in_buf = NULL, *out_buf = NULL;

  if (mEOS) {
    OMX_LOGD("EOS reached, waiting for stop");
    return OMX_ErrorNoMore;
  }

  if (mInputPort->isEmpty() || mOutputPort->isEmpty()) {
    // No enough resource
    return OMX_ErrorNoMore;
  }

  in_buf = mInputPort->popBuffer();

  // Process codec config data
  if (in_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    if (mCodecSpecficDataSent) {
      vector<CodecSpeficDataType>::iterator it;
      while (!mCodecSpeficData.empty()) {
        it = mCodecSpeficData.begin();
        if (it->data != NULL)
          free(it->data);
        mCodecSpeficData.erase(it);
      }
    }
    CodecSpeficDataType extra_data;
    extra_data.data = malloc(in_buf->nFilledLen);
    extra_data.size = in_buf->nFilledLen;
    char *pData = reinterpret_cast<char *>(in_buf->pBuffer) + in_buf->nOffset;
    memcpy(extra_data.data, in_buf->pBuffer + in_buf->nOffset,
        in_buf->nFilledLen);
    mCodecSpeficData.push_back(extra_data);
    in_buf->nFilledLen = 0;
    in_buf->nOffset = 0;
    returnBuffer(mInputPort, in_buf);
    return err;
  }

  if (in_buf->nFlags & OMX_BUFFERFLAG_EOS) {
    OMX_LOGD("meet EOS in in_buf, waiting for stop");
    mEOS = OMX_TRUE;
  }

  if (mInited == OMX_FALSE) {
    OMX_U32 codec_data_size = 0;
    vector<CodecSpeficDataType>::iterator it;
    for (it = mCodecSpeficData.begin();
         it != mCodecSpeficData.end(); it++) {
      codec_data_size += it->size;
    }
    OMX_U8 * data = (OMX_U8*)malloc(codec_data_size);
    OMX_U32 offset = 0;
    for (it = mCodecSpeficData.begin();
         it != mCodecSpeficData.end(); it++) {
      OMX_LOGD("Merge extra data %p, size %d", it->data, it->size);
      memcpy(data + offset, it->data, it->size);
      offset += it->size;
    }
    OmxPortImpl *port = (mPortMap.find(1))->second;
    OMX_AUDIO_PARAM_PCMMODETYPE &pcmParams = static_cast<OmxAudioPort*>(port)->getPcmOutParam();
    OMX_LOGD("pcmParams on outport: nPortIndex = %d, bInterleaved = %d, nBitPerSample = %d, "
          "nChannels = %d, nSamplingRate = %d",
          pcmParams.nPortIndex, pcmParams.bInterleaved, pcmParams.nBitPerSample,
          pcmParams.nChannels, pcmParams.nSamplingRate);
    err = mFFmpegDecoder->init(getCodecType(),
        pcmParams.nBitPerSample, pcmParams.nChannels, pcmParams.nSamplingRate,
        data, codec_data_size);
    if (err != OMX_ErrorNone) {
      OMX_LOGE("Failed to init ffmpeg decoder!");
      postEvent(OMX_EventError, 0, 0);
      return err;
    }
    mInited = OMX_TRUE;
  }

  // Get output buffer
  out_buf = mOutputPort->popBuffer();

  if (in_buf->nFilledLen > 0) {
    OMX_U32 consume_size=0;
    err = mFFmpegDecoder->decode(in_buf->pBuffer + in_buf->nOffset, in_buf->nFilledLen,
          in_buf->nTimeStamp, out_buf->pBuffer + out_buf->nOffset,
          &(out_buf->nFilledLen), &consume_size,
          &(out_buf->nTimeStamp));
    if (err != OMX_ErrorNone) {
      OMX_LOGE("Failed to decode, just return buffer to client.");
      returnBuffer(mInputPort, in_buf);
      returnBuffer(mOutputPort, out_buf);
      return OMX_ErrorInsufficientResources;
    }

    if (in_buf->nFilledLen >= consume_size) {
      in_buf->nFilledLen -= consume_size;
      in_buf->nOffset += consume_size;
    }
  }

  if (in_buf->nFilledLen == 0) {
    OMX_LOGV("Input buffer %p empty done", in_buf);
    in_buf->nOffset = 0;
    returnBuffer(mInputPort, in_buf);
  }

  if (mEOS == OMX_TRUE || out_buf->nFilledLen !=0) {
    if (mEOS) {
      OMX_LOGD("Audio EOS reached, post event EOS!");
      out_buf->nFlags |= OMX_BUFFERFLAG_EOS;
    }
    //OMX_LOGD("Output buffer %p, pts %lld(%fs) fill done",
    //    out_buf, out_buf->nTimeStamp, out_buf->nTimeStamp / (float)1000000);
    returnBuffer(mOutputPort, out_buf);
  }

  return err;
}

OMX_ERRORTYPE OmxAudioDecoder::flush() {
  return mFFmpegDecoder->flush();
}

OMX_ERRORTYPE OmxAudioDecoder::componentDeInit(
    OMX_HANDLETYPE hComponent) {
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxAudioDecoder *adec = static_cast<OmxAudioDecoder *>(comp);
  delete adec;
  return OMX_ErrorNone;
}

}

