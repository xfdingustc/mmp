#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <termios.h>


#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "com_type.h"
#include "OSAL_api.h"
#include "pe_type.h"
#include "pe_api.h"
#include "const.h"
#include "MRVLMediaPlayer.h"
#include "pe_debug.h"



using namespace android;
MRVLMediaPlayer* mrvlMediaPlayerADV = NULL;
MRVLMediaPlayer* mrvlMediaPlayerMOV = NULL;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: %s file path fd mark(0:filesrc,1:fdsrc)\n", argv[0]);
    return -1;
  }

  printf("input first URL is %s, the second URL is %s\n", argv[1], argv[2]);

  mrvlMediaPlayerADV = new MRVLMediaPlayer();
  mrvlMediaPlayerADV->setDataSource(argv[1],NULL);
  mrvlMediaPlayerADV->prepare();
  mrvlMediaPlayerADV->start();
  mrvlMediaPlayerMOV = new MRVLMediaPlayer();
  mrvlMediaPlayerMOV->setDataSource(argv[2],NULL);
  mrvlMediaPlayerMOV->prepare();

  int i = 0;
  while(i < 15) {
    printf("The first stream play %d seocnds!!!\n", i );
    sleep(1);
    i++;
  }
  mrvlMediaPlayerADV->reset();
  delete mrvlMediaPlayerADV;

  mrvlMediaPlayerMOV->start();
  i = 0;
  while(i < 20) {
    printf("The second play %d seocnds!!!\n", i );
    sleep(1);
    i++;
  }
  mrvlMediaPlayerMOV->reset();
  delete mrvlMediaPlayerMOV;

}


