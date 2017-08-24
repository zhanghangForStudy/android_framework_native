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

#ifndef ANDROID_GUI_BUFFERQUEUECORE_H
#define ANDROID_GUI_BUFFERQUEUECORE_H

#include <gui/BufferItem.h>
#include <gui/BufferQueueDefs.h>
#include <gui/BufferSlot.h>
#include <gui/OccupancyTracker.h>

#include <utils/Condition.h>
#include <utils/Mutex.h>
#include <utils/NativeHandle.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/StrongPointer.h>
#include <utils/Trace.h>
#include <utils/Vector.h>

#include <list>
#include <set>

#define BQ_LOGV(x, ...) ALOGV("[%s] " x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGD(x, ...) ALOGD("[%s] " x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGI(x, ...) ALOGI("[%s] " x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGW(x, ...) ALOGW("[%s] " x, mConsumerName.string(), ##__VA_ARGS__)
#define BQ_LOGE(x, ...) ALOGE("[%s] " x, mConsumerName.string(), ##__VA_ARGS__)

#define ATRACE_BUFFER_INDEX(index)                                   \
    if (ATRACE_ENABLED()) {                                          \
        char ___traceBuf[1024];                                      \
        snprintf(___traceBuf, 1024, "%s: %d",                        \
                mCore->mConsumerName.string(), (index));             \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);  \
    }

namespace android {

class IConsumerListener;
class IGraphicBufferAlloc;
class IProducerListener;

class BufferQueueCore : public virtual RefBase {

    friend class BufferQueueProducer;
    friend class BufferQueueConsumer;

public:
    // Used as a placeholder slot number when the value isn't pointing to an
    // existing buffer.
    enum { INVALID_BUFFER_SLOT = BufferItem::INVALID_BUFFER_SLOT };

    // We reserve two slots in order to guarantee that the producer and
    // consumer can run asynchronously.
    enum { MAX_MAX_ACQUIRED_BUFFERS = BufferQueueDefs::NUM_BUFFER_SLOTS - 2 };

    enum {
        // The API number used to indicate the currently connected producer
        // 被用来表示当前已连接的生产者的API号码
        CURRENTLY_CONNECTED_API = -1,

        // The API number used to indicate that no producer is connected
        // 被用来标识当前没有生产者被连接的API号码
        NO_CONNECTED_API        = 0,
    };

    // 具体的池子类？
    typedef Vector<BufferItem> Fifo;

    // BufferQueueCore manages a pool of gralloc memory slots to be used by
    // producers and consumers. allocator is used to allocate all the needed
    // gralloc buffers.
    //
    // BufferQueue对象管理一个gralloc内存槽位池子，此池子被生产者与消耗者使用。
    // 而allocator参数指向的IGraphicBufferAlloc对象被用来分配所有的需要的gralloc buffer对象
    BufferQueueCore(const sp<IGraphicBufferAlloc>& allocator = NULL);
    virtual ~BufferQueueCore();

private:
    // Dump our state in a string
    void dump(String8& result, const char* prefix) const;

    // getMinUndequeuedBufferCountLocked returns the minimum number of buffers
    // that must remain in a state other than DEQUEUED. The async parameter
    // tells whether we're in asynchronous mode.
    //
    // getMinUndequeuedBufferCountLocked方法返回必须保留在非DEQUEUED状态下的buffer对象
    // 的最小数目。异步参数来决定是否我们处于一个异步模式之中
    int getMinUndequeuedBufferCountLocked() const;

    // getMinMaxBufferCountLocked returns the minimum number of buffers allowed
    // given the current BufferQueue state. The async parameter tells whether
    // we're in asynchonous mode.
    //
    // getMinMaxBufferCountLocked方法返回在当前BufferQueue的状态下，允许外发的buffer对象
    // 的最小数目
    int getMinMaxBufferCountLocked() const;

