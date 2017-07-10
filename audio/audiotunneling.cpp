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

/**
* @file Audiotunneling.cpp
* @author  hufen@marvell.com
* @date 2014.01.10
* @brief  : using ADEC to decode mp3/voribs/aac/amr's es data.
*/

#define LOG_NDEBUG 0
#define LOG_TAG "AudioTunneling"

#include <audiotunneling.h>

AudioTunneling::AudioTunneling() {

}

AudioTunneling:: ~AudioTunneling() {
  release();
}

void AudioTunneling::prepare(HANDLE sndhandle, audio_format_t format) {
  ALOGD("prepare()");
  initalParameter();

  HRESULT err = SUCCESS;
  AMP_COMPONENT_CONFIG mConfig;
  assert(mSNDHandle);
  mSNDHandle = sndhandle;

  //Create ADEC component;
  AMP_FACTORY factory = NULL;
  err = AMP_GetFactory(&factory);
  VERIFY_RESULT(err);
  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_ADEC,
      1, &mAmpHandle);
  VERIFY_RESULT(err);
  memset(&mConfig, 0, sizeof(AMP_COMPONENT_CONFIG));
  mConfig._d = AMP_COMPONENT_ADEC;
  mConfig._u.pADEC.mode = AMP_NON_TUNNEL;
  mConfig._u.pADEC.eAdecFmt = getAMPCodec(format);
  mConfig._u.pADEC.uiInPortNum = 1;
  mConfig._u.pADEC.uiOutPortNum = 1;
  mConfig._u.pADEC.ucFrameIn = AMP_ADEC_STEAM;//AMP_ADEC_STEAM;
  setADECParameter(mConfig._u.pADEC.eAdecFmt);

  //Open ADEC component;
  AMP_RPC(err, AMP_ADEC_Open, mAmpHandle, &mConfig);
  VERIFY_RESULT(err);

  //Connect
  err = AMP_ConnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone, static_cast<void *>(this));
  VERIFY_RESULT(err);
  err = AMP_ConnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone, static_cast<void *>(this));
  VERIFY_RESULT(err);
  start();
}

void AudioTunneling::release() {
  ALOGD("release()");
  HRESULT err = SUCCESS;

  if (mInBDChain != NULL) {
    destroyBDChain(mInBDChain, mAmpHandle, AMP_PORT_INPUT);
    mInBDChain = NULL;
  }
  if (mOutBDChain != NULL) {
    destroyBDChain(mOutBDChain, mAmpHandle, AMP_PORT_OUTPUT);
    mOutBDChain = NULL;
  }
  if (mAmpHandle != NULL) {
    AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_IDLE);
    VERIFY_RESULT(err);
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_INPUT,
        0, pushBufferDone);
    VERIFY_RESULT(err);
    err = AMP_DisconnectApp(mAmpHandle, AMP_PORT_OUTPUT,
        0, pushBufferDone);
    VERIFY_RESULT(err);
    AMP_RPC(err, AMP_ADEC_Close, mAmpHandle);
    VERIFY_RESULT(err);
    AMP_RPC(err, AMP_ADEC_Destroy, mAmpHandle);
    VERIFY_RESULT(err);
    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
}

