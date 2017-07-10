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
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;

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

public class SelectorFragment extends Fragment {
  final static String TAG = "SelectorFragment";

  private static String USB_ROOT_DIR = "/mnt/media_rw";

  private Button   play_url_btn;
  private Button   mFreshBtn;
  private EditText mUrlText;
  private ListView mSelectList;

  private VideoItemList mSelectItems;
  private String mCurrentDir;

  private OnStreamSelectedListener mStreamListener;


  public interface OnStreamSelectedListener {
    void OnStreamSelected(String path);
  }


  public SelectorFragment() {
    Log.d(TAG, "constructor");
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle icicle) {
    Log.d(TAG, "onCreateView");
    // Inflate the layout for this fragment
    View v = inflater.inflate(R.layout.video_selector, container, false);

    play_url_btn = (Button) v.findViewById(R.id.play_button);
    mFreshBtn = (Button) v.findViewById(R.id.refresh_button);
    mUrlText = (EditText) v.findViewById(R.id.video_selection_input);
    mSelectList = (ListView) v.findViewById(R.id.select_list);

    play_url_btn.setOnClickListener(mOnClickListener);
    mFreshBtn.setOnClickListener(mOnClickListener);

    mUrlText.requestFocus();

    Log.d(TAG, "finish create SelectorFragment");
    return v;
  }


