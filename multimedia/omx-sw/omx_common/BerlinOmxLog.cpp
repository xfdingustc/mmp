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


#include <BerlinOmxLog.h>
#include <stdio.h>
#include <stdarg.h>

#define STRINGIFY(x) case x: return #x

void omx_log_printf(const char* tag, OMX_LOG_LEVEL level, const char * fmt, ...) {
  char var[256];
  va_list var_args;
  va_start (var_args, fmt);
  vsprintf(var, fmt, var_args);
  fprintf(stderr, "%s/%s: %s\n",
          ((level > OMX_LOG_LEVEL_RESERVE) ? "F" : level_string[level]), tag, var);
  // vfprintf(stdout, level, var_args);
  va_end (var_args);
}

OMX_STRING GetOMXBufferFlagDescription(OMX_U32 flag) {
  static char flag_des[1024];
  char flag_tmp[128];
  OMX_U32 omx_prefix_len = strlen("OMX_BUFFERFLAG_");
  OMX_U32 offset = 0;
  memset(flag_des, 0, 1024);

#define PRINT_FLAG(OMX_FLAG) \
    do { \
      if (flag & OMX_FLAG) { \
        sprintf(flag_tmp, "%s", #OMX_FLAG); \
        sprintf(flag_des + offset, "%s ", flag_tmp + omx_prefix_len); \
        offset += strlen(#OMX_FLAG) - omx_prefix_len + 1; \
      } \
    } while(0);

  PRINT_FLAG(OMX_BUFFERFLAG_STARTTIME);
  PRINT_FLAG(OMX_BUFFERFLAG_DECODEONLY);
  PRINT_FLAG(OMX_BUFFERFLAG_DATACORRUPT);
  PRINT_FLAG(OMX_BUFFERFLAG_ENDOFFRAME);
  PRINT_FLAG(OMX_BUFFERFLAG_EXTRADATA);
  PRINT_FLAG(OMX_BUFFERFLAG_CODECCONFIG);
  PRINT_FLAG(OMX_BUFFERFLAG_SYNCFRAME);
  PRINT_FLAG(OMX_BUFFERFLAG_EOS);
  return flag_des;
}

OMX_STRING OmxState2String(OMX_STATETYPE state) {
  switch (state) {
    STRINGIFY(OMX_StateLoaded);
    STRINGIFY(OMX_StateIdle);
    STRINGIFY(OMX_StateExecuting);
    STRINGIFY(OMX_StatePause);
    STRINGIFY(OMX_StateWaitForResources);
    STRINGIFY(OMX_StateKhronosExtensions);
    STRINGIFY(OMX_StateVendorStartUnused);
    STRINGIFY(OMX_StateMax);
    default: return "state - unknown";
  }
}

OMX_STRING OmxIndex2String(OMX_INDEXTYPE index) {
  switch(index) {
    STRINGIFY(OMX_IndexComponentStartUnused);
    STRINGIFY(OMX_IndexParamPriorityMgmt);
    STRINGIFY(OMX_IndexParamAudioInit);
    STRINGIFY(OMX_IndexParamImageInit);
    STRINGIFY(OMX_IndexParamVideoInit);
    STRINGIFY(OMX_IndexParamOtherInit);
    STRINGIFY(OMX_IndexParamNumAvailableStreams);
    STRINGIFY(OMX_IndexParamActiveStream);
    STRINGIFY(OMX_IndexParamSuspensionPolicy);
    STRINGIFY(OMX_IndexParamComponentSuspended);
    STRINGIFY(OMX_IndexConfigCapturing);
    STRINGIFY(OMX_IndexConfigCaptureMode);
    STRINGIFY(OMX_IndexAutoPauseAfterCapture);
    STRINGIFY(OMX_IndexParamDisableResourceConcealment);
    STRINGIFY(OMX_IndexConfigMetadataItemCount);
    STRINGIFY(OMX_IndexConfigContainerNodeCount);
    STRINGIFY(OMX_IndexConfigMetadataItem);
    STRINGIFY(OMX_IndexConfigCounterNodeID);
    STRINGIFY(OMX_IndexParamMetadataFilterType);
    STRINGIFY(OMX_IndexParamMetadataKeyFilter);
    STRINGIFY(OMX_IndexConfigPriorityMgmt);
    STRINGIFY(OMX_IndexParamStandardComponentRole);

    STRINGIFY(OMX_IndexPortStartUnused);
    STRINGIFY(OMX_IndexParamPortDefinition);
    STRINGIFY(OMX_IndexParamCompBufferSupplier);
    STRINGIFY(OMX_IndexReservedStartUnused);

    STRINGIFY(OMX_IndexAudioStartUnused);
    STRINGIFY(OMX_IndexParamAudioPortFormat);
    STRINGIFY(OMX_IndexParamAudioPcm);
    STRINGIFY(OMX_IndexParamAudioAac);
    STRINGIFY(OMX_IndexParamAudioRa);
    STRINGIFY(OMX_IndexParamAudioMp3);
    STRINGIFY(OMX_IndexParamAudioAdpcm);
    STRINGIFY(OMX_IndexParamAudioG723);
    STRINGIFY(OMX_IndexParamAudioG729);
    STRINGIFY(OMX_IndexParamAudioAmr);
    STRINGIFY(OMX_IndexParamAudioWma);
    STRINGIFY(OMX_IndexParamAudioSbc);
    STRINGIFY(OMX_IndexParamAudioMidi);
    STRINGIFY(OMX_IndexParamAudioGsm_FR);
    STRINGIFY(OMX_IndexParamAudioMidiLoadUserSound);
    STRINGIFY(OMX_IndexParamAudioG726);
    STRINGIFY(OMX_IndexParamAudioGsm_EFR);
    STRINGIFY(OMX_IndexParamAudioGsm_HR);
    STRINGIFY(OMX_IndexParamAudioPdc_FR);
    STRINGIFY(OMX_IndexParamAudioPdc_EFR);
    STRINGIFY(OMX_IndexParamAudioPdc_HR);
    STRINGIFY(OMX_IndexParamAudioTdma_FR);
    STRINGIFY(OMX_IndexParamAudioTdma_EFR);
    STRINGIFY(OMX_IndexParamAudioQcelp8);
    STRINGIFY(OMX_IndexParamAudioQcelp13);
    STRINGIFY(OMX_IndexParamAudioEvrc);
    STRINGIFY(OMX_IndexParamAudioSmv);
    STRINGIFY(OMX_IndexParamAudioVorbis);

    STRINGIFY(OMX_IndexConfigAudioMidiImmediateEvent);
    STRINGIFY(OMX_IndexConfigAudioMidiControl);
    STRINGIFY(OMX_IndexConfigAudioMidiSoundBankProgram);
    STRINGIFY(OMX_IndexConfigAudioMidiStatus);
    STRINGIFY(OMX_IndexConfigAudioMidiMetaEvent);
    STRINGIFY(OMX_IndexConfigAudioMidiMetaEventData);
    STRINGIFY(OMX_IndexConfigAudioVolume);
    STRINGIFY(OMX_IndexConfigAudioBalance);
    STRINGIFY(OMX_IndexConfigAudioChannelMute);
    STRINGIFY(OMX_IndexConfigAudioMute);
    STRINGIFY(OMX_IndexConfigAudioLoudness);
    STRINGIFY(OMX_IndexConfigAudioEchoCancelation);
    STRINGIFY(OMX_IndexConfigAudioNoiseReduction);
    STRINGIFY(OMX_IndexConfigAudioBass);
    STRINGIFY(OMX_IndexConfigAudioTreble);
    STRINGIFY(OMX_IndexConfigAudioStereoWidening);
    STRINGIFY(OMX_IndexConfigAudioChorus);
    STRINGIFY(OMX_IndexConfigAudioEqualizer);
    STRINGIFY(OMX_IndexConfigAudioReverberation);
    STRINGIFY(OMX_IndexConfigAudioChannelVolume);

    STRINGIFY(OMX_IndexImageStartUnused);
    STRINGIFY(OMX_IndexParamImagePortFormat);
    STRINGIFY(OMX_IndexParamFlashControl);
    STRINGIFY(OMX_IndexConfigFocusControl);
    STRINGIFY(OMX_IndexParamQFactor);
    STRINGIFY(OMX_IndexParamQuantizationTable);
    STRINGIFY(OMX_IndexParamHuffmanTable);
    STRINGIFY(OMX_IndexConfigFlashControl);

    STRINGIFY(OMX_IndexVideoStartUnused);
    STRINGIFY(OMX_IndexParamVideoPortFormat);
    STRINGIFY(OMX_IndexParamVideoQuantization);
    STRINGIFY(OMX_IndexParamVideoFastUpdate);
    STRINGIFY(OMX_IndexParamVideoBitrate);
    STRINGIFY(OMX_IndexParamVideoMotionVector);
    STRINGIFY(OMX_IndexParamVideoIntraRefresh);
    STRINGIFY(OMX_IndexParamVideoErrorCorrection);
    STRINGIFY(OMX_IndexParamVideoVBSMC);
    STRINGIFY(OMX_IndexParamVideoMpeg2);
    STRINGIFY(OMX_IndexParamVideoMpeg4);
    STRINGIFY(OMX_IndexParamVideoWmv);
    STRINGIFY(OMX_IndexParamVideoRv);
    STRINGIFY(OMX_IndexParamVideoAvc);
    STRINGIFY(OMX_IndexParamVideoH263);
    STRINGIFY(OMX_IndexParamVideoProfileLevelQuerySupported);
    STRINGIFY(OMX_IndexParamVideoProfileLevelCurrent);
    STRINGIFY(OMX_IndexConfigVideoBitrate);
    STRINGIFY(OMX_IndexConfigVideoFramerate);
    STRINGIFY(OMX_IndexConfigVideoIntraVOPRefresh);
    STRINGIFY(OMX_IndexConfigVideoIntraMBRefresh);
    STRINGIFY(OMX_IndexConfigVideoMBErrorReporting);
    STRINGIFY(OMX_IndexParamVideoMacroblocksPerFrame);
    STRINGIFY(OMX_IndexConfigVideoMacroBlockErrorMap);
    STRINGIFY(OMX_IndexParamVideoSliceFMO);
    STRINGIFY(OMX_IndexConfigVideoAVCIntraPeriod);
    STRINGIFY(OMX_IndexConfigVideoNalSize);

    STRINGIFY(OMX_IndexCommonStartUnused);
    STRINGIFY(OMX_IndexParamCommonDeblocking);
    STRINGIFY(OMX_IndexParamCommonSensorMode);
    STRINGIFY(OMX_IndexParamCommonInterleave);
    STRINGIFY(OMX_IndexConfigCommonColorFormatConversion);
    STRINGIFY(OMX_IndexConfigCommonScale);
    STRINGIFY(OMX_IndexConfigCommonImageFilter);
    STRINGIFY(OMX_IndexConfigCommonColorEnhancement);
    STRINGIFY(OMX_IndexConfigCommonColorKey);
    STRINGIFY(OMX_IndexConfigCommonColorBlend);
    STRINGIFY(OMX_IndexConfigCommonFrameStabilisation);
    STRINGIFY(OMX_IndexConfigCommonRotate);
    STRINGIFY(OMX_IndexConfigCommonMirror);
    STRINGIFY(OMX_IndexConfigCommonOutputPosition);
    STRINGIFY(OMX_IndexConfigCommonInputCrop);
    STRINGIFY(OMX_IndexConfigCommonOutputCrop);
    STRINGIFY(OMX_IndexConfigCommonDigitalZoom);
    STRINGIFY(OMX_IndexConfigCommonOpticalZoom);
    STRINGIFY(OMX_IndexConfigCommonWhiteBalance);
    STRINGIFY(OMX_IndexConfigCommonExposure);
    STRINGIFY(OMX_IndexConfigCommonContrast);
    STRINGIFY(OMX_IndexConfigCommonBrightness);
    STRINGIFY(OMX_IndexConfigCommonBacklight);
    STRINGIFY(OMX_IndexConfigCommonGamma);
    STRINGIFY(OMX_IndexConfigCommonSaturation);
    STRINGIFY(OMX_IndexConfigCommonLightness);
    STRINGIFY(OMX_IndexConfigCommonExclusionRect);
    STRINGIFY(OMX_IndexConfigCommonDithering);
    STRINGIFY(OMX_IndexConfigCommonPlaneBlend);
    STRINGIFY(OMX_IndexConfigCommonExposureValue);
    STRINGIFY(OMX_IndexConfigCommonOutputSize);
    STRINGIFY(OMX_IndexParamCommonExtraQuantData);
    STRINGIFY(OMX_IndexConfigCommonTransitionEffect);

    STRINGIFY(OMX_IndexOtherStartUnused);
    STRINGIFY(OMX_IndexParamOtherPortFormat);
    STRINGIFY(OMX_IndexConfigOtherPower);
    STRINGIFY(OMX_IndexConfigOtherStats);

    STRINGIFY(OMX_IndexTimeStartUnused);
    STRINGIFY(OMX_IndexConfigTimeScale);
    STRINGIFY(OMX_IndexConfigTimeClockState);
    STRINGIFY(OMX_IndexConfigTimeCurrentMediaTime);
    STRINGIFY(OMX_IndexConfigTimeCurrentWallTime);
    STRINGIFY(OMX_IndexConfigTimeMediaTimeRequest);
    STRINGIFY(OMX_IndexConfigTimeClientStartTime);
    STRINGIFY(OMX_IndexConfigTimePosition);
    STRINGIFY(OMX_IndexConfigTimeSeekMode);

    STRINGIFY(OMX_IndexKhronosExtensions);
    STRINGIFY(OMX_IndexVendorStartUnused);

    default: return "STATE_UNKOWN";
  }
}

OMX_STRING OmxDir2String(OMX_DIRTYPE dir) {
  switch(dir) {
    STRINGIFY(OMX_DirInput);
    STRINGIFY(OMX_DirOutput);
    STRINGIFY(OMX_DirMax);
    default: return "DIR_UNKOWN";
  }
}
OMX_STRING OmxClockState2String(OMX_TIME_CLOCKSTATE state) {
  switch(state) {
    STRINGIFY(OMX_TIME_ClockStateRunning);
    STRINGIFY(OMX_TIME_ClockStateWaitingForStartTime);
    STRINGIFY(OMX_TIME_ClockStateStopped);
    STRINGIFY(OMX_TIME_ClockStateKhronosExtensions);
    STRINGIFY(OMX_TIME_ClockStateVendorStartUnused);
    STRINGIFY(OMX_TIME_ClockStateMax);
    default: return "STATE_UNKOWN";
  }
}
