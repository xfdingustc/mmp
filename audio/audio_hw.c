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

#define LOG_TAG "mv_audio_hw"

//#define NDEBUG
#define VERSION_KITKAT 19

#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <tinyalsa/asoundlib.h>

#include <amp_buf_desc.h>
#include <amp_client_support.h>
#include <amp_component.h>
#include <amp_sound_api.h>
#include <OSAL_api.h>
#include "audio_client_api.h"

#define MV_DEBUG 0

#ifdef MV_DEBUG
#define ENTER(_fmt, ...) \
    ALOGD("Enter %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LEAVE(_fmt, ...) \
    ALOGD("Leave %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define MVLOGV(_fmt, ...) \
    ALOGV("Function %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MVLOGD(_fmt, ...) \
    ALOGD("Function %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MVLOGW(_fmt, ...) \
    ALOGW("Function %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define MVLOGE(_fmt, ...) \
    ALOGE("Function %s(), Line %d, " _fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ENTER(_fmt, ...)
#define LEAVE(_fmt, ...)

#define MVLOGV(_fmt, ...)
#define MVLOGD(_fmt, ...)
#define MVLOGW(_fmt, ...)
#define MVLOGE(_fmt, ...)
#endif

//#define DUMP_INPUT_DATA 0
#ifdef DUMP_INPUT_DATA
static FILE *dump_input_ = NULL;
#endif

#ifdef DUMP_OUTPUT_DATA
static FILE *dump_output_ = NULL;
#endif

// for master volume control
extern int setMasterVolume(float volume);
extern int getMasterVolume(float* volume);
extern int32_t initSourceGain();
extern int32_t getMixGain();
static int32_t gSourceGain = 0;

static char *client_argv[] =
    {"client", "iiop:1.0//127.0.0.1:999/AMP::FACTORY/factory"};

struct marvell_audio_device {
    struct audio_hw_device device;
    // who owns alsa device
    struct marvell_stream_in* active_input;

    // for open/close input stream
    pthread_mutex_t mutex;

    // for master volume control
    float master_volume;
    bool master_muted;

    // for mic volume control
    bool mic_muted;
};

struct marvell_stream_out {
    struct audio_stream_out stream;

    // record output config
    struct audio_config config;

    uint32_t buffer_size;

    HANDLE *hSound;
};

struct marvell_stream_in {
    struct audio_stream_in stream;

    void (*release_device)(struct marvell_stream_in* in);

    struct audio_config config;
    struct pcm *pcm;
    size_t buffer_size;
};

static char* FormatToString(audio_format_t format) {
    switch (format) {
        case AUDIO_FORMAT_PCM_8_BIT:
            return "AUDIO_FORMAT_PCM_8_BIT";
        case AUDIO_FORMAT_PCM_16_BIT:
            return "AUDIO_FORMAT_PCM_16_BIT";
        case AUDIO_FORMAT_PCM_32_BIT:
            return "AUDIO_FORMAT_PCM_32_BIT";
        case AUDIO_FORMAT_PCM_8_24_BIT:
            return "AUDIO_FORMAT_PCM_8_24_BIT";
        case AUDIO_FORMAT_MP3:
            return "AUDIO_FORMAT_MP3";
        case AUDIO_FORMAT_AMR_NB:
            return "AUDIO_FORMAT_AMR_NB";
        case AUDIO_FORMAT_AMR_WB:
            return "AUDIO_FORMAT_AMR_WB";
        case AUDIO_FORMAT_AAC:
            return "AUDIO_FORMAT_AAC";
        case AUDIO_FORMAT_VORBIS:
            return "AUDIO_FORMAT_VORBIS";
        default:
            return "unknown format";
    }
}

static char* ChannelToString(audio_channel_mask_t channel_mask) {
    switch (channel_mask) {
        case AUDIO_CHANNEL_OUT_MONO:
            return "AUDIO_CHANNEL_OUT_MONO";
        case AUDIO_CHANNEL_OUT_STEREO:
            return "AUDIO_CHANNEL_OUT_STEREO";
        case AUDIO_CHANNEL_IN_MONO:
            return "AUDIO_CHANNEL_OUT_MONO";
        case AUDIO_CHANNEL_IN_STEREO:
            return "AUDIO_CHANNEL_OUT_MONO";
        default:
            return "unknown channel mask";
    }
}

static  void setAudioMixGain(HANDLE hStream) {
  HRESULT err = SUCCESS;
  AMP_APP_PARAMIXGAIN stMixerGain;
  if (gSourceGain > 0) {
    stMixerGain.uiLeftGain = gSourceGain;
    stMixerGain.uiRghtGain = gSourceGain;
    stMixerGain.uiCntrGain = gSourceGain;
    stMixerGain.uiLeftSndGain = gSourceGain;
    stMixerGain.uiRghtSndGain = gSourceGain;
    stMixerGain.uiLfeGain = gSourceGain;
    stMixerGain.uiLeftRearGain = gSourceGain;
    stMixerGain.uiRhgtRearGain = gSourceGain;
    MVLOGD("Set MixGain : %0x", gSourceGain);
    err = AMP_SND_SetMixerGain(hStream, &stMixerGain);
    if (err != SUCCESS) {
      MVLOGE("set source service MixerGain failed on audiohw");
    }
  }
}

static BOOL IsAudioTunnelMode(audio_format_t format) {
    switch (format) {
      case AUDIO_FORMAT_MP3:
      case AUDIO_FORMAT_AMR_NB:
      case AUDIO_FORMAT_AMR_WB:
      case AUDIO_FORMAT_AAC:
      case AUDIO_FORMAT_VORBIS:
          return true;
      default:
          return false;
    }
  return false;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    if (!out) {
        MVLOGW("Invalid argument");
        return 0;
    }
    LEAVE("stream=%p, sample rate=%d", stream, out->config.sample_rate);
    return out->config.sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    ENTER("stream=%p, rate=%u", stream, rate);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, rate=%u", stream, rate);
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    if (!out) {
        MVLOGW("Invalid argument");
        return 0;
    }
    LEAVE("stream=%p, buffer size=%d", stream, out->buffer_size);
    return out->buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    if (!out) {
        MVLOGW("Invalid argument");
        return 0;
    }
    LEAVE("stream=%p, channel mask=%s", stream, ChannelToString(out->config.channel_mask));
    return out->config.channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    if (!out) {
        MVLOGW("Invalid argument");
        return 0;
    }
    LEAVE("stream=%p, format=%s", stream, FormatToString(out->config.format));
    return out->config.format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    ENTER("stream=%p, format=%d", stream, format);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, format=%d", stream, format);
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    MVLOGD("currently we do nothing here");
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;

    audio_client_suspend_output(stream);

    LEAVE("stream=%p", stream);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    ENTER("stream=%p, fd=%d", stream, fd);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, fd=%d", stream, fd);
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ENTER("stream=%p, kvpairs=%s", stream, kvpairs);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    LEAVE("stream=%p, kvpairs=%s", stream, kvpairs);
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ENTER("stream=%p, keys=%p", stream, keys);
    LEAVE("stream=%p, keys=%p", stream, keys);
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out* out = (struct marvell_stream_out*)stream;
    if (!out) {
        MVLOGW("Invalid argument");
        return 0;
    }
    //TODO: need implement this
    int32_t latency = 0;
    LEAVE("stream=%p, latency=%d", stream, latency);
    return latency;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    ENTER("stream=%p, left=%f, right=%f", stream, left, right);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, left=%f, right=%f", stream, left, right);
    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    //MVLOGD("write size =%d,",  bytes);

#ifdef DUMP_OUTPUT_DATA
    if (!dump_output_){
      dump_output_ = fopen("/data/audio_hw_out_dump_pcm.dat", "wa");
      if (dump_output_) {
        ALOGE("%s() line %d, open /data/audio_hw_out_dump_pcm.dat success",
            __FUNCTION__, __LINE__);
      }
    }
    if (dump_output_) {
      fwrite(buffer, bytes, 1, dump_output_);
      fflush(dump_output_);
    }
#endif
//    ENTER("stream=%p, buffer=%p, bytes=%d", stream, buffer, bytes);
    if (!stream || !buffer) {
      MVLOGE("invalid argument, stream=%p, buffer=%p", stream, buffer);
      return 0;
    }
    struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
    if (gSourceGain != getMixGain()) {
      gSourceGain = getMixGain();
      setAudioMixGain(out->hSound);
    }

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      // proc data; send all parameters to decoded data to SND
      processData(buffer, bytes);
    } else
#endif
    {
      HRESULT rc;
      if(isLowlatncy()) {
        MVLOGV("AMP_SND_PushPCM");
        rc = AMP_SND_PushPCM(out->hSound, buffer, bytes);
      } else {
        MVLOGV("AMP_SND_PushPCM_NonBlock");
        rc = AMP_SND_PushPCM_NonBlock(out->hSound, buffer, bytes);
      }
      if (rc != S_OK) {
        MVLOGE("SND_PushPCM fail:0x%x, out->hSound = %p", rc, out->hSound);
      }
    }
//    LEAVE("stream=%p, buffer=%p, bytes=%d", stream, buffer, bytes);
    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      uint32_t frames_num = getRendorSampleNum();
      *dsp_frames = frames_num;
      MVLOGV("out_get_render_position dsp frame num = %u", frames_num);
     }
#endif
    //LEAVE("stream=%p", stream);
    return 0;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ENTER("stream=%p, effect=%p", stream, effect);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, effect=%p", stream, effect);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ENTER("stream=%p, effect=%p", stream, effect);
    MVLOGW("unsupported yet");
    LEAVE("stream=%p, effect=%p", stream, effect);
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    //ENTER("stream=%p", stream);
    return -EINVAL;
}

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
static int out_flush(const struct audio_stream_out *stream)
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      adecFlush();
    }
    return 0;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
    uint64_t *frames, struct timespec *timestamp)
{
    //ENTER("stream=%p", stream);
    return 0;
}

