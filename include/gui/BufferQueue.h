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

#ifndef ANDROID_GUI_BUFFERQUEUE_H
#define ANDROID_GUI_BUFFERQUEUE_H

#include <gui/BufferItem.h>
#include <gui/BufferQueueDefs.h>
#include <gui/IGraphicBufferConsumer.h>
#include <gui/IGraphicBufferProducer.h>
#include <gui/IConsumerListener.h>

// These are only required to keep other parts of the framework with incomplete
// dependencies building successfully
#include <gui/IGraphicBufferAlloc.h>

namespace android {

class BufferQueue {
public:
    // BufferQueue will keep track of at most this value of buffers.
    // Attempts at runtime to increase the number of buffers past this will fail.
    //
    // BufferQueue对象最多会追踪这么多的buffer对象；
    // 视图在运行期增加buffer对象的数量，将会失败
    enum { NUM_BUFFER_SLOTS = BufferQueueDefs::NUM_BUFFER_SLOTS };
    // Used as a placeholder slot# when the value isn't pointing to an existing buffer.
    //
    // 当具体的值不在指向一个存在的buffer对象的时候，使用一个占位符
    enum { INVALID_BUFFER_SLOT = BufferItem::INVALID_BUFFER_SLOT };
    // Alias to <IGraphicBufferConsumer.h> -- please scope from there in future code!
    enum {
        NO_BUFFER_AVAILABLE = IGraphicBufferConsumer::NO_BUFFER_AVAILABLE,
        PRESENT_LATER = IGraphicBufferConsumer::PRESENT_LATER,
    };

    // When in async mode we reserve two slots in order to guarantee that the
    // producer and consumer can run asynchronously.
    // 当处于异步模式的时候，我们储备两个槽位，用以保证生产者和消耗者能够异步运行。
    enum { MAX_MAX_ACQUIRED_BUFFERS = NUM_BUFFER_SLOTS - 2 };

    // for backward source compatibility
    typedef ::android::ConsumerListener ConsumerListener;

    // ProxyConsumerListener is a ConsumerListener implementation that keeps a weak
    // reference to the actual consumer object.  It forwards all calls to that
    // consumer object so long as it exists.
    //
    // This class exists to avoid having a circular reference between the
    // BufferQueue object and the consumer object.  The reason this can't be a weak
    // reference in the BufferQueue class is because we're planning to expose the
    // consumer side of a BufferQueue as a binder interface, which doesn't support
    // weak references.
    //
    // ProxyConsumerListener实现了ConsumerListener，它将对真正的消耗者对象，保持一个弱引用。
    // 如果真正的消耗者对象存在，则此对象将会把所有的调用转向给真正的消耗者对象。
    //
    // 此类的设计，是为了避免在BufferQueue对象和消耗者独显之间，出现循环引用。
    // 不能在BufferQueue对象之中出现弱引用的原因是因为，我们计划暴露BufferQueue中的消耗者
    // 为一个binder接口，binder接口并不支持弱引用
    class ProxyConsumerListener : public BnConsumerListener {
    public:
        ProxyConsumerListener(const wp<ConsumerListener>& consumerListener);
        virtual ~ProxyConsumerListener();
        virtual void onFrameAvailable(const BufferItem& item) override;
        virtual void onFrameReplaced(const BufferItem& item) override;
        virtual void onBuffersReleased() override;
        virtual void onSidebandStreamChanged() override;
        virtual bool getFrameTimestamps(uint64_t frameNumber,
                FrameTimestamps* outTimestamps) const override;
    private:
        // mConsumerListener is a weak reference to the IConsumerListener.  This is
        // the raison d'etre of ProxyConsumerListener.
        wp<ConsumerListener> mConsumerListener;
    };

    // BufferQueue manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the
    // needed gralloc buffers.
    // BufferQueue对象管理一个gralloc内存槽位池子，此池子被生产者与消耗者使用。
    // 而allocator参数指向的IGraphicBufferAlloc对象被用来分配所有的需要的gralloc buffer对象
    static void createBufferQueue(sp<IGraphicBufferProducer>* outProducer,
            sp<IGraphicBufferConsumer>* outConsumer,
            const sp<IGraphicBufferAlloc>& allocator = NULL);

private:
    BufferQueue(); // Create through createBufferQueue
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_BUFFERQUEUE_H
