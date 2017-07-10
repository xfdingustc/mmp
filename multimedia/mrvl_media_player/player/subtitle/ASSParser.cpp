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
#include <utils/Log.h>

#include "ASSParser.h"

#undef  LOG_TAG
#define LOG_TAG "ASSParser"

namespace mmp {

list<MmpBuffer*> ASSParser::ParsePacket(MmpBuffer* pkt) {
  list<MmpBuffer*> subtitles;
  subtitles.clear();

  uint8_t* ass_payload = pkt->data;
  if (!ass_payload) {
    delete pkt;
    return subtitles;
  }

  char *sub = NULL;
  const char *str = (const char*)(pkt->data);
  int sh, sm, ss, sc, eh, em, es, ec;
  int64_t start_pts, end_pts;

  char *layer = new char[pkt->size + 1];
  if (!layer) {
    ALOGE("%s() line %d, Out of memory", __FUNCTION__, __LINE__);
    goto out;
  }

  while (strlen(str) && (sub = strstr(str, "Dialogue:"))) {
    char *next_sub = NULL;
    if (sub) {
      // Search whether more than one subtitle is there.
      next_sub = strstr(sub + strlen("Dialogue:"), "Dialogue:");
    }
    int sub_len = 0;
    if (next_sub) {
      sub_len = next_sub - sub;
    } else {
      sub_len = strlen(sub);
    }
    char *subtitle  = new char[sub_len + 1];
    if (!subtitle) {
      ALOGE("%s() line %d, Out of memory", __FUNCTION__, __LINE__);
      break;
    }
    memset(subtitle, 0x00, sub_len + 1);
    memcpy(subtitle, sub, sub_len);
    ALOGV("%s() line %d, subtitle = %s, sub_len = %d",
        __FUNCTION__, __LINE__, subtitle, sub_len);
    if (sscanf(subtitle, "%*[^:]:%[^,],%d:%d:%d%*c%d,%d:%d:%d%*c%d",
        layer, &sh, &sm, &ss, &sc, &eh,&em, &es, &ec) != 9) {
      ALOGE("%s() line %d, Failed to parse subtitle", __FUNCTION__, __LINE__);
      delete subtitle;
      continue;
    } else {
      start_pts = (sh * 360000.0) + (sm * 6000.0) + (ss * 100.0) + sc;
      end_pts = (eh * 360000.0) + (em * 6000.0) + (es * 100.0) + ec;
      // Pts in us.
      start_pts *= 10000;
      end_pts *= 10000;
      ALOGV("%s() line %d, pts = %lld us, end pts = %lld us",
          __FUNCTION__, __LINE__, start_pts, end_pts);

      // Find start position of content.
      uint32_t i = 0;
      uint32_t j = 0;
      int data_offset = 0;
      for (i = 0; i < sub_len; i++) {
        if (subtitle[i] == ',') {
          if (++j == 9) {
            data_offset = i + 1;
            break;
          }
        }
      }
      ALOGV("%s() line %d, subtitle = %s, data_offset = %d",
          __FUNCTION__, __LINE__, subtitle, data_offset);

      memset(layer, 0x00, pkt->size + 1);
      // Currently skip all the internal set in {}
      for (i = data_offset, j = 0; (i < sub_len + 1) && (subtitle[i] != '\0'); i++) {
        if (subtitle[i] == '\{') {
          while(subtitle[i] != '\}') { i++; }
        } else {
          layer[j++] = subtitle[i];
        }
      }

      layer[j-2] = '\0';

      ALOGV("%s() line %d, layer = %s", __FUNCTION__, __LINE__, layer);
      if (!strlen(layer)) {
        continue;
      }

      // For subtitles contains "\n", "\N", we need separate it.
      char *content = layer;
      char *new_line = NULL;
      do {
        new_line = strstr(content, "\\n");
        if (!new_line) {
          new_line = strstr(content, "\\N");
        }
        ALOGV("%s() line %d, new_line = %s", __FUNCTION__, __LINE__, new_line);
        int len = 0;
        if (new_line) {
          len = new_line - content;
        } else {
          len = strlen(content);
        }
        ALOGV("%s() line %d, len = %d", __FUNCTION__, __LINE__, len);
        if (!len) {
          break;
        }

        MmpBuffer* newsub = new MmpBuffer;
        if (!newsub) {
          ALOGE("Out of memory");
          goto out;
        }
        newsub->data = (uint8_t *)malloc(len + 1);
        if (!newsub->data) {
          ALOGE("Out of memory");
          goto out;
        }
        memset(newsub->data, 0x00, len + 1);
        newsub->pts = start_pts;
        newsub->end_pts = end_pts;
        newsub->size = len;
        memcpy(newsub->data, content, len);
        ALOGE("%s() line %d, newsub->pts = %lld, newsub->end_pts = %lld, len=%d, data=%s",
            __FUNCTION__, __LINE__, newsub->pts, newsub->end_pts, len, (char *)newsub->data);
        subtitles.push_back(newsub);
        // next line
        if (!new_line) {
          break;
        } else {
          content = new_line + 2;
        }
      } while (strlen(content));
    }

    delete []subtitle;
    // Next subtitle
    if (next_sub) {
      str = next_sub;
    } else {
      break;
    }
  }

out:
  ALOGD("%s() line %d, subtitles.size() = %d",
      __FUNCTION__, __LINE__, subtitles.size());
  delete []layer;
  delete pkt;
  return subtitles;

}


}
