/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHROME_HTTP_DATA_SOURCE_H_

#define CHROME_HTTP_DATA_SOURCE_H_

#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/threads.h>

namespace android {

struct SfDelegate;

enum Flags {
    kWantsPrefetching      = 1,
    kStreamedFromLocalHost = 2,
    kIsCachingDataSource   = 4,
    kIsHTTPBasedSource     = 8,
};

struct ChromiumHTTPDataSource : public RefBase {
    ChromiumHTTPDataSource(uint32_t flags = 0);

    virtual status_t connect(
            const char *uri,
            const KeyedVector<String8, String8> *headers = NULL,
            off64_t offset = 0);

    virtual void disconnect();

    virtual status_t initCheck() const;

    virtual ssize_t readAt(off64_t offset, void *data, size_t size);
    virtual status_t getSize(off64_t *size);
    virtual uint32_t flags();

    virtual sp<DecryptHandle> DrmInitialization(const char *mime);

    virtual void getDrmInfo(sp<DecryptHandle> &handle, DrmManagerClient **client);

    virtual String8 getUri();

    virtual String8 getMIMEType() const;

    virtual status_t reconnectAtOffset(off64_t offset);

    void setUID(uid_t uid);
    bool getUID(uid_t *uid) const;

    void addBandwidthMeasurement(size_t numBytes, int64_t delayUs);

    // Returns true if bandwidth could successfully be estimated,
    // false otherwise.
    bool estimateBandwidth(int32_t *bandwidth_bps);

protected:
    virtual ~ChromiumHTTPDataSource();

private:
    friend struct SfDelegate;

    enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        READING,
        DISCONNECTING
    };

    const uint32_t mFlags;

    mutable Mutex mLock;
    Condition mCondition;

    State mState;

    SfDelegate *mDelegate;

    AString mURI;
    KeyedVector<String8, String8> mHeaders;

    off64_t mCurrentOffset;

    // Any connection error or the result of a read operation
    // (for the lattter this is the number of bytes read, if successful).
    ssize_t mIOResult;

    int64_t mContentSize;

    String8 mContentType;

    sp<DecryptHandle> mDecryptHandle;
    DrmManagerClient *mDrmManagerClient;

    bool mUIDValid;
    uid_t mUID;

    Mutex mBandwidthLock;

    struct BandwidthEntry {
        int64_t mDelayUs;
        size_t mNumBytes;
    };
    List<BandwidthEntry> mBandwidthHistory;
    size_t mNumBandwidthHistoryItems;
    int64_t mTotalTransferTimeUs;
    size_t mTotalTransferBytes;

    enum {
        kMinBandwidthCollectFreqMs = 1000,   // 1 second
        kMaxBandwidthCollectFreqMs = 60000,  // one minute
    };

    int64_t mPrevBandwidthMeasureTimeUs;
    int32_t mPrevEstimatedBandWidthKbps;
    int32_t mBandWidthCollectFreqMs;

    void disconnect_l();

    status_t connect_l(
            const char *uri,
            const KeyedVector<String8, String8> *headers,
            off64_t offset);

    static void InitiateRead(
            ChromiumHTTPDataSource *me, void *data, size_t size);

    void initiateRead(void *data, size_t size);

    void onConnectionEstablished(
            int64_t contentSize, const char *contentType);

    void onConnectionFailed(status_t err);
    void onReadCompleted(ssize_t size);
    void onDisconnectComplete();

    void clearDRMState_l();

    DISALLOW_EVIL_CONSTRUCTORS(ChromiumHTTPDataSource);
};

}  // namespace android

#endif  // CHROME_HTTP_DATA_SOURCE_H_