    // getMaxBufferCountLocked returns the maximum number of buffers that can be
    // allocated at once. This value depends on the following member variables:
    //
    //     mMaxDequeuedBufferCount
    //     mMaxAcquiredBufferCount
    //     mMaxBufferCount
    //     mAsyncMode
    //     mDequeueBufferCannotBlock
    //
    // Any time one of these member variables is changed while a producer is
    // connected, mDequeueCondition must be broadcast.
    //
    // getMaxBufferCountLocked返回能够马上分配的buffer对象的最大数目。此返回值依赖
    // 于一下几个成员变量：
    //   mMaxDequeuedBufferCount：最大出队数
    //   mMaxAcquiredBufferCount:最大消耗数
    //           mMaxBufferCount:同一个时刻，能够进行分配的buffer对象的最大数量
    //                mAsyncMode:是否处于异步模式之中
    // mDequeueBufferCannotBlock:是否可以非阻塞的出队，即dequeueBuffer方法是否是可阻塞的
    // 任何时候，这些成员变量在一个生产者被连接的时候，发生了改变，mDequeueCondition必须收到一个通知
    int getMaxBufferCountLocked() const;

    // This performs the same computation but uses the given arguments instead
    // of the member variables for mMaxBufferCount, mAsyncMode, and
    // mDequeueBufferCannotBlock.
    int getMaxBufferCountLocked(bool asyncMode,
            bool dequeueBufferCannotBlock, int maxBufferCount) const;

    // clearBufferSlotLocked frees the GraphicBuffer and sync resources for the
    // given slot.
    //
    // clearBufferSlotLocked方法释放给定槽位的GraphicBuffer对象，并同步相应的资源
    void clearBufferSlotLocked(int slot);

    // freeAllBuffersLocked frees the GraphicBuffer and sync resources for
    // all slots, even if they're currently dequeued, queued, or acquired.
    //
    // freeAllBuffersLocked方法释放所有槽位的GraphicBuffer对象，并同步它们的资源；
    // 即便这些槽位当前的状态为出队、入队或者被消耗者获取的
    void freeAllBuffersLocked();

    // discardFreeBuffersLocked releases all currently-free buffers held by the
    // queue, in order to reduce the memory consumption of the queue to the
    // minimum possible without discarding data.
    //
    // discardFreeBuffersLocked方法释放所有被队列持有的，当前正处于自由状态的buffer对象，
    // 以便减少此对队列的内存消耗，从而尽可能的，通过非清除数据的方式，来最小化内存
    void discardFreeBuffersLocked();

    // If delta is positive, makes more slots available. If negative, takes
    // away slots. Returns false if the request can't be met.
    //
    // 如果delta入参是正数，生成更多的可用槽位。如果是负数，则减去相关槽位。
    // 如果请求没有被处理，则返回false
    bool adjustAvailableSlotsLocked(int delta);

    // waitWhileAllocatingLocked blocks until mIsAllocating is false.
    // 此方法阻塞，直到mIsAllocating变为false
    void waitWhileAllocatingLocked() const;

#if DEBUG_ONLY_CODE
    // validateConsistencyLocked ensures that the free lists are in sync with
    // the information stored in mSlots
    void validateConsistencyLocked() const;
#endif

    // mAllocator is the connection to SurfaceFlinger that is used to allocate
    // new GraphicBuffer objects.
    //
    // mAllocator是连接到SurfaceFlinger的，被用来分配新的GraphicBuffer对象
    sp<IGraphicBufferAlloc> mAllocator;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of BufferQueueCore objects. It must be locked whenever any
    // member variable is accessed.
    //
    // mMutex是一个互斥量，它被用来并发访问BufferQueueCore对象的成员变量。
    // 任何成员变量无论合适，在被访问的时候，都需要锁住此互斥量
    mutable Mutex mMutex;