static int out_set_callback(const struct audio_stream_out *stream,
    stream_callback_t callback, void *cookie)
{
   ENTER("stream=%p", stream);
   return 0;
}

static int out_pause(const struct audio_stream_out *stream)
{
   ENTER("stream=%p", stream);
   struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
   if (IsAudioTunnelMode(out->config.offload_info.format)) {
     adecPause();
   }
   return 0;
}

static int out_resume(const struct audio_stream_out *stream)
{
   struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
   ENTER("stream=%p", stream);
   if (IsAudioTunnelMode(out->config.offload_info.format)) {
     adecResume();
   }
   return 0;
}

/*  Don't need it yet, or offload music can't repeat play.
static int out_drain(const struct audio_stream_out *stream, audio_drain_type_t type )
{
    ENTER("stream=%p", stream);
    struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
    MVLOGD("drain type %d", type);
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      MVLOGD("adecRelease");
      adecRelease();
    }
    return 0;
}
*/
#endif

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    if (!stream) {
        MVLOGE("invalid argument");
        return 0;
    }
    struct marvell_stream_in* in = (struct marvell_stream_in*)stream;
    uint32_t sample_rate = in->config.sample_rate;
    LEAVE("sample rate %u", sample_rate);
    return sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    MVLOGW("unsupported operation");
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    if (!stream) {
        MVLOGE("invalid argument");
        return 0;
    }
    struct marvell_stream_in* in = (struct marvell_stream_in*)stream;
    size_t size = in->buffer_size;
    LEAVE("buffer size %d", size);
    return size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    if (!stream) {
        MVLOGE("invalid argument");
        return AUDIO_CHANNEL_IN_MONO;
    }
    struct marvell_stream_in* in = (struct marvell_stream_in*)stream;
    audio_channel_mask_t channel_mask = in->config.channel_mask;
    LEAVE("channel mask %s", ChannelToString(channel_mask));
    return channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    ENTER("stream=%p", stream);
    if (!stream) {
        MVLOGE("invalid argument");
        return AUDIO_FORMAT_INVALID;
    }
    struct marvell_stream_in* in = (struct marvell_stream_in*)stream;
    audio_format_t format = in->config.format;
    LEAVE("format %s", FormatToString(format));
    return format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    MVLOGW("unsupported yet");
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    MVLOGD("do nothing here");
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    MVLOGW("unsupported yet, stream=%p, fd=%d", stream, fd);
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    MVLOGW("unsupported yet, stream=%p, kvpairs=(%s)", stream, kvpairs);
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    MVLOGW("unsupported yet, stream=%p, keys=(%s)", stream, keys);
    return strdup("");
}

