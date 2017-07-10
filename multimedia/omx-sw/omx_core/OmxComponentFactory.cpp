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

#include <OmxComponentFactory.h>
#include <BerlinOmxCommon.h>
#include <OmxVideoDecoder.h>
#include <OmxAudioDecoder.h>

#define LOG_TAG "OmxComponentFactory"

namespace mmp {

const char *audiodecoderPrefix = "audio_decoder";
const char *videodecoderPrefix = "video_decoder";


OMX_ERRORTYPE OmxComponentFactory::CreateComponentEntry(
      OMX_HANDLETYPE *handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE *callBacks) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OmxComponentImpl *comp = NULL;

  OMX_LOGD("Omx Create Component %s", componentName);
  if (NULL != strstr(componentName, audiodecoderPrefix)) {
    comp = new OmxAudioDecoder(componentName);
  } else if (NULL != strstr(componentName, videodecoderPrefix)) {
    comp = new OmxVideoDecoder(componentName);
  } else {
    OMX_LOGE("Component %s is not found!!!", componentName);
    err = OMX_ErrorInvalidComponentName;
    return err;
  }

  if (comp) {
    *handle = comp->makeComponent(comp);
    comp->setCallbacks(callBacks, appData);
    comp->componentInit();
    return OMX_ErrorNone;
  }

  return OMX_ErrorInsufficientResources;
}

}