    // mIsAbandoned indicates that the BufferQueue will no longer be used to
    // consume image buffers pushed to it using the IGraphicBufferProducer
    // interface. It is initialized to false, and set to true in the
    // consumerDisconnect method. A BufferQueue that is abandoned will return
    // the NO_INIT error from all IGraphicBufferProducer methods capable of
    // returning an error.
    //
    // mIsAbandoned属性标识，BufferQueue将不在被用来处理，通过IGraphicBufferProducer
    // 接口推送过来的图片缓冲区了。
    // 该属性的初始值是false，并且在consumerDisconnect方法中设置为true.
    // 如果一个BufferQueue对象处于丢弃状态，那么所有来自于IGraphicBufferProducer接口，
    // 并且拥有一个返回错误的能力的方法，都将返回一个NO_INIT错误。
    bool mIsAbandoned;

    // mConsumerControlledByApp indicates whether the connected consumer is
    // controlled by the application.
    // mConsumerControlledByApp属性标识是否已连接的消耗者，是由application控制的
    bool mConsumerControlledByApp;

    // mConsumerName is a string used to identify the BufferQueue in log
    // messages. It is set by the IGraphicBufferConsumer::setConsumerName
    // method.
    String8 mConsumerName;

    // mConsumerListener is used to notify the connected consumer of
    // asynchronous events that it may wish to react to. It is initially
    // set to NULL and is written by consumerConnect and consumerDisconnect.
    // mConsumerListener属性用来将一些，被当前被连接的消耗者感兴趣的异步事件，
    // 发送给消耗者。此属性初始值为null，并且它被consumerConnect和consumerDisconnect
    // 修改
    sp<IConsumerListener> mConsumerListener;

    // mConsumerUsageBits contains flags that the consumer wants for
    // GraphicBuffers.
    uint32_t mConsumerUsageBits;

    // mConnectedApi indicates the producer API that is currently connected
    // to this BufferQueue. It defaults to NO_CONNECTED_API, and gets updated
    // by the connect and disconnect methods.
    //
    // mConnectedApi属性标识当前连接到BufferQueue上的生产者的API。
    // 此属性值默认为NO_CONNECTED_API，并且通过connect方法和disconnect方法来
    // 进行更新
    int mConnectedApi;
    // PID of the process which last successfully called connect(...)
    // 上一个调用connect方法的进程ID
    pid_t mConnectedPid;

    // mConnectedProducerToken is used to set a binder death notification on
    // the producer.
    //
    // mConnectedProducerToken被用来设置一个生产者的binder死亡监听器
    sp<IProducerListener> mConnectedProducerListener;

    // mSlots is an array of buffer slots that must be mirrored on the producer
    // side. This allows buffer ownership to be transferred between the producer
    // and consumer without sending a GraphicBuffer over Binder. The entire
    // array is initialized to NULL at construction time, and buffers are
    // allocated for a slot when requestBuffer is called with that slot's index.
    //
    // typedef BufferSlot SlotsType[NUM_BUFFER_SLOTS]
    // mSlots是一个buffer槽位数组，此数组必须是生产者那边的槽位数组的一个镜像。
    // 此槽位数组允许buffer的使用权在生产者和消耗者之间转换，而在这个转换过程之中
    // 不需要在binder机制之中，传递GraphicBuffer对象。
    // 整个数组在构造器阶段，被初始化为null，并且在requestBuffer被调用的时候，
    // 才会分配相关槽位上的buffer对象
    BufferQueueDefs::SlotsType mSlots;

    // mQueue is a FIFO of queued buffers used in synchronous mode.
    //
    // mQueue是一个在同步模式下使用的，包含buffer对象的先进先出队列
    Fifo mQueue;

    // mFreeSlots contains all of the slots which are FREE and do not currently
    // have a buffer attached.
    std::set<int> mFreeSlots;

    // mFreeBuffers contains all of the slots which are FREE and currently have
    // a buffer attached.
    //
    // mFreeBuffers属性包含了所有当前自由，且粘合了一个buffer对象的槽位(的索引)
    std::list<int> mFreeBuffers;