#define MIC_CAPTURE_VOLUME "Mic Capture Volume"
static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    ENTER("stream=%p, gain=%f", stream, gain);

    if (gain < 0.0f || gain > 1.0f) {
        MVLOGE("Invalid argument, gain=%f", gain);
        return -1;
    }

    struct mixer* mixer = mixer_open(0);
    if (!mixer) {
        MVLOGE("Failed to open mixer");
        return -1;
    }

    struct mixer_ctl* ctl = mixer_get_ctl_by_name(mixer, MIC_CAPTURE_VOLUME);
    if (!ctl) {
        MVLOGE("Failed to get mixer control(%s)", MIC_CAPTURE_VOLUME);
    } else {
        MVLOGD("ctl(%s) value range %d->%d",
                                MIC_CAPTURE_VOLUME,
                                mixer_ctl_get_range_min(ctl),
                                mixer_ctl_get_range_max(ctl));
        unsigned int i = 0, num_values = 0;
        int percent = (int) (gain * 100);
        num_values = mixer_ctl_get_num_values(ctl);
        for (i = 0; i < num_values; i++) {
            ALOGD("previous capture volume(%d, %d)", i, mixer_ctl_get_value(ctl, i));
            if (mixer_ctl_set_percent(ctl, i, percent)) {
                MVLOGE("Failed to set ctl value(%d, %d)", i, percent);
            }
            MVLOGD("new capture volume(%d, %d)", i, mixer_ctl_get_value(ctl, i));
        }
    }

    mixer_close(mixer);
    LEAVE("stream=%p, gain=%f", stream, gain);
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    ENTER("stream=%p, buffer=%p, bytes=%d", stream, buffer, bytes);
    int bytes_read = 0;
    if(!stream ||!buffer ||bytes <= 0) {
        MVLOGE("Invalid argument, stream=%p, buffer=%p, bytes=%d", stream, buffer, bytes);
        goto EXIT;
    }

    struct marvell_stream_in* in =  (struct marvell_stream_in*)stream;
    if ((in->pcm == NULL) || (!pcm_is_ready(in->pcm))) {
        MVLOGE("No pcm device or device is not ready, cannot read data");
        goto EXIT;
    }

    int ret = pcm_read(in->pcm, buffer, bytes);
    if (ret != 0) {
        MVLOGE("Failed to read pcm data from alsa driver, ret=%d", ret);
        goto EXIT;
    }

    bytes_read = bytes;

