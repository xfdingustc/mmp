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

#define LOG_TAG "BerlinOmxParseXmlCfg"
#include <BerlinOmxParseXmlCfg.h>
#include <BerlinOmxLog.h>

#include <libexpat/expat.h>

namespace berlin {

  static const unsigned int buffer_size = 512;

  BerlinOmxParseXmlCfg::BerlinOmxParseXmlCfg() {
    mCfgFile = NULL;
    mHandle = NULL;
    mOption = NULL;
    mParsed = OMX_FALSE;
    mNeedExit = OMX_FALSE;
  }

  BerlinOmxParseXmlCfg::BerlinOmxParseXmlCfg(const char *cfgFile) {
    mCfgFile = cfgFile;
    mHandle = NULL;
    mOption = NULL;
    mParsed = OMX_FALSE;
    mNeedExit = OMX_FALSE;
  }

  BerlinOmxParseXmlCfg::~BerlinOmxParseXmlCfg() {
  }

  OMX_BOOL BerlinOmxParseXmlCfg::parseConfig(const char *handle,
      const char *option) {
    mHandle = handle;
    mOption = option;
    if (!mHandle || !mOption) {
      OMX_LOGW("The parse handle or option is not set.");
      return OMX_FALSE;
    }
    FILE *file = NULL;
    if (mCfgFile) {
      file = fopen(mCfgFile, "r");
    } else {
      OMX_LOGW("The config file is not set.");
      return OMX_FALSE;
    }
    if (file == NULL) {
      OMX_LOGE("Fail to open the config file %s.", mCfgFile);
      return OMX_FALSE;
    }
    XML_Parser parser = ::XML_ParserCreate(NULL);
    if (!parser) {
      OMX_LOGE("Fail to create the Parser.");
      fclose(file);
      return OMX_FALSE;
    }
    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(
            parser, StartElementHandlerWrapper, EndElementHandlerWrapper);
    mNeedExit = OMX_FALSE;
    mParsed = OMX_FALSE;
    while (mNeedExit == OMX_FALSE) {
      void *buff = ::XML_GetBuffer(parser, buffer_size);
      if (buff == NULL) {
        OMX_LOGE("Failed in call to XML_GetBuffer().");
        break;
      }
      OMX_U32 bytes_read = ::fread(buff, 1, buffer_size, file);
      if (bytes_read < 0) {
        OMX_LOGE("Failed in call to read().");
        break;
      }
      if (::XML_ParseBuffer(parser, bytes_read, bytes_read == 0) != XML_STATUS_OK) {
        OMX_LOGE("Failed in call to XML_ParseBuffer().");
        break;
      }
      if (bytes_read == 0) {
          break;
      }
    }
    ::XML_ParserFree(parser);
    fclose(file);
    return mParsed == OMX_TRUE ? OMX_TRUE : OMX_FALSE;
  }

  char * BerlinOmxParseXmlCfg::getParseData() {
    if (mParsed == OMX_FALSE) {
      OMX_LOGD("The config data has not been parsed.");
      return NULL;
    }
    return mParseData;
  }

  void BerlinOmxParseXmlCfg::StartElementHandlerWrapper(void *me,
      const char *name, const char **attrs) {
    static_cast<BerlinOmxParseXmlCfg *>(me)->startElementHandler(name, attrs);
  }

  void BerlinOmxParseXmlCfg::EndElementHandlerWrapper(void *me,
      const char *name) {
    static_cast<BerlinOmxParseXmlCfg *>(me)->endElementHandler(name);
  }

  void BerlinOmxParseXmlCfg::startElementHandler(const char *name,
      const char **attrs) {
    OMX_LOGV("start handle  <%s>", name);
    if (!strcmp(name, mHandle)) {
      int i = 0;
      while (attrs[i] != NULL) {
        if (!strcmp(attrs[i], mOption)) {
          if (attrs[i + 1] == NULL) {
            OMX_LOGD("There is no config data for this option.");
            mNeedExit = OMX_TRUE;
            return;
          }
          memset(mParseData, 0, 100);
          strcpy(mParseData, attrs[i + 1]);
          OMX_LOGD("The config data is parsed successful: %s", attrs[i + 1]);
          mParsed = OMX_TRUE;
          mNeedExit = OMX_TRUE;
          return;
        }
        i++;
      }
      OMX_LOGD("The option is not found for this handle.");
      mNeedExit = OMX_TRUE;
      return;
    }
  }

  void BerlinOmxParseXmlCfg::endElementHandler(const char *name) {
    OMX_LOGV("end handle  <%s\>", name);
  }

}  // namespace Berlin

