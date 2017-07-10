package com.marvell.mmpautotest;

//  import com.marvell.mmpautotest.MmpView;

public class MmpTestBase
{

  protected static final int SLEEP_TIME = 1000;
  protected static final int SHORT_SLEEP_TIME = 200;

  public int mMax = 0;
  public int mProgress = 0;

  public boolean run() {
    return true;
  }

  protected boolean mResetFlag = false;

  public void reset() {
    MmpView.getView().reset();
    mResetFlag = true;
  }

}