void AudioTunneling::start() {
  AMP_BD_HANDLE hBD;
  AMP_SHM_HANDLE hShm;
  AMP_BDTAG_MEMINFO MemInfo;
  AMP_BDTAG_AUD_FRAME_INFO FrameInfo;
  UINT8  cTagIdx = 0;
  UINT32 ulRemain = 0;
  HRESULT err = SUCCESS;
  //create input/output bd chain;
  err = AMP_BDCHAIN_Create(TRUE, &mInBDChain);
  VERIFY_RESULT(err);
  err = AMP_BDCHAIN_Create(TRUE, &mOutBDChain);
  VERIFY_RESULT(err);

  memset(&MemInfo, 0, sizeof(MemInfo));
  memset(&FrameInfo, 0, sizeof(FrameInfo));

  for (int i = 0; i < INPUT_BD_NUM; i++) {
    err = AMP_BD_Allocate(&hBD);
    VERIFY_RESULT(err);

    /* Allocate MEMINFO */
    err = AMP_SHM_Allocate(AMP_SHM_DYNAMIC, HAL_BUFFER_SIZE, 32, &hShm);
    VERIFY_RESULT(err);

    MemInfo.Header.eType = AMP_BDTAG_ASSOCIATE_MEM_INFO;
    MemInfo.Header.uLength = sizeof(MemInfo);
    MemInfo.uMemHandle = (UINT32)hShm;
    MemInfo.uMemOffset = 0;
    MemInfo.uSize = HAL_BUFFER_SIZE;

    /* Append to the end */
    err = AMP_BDTAG_Append (hBD, (UINT8 *)&MemInfo, &cTagIdx, &ulRemain);
    VERIFY_RESULT(err);
    err = AMP_BDCHAIN_PushItem(mInBDChain, hBD);
    VERIFY_RESULT(err);

    //Register input BD
    AMP_RPC(err, AMP_ADEC_RegisterBD, mAmpHandle, AMP_PORT_INPUT, 0, hBD);
    VERIFY_RESULT(err);
  }

  for (int i = 0; i < OUTPUT_BD_NUM; i++) {
    err = AMP_BD_Allocate(&hBD);
    VERIFY_RESULT(err);
    err = AMP_BD_Ref(hBD);
    VERIFY_RESULT(err);

    /* Allocate MEMINFO */
    err = AMP_SHM_Allocate(AMP_SHM_DYNAMIC, 8*4096, 32, &hShm);
    VERIFY_RESULT(err);
    FrameInfo.Header.eType = AMP_BDTAG_AUD_FRAME_CTRL;
    FrameInfo.Header.uLength = sizeof(FrameInfo);
    FrameInfo.uMemHandle = (UINT32)hShm;
    FrameInfo.uChanNr = 2;
    FrameInfo.uBitDepth = 16;
    FrameInfo.uSampleRate = 44100;
    FrameInfo.uDataLength = 0;
    FrameInfo.uSize = AMP_AUD_MAX_BUF_NR * 4096;
    FrameInfo.uMemOffset[0] = 0 * 4096;
    FrameInfo.uMemOffset[1] = 1 * 4096;
    FrameInfo.uMemOffset[2] = 2 * 4096;
    FrameInfo.uMemOffset[3] = 3 * 4096;
    FrameInfo.uMemOffset[4] = 4 * 4096;
    FrameInfo.uMemOffset[5] = 5 * 4096;
    FrameInfo.uMemOffset[6] = 6 * 4096;
    FrameInfo.uMemOffset[7] = 7 * 4096;

    /* Append to the end */
    err = AMP_BDTAG_Append (hBD, (UINT8 *)&FrameInfo, &cTagIdx, &ulRemain);
    VERIFY_RESULT(err);
    err = AMP_BDCHAIN_PushItem(mOutBDChain, hBD);
    VERIFY_RESULT(err);

    //Register Output BD
    AMP_RPC(err, AMP_ADEC_RegisterBD, mAmpHandle, AMP_PORT_OUTPUT, 0, hBD);
    VERIFY_RESULT(err);
  }

  AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_EXECUTING);
  VERIFY_RESULT(err);
}

void AudioTunneling::flush() {
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_IDLE);
  VERIFY_RESULT(err);

  int waitCount = 70;
  while ((mSendInBDNum > mAmpBackInBDNum || mSendOutBDNum > mAmpBackOutBDNum)
      && (waitCount > 0)) {
    usleep(10*1000);
    waitCount--;
  }
  // Most waiting time = 700ms;
  ALOGD("SendInBDNum %u AmpBackInBDNum %u SendOutBDNum %d outAmpbd %d wait %d\n",
      mSendInBDNum, mAmpBackInBDNum, mSendOutBDNum, mAmpBackOutBDNum, waitCount);
  mAmpBackInBDNum = 0;
  mAmpBackOutBDNum = 0;
  mSendInBDNum = 0;
  mSendOutBDNum = 0;
  mSampleNumInframe = 0;
  if (!mIsEOS) {
    AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_EXECUTING);
  }
  VERIFY_RESULT(err);
}

void AudioTunneling::pause() {
  HRESULT err = SUCCESS;
  AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_PAUSED);
  VERIFY_RESULT(err);
}

