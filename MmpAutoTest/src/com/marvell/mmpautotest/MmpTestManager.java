package com.marvell.mmpautotest;


public class MmpTestManager
{
  public static MmpTestManager getManager(){
    if (sManager == null) {
      sManager = new MmpTestManager();
      sManager.init();
    }

    return sManager;
  }

  private static MmpTestManager sManager = null;


  protected void finalize(){
    reset();
  }

  private void init() {
    mTestThread = new TestThread();

  }

  public void start(String name) {
    LogManager.getManager().outputLog("Start test " + name);
    mTestName = name;
    reset();
    mTestThread = new TestThread();
    mTestThread.start();
  }

  public void stop() {
  }

  public void reset() {
    if (mTestThread.isRunning) {
      mTestThread.reset();
      try {
        mTestThread.join();
        mTestThread = null;
      } catch (Exception e) {
        return;
      }
    }
  }

  TestThread mTestThread;
  private MmpTestBase mTest = null;
  private String mTestName = null;

  private class TestThread extends Thread {
    public boolean isRunning = false;

    public void run() {
      isRunning = true;
      if (mTestName == "BAT") {
        mTest = new BatTest();
        boolean ret = mTest.run();
        if (ret == false) {
          LogManager.getManager().outputLog("BAT failed");
        }
      } else if (mTestName == "Smoke") {
        mTest = new SmokeTest();
        boolean ret = mTest.run();
        if (ret == false) {
          LogManager.getManager().outputLog("Smoke test failed");
        }
      }
    }
    public void reset() {
      LogManager.getManager().outputLog("Test is reset");
      mTest.reset();
      isRunning = false;
    }

  }

  public static final int TEST_MANAGER_CALLBACK_KWHAT_UPDATE_MAX = 1;
  public static final int TEST_MANAGER_CALLBACK_KWHAT_UPDATE_PROGRESS = 2;

  public int getMax() {
    if (mTest != null) {
      return mTest.mMax;
    }
    return 0;
  }

  public int getProgress() {
    if (mTest != null) {
      return mTest.mProgress;
    }
    return 0;
  }

  public void setMax(int max) {
    mCallbackListener.OnTestManagerCallback(TEST_MANAGER_CALLBACK_KWHAT_UPDATE_MAX, max);
  }

  public void updateProgress(int progress) {
    mCallbackListener.OnTestManagerCallback(TEST_MANAGER_CALLBACK_KWHAT_UPDATE_PROGRESS, progress);
  }

  public interface OnTestManagerCallbackListener {
    void OnTestManagerCallback(int kWhat, int extra);
  }

  public void setTestManagerCallbackListener(OnTestManagerCallbackListener callback) {
    mCallbackListener = callback;
  }

  private OnTestManagerCallbackListener mCallbackListener;



}
