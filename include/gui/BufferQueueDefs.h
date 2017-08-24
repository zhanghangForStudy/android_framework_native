/*
 * Copyright 2014 The Android Open Source Project
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

#ifndef ANDROID_GUI_BUFFERQUEUECOREDEFS_H
#define ANDROID_GUI_BUFFERQUEUECOREDEFS_H

#include <gui/BufferSlot.h>

namespace android {
    class BufferQueueCore;

    namespace BufferQueueDefs {
        // BufferQueue will keep track of at most this value of buffers.
        // Attempts at runtime to increase the number of buffers past this
        // will fail.
        // BufferQueue将保持对缓存的此值进行追踪。
        // 试图在运行期期间，增加buffer数组的长度，将会失败
        // 早期的版本中是32？
        enum { NUM_BUFFER_SLOTS = 64 };

        // 定义BufferSlot[NUM_BUFFER_SLOTS]的别名为SlotsType
        typedef BufferSlot SlotsType[NUM_BUFFER_SLOTS];
    } // namespace BufferQueueDefs
} // namespace android

#endif
