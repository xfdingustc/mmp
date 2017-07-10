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
extern "C" {
#include "stdio.h"
}
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include "MediaPlayerOnlineDebug.h"
namespace mmp {
static sp<IOnlineDebug> online_debug;

static inline const char *OnOrOff(uint64_t bit) {
  if (bit == 0) {
    return "Off";
  } else {
    return "On";
  }
}

static void PrintMainMenu() {
  printf("\nPlease select sub menu:\n");
  printf("1:\tCore Player\n");
  printf("2:\tData Reader\n");
  printf("3:\tMHAL Video Engine\n");
  printf("4:\tMHAL Audio Engine\n");
  printf("5:\tMHAL Subtitle Engine\n");
  printf("6:\tData Source\n");
  printf("0:\tExit\n");
}

static void PrintCorePlayerMenu() {
  printf("\nPlease set core player debug level:\n");
  uint64_t bit_mask = online_debug->GetBitMask();
  printf("3:\tPrint ES data log %s\n",      OnOrOff(bit_mask&MASK_CP_ES_DATA));
  //printf("6:\tDump HLS source data %s\n",   OnOrOff(bit_mask&MASK_CP_DUMP_HLS));
  printf("7:\tDump Audio ES Data %s\n",     OnOrOff(bit_mask&MASK_CP_DUMP_AUDIO));
  printf("8:\tDump Video ES Data %s\n",     OnOrOff(bit_mask&MASK_CP_DUMP_VIDEO));
  printf("9:\tDump Subtitle Data %s\n", "Off");
  printf("0:\tBack\n");
}

static void PrintDataReaderMenu() {
  printf("\nData Reader Menu\n");
  uint64_t bit_mask = online_debug->GetBitMask();
  printf("1:\tAPI            %s\n",         OnOrOff(bit_mask&MASK_DR_API));
  printf("2:\tRuntime        %s\n",         OnOrOff(bit_mask&MASK_DR_RUNTIME));
  printf("3:\tVideo          %s\n",         OnOrOff(bit_mask&MASK_DR_VIDEO));
  printf("4:\tAudio          %s\n",         OnOrOff(bit_mask&MASK_DR_AUDIO));
  printf("5:\tSub            %s\n",         OnOrOff(bit_mask&MASK_DR_SUB));
  printf("0:\tBack\n");
}

static void PrintMHALVideoEngineMenu() {
  printf("\nMHAL Video Engine Menu\n");
  printf("0:\tBack\n");
}

static void PrintMHALAudioEngineMenu() {
  printf("\nMHAL Audio Engine Menu\n");
  printf("0:\tBack\n");
}

static void PrintMHALSubtitleEngineMenu() {
  printf("\nMHAL Subtitle Engine Menu\n");
  printf("0:\tBack\n");
}

static int32_t CorePlayerMenuEntrance() {
  int32_t input = -1;
  uint64_t bit_mask = online_debug->GetBitMask();

  scanf("%d", &input);
  switch (input) {
    case 1:
      break;
    case 2:
      break;
    case 3:
      online_debug->SetBitMask(bit_mask^MASK_CP_ES_DATA);
      printf("Print ES data log %s\n", OnOrOff(bit_mask^MASK_CP_ES_DATA));
      break;
    case 6:
      //online_debug->SetBitMask(bit_mask^MASK_CP_DUMP_HLS);
      //printf("Dump HLS data %s\n", OnOrOff(bit_mask^MASK_CP_DUMP_HLS));
      break;
    case 7:
      online_debug->SetBitMask(bit_mask^MASK_CP_DUMP_AUDIO);
      printf("Dump Video ES Data %s\n", OnOrOff(bit_mask^MASK_CP_DUMP_AUDIO));
      break;
    case 8:
      online_debug->SetBitMask(bit_mask^MASK_CP_DUMP_VIDEO);
      printf("Dump Video ES Data %s\n", OnOrOff(bit_mask^MASK_CP_DUMP_VIDEO));
      break;
    case 9:
      printf("Dump subtitle data is selected");
      break;
    case 0:
      return -1;
  }
  return 0;
}

static int32_t DataReaderMenuEntrance() {
  int32_t input = -1;
  scanf("%d", &input);
  uint64_t bit_mask = online_debug->GetBitMask();
  switch (input) {
    case 1:
      online_debug->SetBitMask(bit_mask^MASK_DR_API);
      break;
    case 2:
      online_debug->SetBitMask(bit_mask^MASK_DR_RUNTIME);
      break;
    case 3:
      online_debug->SetBitMask(bit_mask^MASK_DR_VIDEO);
      break;
    case 4:
      online_debug->SetBitMask(bit_mask^MASK_DR_AUDIO);
      break;
    case 5:
      online_debug->SetBitMask(bit_mask^MASK_DR_SUB);
      break;
    case 0:
      return -1;
  }
  return 0;
}

static int32_t MHALVideoEngineEntrance() {
  int32_t input = -1;
  scanf("%d", &input);
  switch (input) {
    case 0:
      return -1;
  }
  return 0;
}
static int32_t MHALAudioEngineEntrance() {
  int32_t input = -1;
  scanf("%d", &input);
  switch (input) {
    case 0:
      return -1;
  }
  return 0;
}

static int32_t MHALSubtitleEngineEntrance() {
  int32_t input = -1;
  scanf("%d", &input);
  switch (input) {
    case 0:
      return -1;
  }
  return 0;
}

static void PrintDataSourceMenu() {
  printf("\nPlease set data source debug level:\n");
  uint64_t bit_mask = online_debug->GetBitMask();
  printf("2:\tDump HLS Data %s\n", OnOrOff(bit_mask & MASK_DS_DUMP_HLS));
  printf("0:\tBack\n");
}

static int32_t DataSourceEntrance() {
  int32_t input = -1;
  uint64_t bit_mask = online_debug->GetBitMask();

  scanf("%d", &input);
  switch (input) {
    case 1:
      break;
    case 2:
      online_debug->SetBitMask(bit_mask^MASK_DS_DUMP_HLS);
      printf("Dump HLS Data %s\n", OnOrOff(bit_mask ^ MASK_DS_DUMP_HLS));
      break;
    case 0:
      return -1;
  }
  return 0;
}

static int32_t MainMenuEntrance() {
  int32_t input = -1;
  scanf("%d", &input);
  switch (input) {
    case 1:
      do {
        PrintCorePlayerMenu();
      } while(CorePlayerMenuEntrance() != -1);
      break;
    case 2:
      do {
        PrintDataReaderMenu();
      } while(DataReaderMenuEntrance() != -1);
      break;
    case 3:
      do {
        PrintMHALVideoEngineMenu();
      } while(MHALVideoEngineEntrance() != -1);
      break;
    case 4:
      do {
        PrintMHALAudioEngineMenu();
      } while(MHALAudioEngineEntrance() != -1);
      break;
    case 5:
      do {
        PrintMHALSubtitleEngineMenu();
      } while(MHALSubtitleEngineEntrance() != -1);
      break;
    case 6:
      do {
        PrintDataSourceMenu();
      } while(DataSourceEntrance() != -1);
      break;
    case 0:
      return -1;
  }
  return 0;
}
}


int main(void) {

#if 0
  if (android::OK != android::pollForService(android::IOnlineDebug::kServiceName,
      &android::online_debug)) {
    exit(0);
  }
  printf("\n----------------Welcome to Marvell Media Player Online Debug System----------------\n");
  do {
    android::PrintMainMenu();
  } while(android::MainMenuEntrance() != -1);
#endif

}
