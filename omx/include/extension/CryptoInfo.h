
/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ANDORID_CRYPTOINFO_H
#define ANDROID_CRYPTOINFO_H

#include <media/stagefright/foundation/ADebug.h>

namespace android {

    struct CryptoInfo {
        // Borrowed from /AndroidMaster/frameworks/native/include/media/hardware/CryptoAPI.h.
        // This class should be synced with com.google.android.tv.media.Mediasource.CryptoInfo.
        enum Mode {
            kMode_Unencrypted = 0,
            kMode_AES_CTR = 1,

            // Neither key nor iv are being used in this mode.
            // Each subsample is encrypted w/ an iv of all zeroes.
            kMode_AES_WV = 2,   // FIX constant
        };
        struct SubSample {
            size_t mNumBytesOfClearData;
            size_t mNumBytesOfEncryptedData;
        };

        uint32_t mMode;
        uint8_t mKey[16];
        // UUID of the DRM system.  Refer to PIFF 5.3.1.
        uint8_t mDrmId[16];
        uint8_t mIv[16];
        uint32_t mNumSubSamples;

        // mSubSamples will have the size specified in mNumSubSamples.
        // In case of WV, each sub-sample represents the crypto unit.
        SubSample mSubSamples[1];

        static inline CryptoInfo *newCryptoInfo(uint32_t numSubSamples) {
            CHECK_GT(numSubSamples, 0u);
            uint32_t size =
                sizeof(CryptoInfo) + sizeof(SubSample) * (numSubSamples - 1);
            uint8_t *p = new uint8_t[size];
             memset(p, 0, size);
            CryptoInfo *cryptoInfo = reinterpret_cast < CryptoInfo * >(p);
             cryptoInfo->mNumSubSamples = numSubSamples;
             return cryptoInfo;
        }
        static inline CryptoInfo *newCryptoInfo(const uint8_t * byteArray,
                                                size_t size) {
            uint8_t *p = new uint8_t[size];
             memcpy(p, byteArray, size);
            CryptoInfo *cryptoInfo = reinterpret_cast < CryptoInfo * >(p);

             CHECK_EQ(size,
                      sizeof(CryptoInfo) +
                      sizeof(SubSample) * (cryptoInfo->mNumSubSamples - 1));
             return cryptoInfo;
        }
        static inline void deleteCryptoInfo(CryptoInfo * cryptoInfo) {
            uint8_t *p = reinterpret_cast < uint8_t * >(cryptoInfo);
            delete[]p;
        }
    };

#define UUID_WIDEVINE  "\xed\xef\x8b\xa9\x79\xd6\x4a\xce\xa3\xc8\x27\xdc\xd5\x1d\x21\xed"
#define UUID_PLAYREADY "\x9a\x04\xf0\x79\x98\x40\x42\x86\xab\x92\xe6\x5b\xe0\x88\x5f\x95"

};                              // namespace android

#endif // ANDROID_CRYPTOINFO_H
