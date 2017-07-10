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
#include <mediahelper.h>
#define MEDIA_PATH_LENGTH 256

static MediainfoEntry *mediainfo_entry = NULL;
static MEDIA_U8 mediainfo_remove_data = 0;
static MEDIA_S8 mediainfo_buildup_count = 0;
/* Always add at least this many bytes when extending the buffer.  */
#define MIN_CHUNK 64

/* Read up to (and including) a TERMINATOR from STREAM into *LINEPTR
   + OFFSET (and null-terminate it). *LINEPTR is a pointer returned from
   malloc (or NULL), pointing to *N characters of space.  It is realloc'd
   as necessary.  Return the number of characters read (not including the
   null terminator), or -1 on error or EOF.
   NOTE: There is another getstr() function declared in <curses.h>.  */
static MEDIA_S32 mediainfo_getstr(MEDIA_S8 **lineptr, MEDIA_U32 *n,
    FILE *stream, MEDIA_S8 terminator, MEDIA_U32 offset) {
  MEDIA_S32 nchars_avail;  /* Allocated but unused chars in *LINEPTR.  */
  MEDIA_S8 *read_pos;      /* Where we're reading into *LINEPTR. */
  MEDIA_S32 ret;

  if (!lineptr || !n || !stream)
    return -1;

  if (!*lineptr) {
    *n = MIN_CHUNK;
    *lineptr = malloc(*n);
    if (!*lineptr)
      return -1;
  }

  nchars_avail = *n - offset;
  read_pos = *lineptr + offset;

  for (;;) {
    register int c = getc(stream);

    /* We always want at least one char left in the buffer, since we
    always (unless we get an error while reading the first char)
    NUL-terminate the line buffer.  */

    assert(*n - nchars_avail == read_pos - *lineptr);
    if (nchars_avail < 2) {
      if (*n > MIN_CHUNK)
        *n *= 2;
      else
        *n += MIN_CHUNK;

      nchars_avail = *n + *lineptr - read_pos;
      *lineptr = realloc(*lineptr, *n);
      if (!*lineptr)
        return -1;
      read_pos = *n - nchars_avail + *lineptr;
      assert(*n - nchars_avail == read_pos - *lineptr);
    }

    if (c == EOF || ferror (stream)) {
      /* Return partial line, if any.  */
      if (read_pos == *lineptr)
        return -1;
      else
        break;
    }

    if (c == '\r' || c == '\n') {
      /* Return the line.  */
      if (c == '\r') {
        /* skip the '\n' */
        getc(stream);
      }
     break;
    }

    *read_pos++ = c;
    nchars_avail--;
  }

  /* Done - NUL terminate and return the number of chars read.  */
  *read_pos = '\0';

  ret = read_pos - (*lineptr + offset);
  return ret;
}

MEDIA_S32 mediainfo_getline (MEDIA_S8 **lineptr,
    MEDIA_U32 *n, FILE *stream) {
  return mediainfo_getstr(lineptr, n, stream, '\n', 0);
}

static MEDIA_U32 mediainfo_split_property_value(MEDIA_S8 *lineptr) {
  MEDIA_U32 start = 0;
  if (mediainfo_entry) {
    if (mediainfo_entry->elemts) {
      MediainfoHelperEntry * realloc_elemt = NULL;
      realloc_elemt = realloc(mediainfo_entry->elemts,
          (mediainfo_entry->count + 1) * sizeof(MediainfoHelperEntry));
      if (realloc_elemt == NULL) {
        mediainfo_teardown();
        return 0;
      } else {
        mediainfo_entry->elemts = realloc_elemt;
      }
    } else {
      mediainfo_entry->elemts = malloc(sizeof(MediainfoHelperEntry));
    }
    if (mediainfo_entry->elemts == NULL) {
      mediainfo_teardown();
      return 0;
    }

    for (start = 0; start < strlen(lineptr); start++) {
      if (lineptr[start] == '=') {
        mediainfo_entry->elemts[mediainfo_entry->count].key = malloc(start + 1);
        if (mediainfo_entry->elemts[mediainfo_entry->count].key) {
          memset(mediainfo_entry->elemts[mediainfo_entry->count].key,
              0x0, start + 1);
          memcpy(mediainfo_entry->elemts[mediainfo_entry->count].key, lineptr,
              start);
        }
        mediainfo_entry->elemts[mediainfo_entry->count].value = malloc(
            strlen(lineptr) - start);
        if (mediainfo_entry->elemts[mediainfo_entry->count].value) {
          memset(mediainfo_entry->elemts[mediainfo_entry->count].value, 0x0,
              strlen(lineptr) - start);
          memcpy(mediainfo_entry->elemts[mediainfo_entry->count].value,
              lineptr + start + 1, strlen(lineptr) - start - 1);
        }
        break;
      }
    }
    mediainfo_entry->count += 1;
  }
  return 1;
}

