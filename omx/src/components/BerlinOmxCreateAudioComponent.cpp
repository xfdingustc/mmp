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

#define LOG_TAG "CreateAudioComponentEntry"
#include <BerlinOmxAmpAudioDecoder.h>
#include <BerlinOmxAmpAudioRenderer.h>
#include <BerlinOmxCommon.h>
#include <OMX_Component.h>


namespace berlin {

extern "C" {
  OMX_API OMX_ERRORTYPE CreateComponentEntry(
      OMX_HANDLETYPE *handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE *callBacks) {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    const char *audiodecoderPrefix = "audio_decoder";
    const char *audio_renderer_Prefix = "audio_renderer";

    OMX_LOGD("Omx Create Component %s", componentName);
    if (!strncmp(componentName, kCompNamePrefix, kCompNamePrefixLength)) {
      if (NULL != strstr(componentName, audiodecoderPrefix)) {
        err = OmxAmpAudioDecoder::createAmpAudioDecoder(handle,
            componentName, appData, callBacks);
        return err;
      } else if (NULL != strstr(componentName, audio_renderer_Prefix)) {
        err = OmxAmpAudioRenderer::createComponent(handle,
            componentName, appData, callBacks);
        return err;
      } else {
        OMX_LOGE("Amp component %s is not found!!!", componentName);
        err = OMX_ErrorInvalidComponentName;
        return err;
      }
    } else {
      OMX_LOGE("%s is not the Marvell Component!!!", componentName);
      err = OMX_ErrorInvalidComponentName;
      return err;
    }
  }
}
}
