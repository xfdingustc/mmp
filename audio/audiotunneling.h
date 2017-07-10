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
 * @brief  Define the class AudioTunneling
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AudioTunneling"

#ifndef AUDIO_TUNNELING_H_
#define AUDIO_TUNNELING_H_

#include <amp_buf_desc.h>
#include <amp_client_support.h>
#include <amp_component.h>
#include <amp_sound_api.h>
#include <cutils/log.h>
#include <hardware/audio.h>

#ifdef _ANDROID_
#include <RefBase.h>
#endif

#ifdef _ANDROID_
  using android::sp;
#endif

#define VERIFY_RESULT(x)\
  do {\
    if ((x) != SUCCESS) {\
      ALOGE("error=%x, line=%d", x, __LINE__);\
    } \
  }while(0)

#define INPUT_BD_NUM            6
#define OUTPUT_BD_NUM           6*3
#define HAL_BUFFER_SIZE         2048

class AudioTunneling: public android::RefBase {
  public:
    AudioTunneling();
    ~AudioTunneling();

  public:
    void prepare(HANDLE sndhandle, audio_format_t format);
    void release();
    void start();
    void flush();
    void resume();
    void pause();
    void initalParameter();
    void setADECParameter(int adecType);
    bool processData(void *pData, int dataLength);
    static HRESULT pushBufferDone(
        AMP_COMPONENT component,
        AMP_PORT_IO port_Io,
        UINT32 port_idx,
        AMP_BD_ST *bd,
        void *context);
    UINT32 getRendorSampleNum();
  private:
    static HRESULT getTag(AMP_BD_HANDLE hBD,
    AMP_BDTAG_T eTagType,
        VOID **pBDTag,
        UINT8 *pIdx);
    static int getAMPCodec(audio_format_t format);
    static HRESULT destroyBDChain(AMP_BDCHAIN *pBDChain,
        AMP_COMPONENT handle,
        AMP_PORT_IO port_Io);

  public:
    AMP_COMPONENT mAmpHandle;
    HANDLE mSNDHandle;
    AMP_BDCHAIN *mInBDChain;
    AMP_BDCHAIN *mOutBDChain;
    UINT32 mAmpBackInBDNum;
    UINT32 mAmpBackOutBDNum;
    UINT32 mSendInBDNum;
    UINT32 mSendOutBDNum;
    UINT32 mSampleNumInframe;
    BOOL mIsEOS;
};

#endif
