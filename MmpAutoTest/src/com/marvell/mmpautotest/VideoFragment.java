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

public class VideoFragment extends Fragment {
  final static String TAG = "VideoFragment";

  private ImageButton play_pause_btn;
  private ImageButton stop_btn;

  private SeekBar player_seek_bar;
  private SeekBar volume_bar;
  private TextView media_time_view;
  private TextView media_duration_view;

  private SurfaceView mPreview;

  private TextView audios;
  private ListView audio_listview;
  private TextView subtitles;
  private ListView subtitle_listview;

  private MmpView mMmpView = null;

  public VideoFragment() {
    Log.d(TAG, "constructor");
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle icicle) {
    Log.d(TAG, "onCreateView");
    // Inflate the layout for this fragment
    View v = inflater.inflate(R.layout.video_fragment, container, false);

    initUI(v);
    setElementsListener();

    mMmpView = MmpView.getView();
    mMmpView.setContext(getActivity());
    mMmpView.setSurfaceView(mPreview);
    mMmpView.setMmpViewCallbackListener(mOnMmpViewCallbackListener);

    Log.d(TAG, "finish create VideoFragment");
    return v;
  }


  private void initUI(View v) {
    mPreview = (SurfaceView) v.findViewById(R.id.video_output_surface);

    audios = (TextView) v.findViewById(R.id.audios);
    audio_listview = (ListView) v.findViewById(R.id.audio_listview);
    subtitles = (TextView) v.findViewById(R.id.subtitles);
    subtitle_listview = (ListView) v.findViewById(R.id.subtitle_listview);

    // Control Panel
    play_pause_btn = (ImageButton) v.findViewById(R.id.play_pause_button);
    stop_btn = (ImageButton) v.findViewById(R.id.stop_button);
    volume_bar = (SeekBar) v.findViewById(R.id.volume_bar);
    player_seek_bar = (SeekBar) v.findViewById(R.id.media_seek_bar);
    media_time_view = (TextView) v.findViewById(R.id.media_time);
    media_duration_view = (TextView) v.findViewById(R.id.media_duration);

    int maxVolume = ((AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE)).
        getStreamMaxVolume(AudioManager.STREAM_MUSIC);
    Log.d(TAG, "maxVolume " + maxVolume);
    volume_bar.setMax(maxVolume);
    int currentVolume = ((AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE)).
        getStreamVolume(AudioManager.STREAM_MUSIC);
    Log.d(TAG, "currentVolume " + currentVolume);
    volume_bar.setProgress(currentVolume);
  }


  private OnMmpViewCallbackListener mOnMmpViewCallbackListener = new OnMmpViewCallbackListener() {
    @Override
    public void OnMmpViewCallback(int kWhat, int extra) {
      switch (kWhat) {
        case MmpView.MMP_VIEW_CALLBACK_KWHAT_UPDATE_DURATION:
          SimpleDateFormat sDateFormat = new SimpleDateFormat("HH:mm:ss");
          String duration = sDateFormat.format(extra);
          player_seek_bar.setMax(extra);
          media_duration_view.setText(duration);
          break;
        case MmpView.MMP_VIEW_CALLBACK_KWHAT_UPDATE_TIME:
          player_seek_bar.setProgress(extra);
          sDateFormat = new SimpleDateFormat("HH:mm:ss");
          String current_time = sDateFormat.format(extra);
          media_time_view.setText(current_time);
          break;
        case MmpView.MMP_VIEW_CALLBACK_KWHAT_PLAYBACK_COMPLETE:
          resetControlPanel();
          break;
      }
    }
  };


  private OnClickListener mOnClickListener = new OnClickListener() {
    @Override
    public void onClick(View btn) {
      switch (btn.getId()) {
        case R.id.play_pause_button:
          if (mMmpView.isPlaying()) {
            mMmpView.pause();
            play_pause_btn.setImageDrawable(getResources().getDrawable(R.drawable.start_button));
          } else {
            mMmpView.start();
            play_pause_btn.setImageDrawable(getResources().getDrawable(R.drawable.pause_button));
          }
          break;
        case R.id.stop_button:
          mMmpView.stop();
          resetControlPanel();
          break;
        case R.id.audios:
          Log.d(TAG, "to create audio list");
          createAudioSubstreamSelectionList();
          break;
        case R.id.subtitles:
          Log.d(TAG, "to create subtitle list");
          createSubtitleSubstreamSelectionList();
          break;
        default:
          break;
      }
    }
  };


  private void setElementsListener() {
    play_pause_btn.setOnClickListener(mOnClickListener);
    stop_btn.setOnClickListener(mOnClickListener);
    audios.setOnClickListener(mOnClickListener);
    subtitles.setOnClickListener(mOnClickListener);

    player_seek_bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
      @Override
      public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (fromUser == true && mMmpView != null) {
          mMmpView.seek(progress);
        }
      }