void AudioTunneling::resume() {
  HRESULT err = SUCCESS;
  if (!mIsEOS) {
    AMP_RPC(err, AMP_ADEC_SetState, mAmpHandle, AMP_EXECUTING);
  }
  VERIFY_RESULT(err);
}

void AudioTunneling::initalParameter() {
  mAmpHandle = NULL;
  mSNDHandle = NULL;
  mInBDChain = NULL;
  mOutBDChain = NULL;
  mAmpBackInBDNum = 0;
  mAmpBackOutBDNum = 0;
  mSendInBDNum = 0;
  mSendOutBDNum = 0;
  mSampleNumInframe = 0;
  mIsEOS = false;
}

void AudioTunneling::setADECParameter(int adectype) {
  HRESULT err = SUCCESS;
  AMP_ADEC_PARAIDX para_index = 0;
  AMP_ADEC_PARAS mParas;
  memset(&mParas, 0, sizeof(mParas));
  switch (adectype) {
    case AMP_HE_AAC:
      mParas._d = AMP_ADEC_PARAIDX_AAC;
      mParas._u.AAC.ucLayer = 0;           //*< optional */
      mParas._u.AAC.ucProfileType = 1;     // _LC;
      mParas._u.AAC.ucStreamFmt = 0;       // AAC_SF_MP2ADTS;
      mParas._u.AAC.ucVersion = 1;         //*< optional */
      mParas._u.AAC.uiBitRate = 6000;      //*< optional */
      mParas._u.AAC.uiChanMask = 0x2;      //*< optional */
      mParas._u.AAC.uiInThresh = (768 * 6);//*< optional */
      mParas._u.AAC.uiSampleRate = 4;      //*< optional */
      mParas._u.AAC.unBitDepth = 8;        //*< optional */
      mParas._u.AAC.unChanMode = 2;        //*< optional */
      mParas._u.AAC.unChanNr = 2;          //*< optional */
      mParas._u.AAC.unLfeMode = 0;         //*< optional */
      ALOGD("Open component with format AAC");
      AMP_RPC(err, AMP_ADEC_SetParameters, mAmpHandle,
          AMP_ADEC_PARAIDX_AAC, &mParas);
      break;
    default:
      ALOGD("Use default param\n");
      break;
  }
  ALOGD("Open component with format %d.", adectype);
}

