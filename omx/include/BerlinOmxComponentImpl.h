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

#ifndef BERLIN_OMX_COMPONENT_IMPL_H_
#define BERLIN_OMX_COMPONENT_IMPL_H_

#include <BerlinOmxComponent.h>
#include <BerlinOmxPortImpl.h>
#include <BerlinOmxAmpPort.h>

#include <vector>
#include <map>
#include <queue>

#define UUID_WMDRM "\x7a\x07\x9b\xb6\xda\xa4\x4e\x12\xa5\xca\x91\xd3\x8d\xc1\x1a\x8d"
#define UUID_MARLIN "\x5E\x62\x9A\xF5\x38\xDA\x40\x63\x89\x77\x97\xFF\xBD\x99\x02\xD4"

using namespace std;
namespace berlin {

static const OMX_U32 kDefaultLoopSleepTime = 10000;

typedef map<OMX_U32, OmxPortImpl *>::iterator port_iterator;
typedef pair<OMX_U32, OmxPortImpl *> port_pair;
typedef struct OmxCommand {
  OMX_COMMANDTYPE cmd;
  OMX_U32 param;
  OMX_PTR cmdData;
}OmxCommand;

#ifdef _ANDROID_
enum OMX_ANDROID_INDEXTYPE {
  OMX_IndexAndroidEnableNativeBuffers = OMX_IndexVendorStartUnused,
  OMX_IndexAndroidGetNativeBufferUsage,
  OMX_IndexAndroidStoreMetadataInBuffers,
  OMX_IndexAndroidUseNativeBuffer,
  OMX_IndexAndroidPrepareForAdaptivePlayback,
};

#define OMX_COLOR_FormatAndroidOpaque 0x7F000789
#endif

class OmxComponentImpl : public OmxComponent {
  friend class OmxPortImpl;

public:
  OmxComponentImpl();
  virtual ~OmxComponentImpl();
  virtual OMX_ERRORTYPE getComponentVersion(
      OMX_STRING componentName,
      OMX_VERSIONTYPE* componentVersion,
      OMX_VERSIONTYPE* specVersion,
      OMX_UUIDTYPE* componentUUID);

  virtual OMX_ERRORTYPE sendCommand(
      OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR cmdData);

  virtual OMX_ERRORTYPE getParameter(
      OMX_INDEXTYPE index, OMX_PTR params);

  virtual OMX_ERRORTYPE setParameter(
      OMX_INDEXTYPE index, const OMX_PTR params);

  virtual OMX_ERRORTYPE getConfig(
      OMX_INDEXTYPE index, OMX_PTR config);

  virtual OMX_ERRORTYPE setConfig(
      OMX_INDEXTYPE index, const OMX_PTR config);

  virtual OMX_ERRORTYPE getExtensionIndex(
      const OMX_STRING name, OMX_INDEXTYPE *index);
  virtual OMX_ERRORTYPE getState(OMX_STATETYPE *state);
  virtual OMX_ERRORTYPE useBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      OMX_U32 size,
      OMX_U8 *buffer);

  virtual OMX_ERRORTYPE allocateBuffer(
      OMX_BUFFERHEADERTYPE **bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      OMX_U32 size);

  virtual OMX_ERRORTYPE freeBuffer(
      OMX_U32 portIndex,
      OMX_BUFFERHEADERTYPE *buffer);

  virtual OMX_ERRORTYPE useEGLImage(
      OMX_BUFFERHEADERTYPE** bufHdr,
      OMX_U32 portIndex,
      OMX_PTR appPrivate,
      void* eglImage);
  virtual OMX_ERRORTYPE componentDeInit(
      OMX_HANDLETYPE hComponent);
  virtual OMX_ERRORTYPE emptyThisBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE fillThisBuffer(OMX_BUFFERHEADERTYPE *buffer);
  virtual OMX_ERRORTYPE componentRoleEnum(OMX_U8 *role, OMX_U32 index);
  virtual OMX_ERRORTYPE componentTunnelRequest(
      OMX_U32 port,
      OMX_HANDLETYPE tunneledComp,
      OMX_U32 tunneledPort,
      OMX_TUNNELSETUPTYPE* tunnelSetup);
  virtual OMX_ERRORTYPE componentTunnelTearDown();
  virtual OMX_ERRORTYPE componentInit();