    // mUnusedSlots contains all slots that are currently unused. They should be
    // free and not have a buffer attached.
    //
    // mUnusedSlots包含所有的当前未被使用的槽位(的索引)。
    // 这些槽位应该被释放，并且不应该有一个粘合的buffer对象。
    std::list<int> mUnusedSlots;

    // mActiveBuffers contains all slots which have a non-FREE buffer attached.
    // mActiveBuffers包含所有，应有一个非自由的buffer对象的槽位(的索引)
    std::set<int> mActiveBuffers;

    // mDequeueCondition is a condition variable used for dequeueBuffer in
    // synchronous mode.
    //
    //mDequeueCondition属性由dequeueBuffer方法在同步模式下使用
    mutable Condition mDequeueCondition;

    // mDequeueBufferCannotBlock indicates whether dequeueBuffer is allowed to
    // block. This flag is set during connect when both the producer and
    // consumer are controlled by the application.
    //
    // mDequeueBufferCannotBlock属性标识是否dequeueBuffer方法允许阻塞。
    bool mDequeueBufferCannotBlock;

    // mDefaultBufferFormat can be set so it will override the buffer format
    // when it isn't specified in dequeueBuffer.
    PixelFormat mDefaultBufferFormat;

    // mDefaultWidth holds the default width of allocated buffers. It is used
    // in dequeueBuffer if a width and height of 0 are specified.
    uint32_t mDefaultWidth;

    // mDefaultHeight holds the default height of allocated buffers. It is used
    // in dequeueBuffer if a width and height of 0 are specified.
    uint32_t mDefaultHeight;

    // mDefaultBufferDataSpace holds the default dataSpace of queued buffers.
    // It is used in queueBuffer if a dataspace of 0 (HAL_DATASPACE_UNKNOWN)
    // is specified.
    android_dataspace mDefaultBufferDataSpace;

    // mMaxBufferCount is the limit on the number of buffers that will be
    // allocated at one time. This limit can be set by the consumer.
    //
    // mMaxBufferCount属性限制同一个时刻，能够进行分配的buffer对象的最大数量
    int mMaxBufferCount;

    // mMaxAcquiredBufferCount is the number of buffers that the consumer may
    // acquire at one time. It defaults to 1, and can be changed by the consumer
    // via setMaxAcquiredBufferCount, but this may only be done while no
    // producer is connected to the BufferQueue. This value is used to derive
    // the value returned for the MIN_UNDEQUEUED_BUFFERS query to the producer.
    //
    // mMaxAcquiredBufferCount属性表示消耗者可以在同一时刻，获取的buffer对象的数目。
    // 默认为1，并且可以由消耗者通过setMaxAcquiredBufferCount方法来进行改变，但是此操作
    // 只能在没有生产者连接到此BufferQueue对象的时候，才能执行。
    int mMaxAcquiredBufferCount;

    // mMaxDequeuedBufferCount is the number of buffers that the producer may
    // dequeue at one time. It defaults to 1, and can be changed by the producer
    // via setMaxDequeuedBufferCount.
    //
    // mMaxDequeuedBufferCount表示生产者可以在同一时刻出队的buffer对象数量。默认为1，
    // 并且可以由生产者通过setMaxDequeuedBufferCount方法来进行改变
    int mMaxDequeuedBufferCount;

    // mBufferHasBeenQueued is true once a buffer has been queued. It is reset
    // when something causes all buffers to be freed (e.g., changing the buffer
    // count).
    // 一旦有一个buffer对象入队了，则mBufferHasBeenQueued的值为true.
    // 而一旦有引起所有buffer对象自由化的事件发生了，则此属性值将被重置
    bool mBufferHasBeenQueued;

    // mFrameCounter is the free running counter, incremented on every
    // successful queueBuffer call and buffer allocation.
    //
    // mFrameCounter属性是一个自由运行的计数器，每次入队调用和buffer分配的时候，
    // 就会增加此计数器
    uint64_t mFrameCounter;