bool AudioTunneling::processData(void *pData, int dataLength) {
  if (pData == NULL || dataLength <0) {
    ALOGE("Invalid package!!!");
    return false;
  }

  HRESULT err = SUCCESS;
  UINT8 cTagIdx = 0;
  UINT32 uInBDNr = 0;
  UINT32 uOutBDNr = 0;
  AMP_BD_HANDLE hBD;
  AMP_SHM_HANDLE hShm;
  AMP_BDCHAIN *ptmpInBDChain = NULL;
  AMP_BDCHAIN *ptmpOutBDChain = NULL;
  AMP_BDTAG_MEMINFO *pMemInfo = NULL;
  VOID *pInPutVirAddr = NULL;
  VOID *pOutPutVirAddr = NULL;

  ptmpInBDChain = mInBDChain;
  ptmpOutBDChain = mOutBDChain;

  while ((uInBDNr <= 0)) {
    //Send output BD to amp .
    err = AMP_BDCHAIN_GetItemNum(ptmpOutBDChain, &uOutBDNr);
    VERIFY_RESULT(err);
    while ( uOutBDNr > 0) {
      err = AMP_BDCHAIN_PopItem(ptmpOutBDChain, &hBD);
      VERIFY_RESULT(err);
      if (err != SUCCESS || hBD == NULL) {
        /* No free BD to use */
        ALOGE("ptmpOutBDChain %p, hBD %p/%d, result %x\n",
            ptmpOutBDChain, hBD, uOutBDNr, err);
        return false;
      }
      AMP_RPC(err, AMP_ADEC_PushBD, mAmpHandle,
                   AMP_PORT_OUTPUT, 0, hBD);
      VERIFY_RESULT(err);
      mSendOutBDNum++;
      ALOGV("<Out> %u /%u send BD to AMP Done.", mAmpBackOutBDNum, mSendOutBDNum);
      err = AMP_BDCHAIN_GetItemNum(ptmpOutBDChain, &uOutBDNr);
      VERIFY_RESULT(err);
    }

    // Waiting for Adec decode DBs if there no BD to use.
    err = AMP_BDCHAIN_GetItemNum(ptmpInBDChain, &uInBDNr);
    VERIFY_RESULT(err);
    if (uInBDNr <= 0) {
      usleep(10*1000);
      ALOGW("No input BD to save data, sleep 10ms !!");
    }
  }

  err = AMP_BDCHAIN_PopItem(ptmpInBDChain, &hBD);
  VERIFY_RESULT(err);
  if (err != SUCCESS || hBD == NULL) {
    /* No free BD to use */
    ALOGE("ptmpInBDChain %p, hBD %p/%d, result %x\n",
    ptmpInBDChain, hBD, uInBDNr, err);
    return false;
  }
  err = getTag(hBD, AMP_BDTAG_ASSOCIATE_MEM_INFO,
            (void **)&pMemInfo, &cTagIdx);
  VERIFY_RESULT(err);
  if (pMemInfo != NULL) {
    hShm = pMemInfo->uMemHandle;
    if (dataLength < HAL_BUFFER_SIZE) {
      pMemInfo->uFlag |= AMP_MEMINFO_FLAG_EOS_MASK;
      ALOGD("Send EOS to ADEC. dataLength %d", dataLength);
    }
    err = AMP_SHM_GetVirtualAddress (hShm, 0, &pInPutVirAddr);
    memcpy(pInPutVirAddr, pData, dataLength);
    AMP_SHM_CleanCache(pMemInfo->uMemHandle, 0, pMemInfo->uSize);
    AMP_RPC(err, AMP_ADEC_PushBD, mAmpHandle, AMP_PORT_INPUT, 0, hBD);
    mSendInBDNum++;
    VERIFY_RESULT(err);
    ALOGV("<In> %u /%u send BD to AMP Done.", mAmpBackInBDNum, mSendInBDNum);
    int waitCount = 0;
    if (pMemInfo->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
      while ( (waitCount < 100) && (!mIsEOS)) {
        waitCount++;
        usleep(10*1000);
        ALOGV("Waiting ADEC to return the last decoded BD.");
        // If all amp return all output BD, set EOS as true
        // and can't block this pipeline.
        if (mSendOutBDNum == mAmpBackOutBDNum) {
          mIsEOS = true;
        }
      }
      ALOGD("Wait time %d ms, mIsEOS %d.", waitCount * 10, mIsEOS);
    }
  }
  return true;
}


