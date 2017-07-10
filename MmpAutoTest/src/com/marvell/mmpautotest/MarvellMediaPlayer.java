package com.marvell.mmpautotest;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;


import android.os.Bundle;
import android.os.Parcel;
import android.util.Log;
import android.content.Context;
import android.media.MediaPlayer;
import android.media.MediaPlayer.TrackInfo;
import android.net.Uri;

public class MarvellMediaPlayer extends MediaPlayer {

  final static String TAG = "MarvellMediaPlayer";

  private long api_start_time = 0;
  private long api_stop_time = 0;

  public long getAPIDelay() {
    return api_stop_time - api_start_time;
  }

  @Override
  public void setDataSource(Context context, Uri uri) {
    api_start_time = android.os.SystemClock.uptimeMillis();
    try {
      super.setDataSource(context, uri);
    } catch (Exception ex) {
      return;
    }
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  @Override
  public void start() {
    api_start_time = android.os.SystemClock.uptimeMillis();
    super.start();
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  @Override
    public void stop() {
    api_start_time = android.os.SystemClock.uptimeMillis();
    super.stop();
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  @Override
  public void pause() {
    api_start_time = android.os.SystemClock.uptimeMillis();
    super.pause();
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  @Override
    public void reset() {
    api_start_time = android.os.SystemClock.uptimeMillis();
    super.reset();
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  public TrackInfo[] enumerateAudioTrack(){
    TrackInfo[] track_info = getTrackInfo();

    int audio_track_num = 0;

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];

      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_AUDIO) {
        audio_track_num++;
      }
    }

    TrackInfo[] audio_track_info = new TrackInfo[audio_track_num];

    for (int i = 0, j = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];
      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_AUDIO) {
        audio_track_info[j++] = track_info[i];
      }
    }
    return audio_track_info;
  }

  public int getCurrentAudioTrack() {
    // Todo: implement this
    return 0;
  }

  public void selectAudioTrack(int index) {
    api_start_time = android.os.SystemClock.uptimeMillis();
    TrackInfo[] track_info = getTrackInfo();

    Log.i(TAG, "index = " + index);

    int audio_track_num = 0;
    int all_track_index = 0;

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];

      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_AUDIO) {
        if (audio_track_num++ == index) {
            all_track_index = i;
            break;
        }
      }
    }

    Log.i(TAG, "select audio track " + all_track_index);
    super.selectTrack(all_track_index);
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

  public TrackInfo[] enumerateSubtitleTrack(){
    TrackInfo[] track_info = getTrackInfo();

    int audio_track_num = 0;

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];

      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT) {
        audio_track_num++;
      }
    }

    TrackInfo[] audio_track_info = new TrackInfo[audio_track_num];

    for (int i = 0, j = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];
      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT) {
        audio_track_info[j++] = track_info[i];
      }
    }
    return audio_track_info;
  }

  public int getCurrentSubtitleTrack() {
    // Todo: implement this
    return 0;
  }

  public void selectSubtitleTrack(int index) {
    api_start_time = android.os.SystemClock.uptimeMillis();
    TrackInfo[] track_info = getTrackInfo();

    Log.i(TAG, "index = " + index);

    int audio_track_num = 0;
    int all_track_index = 0;

    for (int i = 0; i < track_info.length; i++) {
      final TrackInfo info = track_info[i];

      if (info.getTrackType() == TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT) {
        if (audio_track_num++ == index) {
            all_track_index = i;
            break;
        }
      }
    }

    Log.i(TAG, "select subtitle track " + all_track_index);
    super.selectTrack(all_track_index);
    api_stop_time = android.os.SystemClock.uptimeMillis();
  }

}
