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

#ifndef BERLIN_OMX_PARSE_XML_CFG_H_
#define BERLIN_OMX_PARSE_XML_CFG_H_

#include <BerlinOmxCommon.h>

namespace berlin {

class BerlinOmxParseXmlCfg {
public:
  BerlinOmxParseXmlCfg();
  BerlinOmxParseXmlCfg(const char *cfgFile);
  ~BerlinOmxParseXmlCfg();
  OMX_BOOL parseConfig(const char *handle, const char *option);
  char * getParseData();
  static void StartElementHandlerWrapper(void *me, const char *name,
      const char **attrs);
  static void EndElementHandlerWrapper(void *me, const char *name);

private:
  void startElementHandler(const char *name, const char **attrs);
  void endElementHandler(const char *name);

private:
  const char *mCfgFile;
  const char *mHandle;
  const char *mOption;
  char mParseData[100];
  OMX_BOOL mParsed;
  OMX_BOOL mNeedExit;
};

}  // namespace Berlin
#endif   // BERLIN_OMX_PARSE_XML_CFG_H_