  virtual OMX_ERRORTYPE initRole() = 0;
  virtual OMX_ERRORTYPE initPort() = 0;
  virtual OMX_ERRORTYPE prepare() = 0;
  virtual OMX_ERRORTYPE preroll() = 0;
  virtual OMX_ERRORTYPE start() = 0;
  virtual OMX_ERRORTYPE pause() = 0;
  virtual OMX_ERRORTYPE resume() = 0;
  virtual OMX_ERRORTYPE stop() = 0;
  virtual OMX_ERRORTYPE release() = 0;
  virtual OMX_ERRORTYPE flush() = 0;
  inline OMX_STATETYPE getState() {return mState;};
  inline AMP_COMPONENT getAmpHandle() {return mAmpHandle;};
  OmxPortImpl* getPort(OMX_U32 port_index);

protected:
  void addRole(const char * role);
  void addPort(OmxPortImpl *port);
  OMX_ERRORTYPE flushPort(OmxPortImpl* port);
  OMX_ERRORTYPE returnBuffer(OmxPortImpl* port, OMX_BUFFERHEADERTYPE* buf);
  void returnCachedBuffers(OmxPortImpl *port);
  virtual void onPortFlushCompleted(OMX_U32 portIndex);
  virtual void onPortEnableCompleted(OMX_U32 portIndex, OMX_BOOL enabled);
  virtual OMX_BOOL onPortDisconnect(OMX_BOOL disconnect, OMX_PTR type);
  virtual void onPortClockStart(OMX_BOOL start);
#ifdef VIDEO_DECODE_ONE_FRAME_MODE
  virtual void onPortVideoDecodeMode(
      OMX_U32 enable = 1, OMX_S32 mode = -1);
#endif

private:
  OMX_ERRORTYPE setRole(OMX_STRING role);
  OMX_ERRORTYPE getRole(OMX_STRING role);
  OMX_ERRORTYPE getParameterDomainInit(OMX_PORT_PARAM_TYPE* port_param,
                                       OMX_PORTDOMAINTYPE domain);
  OMX_ERRORTYPE processCommand();
  OMX_ERRORTYPE setState(OMX_STATETYPE state);
  OMX_ERRORTYPE isStateTransitionValid(OMX_STATETYPE old_state,
                                       OMX_STATETYPE new_state);
  OMX_BOOL checkAllPortsResource(OMX_BOOL check_alloc);
  OMX_ERRORTYPE enablePort(OMX_U32 port_index);
  inline OMX_BOOL hasPendingResource();
  OMX_U32 getFlushPortsCounts();
  OMX_ERRORTYPE returnBuffers();


public:
  vector<const char *> mRoles;
  char mName[OMX_MAX_STRINGNAME_SIZE];
  AMP_COMPONENT mAmpHandle;
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  OMX_MEDIACONTAINER_INFOTYPE mContainerType;
#endif
#ifdef OMX_IndexExt_h
  OMX_RESOURCE_INFO mResourceInfo;
#endif
#ifdef OMX_IVCommonExt_h
  OMX_CONFIG_DRMTYPE mDrm;
#endif
  OMX_BOOL mSecure;

protected:
  const char * mActiveRole;
  map<OMX_U32, OmxPortImpl*> mPortMap;
  OMX_BOOL mStateInprogress;

private:
  KDThreadMutex* mLock;
  queue<OmxCommand> mCmdQueue;
  OMX_STATETYPE mState;
  OMX_STATETYPE mTargetState;
  OMX_U32 mFlushedPorts;
};
}  // namespace berlin
#endif  // BERLIN_OMX_COMPONENT_H_