HRESULT AudioTunneling::pushBufferDone(
    AMP_COMPONENT component,
    AMP_PORT_IO port_Io,
    UINT32 port_idx,
    AMP_BD_ST *bd,
    void *context) {
  HRESULT err = SUCCESS;
  AudioTunneling *decoder = static_cast<AudioTunneling *>(context);
  if (decoder == NULL) {
    ALOGE("Adec return wrong data!!!");
    return HRESULT_GEN(GENERIC, ERR_ERRPARAM);;
  }
  if (AMP_PORT_INPUT == port_Io && 0 == port_idx) {
    decoder->mAmpBackInBDNum++;
    ALOGV("[In]%u/%u PushBuffer Done",
         decoder->mAmpBackInBDNum, decoder->mSendInBDNum);
    AMP_BDTAG_MEMINFO *pMemInfo = NULL;
    UINT8 cTagIdx = 0;
    err = AMP_BDCHAIN_PushItem(decoder->mInBDChain, bd);
    VERIFY_RESULT(err);
    err = decoder->getTag(bd, AMP_BDTAG_ASSOCIATE_MEM_INFO,
            (void **)&pMemInfo, &cTagIdx);
    VERIFY_RESULT(err);
    if (pMemInfo == NULL) {
     ALOGE("Can't get memoryinfo data");
    } else {
      if (pMemInfo->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
       ALOGD("[In]Recieve EOS tag.");
      }
    }
  } else if (AMP_PORT_OUTPUT == port_Io && 0 == port_idx){
    decoder->mAmpBackOutBDNum++;
    ALOGV("[Out]%u/%u PushBuffer Done",
         decoder->mAmpBackOutBDNum, decoder->mSendOutBDNum);
    AMP_BDTAG_AUD_FRAME_INFO *frame_info = NULL;
    UINT8 cTagIdx = 0;
    void *pVirAddr = NULL;
    err = decoder->getTag(bd, AMP_BDTAG_AUD_FRAME_CTRL,(void **)&frame_info, &cTagIdx);
    VERIFY_RESULT(err);
    if(frame_info != NULL) {
      UINT32 framesize = (frame_info->uChanNr) * ((frame_info->uBitDepth)>>3);
      decoder->mSampleNumInframe = frame_info->uDataLength / framesize;
      if (frame_info->uFlag & AMP_MEMINFO_FLAG_EOS_MASK) {
        ALOGD("[Out]Receive EOS tag");
        decoder->mIsEOS = true;
      }
      // Got pcm data from amp;
      AMP_SHM_GetVirtualAddress(frame_info->uMemHandle, 0, &pVirAddr);
      if (pVirAddr != NULL && frame_info->uDataLength != 0) {
        // return PCM data to SND to rendor;
        err = AMP_SND_PushPCM(decoder->mSNDHandle, (VOID *)pVirAddr, frame_info->uDataLength);
        if (err != S_OK) {
         ALOGE("AMP_SND_PushPCM fail:0x%x, decoder->mSNDHandle = %p", err, decoder->mSNDHandle);
        }
      } else {
        ALOGE("Can't got pcm data!!!");
      }
      err = AMP_BDCHAIN_PushItem(decoder->mOutBDChain, bd);
      ALOGV("[Out]  size %d, datalength %d, shm %x buf num %d ,framerate %d channel num %d",
            "mSampleNumInframe %u",
            frame_info->uSize,
            frame_info->uDataLength,
            frame_info->uMemHandle,
            frame_info->uBufNr,
            frame_info->uSampleRate,
            frame_info->uChanNr,
            decoder->mSampleNumInframe);
    } else {
      ALOGE("Can't get frameinfo data");
    }
  }
  return err;
}

HRESULT AudioTunneling::getTag(AMP_BD_HANDLE hBD,
    AMP_BDTAG_T eTagType,
    VOID **pBDTag,
    UINT8 *pIdx)
{
  HRESULT err = SUCCESS;
  UINT uiTagIdx = 0, uiTagNr = 0;
  BOOL bTagFound = FALSE;

  assert(pBDTag && pIdx);

  err = AMP_BDTAG_GetNum(hBD, &uiTagNr);
  VERIFY_RESULT(err);
  for (uiTagIdx = 0; uiTagIdx < uiTagNr; uiTagIdx++) {
      err = AMP_BDTAG_GetWithIndex(hBD, uiTagIdx, pBDTag);
      VERIFY_RESULT(err);
      if (((AMP_BDTAG_H *)(*pBDTag))->eType == eTagType) {
          bTagFound = TRUE;
          *pIdx = uiTagIdx;
          break;
      }
  }

  if (bTagFound) {
    err= SUCCESS;
  } else {
    err = HRESULT_GEN(GENERIC, ERR_ERRPARAM);
  }
  return err;
}

int AudioTunneling::getAMPCodec(audio_format_t format) {
  switch (format) {
    case AUDIO_FORMAT_MP3:
      return AMP_MP3;
    case AUDIO_FORMAT_AMR_NB:
      return AMP_AMRNB;
    case AUDIO_FORMAT_AMR_WB:
      return AMP_AMRNB;
    case AUDIO_FORMAT_AAC:
      return AMP_HE_AAC;
    case AUDIO_FORMAT_VORBIS:
      return AMP_VORBIS;
    default:
      ALOGE("amp audio codec %d not supported", format);
  }
  return -1;
}

