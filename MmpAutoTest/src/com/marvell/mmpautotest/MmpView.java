package com.marvell.mmpautotest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.AssetFileDescriptor;
import android.content.Intent;
import android.drm.DrmEvent;
import android.drm.DrmInfo;
import android.drm.DrmInfoRequest;
import android.drm.DrmInfoStatus;
import android.drm.DrmManagerClient;
import android.drm.DrmStore.RightsStatus;
import android.os.Handler;
import android.os.AsyncTask;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnBufferingUpdateListener;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.media.MediaPlayer.OnVideoSizeChangedListener;
import android.media.MediaPlayer.TrackInfo;
import android.net.Uri;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Toast;
import android.os.ConditionVariable;



import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;






import com.marvell.mmpautotest.MarvellMediaPlayer;



public class MmpView implements OnBufferingUpdateListener, OnCompletionListener,
    OnErrorListener, OnPreparedListener, OnVideoSizeChangedListener, SurfaceHolder.Callback {

  final static String TAG = "MarvellMediaPlayerTestBase";

  public static MmpView getView() {
    if (sMmpView == null) {
      sMmpView = new MmpView();
      sMmpView.init();
    }

    return sMmpView;
  }

  private static MmpView sMmpView = null;

  private SurfaceView mPreview;
  private SurfaceHolder holder;
  private long mStartTime;

  private MarvellMediaPlayer mMarvellMediaPlayer = null;

  Context active_context_;

  private void init() {
  }

  public void setContext(Context context) {
    active_context_ = context;
  }

  public void setSurfaceView( SurfaceView preview) {
    mPreview = preview;
    holder = mPreview.getHolder();
    holder.addCallback((Callback) this);
  }

  public static final int MMP_VIEW_CALLBACK_KWHAT_UPDATE_TIME = 1;
  public static final int MMP_VIEW_CALLBACK_KWHAT_UPDATE_DURATION = 2;
  public static final int MMP_VIEW_CALLBACK_KWHAT_PLAYBACK_COMPLETE = 3;

  public interface OnMmpViewCallbackListener {
    void OnMmpViewCallback(int kWhat, int extra);
  }

  public void setMmpViewCallbackListener(OnMmpViewCallbackListener callback) {
    mCallbackListener = callback;
  }

  private OnMmpViewCallbackListener mCallbackListener;



  private LogManager mLogManager = LogManager.getManager();


  public void playVideo(String path) {
    mLogManager.outputLog("To play stream " + path);
    playVideo(path, mMarvellMediaPlayer);
  }

  public void stop() {
    handler.removeCallbacks(updateThread);
    resetMediaPlayer(mMarvellMediaPlayer);
    mMarvellMediaPlayer = null;
  }

  public void start() {
    if (mMarvellMediaPlayer != null) {
      startMediaPlayer(mMarvellMediaPlayer);
    }
  }

  public void pause() {
    if (mMarvellMediaPlayer != null) {
      pauseMediaPlayer(mMarvellMediaPlayer);
    }
  }

  public void reset() {
    if (mMarvellMediaPlayer != null) {
      resetMediaPlayer(mMarvellMediaPlayer);
    }
  }

  public void release() {
    playerRelease(mMarvellMediaPlayer);

  }

  public void seek(int msec) {
    if (mMarvellMediaPlayer != null) {
      playerSeek(mMarvellMediaPlayer, msec);
    }
  }

  public int getDuration() {
    if (mMarvellMediaPlayer != null) {
      return mMarvellMediaPlayer.getDuration();
    } else {
      return 0;
    }
  }
  public int getCurrentPosition() {
    if (mMarvellMediaPlayer != null) {
      return mMarvellMediaPlayer.getCurrentPosition();
    } else {
      return 0;
    }
  }

  public TrackInfo[] enumerateAudioTrack() {
    if (mMarvellMediaPlayer != null) {
      return mMarvellMediaPlayer.enumerateAudioTrack();
    } else {
      return null;
    }
  }

  public void selectAudioTrack(int index) {
    if (mMarvellMediaPlayer == null) {
      return;
    }
    mMarvellMediaPlayer.selectAudioTrack(index);
    mLogManager.outputLog("Audio track is set to " + index + " Cost: " +
        mMarvellMediaPlayer.getAPIDelay());
  }

  public TrackInfo[] enumerateSubtitleTrack() {
    if (mMarvellMediaPlayer != null) {
      return mMarvellMediaPlayer.enumerateSubtitleTrack();
    } else {
      return null;
    }
  }

  public void selectSubtitleTrack(int index) {
    if (mMarvellMediaPlayer == null) {
      return;
    }
    mMarvellMediaPlayer.selectSubtitleTrack(index);
    mLogManager.outputLog("Subtitle track is set to " + index + " Cost: " +
        mMarvellMediaPlayer.getAPIDelay());
  }

  public boolean isPlaying() {
    if (mMarvellMediaPlayer != null) {
      return mMarvellMediaPlayer.isPlaying();
    } else {
      return false;
    }
  }

  public void setVolume(float volume) {
    if (mMarvellMediaPlayer != null) {
      mLogManager.outputLog("set volume " + volume + " to player");
      Log.d(TAG, "set volume " + volume + " to player");
      mMarvellMediaPlayer.setVolume(volume, volume);
    }
  }

  private void playVideo(String path, MarvellMediaPlayer player) {
    try {
      resetMediaPlayer(player);
      player = mMarvellMediaPlayer = new MarvellMediaPlayer();

      player.setOnPreparedListener(this);
      player.setOnVideoSizeChangedListener(this);
      player.setOnCompletionListener(this);
      player.setOnBufferingUpdateListener(this);
      player.setOnErrorListener(this);

      player.setDataSource(active_context_, Uri.parse(path));
      mLogManager.outputLog("setDataSource:" + path
          + " Cost: " + player.getAPIDelay() + "ms");
      mContentUri = path;

      player.setDisplay(holder);
      player.setAudioStreamType(AudioManager.STREAM_MUSIC);
      player.setScreenOnWhilePlaying(true);

      mStartTime = android.os.SystemClock.uptimeMillis();
      mLogManager.outputLog("Preparing...");
      mMarvellMediaPlayer.prepareAsync();
    } catch (Exception ex) {
      Log.w(TAG, "Unable to open content: " + path, ex);
      return;
    }

  }

  private void stopMediaPlayer(MarvellMediaPlayer player) {
    player.stop();
    mLogManager.outputLog("Player stop. Cost: " + player.getAPIDelay() + "ms");
  }

  private void startMediaPlayer(MarvellMediaPlayer player) {
    player.start();
    mLogManager.outputLog("Player start.");
  }

  private void pauseMediaPlayer(MarvellMediaPlayer player) {
    player.pause();
    mLogManager.outputLog("Player pause.");
  }

  private void resetMediaPlayer(MarvellMediaPlayer player) {
    if (player != null) {
      player.reset();
      mLogManager.outputLog("Player reset. Cost: " + player.getAPIDelay() + "ms");
      player.release();
      player = null;
    }
  }

  private void playerRelease(MarvellMediaPlayer player) {
    player.reset();
    player.release();
  }

  private void playerSeek(MarvellMediaPlayer player, int msec) {
    player.seekTo(msec);
  }

  /**
   * Returns the url trims '/manifest' from the SmoothStreaming manifest url.
   */
  private static String getSmoothStreamingBaseUrl(String manifestUrl) {
      String lowerCaseUrl = manifestUrl.toLowerCase();
      int index = lowerCaseUrl.lastIndexOf(".ism/manifest");
      if (index > 0) {
          return manifestUrl.substring(0, index + 4);
      } else {
          return manifestUrl;
      }
  }

  private String mContentUri = null;
  DrmManagerClient mDrmManagerClient = null;



  private static String sendSoapAction(String serverUrl, String request, String action) {
      // TODO: Remove this.
      //StrictMode.setThreadPolicy(StrictMode.ThreadPolicy.LAX);

      StringBuffer buf = new StringBuffer();
      HttpURLConnection urlConnection = null;
      try {
          urlConnection = (HttpURLConnection) new URL(serverUrl).openConnection();
          urlConnection.setDoOutput(true);
          urlConnection.setChunkedStreamingMode(0);
          urlConnection.setRequestProperty("Content-Type", "text/xml");
          urlConnection.setRequestProperty("SOAPAction", action);

          OutputStream out = new BufferedOutputStream(urlConnection.getOutputStream());
          out.write(request.getBytes());
          out.close();

          InputStream in = new BufferedInputStream(urlConnection.getInputStream());
          int c;
          while ((c = in.read()) != -1) {
              buf.append((char) c);
          }
          in.close();
      } catch (IOException e) {
          Log.e(TAG, "sendSoapAction: " + e);
          e.printStackTrace();
      } finally {
          if (urlConnection != null) {
              urlConnection.disconnect();
          }
      }
      return buf.toString();
  }

  public void onPrepared(MediaPlayer player) {
    mLogManager.outputLog("Prepare completed");
    mCallbackListener.OnMmpViewCallback(MMP_VIEW_CALLBACK_KWHAT_UPDATE_DURATION,
        player.getDuration());
    DrmManagerClient drmManagerClient = new DrmManagerClient(active_context_);
    // PlayReady DRM plugin requries that the url should end with '.ism'.
    String rightsUri = getSmoothStreamingBaseUrl(mContentUri);
    int rightsStatus = drmManagerClient.checkRightsStatus(rightsUri);
    Log.d(TAG, "onPrepare: checkRights " + rightsUri + " - " + rightsStatus);
    switch (rightsStatus) {
        case RightsStatus.RIGHTS_NOT_ACQUIRED:
        case RightsStatus.RIGHTS_EXPIRED:
            mDrmManagerClient = drmManagerClient;
            new AcquireRightsTask().execute(rightsUri);
            break;
        // The contents can't be handled by any DRM plugins or clear contents.
        case RightsStatus.RIGHTS_INVALID:
        case RightsStatus.RIGHTS_VALID:
            startMediaPlayer((MarvellMediaPlayer)player);
            break;
    }

    handler.post(updateThread);
  }

  public void onCompletion(MediaPlayer mp) {
    mLogManager.outputLog("Playback complete");

    // Reset UI at the same time.
    handler.removeCallbacks(updateThread);
    mCallbackListener.OnMmpViewCallback(MMP_VIEW_CALLBACK_KWHAT_UPDATE_TIME, 0);
    mCallbackListener.OnMmpViewCallback(MMP_VIEW_CALLBACK_KWHAT_UPDATE_DURATION, 0);
    mCallbackListener.OnMmpViewCallback(MMP_VIEW_CALLBACK_KWHAT_PLAYBACK_COMPLETE, 0);

    if (mMarvellMediaPlayer == mp) {
      mMarvellMediaPlayer.reset();
      mLogManager.outputLog("Player reset. Cost: " + mMarvellMediaPlayer.getAPIDelay() + "ms");
      mMarvellMediaPlayer.release();
      mMarvellMediaPlayer = null;
    }
  }

  public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
    mLogManager.outputLog("Video resolution changed width = "
        + width + " height " + height);
  }

  public void onBufferingUpdate(MediaPlayer arg0, int percent) {
    mLogManager.outputLog("BufferingUpdate percent:" + percent);
  }

  @Override
  public boolean onError(MediaPlayer mp, int what, int extra) {
    mLogManager.outputLog("onError: (" + what + ", " + extra + ")");
    return false;
  }

  public void surfaceChanged(SurfaceHolder surfaceholder, int i, int j, int k) {
    Log.d(TAG, "surfaceChanged called");

  }

  public void surfaceDestroyed(SurfaceHolder surfaceholder) {
    Log.d(TAG, "surfaceDestroyed called");
  }

  public void surfaceCreated(SurfaceHolder holder) {
    Log.d(TAG, "surfaceCreated called");
  }

  Handler handler = new Handler();
  Runnable updateThread = new Runnable() {
    public void run() {
      if (mMarvellMediaPlayer != null) {
        mCallbackListener.OnMmpViewCallback(MMP_VIEW_CALLBACK_KWHAT_UPDATE_TIME,
            mMarvellMediaPlayer.getCurrentPosition());
        handler.postDelayed(updateThread, 500);
      }
    }
  };
  private static final String PR_NO_DATA = "PRNoData";
  private static final String PR_MIMETYPE = "application/vnd.ms-playready";
  private static final String PR_KEY_CONTENTS_PATH = "PRContentPath";
  private static final String PR_KEY_LICENSE_SERVER_URL = "PRLicenseServerUrl";
  private static final String PR_KEY_LICENSE_RESPONSE = "PRLicenseResponse";
  private static final String PR_KEY_LICENSE_ACK_RESPONSE = "PRLicenseAckResponse";


  // DRM related
  private class AcquireRightsTask extends AsyncTask<String, String, Boolean> {
        @Override
        protected Boolean doInBackground(String... urls) {
            if (urls.length < 1) return false;
            String contentUrl = urls[0];
            DrmInfoRequest drmInfoRequest = new DrmInfoRequest(
                    DrmInfoRequest.TYPE_RIGHTS_ACQUISITION_INFO, PR_MIMETYPE);
            drmInfoRequest.put(PR_KEY_CONTENTS_PATH, contentUrl);

            DrmInfo drmInfo = mDrmManagerClient.acquireDrmInfo(drmInfoRequest);
            if (drmInfo == null) {
                Log.e(TAG, "Failed to DrmManagerClient.acquireDrmInfo("
                        + "TYPE_RIGHTS_ACQUISITION_INFO)");
                return false;
            }

            publishProgress("Sending license challenge");
            final String licenseServerUrl = (String) drmInfo.get(PR_KEY_LICENSE_SERVER_URL);
            // For the custom rights url, please refer to
            //   https://playready.directtaps.net/pr/doc/customrights
            final String customRightsUrl = licenseServerUrl +
                    "?PlayRight=1&FirstPlayExpiration=60";
            final String challenge = new String(drmInfo.getData());
            final String licenseResponse = sendSoapAction(customRightsUrl, challenge,
                    "http://schemas.microsoft.com/DRM/2007/03/protocols/AcquireLicense");
            drmInfo.put(PR_KEY_LICENSE_RESPONSE, licenseResponse);
            if (executeProcessDrmInfo(mDrmManagerClient, drmInfo) == DrmInfoStatus.STATUS_ERROR) {
                Log.e(TAG, "Failed to DrmManagerClient.processDrmInfo("
                        + "TYPE_RIGHTS_ACQUISITION_INFO)");
                return false;
            }

            drmInfoRequest = new DrmInfoRequest(
                    DrmInfoRequest.TYPE_RIGHTS_ACQUISITION_PROGRESS_INFO, PR_MIMETYPE);
            drmInfoRequest.put(PR_KEY_CONTENTS_PATH, contentUrl);

            drmInfo = mDrmManagerClient.acquireDrmInfo(drmInfoRequest);
            if (drmInfo == null) {
                Log.e(TAG, "Failed to DrmManagerClient.acquireDrmInfo("
                        + "TYPE_RIGHTS_ACQUISITION_PROGRESS_INFO)");
                return false;
            }

            final String ack = new String(drmInfo.getData());
            if (!ack.isEmpty() && !ack.equals(PR_NO_DATA)) {
                publishProgress("Sending license ACK");
                final String licenseAckResponse = sendSoapAction(licenseServerUrl, ack,
                        "http://schemas.microsoft.com/DRM/2007/03/protocols/AcknowledgeLicense");
                drmInfo.put(PR_KEY_LICENSE_ACK_RESPONSE, licenseResponse);
                if (executeProcessDrmInfo(mDrmManagerClient, drmInfo)
                        == DrmInfoStatus.STATUS_ERROR) {
                    Log.e(TAG, "Failed to DrmManagerClient.processDrmInfo("
                            + "TYPE_RIGHTS_ACQUISITION_PROGRESS_INFO)");
                    return false;
                }
            }
            return true;
        }

        @Override
        protected void onPostExecute(Boolean success) {
            String message = success ? "Success in acquring rights" : "Failed in acquiring rights";
            Toast.makeText(active_context_, message, Toast.LENGTH_SHORT).show();

            if (success) {
                startMediaPlayer(mMarvellMediaPlayer);
            }
        }

        @Override
        protected void onProgressUpdate(String... messages) {
            Toast.makeText(active_context_, messages[0], Toast.LENGTH_SHORT).show();
        }
    }


    private static int executeProcessDrmInfo(DrmManagerClient drmClient, DrmInfo drmInfo) {
        final int kWaitTimeMs = 30000; // 30 secs max
        final ConditionVariable processDrmInfoDone = new ConditionVariable();
        final int[] statusCodes = new int[1];
        statusCodes[0] = DrmInfoStatus.STATUS_ERROR;

        drmClient.setOnEventListener(new DrmManagerClient.OnEventListener() {
            @Override
            public void onEvent(DrmManagerClient client, DrmEvent event) {
                switch (event.getType()) {
                // In case of TYPE_RIGHTS_ACQUISITION_PROGRESS_INFO, the event type will be -1.
                // See DrmManagerClient.getEventType().
                case -1:
                case DrmEvent.TYPE_DRM_INFO_PROCESSED:
                    Log.d(TAG, "processDrmInfo() completed");
                    DrmInfoStatus drmInfoStatus = (DrmInfoStatus) event.getAttribute(
                            DrmEvent.DRM_INFO_STATUS_OBJECT);
                    statusCodes[0] = drmInfoStatus.statusCode;
                    break;
                default:
                    Log.d(TAG, "Unkown event: " + event.getType());
                    break;
                }
                processDrmInfoDone.open();
            }
        });

        if (DrmManagerClient.ERROR_NONE != drmClient.processDrmInfo(drmInfo)) {
            return DrmInfoStatus.STATUS_ERROR;
        }
        processDrmInfoDone.block(kWaitTimeMs);

        return statusCodes[0];
    }


}