  /**
     * Called when a fragment is first attached to its activity.
     * {@link #onCreate(Bundle)} will be called after this.
     */
  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);

    mStreamListener = (OnStreamSelectedListener)activity;
  }

  @Override
  public void onResume() {
    Log.d(TAG, "onResume");
    super.onResume();

    initEnvironmentVars();
    if (EnvironmentVarManager.usbMountPoint != null) {
      USB_ROOT_DIR = EnvironmentVarManager.usbMountPoint;
      mCurrentDir = USB_ROOT_DIR;
    }
    VideoItemList new_list = createVil(mCurrentDir);
    if (null != new_list) {
      mSelectItems = new_list;
      mSelectList.setAdapter(mSelectItems);
      mSelectList.setOnItemClickListener(mSelectHandler);
    }
  }

  @Override
  public void onPause() {
    Log.d(TAG, "onPause");
    super.onPause();
  }


  // Return true to prevent this event from being propagated further,
  // or false to indicate that you have not handled this event
  // and it should continue to be propagated.
  public boolean myOnKeyDown(int keyCode, KeyEvent event) {
    int progress = 0;
    int maxVolume = 100;
    switch (keyCode) {
      case KeyEvent.KEYCODE_BACK:
        if ((null != mSelectItems) &&
            (null != mSelectHandler) &&
            !mSelectItems.getIsRoot()) {
          File dir = new File(mSelectItems.getPath());
          if(dir.canRead()) {
            // Goto parent directory.
            mSelectHandler.onItemClick(null, null, 0, 0);
          }
          // Prevent this event from being propagated further.
          return true;
        }
        break;
    }
    return false;
  }

  private void initEnvironmentVars() {
    // Get USB mount location
    StorageManager mStorageManager =
        (StorageManager) getActivity().getSystemService(Context.STORAGE_SERVICE);
    StorageVolume[] storageVolumes = mStorageManager.getVolumeList();

    String usb_mount = null;
    for (int i = 0; i < storageVolumes.length; i++) {
      String sharePath = storageVolumes[i].getPath();
      String shareState = mStorageManager.getVolumeState(sharePath);
      if (sharePath.startsWith("/mnt/media")) {
        usb_mount = sharePath;
      }
    }

    if (usb_mount == null) {
      EnvironmentVarManager.usbMountPoint = "/mnt/media_rw/usb";
    } else {
      EnvironmentVarManager.usbMountPoint = usb_mount;
    }

  }


  private OnClickListener mOnClickListener = new OnClickListener() {
    @Override
    public void onClick(View btn) {
      switch (btn.getId()) {
        case R.id.play_button:
        Log.d(TAG, "OnItemClickListener " + mUrlText.getText().toString());
        mStreamListener.OnStreamSelected(mUrlText.getText().toString());
        break;

        case R.id.refresh_button:
          initEnvironmentVars();
          if (EnvironmentVarManager.usbMountPoint != null) {
            USB_ROOT_DIR = EnvironmentVarManager.usbMountPoint;
            mCurrentDir = USB_ROOT_DIR;
          }
          VideoItemList new_list = createVil(mCurrentDir);
          if (null != new_list) {
            mSelectItems = new_list;
            mSelectList.setAdapter(mSelectItems);
            mSelectList.setOnItemClickListener(mSelectHandler);
          }
          break;
        default:
          break;
      }
    }
  };


  private OnItemClickListener mSelectHandler = new OnItemClickListener() {
    @Override
    public void onItemClick(AdapterView parent, View v, int position, long id) {
      if ((position >= 0) && (position < mSelectItems.getCount())) {
        VideoItem item = mSelectItems.getItem(position);
        if (item.getIsDir()) {
          VideoItemList new_list = createVil(item.getUrl());
          if (null != new_list) {
            mSelectItems = new_list;
            mSelectList.setAdapter(mSelectItems);
          }
        } else {
          Log.d(TAG, "OnItemClickListener " + item.getUrl());
          mStreamListener.OnStreamSelected(item.getUrl());
        }
      }
    }
  };



  /**
   * VideoItem is a class used to represent a selectable item on the listbox
   * used to select videos to playback.
   */
  private static class VideoItem {
    private final String mToStringName;
    private final String mName;
    private final String mUrl;
    private final boolean mIsDir;

    public VideoItem(String name, String url, boolean isDir) {
      mName = name;
      mUrl = url;
      mIsDir = isDir;

      if (isDir) {
        mToStringName = String.format("[dir] %s", name);
      } else {
        int ndx = url.indexOf(':');
        if (ndx > 0) {
          mToStringName = String.format("[%s] %s", url.substring(0, ndx), name);
        } else {
          mToStringName = name;
        }
      }
    }

    public static VideoItem createFromLinkFile(File f) {
      VideoItem retVal = null;

      try {
        BufferedReader rd = new BufferedReader(new FileReader(f));
        String name = rd.readLine();
        String url = rd.readLine();
        if ((null != name) && (null != url)) {
          retVal = new VideoItem(name, url, false);
        }
      } catch (FileNotFoundException e) {
      } catch (IOException e) {
      }

      return retVal;
    }

    @Override
      public String toString() {
      return mToStringName;
    }

    public String getName() {
      return mName;
    }

    public String getUrl() {
      return mUrl;
    }

    public boolean getIsDir() {
      return mIsDir;
    }
  }


  /**
   * VideoItemList is an array adapter of video items used by the android
   * framework to populate the list of videos to select.
   */
  private class VideoItemList extends ArrayAdapter<VideoItem> {
    private final String mPath;
    private final boolean mIsRoot;

    private VideoItemList(String path, boolean isRoot) {
      super(getActivity(), R.layout.video_list_item, R.id.video_list_item);
      mPath = path;
      mIsRoot = isRoot;
    }

    public String getPath() {
      return mPath;
    }

    public boolean getIsRoot() {
      return mIsRoot;
    }
  }

  private VideoItemList createVil(String p) {
    mCurrentDir = p;
    boolean is_root = USB_ROOT_DIR.equals(p);

    File dir = new File(p);
    if (!dir.isDirectory() || !dir.canRead())
      return null;

    VideoItemList retVal = new VideoItemList(p, is_root);

    // If this is not the root directory, go ahead and add the back link to
    // our parent.
    if (!is_root)
      retVal.add(new VideoItem("..", dir.getParentFile().getAbsolutePath(), true));

    // Make a sorted list of directories and files contained in this
    // directory.
    TreeMap<String, VideoItem> dirs = new TreeMap<String, VideoItem>();
    TreeMap<String, VideoItem> files = new TreeMap<String, VideoItem>();

    File search_dir = new File(p);
    File[] flist = search_dir.listFiles();
    if (null == flist)
      return retVal;

    for (File f : flist) {
      if (f.canRead()) {
        if (f.isFile()) {
          String fname = f.getName();
          VideoItem newItem = null;

          if (fname.endsWith(".url")) {
            newItem = VideoItem.createFromLinkFile(f);
          } else {
            String url = "file://" + f.getAbsolutePath();
            newItem = new VideoItem(fname, url, false);
          }

          if (null != newItem)
            files.put(newItem.getName(), newItem);

        } else if (f.isDirectory()) {
          VideoItem newItem = new VideoItem(f.getName(), f.getAbsolutePath(), true);
          dirs.put(newItem.getName(), newItem);
        }
      }
    }

    // now add the the sorted directories to the result set.
    for (VideoItem vi : dirs.values())
      retVal.add(vi);

    // finally add the the sorted files to the result set.
    for (VideoItem vi : files.values())
      retVal.add(vi);

    return retVal;
  }

}
