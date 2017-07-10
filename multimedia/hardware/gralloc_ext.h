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

#ifndef GOOGLETV_GRALLOC_INTERFACE_EXT_H
#define GOOGLETV_GRALLOC_INTERFACE_EXT_H

enum {
    /* External surface is updated by external source. Its buffers are
     * never rendered into and cannot be locked for access.
     * Equal to GRALLOC_USAGE_PRIVATE_3; vendor should avoid using
     * GRALLOC_USAGE_PRIVATE_3.
     */
    GRALLOC_USAGE_EXTERNAL_SURFACE      = 0x80000000,
    GRALLOC_USAGE_SURFACE_ID_0          = 0x40000000,
    GRALLOC_USAGE_SURFACE_ID_1          = 0x20000000,
};

#endif  // GOOGLETV_GRALLOC_INTERFACE_EXT_H