#ifdef DUMP_INPUT_DATA
    if (bytes_read == 0) {
        goto EXIT;
    }

    if (!dump_input_) {
        dump_input_ = fopen("/data/audio_hw_input_dump_pcm.dat", "wa");
        if (dump_input_) {
            MVLOGD("Save audio input data to file /data/audio_hw_input_dump_pcm.dat");
        } else {
            MVLOGW("Cannot save captured data, need 'chmod 777 /data' first");
        }
    }
    if (dump_input_) {
        fwrite(buffer, bytes, 1, dump_input_);
        fflush(dump_input_);
    }
#endif

EXIT:
    LEAVE("stream=%p, buffer=%p, bytes_read=%d", stream, buffer, bytes_read);
    return bytes_read;
}

static void in_release_device(struct marvell_stream_in* in)
{
    ENTER("stream=%p", in);
    if ((in != NULL) && (in->pcm != NULL)) {
        pcm_close(in->pcm);
        in->pcm = NULL;
    }
    LEAVE("stream=%p", in);
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    MVLOGW("unsupported yet");
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    MVLOGW("unsupported yet");
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    MVLOGW("unsupported yet");
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out)
{
    ENTER("dev=%p, handle=%d, devices=%d, flags=%d, config=%p",
                dev, handle, devices, flags, config);

    if (!dev) {
        MVLOGE("Invalid argument");
        goto ERR_EXIT;
    }

    if (!config) {
        MVLOGE("Invalid argument");
        goto ERR_EXIT;
    }

    if (!stream_out) {
        MVLOGE("Invalid argument");
        goto ERR_EXIT;
    }

    struct marvell_stream_out *out =
            (struct marvell_stream_out *)calloc(1, sizeof(struct marvell_stream_out));

    if (!out) {
        MVLOGE("Out of memory while allocating audio_stream_out");
        return -ENOMEM;
    }

    MVLOGD("Output config: (%d, %s, %s)",
        config->sample_rate,
        ChannelToString(config->channel_mask),
        FormatToString(config->format));
    out->config.sample_rate = config->sample_rate;
    out->config.channel_mask = config->channel_mask;
    out->config.format = config->format;

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    out->config.offload_info = config->offload_info;
#endif
    out->buffer_size = 2048;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    out->stream.flush = out_flush;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.set_callback = out_set_callback;
    out->stream.pause = out_pause;
    out->stream.resume = out_resume;
    //out->stream.drain = out_drain;
#endif
    HRESULT err = AMP_SND_CreateStream(NULL, &(out->hSound));
    if (err != SUCCESS) {
        free(out);
        MVLOGE("AMP_SND_CreateStream failed, err=%d", err);
        goto ERR_EXIT;
    }

    AMP_SND_SetSampleRate(out->hSound, 0, config->sample_rate);

    gSourceGain = initSourceGain();
    setAudioMixGain(out->hSound);

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      MVLOGD("Create audiotunneling!!!!");
      initAudioTunneling();
      adecPrepare(out->hSound, out->config.offload_info.format);
    }
