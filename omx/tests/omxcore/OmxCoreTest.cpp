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

#include <gtest/gtest.h>

extern "C" {
#include <OMX_Core.h>
#include <OMX_Component.h>
}

#include <BerlinOmxLog.h>

namespace berlin {
namespace test {

class OmxCoreTest : public testing::Test {
protected:
  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone,  err);
  }

  virtual void TearDown() {
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone,  err);
  }

  void testComponentNameEnum() {
    OMX_U32 compIndex = 0;
    char compName[OMX_MAX_STRINGNAME_SIZE];
    while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
           compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
      OMX_LOGD("Comp %lu, %s", compIndex, compName);
      compIndex++;
    };
    ASSERT_TRUE(compIndex > 0);
  }

  void testGetComponentsOfRole(OMX_IN OMX_STRING role) {
    OMX_ERRORTYPE err;
    OMX_U32 numComps;
    err = OMX_GetComponentsOfRole(role, &numComps, NULL);
    ASSERT_EQ(OMX_ErrorNone, err);
    ASSERT_TRUE(numComps > 0);
    OMX_LOGD("Number of components: %lu", numComps);

    OMX_U8 **compNames = new OMX_U8 *[numComps];
    for (OMX_U32 i = 0; i < numComps; ++i) {
      compNames[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
    }

    OMX_U32 numComps2 = numComps;
    err = OMX_GetComponentsOfRole(role, &numComps2, compNames);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_TRUE(numComps == numComps2);

    for (OMX_U32 i = 0; i < numComps2; ++i) {
      OMX_LOGD("Found component: %s", compNames[i]);
      delete compNames[i];
    }

    delete [] compNames;
    compNames = NULL;
  }

  void testGetRolesOfComponent(OMX_IN OMX_STRING comp) {
    OMX_ERRORTYPE err;
    OMX_U32 numRoles;
    err = OMX_GetRolesOfComponent(comp, &numRoles, NULL);
    ASSERT_EQ(OMX_ErrorNone, err);
    ASSERT_TRUE(numRoles > 0);
    OMX_LOGD("Number of roles: %lu", numRoles);

    OMX_U8 **roleNames = new OMX_U8 *[numRoles];
    for (OMX_U32 i = 0; i < numRoles; ++i) {
      roleNames[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
    }

    OMX_U32 numRoles2 = numRoles;
    err = OMX_GetComponentsOfRole(comp, &numRoles2, roleNames);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_TRUE(numRoles2 == numRoles);

    for (OMX_U32 i = 0; i < numRoles2; ++i) {
      OMX_LOGD("Found role: %s", roleNames[i]);
      delete roleNames[i];
    }

    delete [] roleNames;
    roleNames = NULL;
  }
};

TEST_F(OmxCoreTest, ComponentNameEnumTest) {
  testComponentNameEnum();
}

TEST_F(OmxCoreTest, GetRolesOfComponentTest) {
  OMX_U32 compIndex = 0;
  char compName[OMX_MAX_STRINGNAME_SIZE];
  while (OMX_ErrorNoMore != OMX_ComponentNameEnum(
      compName, OMX_MAX_STRINGNAME_SIZE, compIndex)) {
    OMX_LOGD("Comp %lu, %s", compIndex, compName);
    testGetRolesOfComponent(compName);
    compIndex++;
  };
}

TEST_F(OmxCoreTest, GetComponentsOfRoleVideoDecoderTest) {
  const char *role = NULL;
  role = "video_decoder.avc";
  testGetComponentsOfRole(const_cast<OMX_STRING>(role));
}

GTEST_API_ int main(int argc, char **argv) {
  std::cout << "Running main() from gtest_main.cc\n";
  OMX_LOGD("Running main() from gtest_main.cc");

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

}  // namespace test
}  // namespace android
