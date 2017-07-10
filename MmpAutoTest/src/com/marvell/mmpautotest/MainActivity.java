package com.marvell.mmpautotest;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.app.Fragment;
import android.app.FragmentManager;

import android.support.v13.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;

import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.content.Intent;
import android.media.SoundPool;
import android.media.AudioManager;
import android.media.MediaPlayer.TrackInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;

import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.KeyEvent;
import android.view.Gravity;

import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.ProgressBar;
import android.widget.SeekBar;
import android.widget.ScrollView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.util.Log;

import com.marvell.mmpautotest.MmpTestManager;
import com.marvell.mmpautotest.MmpTestManager.OnTestManagerCallbackListener;
import com.marvell.mmpautotest.VideoFragment;
import com.marvell.mmpautotest.SelectorFragment;
import com.marvell.mmpautotest.SelectorFragment.OnStreamSelectedListener;
import com.marvell.mmpautotest.AutoTestFragment;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Stack;
import java.util.TreeMap;
import java.util.HashMap;
import java.util.Queue;

import java.text.SimpleDateFormat;


public class MainActivity extends Activity
    implements OnTestManagerCallbackListener, OnStreamSelectedListener {

  final static String TAG = "MmpAutoTest";

  private TextView log_output;
  private ScrollView mLogScroll;
  private TextView mMonitor;
  private ScrollView mMonitorScroll;

  private LogManager mLogManager = null;

  private SoundPool mErrorSoundPool = null;
  private HashMap<Integer, Integer> mErrorSpMap = null;

  final int MAX_EFFECT_SOUND_NUM = 2;

  private ViewPager mViewPager;
  private MyAdapter mTabsAdapter;
  private LinearLayout mTabHost;


  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    initViews();

    mTabsAdapter = new MyAdapter(getFragmentManager());
    mViewPager = (ViewPager)findViewById(R.id.pager);
    mViewPager.setAdapter(mTabsAdapter);
    mViewPager.setOnPageChangeListener(mTabsAdapter);

    // TODO: add a new page, add a new tab
    mTabsAdapter.addPage(SelectorFragment.class, 0);
    mTabsAdapter.addPage(AutoTestFragment.class, 1);
    mViewPager.setCurrentItem(0);

    // Running log thread
    mLogManager = LogManager.getManager();
    updateHandler.post(mUpdate);
  }


  @Override
  protected void onDestroy() {
    super.onDestroy();
    log_output.setText("");
    System.exit(0);
  }


  // Return true to prevent this event from being propagated further,
  // or false to indicate that you have not handled this event
  // and it should continue to be propagated.
  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    // Catch Back key for video selector
    if (KeyEvent.KEYCODE_BACK == keyCode) {
      SelectorFragment selectorfragment = (SelectorFragment) mTabsAdapter.getFragment(0);
      if (selectorfragment != null) {
        if (selectorfragment.myOnKeyDown(keyCode, event)) {
          //Prevent this event from being propagated further.
          return true;
        }
      }
    }

    VideoFragment videofragment =
        (VideoFragment) getFragmentManager().findFragmentById(R.id.videofragment);
    if (videofragment.myOnKeyDown(keyCode, event)) {
      //Prevent this event from being propagated further.
      return true;
    }

    return super.onKeyDown(keyCode, event);
  }

  @Override
  public void onBackPressed() {
    finish();
  }


  private void initViews() {
    log_output = (TextView) findViewById(R.id.log_output);
    mLogScroll = (ScrollView)findViewById(R.id.log_scroll);

    mMonitor = (TextView) findViewById(R.id.monitor);
    mMonitorScroll = (ScrollView)findViewById(R.id.monitor_scroll);

    mTabHost = (LinearLayout) findViewById(R.id.tabhost);

    LinearLayout.LayoutParams tabHostLayoutParams = new LinearLayout.LayoutParams(
        LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
    tabHostLayoutParams.weight = 1;
    tabHostLayoutParams.gravity = Gravity.CENTER_VERTICAL;

    // TODO: add a new page, add a new tab
    String titles[] = {"Selector", "AutoTest"};
    for (int i = 0; i < titles.length; i++) {
      TextView tab = new TextView(this);
      tab.setText(titles[i]);
      tab.setTextSize(20);
      tab.setLayoutParams(tabHostLayoutParams);
      tab.setGravity(Gravity.CENTER);
      tab.setOnClickListener(mTabClickListener);
      mTabHost.addView(tab);
    }
    TextView textview = (TextView) mTabHost.getChildAt(0);
    textview.setSelected(true);
  }


  private OnClickListener mTabClickListener = new OnClickListener() {
    @Override
    public void onClick(View v) {
      for (int i = 0; i < mTabHost.getChildCount(); i++) {
        TextView textview = (TextView) mTabHost.getChildAt(i);
        if (textview == v) {
          Log.d(TAG, "the " + i + "th tab is selected");
          textview.setSelected(true);
          mViewPager.setCurrentItem(i);
        } else {
          textview.setSelected(false);
        }
      }
    }
  };


  public void OnTestManagerCallback(int kWhat, int extra) {
    switch (kWhat) {
    }
  }

  public void OnStreamSelected(String path) {
    Log.d(TAG, "OnStreamSelected " + path);
    VideoFragment videofragment =
        (VideoFragment) getFragmentManager().findFragmentById(R.id.videofragment);
    videofragment.playVideo(path);
  }

  public void playSounds(int sound, int number){
      AudioManager am = (AudioManager)this.getSystemService(this.AUDIO_SERVICE);
      float audioMaxVolumn = am.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
      float audioCurrentVolumn = am.getStreamVolume(AudioManager.STREAM_MUSIC);
      float volumnRatio = audioCurrentVolumn/audioMaxVolumn;

      mErrorSoundPool.play(mErrorSpMap.get(sound), 1, 1, 0, 0, 1);
  }


  final Handler updateHandler = new Handler();

  final Runnable mUpdate = new Runnable() {
    public void run() {
      String log;

      // update the log window
      if ((log = mLogManager.getLog()) != null) {
        log_output.append(log);
        mLogScroll.scrollTo(0, 65525);
      }

      // update the monitor window
      if ((log = mLogManager.getError()) != null) {
        playSounds(1,1);
        mMonitor.append(log);
        mMonitorScroll.scrollTo(0, 65525);
      }

      updateHandler.postDelayed(mUpdate, 100);
    }
  };


  public final class MyAdapter extends FragmentPagerAdapter
      implements ViewPager.OnPageChangeListener {

    private static final String KEY_TAB_POSITION = "tab_position";

    final class TabInfo {
      private final Class<?> clss;
      private final Bundle args;

      TabInfo(Class<?> _class, int position) {
        clss = _class;
        args = new Bundle();
        args.putInt(KEY_TAB_POSITION, position);
      }

      public int getPosition() {
        return args.getInt(KEY_TAB_POSITION, 0);
      }
    }

    private final ArrayList<TabInfo> mTabs = new ArrayList <TabInfo>();
    private HashMap<Integer, Fragment> mPageReferenceMap = new HashMap<Integer,Fragment>();

    public MyAdapter(FragmentManager fm) {
      super(fm);
    }

    @Override
    public int getCount() {
      return mTabs.size();
    }

    @Override
    public Fragment getItem(int position) {
      Log.d(TAG, "getItem " + position);
      TabInfo info = mTabs.get(position);
      Fragment f = Fragment.instantiate(MainActivity.this, info.clss.getName(), info.args);
      mPageReferenceMap.put(position, f);
      return f;
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
      super.destroyItem(container, position, object);
      mPageReferenceMap.remove(position);
    }

    @Override
    public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
      // Do nothing
    }

    @Override
    public void onPageSelected(int position) {
      Log.d(TAG, "onPageSelected " + position);
      for (int i = 0; i < mTabHost.getChildCount(); i++) {
        TextView textview = (TextView) mTabHost.getChildAt(i);
        if (position == i) {
          Log.d(TAG, "the " + i + "th page is selected");
          textview.setSelected(true);
        } else {
          textview.setSelected(false);
        }
      }
    }

    @Override
    public void onPageScrollStateChanged(int state) {
      // Do nothing
    }

    public void addPage(Class<?> clss, int position) {
      TabInfo info = new TabInfo(clss, position);
      mTabs.add(info);
      Log.d(TAG, "add a new page into ViewPager");
      notifyDataSetChanged();
    }

    public Fragment getFragment(int key) {
      return mPageReferenceMap.get(key);
    }
  }
}