#endif

    *stream_out = &out->stream;

    LEAVE();
    return 0;

ERR_EXIT:
    if (stream_out != NULL) {
      *stream_out = NULL;
    }
    MVLOGE("cannot open output stream");
    return -1;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    ENTER("dev=%p, stream=%p", dev, stream);
    HRESULT err = SUCCESS;

    struct marvell_stream_out *out = (struct marvell_stream_out *)stream;
    if (out->hSound) {
        err = AMP_SND_DestroyStream(out->hSound);
        if (SUCCESS != err) {
            MVLOGE("AMP_SND_DestroyStream() failed, err=%d", err);
        }
    }

#if PLATFORM_SDK_VERSION >= VERSION_KITKAT
    if (IsAudioTunnelMode(out->config.offload_info.format)) {
      MVLOGD("Close audio tunneling!!!!");
      adecRelease();
    }
#endif

#ifdef DUMP_OUTPUT_DATA
    if (dump_output_) {
        fclose(dump_output_);
    }
#endif

    free(stream);
    LEAVE("dev=%p", dev);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    MVLOGD("unsupported yet, adev=%p, kvpairs=%s", dev, kvpairs);
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    MVLOGD("unsupported yet, adev=%p, keys=%p", dev, keys);
    return NULL;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    MVLOGD("do nothing here for now, adev=%p", dev);
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    MVLOGW("unsupported yet, adev=%p, volume=%f", dev, volume);
    return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    ENTER("adev=%p, volume=%f", dev, volume);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if ((!adev) || (volume < 0.0f) || (volume > 1.0f)) {
        MVLOGE("invalid argument");
        goto EXIT;
    }

    if (fabs(adev->master_volume - volume) < 0.000001f) {
      MVLOGW("The same volume as previous, don't need set again");
      goto EXIT;
    }

    int ret = setMasterVolume(volume);
    if (ret) {
        MVLOGE("set master volume failed, ret=%d", ret);
        goto EXIT;
    }

    MVLOGD("set master volume success, volume=%f", volume);
    adev->master_volume = volume;