/* @brief get media info enable file path
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @param media_path store the media enable file path
 */
static void mediainfo_get_media_enablepath(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type, MEDIA_U8 *media_path) {
  if (type == MEDIA_VIDEO) {
    if (data_type == MEDIA_DUMP_ES) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp", "es",
          "Video", "DumpConfig.txt");
    } else if (data_type == MEDIA_DUMP_FRMAE) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp", "frame",
          "Video", "DumpConfig.txt");
    } else if (data_type == MEDIA_SEEK_CLEAR) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp", "seek",
          "Video", "ClearConfig.txt");
    } else {
      MEDIA_PRINT("Invalid OMX_DUMP_OPTION type %d\n", type);
      return 0;
    }
  } else {
    if (data_type == MEDIA_DUMP_ES) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp",
          "es", "Audio", "DumpConfig.txt");
    } else if (data_type == MEDIA_DUMP_FRMAE) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp",
          "frame", "Audio", "DumpConfig.txt");
    } else if (data_type == MEDIA_SEEK_CLEAR) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s%s%s", "/tmp",
          "seek", "Audio", "ClearConfig.txt");
    } else {
      MEDIA_PRINT("Invalid OMX_DUMP_OPTION type %d\n", type);
      return 0;
    }
  }
}

/* @brief get media file dumped path
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @param media_path store the media file dumped path
 */
static MEDIA_S8 mediainfo_get_media_dumppath(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type, MEDIA_U8 *media_path) {
  MEDIA_U8 media_mount_path[MEDIA_PATH_LENGTH];
  memset(media_mount_path, 0x0, MEDIA_PATH_LENGTH);
  if (mediainfo_get_property("mediainfo_dump_path", media_mount_path) == 0) {
    return 0;
  }
  if (type == MEDIA_VIDEO) {
    if (data_type == MEDIA_DUMP_ES) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s", media_mount_path,
          "video_es.dat");
    } else if (data_type == MEDIA_DUMP_FRMAE) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s", media_mount_path,
          "video_frame.dat");
    } else {
      MEDIA_PRINT("Invalid OMX_DUMP_OPTION type %d\n", type);
      return 0;
    }
  } else {
    if (data_type == MEDIA_DUMP_ES) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s", media_mount_path,
          "audio_es.dat");
    } else if (data_type == MEDIA_DUMP_FRMAE) {
      snprintf(media_path, MEDIA_PATH_LENGTH, "%s/%s", media_mount_path,
          "audio_frame.dat");
    } else {
      MEDIA_PRINT("Invalid OMX_DUMP_OPTION type %d\n", type);
      return 0;
    }
  }
  return 1;
}

/* @brief dump the data
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @return 1 success
 *             0 fail
 */
MEDIA_U8 mediainfo_dump_data(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type, MEDIA_U8 *bits, MEDIA_U32 len) {
  FILE *dump_file = NULL;
  MEDIA_U8 media_path[MEDIA_PATH_LENGTH];
  memset(media_path, 0x0, MEDIA_PATH_LENGTH);
  if (mediainfo_get_media_dumppath(type, data_type, media_path) == 0) {
    return 0;
  }

  dump_file = fopen(media_path, "ab+");

  if (dump_file != NULL) {
    fwrite(bits, 1, len, dump_file);
    fflush(dump_file);
    fclose(dump_file);
  } else {
    return 0;
  }
  return 1;
}

/* @brief judge if the tag is enabled
 * @param type video, audio or subtitle
 * @param data_type es data or decoded data
 * @return 1 the is enabled
 *             0 the doesn't enabled
 */
MEDIA_U8 mediainfo_is_enable_dump(MH_MEDIA_TYPE type,
    MH_DUMP_TYPE data_type) {
  FILE *config_file = NULL;
  MEDIA_U8 isEnable = 0;
  MEDIA_U8 media_path[MEDIA_PATH_LENGTH];
  mediainfo_get_media_enablepath(type, data_type, media_path);
  config_file = fopen(media_path, "rb");

  if (config_file != NULL) {
    if (fread(&isEnable, 1, sizeof(isEnable), config_file) <
          sizeof(isEnable)) {
        isEnable = 0;
    } else {
      isEnable = 1;
    }
    fclose(config_file);
  }
  return isEnable;
}

/* @brief remove the dumped data
 */
void mediainfo_remove_dumped_data(MH_MEDIA_TYPE type) {
  if (mediainfo_entry == NULL) {
    return;
  }
  MEDIA_U8 media_path[MEDIA_PATH_LENGTH];
  memset(media_path, 0x0, MEDIA_PATH_LENGTH);
  mediainfo_remove_data = 1;
  if (mediainfo_get_media_dumppath(type, MEDIA_DUMP_ES, media_path)) {
    remove(media_path);
  }
  if (mediainfo_get_media_dumppath(type, MEDIA_DUMP_FRMAE, media_path)) {
    remove(media_path);
  }
  mediainfo_remove_data = 0;
}


