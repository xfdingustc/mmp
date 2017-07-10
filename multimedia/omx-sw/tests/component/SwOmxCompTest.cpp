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
#define LOG_TAG "OmxCompTest"

#include <gtest/gtest.h>

extern "C" {
#include <OMX_Core.h>
#include <OMX_Component.h>
}

#include <mmu_thread_utils.h>
#include <BerlinOmxCommon.h>

namespace mmp {
namespace test {

static const OMX_U32 kMaxTestGetFreeTimes = 100;

class CondSignal {
public:
  CondSignal() {
    mSignled = OMX_FALSE;
  }

  ~CondSignal() {
  }

  void waitSignal() {
    MmuAutoLock lock(mLock);
    if (!mSignled) {
      mCond.wait(mLock);
    }
  }

  void setSignal() {
    MmuAutoLock lock(mLock);
    mCond.signal();
    mSignled = OMX_TRUE;
  }

  void resetSignal() {
    MmuAutoLock lock(mLock);
    mSignled = OMX_FALSE;
  }

private:
  MmuMutex mLock;
  MmuCondition mCond;
  OMX_BOOL mSignled;
};

class OmxCompTest : public testing::Test {
protected:
  static OMX_ERRORTYPE compTestEventHandler(
      OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData,
      OMX_EVENTTYPE eEvent,
      OMX_U32 Data1,
      OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("%s: pAppData %p, event %x data %lX-%lX",
        __func__, pAppData, eEvent, Data1, Data2);
    OmxCompTest *ctx = reinterpret_cast<OmxCompTest *>(pAppData);
    switch (eEvent) {
      case OMX_EventCmdComplete:
        if (Data1 == OMX_CommandStateSet) {
          ctx->mStateSignal->setSignal();
          OMX_LOGD("Complete change to state %lX", Data2);
        }
        break;
      default:
        break;
    }
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE compTestEmptyBufferDone(
      OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData,
      OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("%s: buffer %p", __func__, pBuffer);
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE compTestFillBufferDone(
      OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData,
      OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("%s: buffer %p", __func__, pBuffer);
    return OMX_ErrorNone;
  }

  OMX_CALLBACKTYPE compTestCallbacks;
  OMX_BUFFERHEADERTYPE ***mBufHdr[4];
  CondSignal *mStateSignal;
  OMX_U32 mTestGetFreeTimes; // Test times of GetHandle and FreeHandle loop

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    compTestCallbacks.EventHandler = compTestEventHandler;
    compTestCallbacks.EmptyBufferDone = compTestEmptyBufferDone;
    compTestCallbacks.FillBufferDone = compTestFillBufferDone;
  }

  virtual void TearDown() {
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void testSetRoleOfComponent(OMX_IN OMX_STRING comp) {
    OMX_ERRORTYPE err;
    OMX_U32 numRoles;
    err = OMX_GetRolesOfComponent(comp, &numRoles, NULL);
    ASSERT_EQ(OMX_ErrorNone, err);
    ASSERT_GE(numRoles, 0LU);
    OMX_LOGD("Component %s, number of roles: %lu", comp, numRoles);

    OMX_U8 **roleNames = new OMX_U8 *[numRoles];
    for (OMX_U32 i = 0; i < numRoles; ++i) {
      roleNames[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
    }

    OMX_U32 numRoles2 = numRoles;
    err = OMX_GetRolesOfComponent(comp, &numRoles2, roleNames);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_TRUE(numRoles2 == numRoles);

    OMX_HANDLETYPE compHandle;
    err = OMX_GetHandle(&compHandle, comp, this , &compTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);

    OMX_PARAM_COMPONENTROLETYPE param_role;
    InitOmxHeader(&param_role);
    for (OMX_U32 i = 0; i < numRoles2; ++i) {
      OMX_LOGD("Set role: %s", roleNames[i]);
      memcpy(param_role.cRole, roleNames[i], OMX_MAX_STRINGNAME_SIZE);
      err = OMX_SetParameter(compHandle, OMX_IndexParamStandardComponentRole,
          &param_role);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("Component %s set role %s, err %x", comp, roleNames[i], err);
      }
      ASSERT_EQ(err, OMX_ErrorNone);
      OMX_PARAM_COMPONENTROLETYPE param_role2;
      InitOmxHeader(&param_role2);
      err = OMX_GetParameter(compHandle, OMX_IndexParamStandardComponentRole,
          &param_role2);
      OMX_LOGD("Get role: %s", param_role2.cRole);
      ASSERT_STREQ(reinterpret_cast<char *>(param_role.cRole),
                   reinterpret_cast<char *>(param_role2.cRole));
      delete roleNames[i];
    }
    delete [] roleNames;
    roleNames = NULL;
    OMX_LOGD("Free component: %s, handle %p", comp, compHandle);
    err = OMX_FreeHandle(compHandle);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void testGetAndFreeHandle(OMX_IN OMX_STRING comp) {
    OMX_ERRORTYPE err;
    OMX_HANDLETYPE compHandle;
    ASSERT_LE(mTestGetFreeTimes, kMaxTestGetFreeTimes);
    while (mTestGetFreeTimes--) {
      err = OMX_GetHandle(&compHandle, comp, this , &compTestCallbacks);
      ASSERT_EQ(err, OMX_ErrorNone);
      OMX_LOGD("%s, freeHandle %p", __func__, compHandle);
      err = OMX_FreeHandle(compHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
    }
  }

  void testMandatoryPortParameter(OMX_HANDLETYPE compHandle,
      OMX_PORT_PARAM_TYPE * portParam) {
    OMX_ERRORTYPE err;
    // Test OMX_IndexParamPortDefinition
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = portParam->nStartPortNumber; i < portParam->nPorts; i++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      err = OMX_SetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
    }
    // Test OMX_IndexParamCompBufferSupplier
    OMX_PARAM_BUFFERSUPPLIERTYPE supplier;
    InitOmxHeader(&supplier);
    for (OMX_U32 i = portParam->nStartPortNumber; i < portParam->nPorts; i++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamCompBufferSupplier, &supplier);
      ASSERT_EQ(err, OMX_ErrorNone);
      err = OMX_SetParameter(
          compHandle, OMX_IndexParamCompBufferSupplier, &supplier);
      ASSERT_EQ(err, OMX_ErrorNone);
    }
  }

  void testPortParameters(OMX_IN OMX_STRING comp) {
    OMX_ERRORTYPE err;
    OMX_HANDLETYPE compHandle;
    // Get component handle
    err = OMX_GetHandle(&compHandle, comp, this , &compTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);

    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    err = OMX_GetParameter(compHandle, OMX_IndexParamAudioInit, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    testMandatoryPortParameter(compHandle, &oParam);
    err = OMX_GetParameter(compHandle, OMX_IndexParamVideoInit, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    testMandatoryPortParameter(compHandle, &oParam);
    err = OMX_GetParameter(compHandle, OMX_IndexParamImageInit, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    testMandatoryPortParameter(compHandle, &oParam);
    err = OMX_GetParameter(compHandle, OMX_IndexParamOtherInit, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    testMandatoryPortParameter(compHandle, &oParam);
    // Free component handle
    err = OMX_FreeHandle(compHandle);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void AllocateAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    OMX_INDEXTYPE param_index = OMX_IndexComponentStartUnused;
    switch (domain) {
      case OMX_PortDomainAudio:
        param_index = OMX_IndexParamAudioInit;
        break;
      case OMX_PortDomainVideo:
        param_index = OMX_IndexParamVideoInit;
        break;
      case OMX_PortDomainImage:
        param_index = OMX_IndexParamImageInit;
        break;
      case OMX_PortDomainOther:
        param_index = OMX_IndexParamOtherInit;
        break;
      default:
        OMX_LOGE("Incoreect domain %x", domain);
    }
    err = OMX_GetParameter(compHandle, param_index, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    if (oParam.nPorts > 0) {
      mBufHdr[domain] = new OMX_BUFFERHEADERTYPE **[oParam.nPorts];
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      mBufHdr[domain][port] =
          new OMX_BUFFERHEADERTYPE *[def.nBufferCountActual];
      for (OMX_U32 j = 0; j < def.nBufferCountActual ; j++) {
        err = OMX_AllocateBuffer(compHandle, &mBufHdr[domain][port][j], i, NULL,
            def.nBufferSize);
        ASSERT_EQ(err, OMX_ErrorNone);
      }
    }
  }

  void FreeAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
    OMX_INDEXTYPE param_index = OMX_IndexComponentStartUnused;
    switch (domain) {
      case OMX_PortDomainAudio:
        param_index = OMX_IndexParamAudioInit;
        break;

      case OMX_PortDomainVideo:
        param_index = OMX_IndexParamVideoInit;
        break;
      case OMX_PortDomainImage:
        param_index = OMX_IndexParamImageInit;
        break;
      case OMX_PortDomainOther:
        param_index = OMX_IndexParamOtherInit;
        break;
      default:
        OMX_LOGE("Incoreect domain %x", domain);
    }
    ASSERT_NE(param_index, OMX_IndexComponentStartUnused);
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    err = OMX_GetParameter(compHandle, param_index, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    if (oParam.nPorts <= 0) {
      return;
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      for (OMX_U32 j = 0; j < def.nBufferCountActual ; j++) {
        err = OMX_FreeBuffer(compHandle, i, mBufHdr[domain][port][j]);
        ASSERT_EQ(err, OMX_ErrorNone);
      }
      delete [] mBufHdr[domain][port];
    }
    delete [] mBufHdr[domain];
  }

  void testLoadedToIdle(OMX_IN OMX_HANDLETYPE compHandle) {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(compHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateLoaded);
    mStateSignal->resetSignal();

    err = OMX_SendCommand(compHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    ASSERT_EQ(err, OMX_ErrorNone);
    AllocateAllPortsBuffer(compHandle, OMX_PortDomainAudio);
    AllocateAllPortsBuffer(compHandle, OMX_PortDomainVideo);
    AllocateAllPortsBuffer(compHandle, OMX_PortDomainImage);
    AllocateAllPortsBuffer(compHandle, OMX_PortDomainOther);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to idle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to idle");
  }

  void testIdleToExec(OMX_IN OMX_HANDLETYPE compHandle) {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(compHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);
    mStateSignal->resetSignal();

    err = OMX_SendCommand(compHandle, OMX_CommandStateSet, OMX_StateExecuting,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Executing");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Executing");
  }

  void testExecToIdle(OMX_IN OMX_HANDLETYPE compHandle) {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(compHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateExecuting);
    mStateSignal->resetSignal();

    err = OMX_SendCommand(compHandle, OMX_CommandStateSet, OMX_StateIdle,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to OMX_StateIdle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to OMX_StateIdle");
  }

  void testIdleToLoaded(OMX_IN OMX_HANDLETYPE compHandle) {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(compHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);

    mStateSignal->resetSignal();
    err = OMX_SendCommand(compHandle, OMX_CommandStateSet, OMX_StateLoaded,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);
    FreeAllPortsBuffer(compHandle, OMX_PortDomainAudio);
    FreeAllPortsBuffer(compHandle, OMX_PortDomainVideo);
    FreeAllPortsBuffer(compHandle, OMX_PortDomainImage);
    FreeAllPortsBuffer(compHandle, OMX_PortDomainOther);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  void testStateTransition(OMX_IN OMX_STRING comp) {
    OMX_ERRORTYPE err;
    // Prepare
    OMX_HANDLETYPE compHandle;
    err = OMX_GetHandle(&compHandle, comp, this, &compTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
    mStateSignal = new CondSignal();

    // Loded -> Idle
    testLoadedToIdle(compHandle);
    // Idle -> Loaded
    testIdleToLoaded(compHandle);
    // Loded -> Idle
    testLoadedToIdle(compHandle);
    // Idle -> Excuting
    testIdleToExec(compHandle);
    // Excuting -> Idle
    testExecToIdle(compHandle);
    // Idle -> Loaded
    testIdleToLoaded(compHandle);
    // Clean up
    delete mStateSignal;
    err = OMX_FreeHandle(compHandle);
    ASSERT_EQ(err, OMX_ErrorNone);
  }
};  // class OmxCompTest

TEST_F(OmxCompTest, GetComponentTest) {
  OMX_U32 compIndex = 0;
  char compName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
      compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
    OMX_LOGD("GetComponentTest: Comp %lu, %s", compIndex, compName);
    mTestGetFreeTimes = 1;
    testGetAndFreeHandle(compName);
    compIndex++;
  };
}

TEST_F(OmxCompTest, PortParametersTest) {
  OMX_U32 compIndex = 0;
  char compName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
      compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
    OMX_LOGD("PortParametersTest: Comp %lu, %s", compIndex, compName);
    testPortParameters(compName);
    compIndex++;
  };
}

TEST_F(OmxCompTest, StateTransitionTest) {
  OMX_U32 compIndex = 0;
  char compName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
      compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
    OMX_LOGD("PortParametersTest: Comp %lu, %s", compIndex, compName);
    testStateTransition(compName);
    compIndex++;
  };
}

TEST_F(OmxCompTest, SetRoleOfComponentTest) {
  OMX_U32 compIndex = 0;
  char compName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
      compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
    OMX_LOGD("SetRoleOfComponentTest: Comp %lu, %s", compIndex, compName);
    testSetRoleOfComponent(compName);
    compIndex++;
  };
}

GTEST_API_ int main(int argc, char **argv) {
  std::cout << "Running main() from gtest_main.cc\n";
  OMX_LOGD("Running main() from gtest_main.cc");

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

}  // namespace test
}