EXIT:
    LEAVE("adev=%p, volume=%f", dev, volume);
    return 0;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    ENTER("adev=%p, volume=%p", dev, volume);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if (adev && volume) {
        if ((adev->master_volume < 0.0f)  && getMasterVolume(&adev->master_volume)) {
            // if fail to get volume from AVSettings, we set it to 0.5f.
            adev->master_volume = 0.5f;
        }
        MVLOGD("master volume is %f", adev->master_volume);
        *volume = adev->master_volume;
    }
    LEAVE("adev=%p, volume=%p", dev, volume);
    return 0;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    ENTER("adev=%p, muted=%d", dev, muted);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if (!adev) {
        MVLOGE("invalid argument");
        goto EXIT;
    }

    // if mute, set volume to 0
    float volume = 0;
    // if unmute, recover volume
    if (!muted) {
        volume = adev->master_volume;
    }

    int ret = setMasterVolume(volume);
    if (ret) {
        MVLOGE("set master mute failed, ret=%d", ret);
        goto EXIT;
    }

    MVLOGD("set master mute success, muted=%d, volume=%f", muted, adev->master_volume);
    adev->master_muted = muted;

EXIT:
    LEAVE("adev=%p, muted=%d", dev, muted);
    return 0;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    ENTER("adev=%p, muted=%p", dev, muted);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if (adev && muted) {
        MVLOGD("master muted is %d", adev->master_muted);
        *muted = adev->master_muted;
    }
    LEAVE("adev=%p, muted=%p", dev, muted);
    return 0;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    MVLOGW("unsupported yet, adev=%p, mode=%d", dev, mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    ENTER("adev=dev, state=%d", dev, state);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if (!adev) {
        MVLOGE("invalid argument");
        return -1;
    }
    adev->mic_muted = state;

    //TODO: we need implement some real logic here

    LEAVE("adev=%p, state=%d", dev, state);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    ENTER("adev=%p, state=%p", dev, state);
    struct marvell_audio_device *adev = (struct marvell_audio_device *)dev;
    if (!adev || !state) {
        MVLOGE("invalid argument, adev=%p, state=%p", adev, state);
        return -1;
    }
    *state = adev->mic_muted;
    LEAVE("adev=%p, state=%d", dev, adev->mic_muted);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    MVLOGD("buffer size 4096");
    return 4096;
}

static void translate_config_1(struct audio_config *config, struct pcm_config* alsa_config)
{
    if (config->channel_mask == AUDIO_CHANNEL_IN_MONO) {
        alsa_config->channels = 1;
    } else if (config->channel_mask == AUDIO_CHANNEL_IN_STEREO) {
        alsa_config->channels = 2;
    } else {
        ALOGW("Unsupported channel config(0x%x), use default(AUDIO_CHANNEL_IN_MONO)",
                                config->channel_mask);
        alsa_config->channels = 1;
    }

    alsa_config->rate = config->sample_rate;

    if (config->format != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGW("Unsupported format(0x%x), use default(AUDIO_FORMAT_PCM_16_BIT)", config->format);
    }

    // only support 'PCM 16BIT'
    alsa_config->format = PCM_FORMAT_S16_LE;
}

