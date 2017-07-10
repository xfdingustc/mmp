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

#ifndef BERLIN_OMX_PORT_IMPL_H_
#define BERLIN_OMX_PORT_IMPL_H_

#include <mmu_thread_utils.h>
#include "OmxPort.h"
#include <queue>
using namespace std;
#ifndef NULL
#define NULL 0
#endif

namespace mmp {

#define CHECKOMXERR(err) \
do { \
  if (OMX_ErrorNone != err) { \
    OMX_LOGE("%s %d error %d", __FUNCTION__, __LINE__, err); \
    return err; \
  } \
} while(0)

typedef enum BUFFER_OWNER {
  OWN_BY_PLAYER = 0x01,
  OWN_BY_OMX = 0x02,
  OWN_BY_LOWER = 0x03
} BUFFER_OWNER;

class OmxBuffer {
 public:
  OmxBuffer(OMX_BUFFERHEADERTYPE* buf, OMX_BOOL alloc, BUFFER_OWNER owner) {
    mBuffer = buf;
    mIsAllocator = alloc;
    mOwner = owner;
  }

  OMX_BUFFERHEADERTYPE * mBuffer;
  OMX_BOOL mIsAllocator;
  BUFFER_OWNER mOwner;
};

class OmxPortImpl : public OmxPort {
friend class OmxComponentImpl;

public:
  OmxPortImpl(void);
  virtual ~OmxPortImpl(void);

  virtual OMX_ERRORTYPE enablePort();
  virtual OMX_ERRORTYPE disablePort();
  virtual OMX_ERRORTYPE flushPort();
  virtual OMX_ERRORTYPE flushComplete();
  virtual OMX_ERRORTYPE markBuffer(OMX_MARKTYPE * mark);
  virtual OMX_ERRORTYPE allocateBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_PTR appPrivate,
      OMX_U32 size);
  virtual OMX_ERRORTYPE freeBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE useBuffer(OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_PTR appPrivate,
      OMX_U32 size,
      OMX_U8 *buffer);

  virtual void pushBuffer(OMX_BUFFERHEADERTYPE* buffer);
  virtual OMX_BUFFERHEADERTYPE* getBuffer();
  virtual OMX_BUFFERHEADERTYPE* popBuffer();
  virtual void getDefinition(OMX_PARAM_PORTDEFINITIONTYPE *definition) {
    *definition = mDefinition;
  }
  virtual void setDefinition(OMX_PARAM_PORTDEFINITIONTYPE *definition){
    mDefinition = *definition;
  };
  virtual void returnBuffer(OMX_BUFFERHEADERTYPE * buffer);
  virtual OMX_ERRORTYPE setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param);
  virtual OMX_ERRORTYPE getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param);
  virtual void setAudioParam(OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param);
  inline OMX_VIDEO_PARAM_PORTFORMATTYPE getVideoParam() {return mVideoParam;};
  inline OMX_AUDIO_PARAM_PORTFORMATTYPE getAudioParam() {return mAudioParam;};
  inline OMX_BUFFERHEADERTYPE* getCachedBuffer() {return mBuffer;};

  bool isEmpty();
  void setBufferOwner(OMX_BUFFERHEADERTYPE *buffer, BUFFER_OWNER owner);

  inline OMX_BUFFERSUPPLIERTYPE getBufferSupplier() { return mBufferSupplier;};
  inline OMX_ERRORTYPE setBufferSupplier(OMX_BUFFERSUPPLIERTYPE supplier) {
    mBufferSupplier = supplier; return OMX_ErrorNone;
  };
  inline bool isInput() {return (mDefinition.eDir == OMX_DirInput);};
  inline OMX_DIRTYPE getDir() {return mDefinition.eDir;};
  inline OMX_BOOL isEnabled() { return mDefinition.bEnabled; };
  inline void setEnabled(OMX_BOOL enable) {mDefinition.bEnabled = enable;};
  inline OMX_BOOL isPopulated() { return mDefinition.bPopulated; };
  inline void setPopulated(OMX_BOOL populated) {mDefinition.bPopulated = populated;};
  inline OMX_U32 getBufferCount() { return mDefinition.nBufferCountActual;};
  inline OMX_U32 getBufferCountMin() { return mDefinition.nBufferCountMin;};
  inline void setBufferCountMin(OMX_U32 cnt) { mDefinition.nBufferCountMin = cnt;};
  inline void setBufferCount(OMX_U32 cnt) { mDefinition.nBufferCountActual = cnt;};
  inline OMX_U32 getBufferSize() { return mDefinition.nBufferSize;};
  inline void setBufferSize(OMX_U32 size) { mDefinition.nBufferSize = size;};
  inline OMX_PORTDOMAINTYPE getDomain() { return mDefinition.eDomain;};
  inline OMX_U32 getPortIndex() {return mDefinition.nPortIndex;};
  inline OMX_AUDIO_PORTDEFINITIONTYPE& getAudioDefinition() {
    return mDefinition.format.audio;
  }
  inline OMX_VIDEO_PORTDEFINITIONTYPE& getVideoDefinition() {
    return mDefinition.format.video;
  }
  inline OMX_IMAGE_PORTDEFINITIONTYPE& getImageDefinition() {
    return mDefinition.format.image;
  }
  inline OMX_OTHER_PORTDEFINITIONTYPE& getOtherDefinition() {
    return mDefinition.format.other;
  }
  inline void setAudioFormat(OMX_AUDIO_PORTDEFINITIONTYPE *audio_def) {
    mDefinition.format.audio = *audio_def;
  }
  inline void setVideoDefinition(OMX_VIDEO_PORTDEFINITIONTYPE *video_def) {
    mDefinition.format.video = *video_def;
  }
  inline void setImageDefinition(OMX_IMAGE_PORTDEFINITIONTYPE *image_def) {
    mDefinition.format.image = *image_def;
  }
  inline void setOtherDefinition(OMX_OTHER_PORTDEFINITIONTYPE *other_def) {
    mDefinition.format.other = *other_def;
  }
  inline OMX_BOOL isTunneled() {return mIsTunneled;};
  inline OMX_BOOL isFlushing() {return mIsFlushing;};
  inline void setTunnel(OMX_HANDLETYPE comp, OMX_U32 port) {
    mTunnelComponent = comp;
    mTunnelPort = port;
    if (mTunnelComponent)
      mIsTunneled = OMX_TRUE;
    else
      mIsTunneled = OMX_FALSE;
  }
  inline void tearDownTunnel() {
    mTunnelComponent = NULL;
    mTunnelPort = NULL;
  }
  inline OMX_HANDLETYPE getTunnelComponent() {return mTunnelComponent;};
  inline OMX_U32 getTunnelPort() {return mTunnelPort;};
public:
  OMX_PARAM_PORTDEFINITIONTYPE mDefinition;
  OMX_VIDEO_PARAM_PORTFORMATTYPE mVideoParam;
  OMX_AUDIO_PARAM_PORTFORMATTYPE mAudioParam;

protected:
  OMX_BOOL mIsTunneled;
  OMX_BUFFERSUPPLIERTYPE mBufferSupplier;
  vector<OmxBuffer *> mBufferList;
private:
  OMX_HANDLETYPE mTunnelComponent;
  OMX_U32 mTunnelPort;
  OMX_BUFFERHEADERTYPE *mBuffer; // Current buffer popped from the queue
  MmuMutex mBufferMutex;
  queue<OMX_BUFFERHEADERTYPE *> mBufferQueue;
  OMX_BOOL  mIsFlushing;
  OMX_BOOL  mInTransition;
  OMX_MARKTYPE mMark;
};

}
#endif