/* @brief judge if the tag is enabled
 * @param type video, audio or subtitle
 * @param tag the judged tag info
 * @return 1 the tag is enabled
 *             0 the tag doesn't enabled
 */
MEDIA_BOOL mediainfo_is_enable(MH_MEDIA_TYPE type, MEDIA_U8 *tag) {

  return 0;
}

/* @brief get the property value
 * @param key property name
 * @return 1 if the property was enable
 *             0 if the property was not enabled
 */
MEDIA_U8 mediainfo_property_enabled(MEDIA_S8 *key) {

  return 1;
}

/* @brief get the property value
 * @param key property name
 * @return 1 if success
 *             0 if fail
 */
MEDIA_U8 mediainfo_get_property(const MEDIA_S8 *key, MEDIA_S8 *value) {
  if (mediainfo_entry == NULL) {
    return 0;
  }
  MEDIA_U32 index = 0;
  for(index = 0; index < mediainfo_entry->count; index++) {
    if (mediainfo_remove_data == 1) {
      MEDIA_PRINT("Mediainfo %d elemts %s compared key %s len %d",
        index, mediainfo_entry->elemts[index].key, key, strlen(key));
    }
    if (memcmp(mediainfo_entry->elemts[index].key, key, strlen(key)) == 0) {
      memcpy(value, mediainfo_entry->elemts[index].value,
          strlen(mediainfo_entry->elemts[index].value));
    }
  }

  return 1;
}

/* @brief set the property value
 * @param key property name
 * @param value property value
 */
void mediainfo_set_property(const MEDIA_S8 *key, MEDIA_S8 *value) {

}

void mediainfo_set_log_callback(void *callback) {

}

void mediainfo_print_info() {

}

void mediainfo_add_tag(
    MH_MEDIA_TYPE type, MEDIA_U8 *tag, MEDIA_BOOL enable) {

}

/* @brief build up mediainfo helper system
 */
MEDIA_U32 mediainfo_buildup() {
  mediainfo_buildup_count ++;
  if (mediainfo_entry) {
    MEDIA_PRINT("MediaHelper already built up");
    return 1;
  }
  if (mediainfo_entry == NULL) {
    mediainfo_entry = malloc(sizeof(MediainfoEntry));
  }
  if (mediainfo_entry) {
    memset(mediainfo_entry, 0x0, sizeof(MediainfoEntry));
  } else {
    MEDIA_PRINT("malloc mediainfo_entry fail\n");
    mediainfo_buildup_count = 0;
    return 0;
  }
  // open the set property file
  FILE *config_file = NULL;
  config_file = fopen("/data/mediaconfig.txt", "r");
  if (config_file != NULL) {
    MEDIA_S8 *linestr = NULL;
    MEDIA_U32 n;
    while(!feof(config_file)) {
      MEDIA_U32 read_len = mediainfo_getline(&linestr, &n, config_file);
      if (read_len > 0) {
        if (mediainfo_split_property_value(linestr) == 0) {
          fclose(config_file);
          mediainfo_buildup_count = 0;
          return 0;
        }
        if (linestr != NULL) {
          free(linestr);
          linestr = NULL;
        }
      }
    }
  } else {
    mediainfo_buildup_count = 0;
    return 0;
  }
  fclose(config_file);
  if (mediainfo_entry->count == 0) {
    mediainfo_teardown();
    return 0;
  }
  MEDIA_PRINT("Print Media helper info");
  MEDIA_U32 index = 0;
  for(index = 0; index < mediainfo_entry->count; index++) {
    MEDIA_PRINT("mediainfo_entry %d elemts %s value %s ",
      index, mediainfo_entry->elemts[index].key,
      mediainfo_entry->elemts[index].value);
  }
  return 1;
}

void mediainfo_teardown() {
  mediainfo_buildup_count --;
  if (mediainfo_buildup_count > 0) {
    MEDIA_PRINT("MediaHelper wait for teardown buildup count %d",
        mediainfo_buildup_count);
    return;
  }
  MEDIA_PRINT("MediaHelper teardown");
  if (mediainfo_entry) {
    if (mediainfo_entry->elemts) {
      MEDIA_S8 index = 0;
      for (index = 0; index < mediainfo_entry->count; index ++) {
        if (mediainfo_entry->elemts[index].key) {
          free(mediainfo_entry->elemts[index].key);
        }
        if (mediainfo_entry->elemts[index].value) {
          free(mediainfo_entry->elemts[index].value);
        }
      }
      free(mediainfo_entry->elemts);
    }
    free(mediainfo_entry);
  }
  mediainfo_entry = NULL;
  mediainfo_buildup_count = 0;
}

void test_main() {
  mediainfo_buildup();
  MEDIA_S8 value[12];
  mediainfo_get_property("mediainfo_dump_path", value);
  MEDIA_PRINT("mediainfo_dump_path is %s\n", value);
}