static void translate_config_2(struct pcm_config* alsa_config, struct audio_config *config)
{
    if (alsa_config->channels == 1) {
        config->channel_mask = AUDIO_CHANNEL_IN_MONO;
    } else {
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
    }

    config->sample_rate = alsa_config->rate;
    config->format = AUDIO_FORMAT_PCM_16_BIT;
}

#define SAMPLE_RATES_LEN 9
#define CHANNELS_LEN 2
static uint32_t sample_rates[SAMPLE_RATES_LEN] =
    {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000};
static uint32_t channels[CHANNELS_LEN] = {1, 2};

static struct pcm* open_capture_device(int card, struct pcm_config* alsa_config)
{
    struct pcm* pcm = NULL;
    pcm = pcm_open(card, 0, PCM_IN, alsa_config);
    if (pcm_is_ready(pcm)) {
        return pcm;
    }

    ALOGW("Cannot open pcm device with requested config(%s), try other configs",
                        pcm_get_error(pcm));
    pcm_close(pcm);
    pcm = NULL;

    int i, j;
    for (i = 0; i < SAMPLE_RATES_LEN; i++) {
        for (j = 0; j < CHANNELS_LEN; j++) {
            alsa_config->channels = channels[j];
            alsa_config->rate = sample_rates[i];
            alsa_config->format = PCM_FORMAT_S16_LE;

            pcm = pcm_open(card, 0, PCM_IN, alsa_config);
            if (pcm_is_ready(pcm)) {
                ALOGD("capture device opened(sample rate:%d, channels: %d, format: %d)",
                                sample_rates[i],
                                channels[j],
                                PCM_FORMAT_S16_LE);
                return pcm;
            }

            pcm_close(pcm);
            pcm = NULL;
        }
    }

    ALOGE("Give up trying, cannot open input stream");
    return NULL;
}

static void init_stream_in(struct marvell_stream_in* in)
{
    memset(in, 0, sizeof(struct marvell_stream_in));

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->release_device = in_release_device;
}

