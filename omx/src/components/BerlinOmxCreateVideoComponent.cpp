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

#define LOG_TAG "CreateVideoComponentEntry"
#include <BerlinOmxAmpVideoDecoder.h>
#include <BerlinOmxCommon.h>
#include <BerlinOmxVideoProcessor.h>
#include <BerlinOmxVideoRender.h>
#include <BerlinOmxVideoScheduler.h>
#include <BerlinOmxVideoEncoder.h>
#include <OMX_Component.h>

const char *kDecoderPrefix = "video_decoder";
const char *kRenderPrefix = "iv_renderer";
const char *kSchedulerPrefix = "video_scheduler";
const char *kProcessorPrefix = "iv_processor";
const char *kEncoderPrefix = "video_encoder";

namespace berlin {

extern "C" {
  OMX_API OMX_ERRORTYPE CreateComponentEntry(
      OMX_HANDLETYPE *handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE *callBacks) {
    OMX_ERRORTYPE err = OMX_ErrorNone;

    OMX_LOGD("Omx Create Component %s", componentName);
    if (!strncmp(componentName, kCompNamePrefix, kCompNamePrefixLength)) {
      if (NULL != strstr(componentName, kDecoderPrefix)) {
        err = OmxAmpVideoDecoder::createAmpVideoDecoder(handle,
            componentName, appData, callBacks);
        return err;
      } else if (NULL != strstr(componentName, kProcessorPrefix)) {
        err = OmxVideoProcessor::createComponent(handle,
            componentName, appData, callBacks);
        return err;
      } else if (NULL != strstr(componentName, kSchedulerPrefix)) {
        err = OmxVideoScheduler::createComponent(handle,
            componentName, appData, callBacks);
        return err;
      } else if (NULL != strstr(componentName, kRenderPrefix)) {
        err = OmxVideoRender::createComponent(handle,
            componentName, appData, callBacks);
        return err;
      } else if (NULL != strstr(componentName, kEncoderPrefix)) {
        err = OmxVideoEncoder::createComponent(handle,
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