      @Override
      public void onStartTrackingTouch(SeekBar seekBar) {
      // TODO Auto-generated method stub
      }

      @Override
      public void onStopTrackingTouch(SeekBar seekBar) {
        // TODO Auto-generated method stub
      }
    });

    volume_bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
      @Override
      public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        int maxVolume = ((AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE)).
            getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        Log.d(TAG, "new volume " + progress + "(" + progress/(float)maxVolume + "/1.0)");
        mMmpView.setVolume(progress/(float)maxVolume);
      }

      @Override
      public void onStartTrackingTouch(SeekBar seekBar) {
      // TODO Auto-generated method stub
      }

      @Override
      public void onStopTrackingTouch(SeekBar seekBar) {
        // TODO Auto-generated method stub
      }
    });
  }


  private void resetControlPanel() {
    player_seek_bar.setMax(0);
    media_duration_view.setText("");
    player_seek_bar.setProgress(0);
    media_time_view.setText("");
    audio_listview.setAdapter(null);
    subtitle_listview.setAdapter(null);
  }


  private void createAudioSubstreamSelectionList() {
    TrackInfo[] track_info = mMmpView.enumerateAudioTrack();
    if (track_info == null) {
      Log.e(TAG, "No audio streams found!");
      return;
    }

    String[] audio_substream_info;
    audio_substream_info = new String[track_info.length];

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];
      String language = info.getLanguage();
      audio_substream_info[i] = language;
    }
    audio_listview.setAdapter(new ArrayAdapter<String>(getActivity(),
        R.layout.substream_list_item, audio_substream_info));

    audio_listview.setOnItemClickListener(new OnItemClickListener() {
      @Override
      public void onItemClick(AdapterView parent, View v, int position, long id) {
        mMmpView.selectAudioTrack(position);
        audio_listview.setAdapter(null);
      }
    });
  }


  private void createSubtitleSubstreamSelectionList() {
    TrackInfo[] track_info = mMmpView.enumerateSubtitleTrack();
    if (track_info == null) {
      Log.e(TAG, "No subtitle streams found!");
      return;
    }

    String[] subtitle_substream_info;
    subtitle_substream_info = new String[track_info.length];

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];
      String language = info.getLanguage();
      subtitle_substream_info[i] = language;
    }
    subtitle_listview.setAdapter(new ArrayAdapter<String>(getActivity(),
        R.layout.substream_list_item, subtitle_substream_info));

    subtitle_listview.setOnItemClickListener(new OnItemClickListener() {
      @Override
      public void onItemClick(AdapterView parent, View v, int position, long id) {
        mMmpView.selectSubtitleTrack(position);
        subtitle_listview.setAdapter(null);
      }
    });
  }


  // Return true to prevent this event from being propagated further,
  // or false to indicate that you have not handled this event
  // and it should continue to be propagated.
  public boolean myOnKeyDown(int keyCode, KeyEvent event) {
    int progress = 0;
    int maxVolume = 100;
    switch (keyCode) {
      case KeyEvent.KEYCODE_VOLUME_UP:
        Log.d(TAG, "KEYCODE_VOLUME_UP");
        progress = volume_bar.getProgress();
        maxVolume = ((AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE)).
            getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        if (progress + 5 <= maxVolume) {
          progress += 5;
        } else {
          progress = maxVolume;
        }
        Log.d(TAG, "KEYCODE_VOLUME_UP, new volume " + progress);
        volume_bar.setProgress(progress);
        mMmpView.setVolume(progress/(float)maxVolume);
        return true;
      case KeyEvent.KEYCODE_VOLUME_DOWN:
        Log.d(TAG, "KEYCODE_VOLUME_DOWN");
        progress = volume_bar.getProgress();
        maxVolume = ((AudioManager) getActivity().getSystemService(Context.AUDIO_SERVICE)).
            getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        if (progress >= 5) {
          progress -= 5;
        } else {
          progress = 0;
        }
        Log.d(TAG, "KEYCODE_VOLUME_DOWN, new volume " + progress);
        volume_bar.setProgress(progress);
        mMmpView.setVolume(progress/(float)maxVolume);
        return true;
      case KeyEvent.KEYCODE_VOLUME_MUTE:
        Log.d(TAG, "KEYCODE_VOLUME_MUTE");
        return true;
    }
    return false;
  }


  public void playVideo(String path) {
    if (mMmpView == null) {
      mMmpView = MmpView.getView();
    }
    Log.d(TAG, "play video " + path);
    mMmpView.playVideo(path);
  }

}
