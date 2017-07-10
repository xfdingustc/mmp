package com.marvell.mmpautotest;

import java.util.Queue;
import java.util.LinkedList;
import java.text.SimpleDateFormat;


public class LogManager
{
  public static LogManager getManager() {
    if (sManager == null) {
      sManager = new LogManager();
      sManager.init();
    }
    return sManager;
  }
  private long mAppStartTime = 0;

  private static LogManager sManager = null;

  private void init() {
    mAppStartTime = android.os.SystemClock.uptimeMillis();
  }

  private Queue<String> mLogQueue = new LinkedList<String>();
  private Queue<String> mErrorQueue = new LinkedList<String>();

  public String getLog() {
    return mLogQueue.poll();
  }

  public String getError() {
    return mErrorQueue.poll();
  }

  public void outputLog(String log) {
    SimpleDateFormat sDateFormat = new SimpleDateFormat("HH:mm:ss");
    long current_time = android.os.SystemClock.uptimeMillis();
    current_time -= mAppStartTime;
    String date = sDateFormat.format(current_time);
    String formated_log = "[" + date + "]\t" + log + "\n";
    mLogQueue.offer(formated_log);
  }

  public void outputError(String error) {
    SimpleDateFormat sDateFormat = new SimpleDateFormat("HH:mm:ss");
    long current_time = android.os.SystemClock.uptimeMillis();
    current_time -= mAppStartTime;
    String date = sDateFormat.format(current_time);
    String formated_log = "[" + date + "]\t" + error + "\n";
    mErrorQueue.offer(formated_log);
  }

}