static bool alsa_capture_device_exist(int* card) {
    if (access("/dev/snd/pcmC0D0c", 0) == 0) {
        *card = 0;
        return true;
    }
    if (access("/dev/snd/pcmC1D0c", 0) == 0) {
        *card = 1;
        return true;
    }
    if (access("/dev/snd/pcmC2D0c", 0) == 0) {
        *card = 2;
        return true;
    }
    if (access("/dev/snd/pcmC3D0c", 0) == 0) {
        *card = 3;
        return true;
    }
    return false;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in)
{
    ENTER("audio device=%p, audio io handle=%d, audio devices=0x%x", dev, handle, devices);

    if (dev == NULL) {
        MVLOGE("Invalid parameter");
        goto ERR_EXIT;
    }

    if (config == NULL) {
        MVLOGE("Invalid parameter");
        goto ERR_EXIT;
    }

    if (stream_in == NULL) {
        MVLOGE("Invalid parameter");
        goto ERR_EXIT;
    }

    if (!audio_is_input_device(devices)) {
        MVLOGE("Invalid parameter(0x%x)", devices);
        goto ERR_EXIT;
    }

    int card = 0;
    if (!alsa_capture_device_exist(&card)) {
        MVLOGE("No sound capture card detected");
        goto ERR_EXIT;
    }

    // only allow to open one input stream at a time,
    // so we need to close previously opened if have
    struct marvell_audio_device* adev = (struct marvell_audio_device*)dev;
    pthread_mutex_lock(&adev->mutex);

    if (adev->active_input != NULL) {
        adev->active_input->release_device(adev->active_input);
        adev->active_input = NULL;
    }

    struct marvell_stream_in* in =
        (struct marvell_stream_in *)calloc(1, sizeof(struct marvell_stream_in));
    if (!in) {
        ALOGE("Out of memory while opening input stream, usually we should not be here");
        pthread_mutex_unlock(&adev->mutex);
        goto ERR_EXIT;
    }

    init_stream_in(in);

    struct pcm_config alsa_config;
    struct pcm* pcm = NULL;
    alsa_config.channels = 1;
    alsa_config.rate = 16000;
    alsa_config.format = PCM_FORMAT_S16_LE;
    alsa_config.period_size = 1024;
    alsa_config.period_count = 4;
    alsa_config.start_threshold = 0;
    alsa_config.stop_threshold = 0;
    alsa_config.silence_threshold = 0;

    ALOGD("requested config: (%d, %s, %s)",
        config->sample_rate,
        ChannelToString(config->channel_mask),
        FormatToString(config->format));

    translate_config_1(config, &alsa_config);

    pcm = open_capture_device(card, &alsa_config);
    if (pcm == NULL) {
        ALOGE("Cannot open capture device, do we have microphone connected ?");
        free(in);
        pthread_mutex_unlock(&adev->mutex);
        goto ERR_EXIT;
    }

    // record actually used capture config
    translate_config_2(&alsa_config, &in->config);

    in->pcm = pcm;
    in->buffer_size = pcm_get_buffer_size(in->pcm);
    ALOGD("Alsa audio driver input buffer size=%d", in->buffer_size);

    // record the stream we opened
    adev->active_input = in;

    *stream_in = &in->stream;
    pthread_mutex_unlock(&adev->mutex);

    LEAVE();
    return 0;

ERR_EXIT:
    if (stream_in != NULL) {
      *stream_in = NULL;
    }
    return -1;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    ENTER("adev=%p, stream=%p", dev, stream);

    if (!dev || !stream) {
        MVLOGW("invalid argument");
        return ;
    }

    struct marvell_audio_device* adev = (struct marvell_audio_device*)dev;
    struct marvell_stream_in* in = (struct marvell_stream_in*)stream;

    pthread_mutex_lock(&adev->mutex);
    if (adev->active_input == in) {
        adev->active_input = NULL;
    }
    in->release_device(in);
    free(in);
#ifdef DUMP_INPUT_DATA
    if (dump_input_) {
        fclose(dump_input_);
        dump_input_ = NULL;
    }
#endif
    pthread_mutex_unlock(&adev->mutex);

    LEAVE();
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    if (!device) {
        MVLOGW("Invalid argument");
        return 0;
    }

    struct marvell_audio_device* adev = (struct marvell_audio_device*) device;
    if (adev->active_input != NULL) {
        adev->device.close_input_stream((struct audio_hw_device*) adev, adev->active_input);
    }

    pthread_mutex_destroy(&adev->mutex);

    MVLOGD("AMP_SND_Deinit() return %d", AMP_SND_Deinit());

    free(adev);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct marvell_audio_device *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct marvell_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    adev->active_input = NULL;

    pthread_mutex_init(&adev->mutex, NULL);

    // init master volume to an invalid value then
    // we will get the value from AVSettings
    adev->master_volume = -1.0f;
    // init master volume to unmuted
    adev->master_muted = false;

    // init mic status to unmuted
    adev->mic_muted = false;
    AMP_FACTORY factory = NULL;
    MVLOGD("AMP_GetFactory() return %d", AMP_GetFactory(&factory));

    if (factory == NULL) {
        MVLOGD("MV_OSAL_Init() return %d", MV_OSAL_Init());
        MVLOGD("AMP_Initialize() return %d", AMP_Initialize(2, client_argv, &factory));
    }

    MVLOGD("AMP_SND_Init() return %d", AMP_SND_Init());

    *device = &adev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "MV ANDROID AUDIO HW HAL",
        .author = "Marvell Technology Group Ltd.",
        .methods = &hal_module_methods,
    },
};
