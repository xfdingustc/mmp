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

#ifndef FFMPEG_AUDIO_DECODER_H_
#define FFMPEG_AUDIO_DECODER_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}
#include <BerlinOmxCommon.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>

namespace mmp {

class FFMpegAudioDecoder {
  public:
    FFMpegAudioDecoder();
    ~FFMpegAudioDecoder();

    OMX_ERRORTYPE init(OMX_AUDIO_CODINGTYPE type,
        OMX_U32 bitsPerSample, OMX_U32 channels, OMX_U32 sampleRate,
        OMX_U8 *codec_data, OMX_U32 data_size);

    OMX_ERRORTYPE deinit();

    OMX_ERRORTYPE flush();

    OMX_ERRORTYPE decode(OMX_U8 *in_buf, OMX_U32 in_size, OMX_TICKS in_pts,
        OMX_U8 *out_buf, OMX_U32 *out_size,
        OMX_U32 *in_consume, OMX_TICKS *pts);

  private:
    AVCodec        *mAVCodec;
    AVCodecContext *mAVCodecContext;
    AVFrame        *mAVFrame;
    SwrContext     *mSwrContext;

    int mOutChannels;
    int mOutSampleRate;
    enum AVSampleFormat mOutSampleFormat;
};

}

#endif