HRESULT AudioTunneling::destroyBDChain(AMP_BDCHAIN* pBDChain,
    AMP_COMPONENT handle,
    AMP_PORT_IO port_Io) {
  AMP_BD_HANDLE hBD;
  AMP_BDTAG_MEMINFO *pMemInfo;
  AMP_BDTAG_AUD_FRAME_INFO *pFrameInfo;
  UINT iBDNum = 0;
  UINT iTagNum = 0;
  HRESULT err = SUCCESS;

  if ((!pBDChain == NULL)|| (handle == NULL)) {
    return SUCCESS;
  }
  err = AMP_BDCHAIN_GetItemNum(pBDChain, &iBDNum);
  VERIFY_RESULT(err);
  if ((AMP_PORT_INPUT ==  port_Io) && (INPUT_BD_NUM != iBDNum)) {
    ALOGE("Input BD num error, uiBDNr %d, expected %d\n",
        iBDNum, INPUT_BD_NUM);
  } else if ((AMP_PORT_OUTPUT ==  port_Io) && (OUTPUT_BD_NUM != iBDNum)) {
    ALOGE("Output BD num error, uiBDNr %d, expected %d\n",
        iBDNum, OUTPUT_BD_NUM);
  }

  for (int i = 0; i < iBDNum; i++) {
    if (AMP_BDCHAIN_PopItem(pBDChain, &hBD) == SUCCESS) {
      AMP_BDTAG_GetNum(hBD, &iTagNum);
      /* Unregister BD */
      if (AMP_PORT_INPUT ==  port_Io) {
        AMP_RPC(err, AMP_ADEC_UnregisterBD, handle, AMP_PORT_INPUT, 0, hBD);
        VERIFY_RESULT(err);
        for (int j = 0; j < iTagNum; j ++) {
          AMP_BDTAG_GetWithIndex(hBD, j, (VOID **)&pMemInfo);
          if (AMP_BDTAG_ASSOCIATE_MEM_INFO == pMemInfo->Header.eType) {
              AMP_SHM_Release(pMemInfo->uMemHandle);
          } else {
            ALOGE("Fail to relase this BD's SHM");
          }
        }
      } else if (AMP_PORT_OUTPUT ==  port_Io) {
        AMP_RPC(err, AMP_ADEC_UnregisterBD, handle, AMP_PORT_OUTPUT, 0, hBD);
        VERIFY_RESULT(err);
        for (int j = 0; j < iTagNum; j ++) {
          AMP_BDTAG_GetWithIndex(hBD, j, (VOID **)&pFrameInfo);
          if (AMP_BDTAG_AUD_FRAME_CTRL == pFrameInfo->Header.eType) {
              AMP_SHM_Release(pFrameInfo->uMemHandle);
          } else {
            ALOGE("Fail to relase this BD's SHM");
          }
        }
      }

      if (AMP_BDTAG_Clear(hBD)!=SUCCESS) {
        ALOGE("Failed to clear DGtag 0x%x!", hBD);
      } else {
        if (AMP_PORT_OUTPUT ==  port_Io) {
          AMP_BD_Unref(hBD);
        }
      }
      /* Free BD */
      if (AMP_BD_Free(hBD)!= SUCCESS) {
        ALOGE("Failed to free BD 0x%x!", hBD);
      }
    }
  }
  err = AMP_BDCHAIN_Destroy(pBDChain);
  VERIFY_RESULT(err);
  return err;
}

UINT32 AudioTunneling::getRendorSampleNum() {
  ALOGV("getRendorSampleNum %u", mAmpBackOutBDNum * mSampleNumInframe);
  return mAmpBackOutBDNum * mSampleNumInframe;
}

static sp<AudioTunneling> gAudioTunneling = NULL ;
extern "C" void initAudioTunneling() {
  if (gAudioTunneling == NULL) {
    gAudioTunneling = new AudioTunneling();
  }
}
extern "C" void adecPrepare(HANDLE sndhandle, audio_format_t format) {
  gAudioTunneling->prepare(sndhandle, format);
}

extern "C" void processData(void* pdata, int dataLength) {
  gAudioTunneling->processData(pdata, dataLength);
}

extern "C" void adecFlush() {
  gAudioTunneling->flush();
}

extern "C" void adecPause() {
  gAudioTunneling->pause();
}

extern "C" void adecResume() {
  gAudioTunneling->resume();
}

extern "C" void adecRelease() {
  gAudioTunneling->release();
}

extern "C" void getRendorSampleNum() {
  gAudioTunneling->getRendorSampleNum();
}
