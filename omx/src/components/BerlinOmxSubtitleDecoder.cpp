/*
**                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
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

#define LOG_TAG "BerlinOmxSubtitleDecoder"

#include "BerlinOmxSubtitleDecoder.h"

#include "BerlinOmxSubtitlePort.h"

#include "SrtDecoder.h"

namespace berlin {

OmxSubtitleDecoder::OmxSubtitleDecoder()
  : mThread(NULL) {
}

OmxSubtitleDecoder::OmxSubtitleDecoder(OMX_STRING name)
  : mThread(NULL),
    mThreadAttr(NULL),
    mShouldExit(OMX_FALSE),
    mSubDecoder(NULL) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
}

OmxSubtitleDecoder::~OmxSubtitleDecoder() {
  delete mSubDecoder;
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxSubtitleDecoder::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_SUBTITLE_DECODER_SRT,
      strlen(OMX_ROLE_SUBTITLE_DECODER_SRT))) {
    addRole(OMX_ROLE_SUBTITLE_DECODER_SRT);
    mSubDecoder = new SrtDecoder();
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxSubtitleDecoder::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNoMore;
  if (!strncmp(mActiveRole, OMX_ROLE_SUBTITLE_DECODER_SRT, strlen(OMX_ROLE_SUBTITLE_DECODER_SRT))) {
    mInputPort = new OmxSrtPort(kTextPortStartNumber, OMX_DirInput);
  }

  addPort(mInputPort);
  mOutputPort = new OmxSubtitlePort(kTextPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);

  OMX_PARAM_PORTDEFINITIONTYPE def;
  mInputPort->getDefinition(&def);
  def.nBufferCountActual = 64;
  mInputPort->setDefinition(&def);


  mOutputPort->getDefinition(&def);
  def.nBufferCountActual = 64;
  mOutputPort->setDefinition(&def);
  return err;
}

OMX_ERRORTYPE OmxSubtitleDecoder::start() {
  mThread = kdThreadCreate(mThreadAttr, threadEntry,(void *)this);
  return OMX_ErrorNone;
}


OMX_ERRORTYPE OmxSubtitleDecoder::stop() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mShouldExit = OMX_TRUE;

  if (mThread) {
    KDint ret;
    void *retval;
    ret = kdThreadJoin(mThread, &retval);
    OMX_LOGD("Stop decoding thread");
    mThread = NULL;
  }
  return err;
}

void* OmxSubtitleDecoder::threadEntry(void * args) {
  OmxSubtitleDecoder *decoder = (OmxSubtitleDecoder *)args;
  prctl(PR_SET_NAME, "OmxSubtitleDecoder", 0, 0, 0);
  decoder->decodeLoop();
  return NULL;
}

void OmxSubtitleDecoder::decodeLoop() {
  OMX_BUFFERHEADERTYPE *in_head = NULL;
  OMX_BUFFERHEADERTYPE *out_head = NULL;

  while(!mShouldExit) {
    if (!mInputPort->isEmpty()) {
      in_head = mInputPort->popBuffer();
      if (NULL != in_head) {
    /*    if (!mOutputPort->isEmpty()) {
          out_head = mOutputPort->getBuffer();
          mSubDecoder->processOneChunk(in_head, out_head);

        } else {
          OMX_LOGD("FFFFFFFFFFFFFFFFFFFFFFFFFFuck empty");
        }
    */
      }
    }
    usleep(5000);

  }
}

OMX_ERRORTYPE OmxSubtitleDecoder::getParameter(OMX_INDEXTYPE index, OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxSubtitleDecoder::setParameter(OMX_INDEXTYPE index, OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxSubtitleDecoder::getConfig(OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxSubtitleDecoder::setConfig(OMX_INDEXTYPE index, const OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}


OMX_ERRORTYPE OmxSubtitleDecoder::componentDeInit(OMX_HANDLETYPE hComponent) {
  // ASSERT(hComponent != NULL)
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxSubtitleDecoder *subtitle_decoder = static_cast<OmxSubtitleDecoder *>(comp);
  delete subtitle_decoder;
  return OMX_ErrorNone;
}


OMX_ERRORTYPE OmxSubtitleDecoder::createComponent(
  OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxSubtitleDecoder* comp = new OmxSubtitleDecoder(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

}

