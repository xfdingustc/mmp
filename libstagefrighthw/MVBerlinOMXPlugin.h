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

#ifndef MVBerlin_OMX_PLUGIN_H_
#define MVBerlin_OMX_PLUGIN_H_

#include <media/hardware/OMXPluginBase.h>

namespace android {

struct MVBerlinOMXPlugin : public OMXPluginBase {
    MVBerlinOMXPlugin();
    virtual ~MVBerlinOMXPlugin();

    virtual OMX_ERRORTYPE makeComponentInstance(
            const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

    virtual OMX_ERRORTYPE destroyComponentInstance(
            OMX_COMPONENTTYPE *component);

    virtual OMX_ERRORTYPE enumerateComponents(
            OMX_STRING name,
            size_t size,
            OMX_U32 index);

    virtual OMX_ERRORTYPE getRolesOfComponent(
            const char *name,
            Vector<String8> *roles);

    virtual OMX_ERRORTYPE setupTunnel(
            OMX_COMPONENTTYPE *outputComponent,
            OMX_U32 outputPortIndex,
            OMX_COMPONENTTYPE *inputComponent,
            OMX_U32 inputPortIndex);

private:
    void init();
    void deinit();

    void *mLibHandle;

    typedef OMX_ERRORTYPE (*InitFunc)();
    typedef OMX_ERRORTYPE (*DeinitFunc)();
    typedef OMX_ERRORTYPE (*ComponentNameEnumFunc)(
            OMX_STRING, OMX_U32, OMX_U32);

    typedef OMX_ERRORTYPE (*GetHandleFunc)(
            OMX_HANDLETYPE *, OMX_STRING, OMX_PTR, OMX_CALLBACKTYPE *);

    typedef OMX_ERRORTYPE (*FreeHandleFunc)(OMX_HANDLETYPE *);

    typedef OMX_ERRORTYPE (*GetRolesOfComponentFunc)(
            OMX_STRING, OMX_U32 *, OMX_U8 **);

    typedef OMX_ERRORTYPE (*SetupTunnelFunc)(
            OMX_HANDLETYPE, OMX_U32, OMX_HANDLETYPE, OMX_U32);

    // Function pointers
    InitFunc mInitFunc;
    DeinitFunc mDeinitFunc;
    ComponentNameEnumFunc mComponentNameEnumFunc;
    GetHandleFunc mGetHandleFunc;
    FreeHandleFunc mFreeHandleFunc;
    GetRolesOfComponentFunc mGetRolesOfComponentHandleFunc;
    SetupTunnelFunc mSetupTunnelFunc;

    // Do not allow copy constructors.
    MVBerlinOMXPlugin(const MVBerlinOMXPlugin &);
    MVBerlinOMXPlugin &operator=(const MVBerlinOMXPlugin &);
};

}  // namespace android

#endif  // MVBerlin_OMX_PLUGIN_H_
