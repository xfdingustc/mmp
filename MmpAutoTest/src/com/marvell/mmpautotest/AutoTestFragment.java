package com.marvell.mmpautotest;

import android.app.Activity;
import android.app.Fragment;

import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;

import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import android.media.AudioManager;
import android.media.MediaPlayer.TrackInfo;

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

import com.marvell.mmpautotest.MmpView;
import com.marvell.mmpautotest.MmpView.OnMmpViewCallbackListener;

public class AutoTestFragment extends Fragment {
  final static String TAG = "AutoTestFragment";

  private RelativeLayout mTestControlPanel = null;
  private ProgressBar mTestProgressBar = null;

  private ImageButton mBatBtn;
  private ImageButton mSmokeBtn;

  private LogManager mLogManager = null;

  public AutoTestFragment() {
    Log.d(TAG, "constructor");
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle icicle) {
    Log.d(TAG, "onCreateView");
    // Inflate the layout for this fragment
    View v = inflater.inflate(R.layout.test_layout, container, false);

    mTestControlPanel = (RelativeLayout) v.findViewById(R.id.test_control_panel);
    mTestControlPanel.setVisibility(View.INVISIBLE);

    mTestProgressBar = (ProgressBar) v.findViewById(R.id.test_progress_bar);

    mBatBtn = (ImageButton) v.findViewById(R.id.bat_button);
    mSmokeBtn = (ImageButton) v.findViewById(R.id.smoke_button);

    mLogManager = LogManager.getManager();

    // TODO: implement auto test
    Log.d(TAG, "finish create AutoTestFragment");
    return v;
  }


  private OnClickListener mOnClickListener = new OnClickListener() {
    @Override
    public void onClick(View btn) {
      switch (btn.getId()) {
        case R.id.bat_button:
        mTestControlPanel.setVisibility(View.VISIBLE);
        MmpTestManager.getManager().start("BAT");
        break;
      case R.id.smoke_button:
        mTestControlPanel.setVisibility(View.VISIBLE);
        mLogManager.outputLog("Start smoke test");
        MmpTestManager.getManager().start("Smoke");
        break;
        default:
          break;
      }
    }
  };

}
