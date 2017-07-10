package com.marvell.mmpautotest;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import java.util.Queue;
import java.util.LinkedList;

//  import com.marvell.mmpautotest.MmpView;

public class SmokeTest extends MmpTestBase
{


  private Queue<String> mSmokePlaylist = new LinkedList<String>();

  final String[] supportedSuffix = {".mkv", ".avi", ".divx",".mp3", ".flac", ".wmv", ".wma", ".asf",
      ".mp4", ".3gp", ".3g2", ".mov", ".mpg", ".ts", ".m2ts", ".swf", ".flv", "vob"};

  public SmokeTest() {
    prepareSmokePlaylist(EnvironmentVarManager.usbMountPoint);
  }

  private boolean isSupportStream(String file_name) {

    for (int i = 0; i < supportedSuffix.length; i++) {
      if (file_name.endsWith(supportedSuffix[i])) {
        return true;
      }
    }

    return false;
  }

  private void prepareSmokePlaylist(String root) {
    try {
      File file = new File(root);
      if (!file.isDirectory()) {
        String full_name = file.getAbsolutePath();
        if (isSupportStream(full_name)) {
          mSmokePlaylist.offer(file.getAbsolutePath());
        }
      } else if (file.isDirectory()) {
        String[] filelist = file.list();
         for (int i = 0; i < filelist.length; i++) {
          File readfile = new File(root + "/" + filelist[i]);
          if (!readfile.isDirectory()) {
            String full_name = readfile.getAbsolutePath();
            if (isSupportStream(full_name)) {
              mSmokePlaylist.offer(readfile.getAbsolutePath());
            }
          } else if (readfile.isDirectory()) {
            prepareSmokePlaylist(root + "/" + filelist[i]);
          }
        }
      }
    } catch (Exception e) {
      //LogManager.getManager().outputLog("prepareSmokePlaylist Exception: " + e.getMessage());
    }
  }

  @Override
  public boolean run() {
    String file_name;
    mMax = mSmokePlaylist.size();
    mProgress = 0;
    while ((file_name = mSmokePlaylist.poll()) != null) {
      LogManager.getManager().outputLog(file_name);

      try {
        boolean ret = runOneTest(file_name);
        mProgress++;
        if (false == ret) {
          LogManager.getManager().outputError("Failed to pass smoke stream name: " + file_name);
          return false;
        } else {
          LogManager.getManager().outputLog("Stream name: " + file_name + " passed");
        }

        if (mResetFlag) {
          LogManager.getManager().outputLog("User canceled smoke");
          return false;
        }
      } catch (Exception e) {
        e.printStackTrace();
        return false;
      }
    }
    return true;

  }

  private boolean runOneTest(String file) throws Exception {
    MmpView.getView().playVideo(file);

    long start_time = MmpView.getView().getCurrentPosition();

    while (MmpView.getView().isPlaying()) {
      Thread.sleep(SHORT_SLEEP_TIME);
      int current_time = MmpView.getView().getCurrentPosition();
      if ((current_time < start_time) ||
          (Math.abs(current_time - start_time - SHORT_SLEEP_TIME) > 50))  {
        MmpView.getView().reset();
        return false;
      } else {
        start_time = current_time;
      }
    }

    MmpView.getView().reset();

    return true;
  }

}