    // mTransformHint is used to optimize for screen rotations.
    //
    // mTransformHint被用来优化屏幕旋转
    uint32_t mTransformHint;

    // mSidebandStream is a handle to the sideband buffer stream, if any
    sp<NativeHandle> mSidebandStream;

    // mIsAllocating indicates whether a producer is currently trying to allocate buffers (which
    // releases mMutex while doing the allocation proper). Producers should not modify any of the
    // FREE slots while this is true. mIsAllocatingCondition is signaled when this value changes to
    // false.
    //
    // mIsAllocating标识是否一个生产者当前正在尝试分配buffer对象（当分配操作完全完成的时候，会释放mMutex互斥量）。
    // 生产者不应该修改任何空闲的槽位，当此属性值为true的时候。当此值变为false的时候，mIsAllocatingCondition
    // 应该收到一个相应的信号
    bool mIsAllocating;

    // mIsAllocatingCondition is a condition variable used by producers to wait until mIsAllocating
    // becomes false.
    // 生产者使用此属性来等待mIsAllocating属性变为false.
    mutable Condition mIsAllocatingCondition;

    // mAllowAllocation determines whether dequeueBuffer is allowed to allocate
    // new buffers
    //
    // mAllowAllocation决定是否dequeueBuffer方法被允许分配新的buffer对象
    bool mAllowAllocation;

    // mBufferAge tracks the age of the contents of the most recently dequeued
    // buffer as the number of frames that have elapsed since it was last queued
    //
    // mBufferAge追踪最近出队的buffer对象内容的年龄，来作为自从它上一次出队开始，已经错过的帧的次数
    uint64_t mBufferAge;

    // mGenerationNumber stores the current generation number of the attached
    // producer. Any attempt to attach a buffer with a different generation
    // number will fail.
    //
    // mGenerationNumber保存氮气被粘贴的生产者的生成数。任何以一个不同的生成数，来
    // 粘贴一个buffer对象的操作都将失败
    uint32_t mGenerationNumber;

    // mAsyncMode indicates whether or not async mode is enabled.
    // In async mode an extra buffer will be allocated to allow the producer to
    // enqueue buffers without blocking.
    //
    // mAsyncMode标识异步模式是否被启用。在异步模式之中一个额外的buffer对象将被分配，用以
    // 允许生产者非阻塞的入队buffer对象
    bool mAsyncMode;

    // mSharedBufferMode indicates whether or not shared buffer mode is enabled.
    //
    // mSharedBufferMode标识是否共享buffer对象模式被启用
    bool mSharedBufferMode;

    // When shared buffer mode is enabled, this indicates whether the consumer
    // should acquire buffers even if BufferQueue doesn't indicate that they are
    // available.
    //
    // 当共享buffer模式被启用了，此属性标识是否消耗这应该获取buffer对象，即便BufferQueue
    // 还没有标识它们是可用的
    bool mAutoRefresh;

    // When shared buffer mode is enabled, this tracks which slot contains the
    // shared buffer.
    // 当共享buffer模式被启用了，此属性追踪那一个槽位包含共享buffer对象
    int mSharedBufferSlot;

    // Cached data about the shared buffer in shared buffer mode
    // 在共享buffer模式下，关于共享buffer的缓存数据
    struct SharedBufferCache {
        SharedBufferCache(Rect _crop, uint32_t _transform, int _scalingMode,
                android_dataspace _dataspace)
        : crop(_crop),
          transform(_transform),
          scalingMode(_scalingMode),
          dataspace(_dataspace) {
        };

        Rect crop;
        uint32_t transform;
        uint32_t scalingMode;
        android_dataspace dataspace;
    } mSharedBufferCache;

    // The slot of the last queued buffer
    //
    // 最后入队buffer的槽位
    int mLastQueuedSlot;

    OccupancyTracker mOccupancyTracker;

    const uint64_t mUniqueId;

}; // class BufferQueueCore

} // namespace android

#endif
