package com.marvell.mmpautotest;

import android.os.SystemProperties;

import java.io.File;

import com.marvell.mmpautotest.MmpView;





public class BatTest extends MmpTestBase
{

  private String mBatPath;

  public BatTest() {
    mBatPath = prepareBatPath(EnvironmentVarManager.usbMountPoint);
  }

  private String prepareBatPath(String usb_mount){
    String batStreamsPath = null;

    File file = new File(usb_mount + "/bat");
    if (file.exists()) {
      batStreamsPath = usb_mount + "/bat";
    }

    return batStreamsPath;

  }


  @Override
  public boolean run() {
    super.run();
    File file = new File(mBatPath);
    File[] bat_file_list = file.listFiles();
    mResetFlag = false;

    mMax = bat_file_list.length;

    for (int i = 0; i < bat_file_list.length; i++) {
      String file_name = mBatPath + "/" + bat_file_list[i].getName();
      LogManager.getManager().outputLog("Start Bat " + i + "" + file_name);
      try {
        boolean ret = runOneTest(file_name);

        mProgress = i + 1;
        if (false == ret) {
          LogManager.getManager().outputLog("Failed to pass bat stream name: " + file_name);
          break;
        } else {
          LogManager.getManager().outputLog("Stream name: " + file_name + " passed");
        }

        if (mResetFlag) {
          LogManager.getManager().outputLog("User canceled BAT");
          return false;
        }
      } catch (Exception e) {
        e.printStackTrace();
        return false;
      }
    }
    LogManager.getManager().outputLog("Congratulations! Bat test has passed.");
    return true;
  }


  private static final int SLEEP_TIME = 1000;
  private static final int SHORT_SLEEP_TIME = 200;

  private boolean runOneTest(String file) throws Exception {
    MmpView.getView().playVideo(file);

    // Seek to the half postion
    MmpView.getView().seek(MmpView.getView().getDuration() / 4);
    Thread.sleep(SLEEP_TIME);


    MmpView.getView().seek(MmpView.getView().getDuration() / 2);
    Thread.sleep(SLEEP_TIME);

    MmpView.getView().seek((MmpView.getView().getDuration() * 3) / 4);
    Thread.sleep(SLEEP_TIME);

    MmpView.getView().reset();

    return true;
  }

}


