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

#ifndef BERLIN_OMX_COMPONENT_IMPL_H_
#define BERLIN_OMX_COMPONENT_IMPL_H_

#include <OmxComponent.h>
#include <OmxPortImpl.h>
#include <EventQueue.h>

#include <vector>
#include <map>
#include <queue>

using namespace std;
namespace mmp {

typedef map<OMX_U32, OmxPortImpl *>::iterator port_iterator;
typedef pair<OMX_U32, OmxPortImpl *> port_pair;


#ifdef _ANDROID_
enum OMX_ANDROID_INDEXTYPE {
  OMX_IndexAndroidEnableNativeBuffers = OMX_IndexVendorStartUnused,
  OMX_IndexAndroidGetNativeBufferUsage,
  OMX_IndexAndroidStoreMetadataInBuffers,
  OMX_IndexAndroidUseNativeBuffer,
};
#endif

class OmxComponentImpl : public OmxComponent {
  friend class OmxPortImpl;

  public:
    OmxComponentImpl(char *name);
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

    OMX_ERRORTYPE componentInit();
    OmxPortImpl* getPort(OMX_U32 port_index);

  protected:
    void addRole(const char * role);
    void addPort(OmxPortImpl *port);
    OMX_ERRORTYPE flushPort(OmxPortImpl* port);
    OMX_ERRORTYPE returnBuffer(OmxPortImpl* port, OMX_BUFFERHEADERTYPE* buf);

    virtual OMX_ERRORTYPE initRole() = 0;
    virtual OMX_ERRORTYPE initPort() = 0;
    virtual OMX_ERRORTYPE processBuffer() { return OMX_ErrorNone; }
    virtual OMX_ERRORTYPE flush() { return OMX_ErrorNone; }

    virtual void onPortEnableCompleted(OMX_U32 portIndex, OMX_BOOL enabled);

    vector<const char *> mRoles;
    char mName[OMX_MAX_STRINGNAME_SIZE];
    const char *mActiveRole;
    map<OMX_U32, OmxPortImpl*> mPortMap;
    OMX_BOOL mStateInprogress;


  private:
    enum {
      kWhatSendCommand = 100,
      kWhatEmptyThisBuffer,
      kWhatFillThisBuffer,
    };

    OMX_ERRORTYPE setRole(OMX_STRING role);
    OMX_ERRORTYPE getRole(OMX_STRING role);
    OMX_ERRORTYPE getParameterDomainInit(OMX_PORT_PARAM_TYPE* port_param,
                                         OMX_PORTDOMAINTYPE domain);
    OMX_ERRORTYPE isStateTransitionValid(OMX_STATETYPE old_state,
                                         OMX_STATETYPE new_state);
    OMX_ERRORTYPE enablePort(OMX_U32 port_index);

    OMX_ERRORTYPE returnBuffers();

    void checkTransitions();
    OMX_ERRORTYPE onProcessCommand(EventInfo &info);
    OMX_ERRORTYPE onSetState(OMX_STATETYPE state);
    OMX_ERRORTYPE onPortFlush(OMX_U32 portIndex, bool sendFlushComplete);

    void onOmxEvent(EventInfo &info);

    class OmxEvent : public OmxEventQueue::Event {
      public:
        OmxEvent(OmxComponentImpl *comp,
                void (OmxComponentImpl::*method)(EventInfo &info))
            : mComp(comp),
              mMethod(method) {
        }

        OmxEvent(OmxComponentImpl *comp,
                void (OmxComponentImpl::*method)(EventInfo &info),
                EventInfo &info)
            : Event(info),
              mComp(comp),
              mMethod(method) {
        }

        virtual ~OmxEvent() {}

      protected:
        virtual void fire(OmxEventQueue *queue, int64_t /* now_us */) {
            (mComp->*mMethod)(mEventInfo);
        }

      private:
        OmxComponentImpl *mComp;
        void (OmxComponentImpl::*mMethod)(EventInfo &info);

        OmxEvent(const OmxEvent &);
        OmxEvent &operator=(const OmxEvent &);
    };

    MmuMutex mLock;
    OMX_STATETYPE mState;
    OMX_STATETYPE mTargetState;
    OMX_U32 mFlushedPorts;

    OmxEventQueue *mOmxEventQueue;
};
}
#endif

