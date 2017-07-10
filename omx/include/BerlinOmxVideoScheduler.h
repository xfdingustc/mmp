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

#ifndef BERLIN_OMX_VIDEO_SCHEDULER_H_
#define BERLIN_OMX_VIDEO_SCHEDULER_H_

#include <BerlinOmxVoutProxy.h>

namespace berlin {
class OmxVideoScheduler : public OmxVoutProxy {
public:
  OmxVideoScheduler();
  OmxVideoScheduler(OMX_STRING name);
  virtual ~OmxVideoScheduler();
  virtual OMX_ERRORTYPE initRole();
  virtual OMX_ERRORTYPE initPort();
  virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
  virtual OMX_S32 getVideoPlane();

  static OMX_ERRORTYPE createComponent(
      OMX_HANDLETYPE* handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE *callBacks);

private:
  OmxPortImpl *mInputPort;
  OmxPortImpl *mOutputPort;
  OmxPortImpl *mClockPort;
};
} // namespace berlin
#endif // BERLIN_OMX_VIDEO_SCHEDULER_H_



