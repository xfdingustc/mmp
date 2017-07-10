
// A simple playback test for MediaPlayer.

#include <string.h>

// frameworks/base/include/
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <media/AudioSystem.h>
#include <media/IMediaPlayerService.h>
#include <media/mediaplayer.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/threads.h>

#define VERIFY(x) \
  if (!(x)) { \
    printf("*** Error at %s:%d\n", __FILE__, __LINE__); \
    exit(1); \
  }

#define CHECK_OK(x) \
  if ((x) != OK) { \
    printf("*** Error at %s:%d:%s(%d)\n", __FILE__, __LINE__, strerror(x), x); \
    exit(1); \
  }


//using std::string;

namespace android {
namespace test {

class TestMediaPlayerListener: public MediaPlayerListener {
 public:
  TestMediaPlayerListener() : eos_(false) {
  }

  virtual void notify(int msg, int ext1, int ext2, const Parcel *obj) {
    printf("TestMediaPlayerListener::notify\n");

    if (msg == MEDIA_PLAYBACK_COMPLETE) {
      eos_ = true;
      eos_cond_.signal();
      printf("TestMediaPlayerListener received(EOS) MEDIA_PLAYBACK_COMPLETE.\n");
    } else if (msg == MEDIA_ERROR) {
      printf("TestMediaPlayerListener received an error notification"
           "(id = %d, ext1 = %d, ext2 = %d)", msg, ext1, ext2);
      exit(1);
    } else {
      printf("TestMediaPlayerListener received an unrecognized notification"
           "(id = %d, ext1 = %d, ext2 = %d)", msg, ext1, ext2);
    }
  }

  void waitUntilEOS() {
    AutoMutex lock(mutex_);
    while (!eos_) {
      eos_cond_.wait(mutex_);
    }
  }

 private:
  Mutex mutex_;
  Condition eos_cond_;
  bool eos_;
};

class MediaPlayerTest {
 public:
  MediaPlayerTest() {
    ProcessState::self()->startThreadPool();
  }

  ~MediaPlayerTest() {
    if (player_ != NULL) {
      CHECK_OK(player_->setListener(NULL));
    }
    if (composer_client_ != NULL) {
      composer_client_->dispose();
    }

    IPCThreadState::self()->stopProcess();
  }

  bool Prepare(const char* file_path, const int screen_width,
             const int screen_height, const int screen_posx,
             const int screen_posy) {
    VERIFY(screen_width > 0);
    VERIFY(screen_height > 0);

    // Compose a surface.
    composer_client_ = new SurfaceComposerClient();
    VERIFY(composer_client_->initCheck() == static_cast<status_t>(OK));

    surface_control_ = composer_client_->createSurface(
        String8("A Surface"),
        0,
        screen_width,
        screen_height,
        PIXEL_FORMAT_RGB_565,
        0);

    VERIFY(surface_control_ != NULL);
    VERIFY(surface_control_->isValid());

    CHECK_OK(surface_control_->setPosition(screen_posx, screen_posy));
    // Put test surface in front of anything else
    VERIFY(surface_control_->setLayer(30000) == static_cast<status_t>(OK));
    VERIFY(surface_control_->show() == static_cast<status_t>(OK));

    surface_ = surface_control_->getSurface();
    VERIFY(surface_ != NULL);
    VERIFY(surface_->isValid());

    player_ = new MediaPlayer();
    if (player_ == NULL) {
      return false;
    }
    listener_ = new TestMediaPlayerListener();
    CHECK_OK(player_->setListener(listener_));
    CHECK_OK(player_->setLooping(0));  // no looping
    CHECK_OK(player_->setDataSource(file_path, NULL));
    //CHECK_OK(player_->setAudioStreamType(AudioSystem::MUSIC));
    CHECK_OK(player_->setVideoSurface(surface_));
    printf("Preparing... \n");
    CHECK_OK(player_->prepare());
    printf("Prepare complete.\n");

    return true;
  }

  bool TestPlayback(int play_sec, int pause_sec) {
    // Test the playback function.
    // play_sec is total duration of playback
    // pause_sec is pause test over the 1 second playback.
    int total_duration_msec = 0;
    CHECK_OK(player_->getDuration(&total_duration_msec));

    int net_play_sec = play_sec;
    const int kStartPlaySec = 1;
    CHECK_OK(player_->start());
    if (0 < pause_sec) {
      if (kStartPlaySec * 1000 <= total_duration_msec) {
        net_play_sec -= kStartPlaySec;
        sleep(kStartPlaySec);
      }
      CHECK_OK(player_->pause());
      sleep(pause_sec);
      CHECK_OK(player_->start());
    }

    if (0 <= net_play_sec && net_play_sec * 1000 < total_duration_msec) {
      sleep(net_play_sec);
    } else {
      listener_->waitUntilEOS();
    }
    CHECK_OK(player_->stop());
    CHECK_OK(player_->reset());

    return true;
  }

 private:
  sp<MediaPlayer> player_;

  sp<Surface> surface_;
  sp<SurfaceComposerClient> composer_client_;
  sp<SurfaceControl> surface_control_;
  sp<TestMediaPlayerListener> listener_;
};

}  // namespace test
}  // namespace android


int main(int argc, char** argv) {

 if (argc != 3) {
   printf("Usage: %s file path fd mark(0:filesrc,1:fdsrc)\n", argv[0]);
   return -1;
 }

  printf("the first URL is %s, the second URL is %s\n", argv[1], argv[2]);

  android::test::MediaPlayerTest ADVobj;
  android::test::MediaPlayerTest MOVobj;
  VERIFY(ADVobj.Prepare(argv[1], 1280, 800, 0, 0));
  VERIFY(MOVobj.Prepare(argv[2], 1280, 800, 0, 0));
  VERIFY(ADVobj.TestPlayback(-1, 0));
  VERIFY(MOVobj.TestPlayback(-1, 0));

  return 0;
}
