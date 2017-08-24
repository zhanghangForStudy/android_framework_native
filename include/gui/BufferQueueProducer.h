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

#ifndef ANDROID_GUI_BUFFERQUEUEPRODUCER_H
#define ANDROID_GUI_BUFFERQUEUEPRODUCER_H

#include <gui/BufferQueueDefs.h>
#include <gui/IGraphicBufferProducer.h>

namespace android {

class BufferSlot;

class BufferQueueProducer : public BnGraphicBufferProducer,
                            private IBinder::DeathRecipient {
public:
    friend class BufferQueue; // Needed to access binderDied

    BufferQueueProducer(const sp<BufferQueueCore>& core);
    virtual ~BufferQueueProducer();

    // requestBuffer returns the GraphicBuffer for slot N.
    //
    // In normal operation, this is called the first time slot N is returned
    // by dequeueBuffer.  It must be called again if dequeueBuffer returns
    // flags indicating that previously-returned buffers are no longer valid.
    //
    // requestBuffer方法为给定的槽位，返回其对应的GraphicBuffer对象。
    // 在一般的操作之中，在槽位第一次通过dequeueBuffer方法返回的时候，此方法被调用。
    // 而当dequeueBuffer方法返回了一个标识，表明之前返回的buffer对象不在有效的时候，
    // 此方法必须被再次调用。
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf);

    // see IGraphicsBufferProducer::setMaxDequeuedBufferCount
    virtual status_t setMaxDequeuedBufferCount(int maxDequeuedBuffers);

    // see IGraphicsBufferProducer::setAsyncMode
    virtual status_t setAsyncMode(bool async);

    // dequeueBuffer gets the next buffer slot index for the producer to use.
    // If a buffer slot is available then that slot index is written to the
    // location pointed to by the buf argument and a status of OK is returned.
    // If no slot is available then a status of -EBUSY is returned and buf is
    // unmodified.
    //
    // The outFence parameter will be updated to hold the fence associated with
    // the buffer. The contents of the buffer must not be overwritten until the
    // fence signals. If the fence is Fence::NO_FENCE, the buffer may be
    // written immediately.
    //
    // The width and height parameters must be no greater than the minimum of
    // GL_MAX_VIEWPORT_DIMS and GL_MAX_TEXTURE_SIZE (see: glGetIntegerv).
    // An error due to invalid dimensions might not be reported until
    // updateTexImage() is called.  If width and height are both zero, the
    // default values specified by setDefaultBufferSize() are used instead.
    //
    // If the format is 0, the default format will be used.
    //
    // The usage argument specifies gralloc buffer usage flags.  The values
    // are enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER.  These
    // will be merged with the usage flags specified by setConsumerUsageBits.
    //
    // The return value may be a negative error value or a non-negative
    // collection of flags.  If the flags are set, the return values are
    // valid, but additional actions must be performed.
    //
    // If IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION is set, the
    // producer must discard cached GraphicBuffer references for the slot
    // returned in buf.
    // If IGraphicBufferProducer::RELEASE_ALL_BUFFERS is set, the producer
    // must discard cached GraphicBuffer references for all slots.
    //
    // In both cases, the producer will need to call requestBuffer to get a
    // GraphicBuffer handle for the returned slot.
    //
    // dequeueBuffer方法获取下一个供生产者使用的buffer槽位索引。
    // 如果一个槽位可用，则具体的槽位索引将被写入outSlot参数指向的地址，并且一个OK的状态被返回。
    // 如果没有槽位可用，则-EBUSY被返回，并且outSlot不被修改。
    // outFence参数将被更新，以持有与此buffer相关的栅栏。buffer对象的内容不准覆盖，除非此栅栏
    // 发送了一个信号。如果此fence是Fence::NO_FENCE的，则此buffer对象可用立即写入(内容)。
    // width和height参数小于或者等于GL_MAX_VIEWPORT_DIMS 与 GL_MAX_TEXTURE_SIZE两值中的最小值。
    // 由于无效尺寸引起的一个错误，可能不会被抛出，除非updateTexImage()方法被调用了。如果width与height
    // 两个值都为0，则有setDefaultBufferSize()方法指定的默认值，将被使用。
    //
    // 如果format参数为0，则默认的格式将被使用。
    //
    // usage参数指定了gralloca buffer对象的用途标志。这些值在gralloc.h文件中被枚举。
    //
    // 返回值可能是一个负值，表明一个错误；或者一个非负数，表示一个标志的集合。如果相关标志被设置了，则返回值是有效的，
    // 但是额外的动作必须被执行。
    // 如果GraphicBufferProducer::BUFFER_NEEDS_REALLOCATION被设置了，则必须清除生产者对返回的槽位中的GraphicBuffer对象引用的
    // 缓存。
    // 如果IGraphicBufferProducer::RELEASE_ALL_BUFFERS被设置了，则生产者必须删除对所有槽位的引用。
    // 在这两种情况下，生产者都将需要调用requestBuffer方法，用以获取返回的槽位对应的GraphicBuffer对象的一个句柄。
    virtual status_t dequeueBuffer(int *outSlot, sp<Fence>* outFence,
            uint32_t width, uint32_t height, PixelFormat format,
            uint32_t usage);

    // See IGraphicBufferProducer::detachBuffer
    virtual status_t detachBuffer(int slot);

    // See IGraphicBufferProducer::detachNextBuffer
    virtual status_t detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence);

    // See IGraphicBufferProducer::attachBuffer
    virtual status_t attachBuffer(int* outSlot, const sp<GraphicBuffer>& buffer);

    // queueBuffer returns a filled buffer to the BufferQueue.
    //
    // Additional data is provided in the QueueBufferInput struct.  Notably,
    // a timestamp must be provided for the buffer. The timestamp is in
    // nanoseconds, and must be monotonically increasing. Its other semantics
    // (zero point, etc) are producer-specific and should be documented by the
    // producer.
    //
    // The caller may provide a fence that signals when all rendering
    // operations have completed.  Alternatively, NO_FENCE may be used,
    // indicating that the buffer is ready immediately.
    //
    // Some values are returned in the output struct: the current settings
    // for default width and height, the current transform hint, and the
    // number of queued buffers.
    //
    // queueBuffer方法将一个填充好的buffer对象返回给BufferQueue对象。
    //
    // 额外的数据将在QueueBufferInput类中被提供。特别的，一个时间戳必须被提供。
    // 此时间戳是纳秒级的，并且单调递增。它的其他含义（例如0点等）是由生产者指定的
    // 并且应该由生产者描述。
    //
    // 调用者可以提供一个栅栏，此栅栏将会在所有的渲染操作完成时，发送一个信号。
    // 除此之外，NO_FENCE也可以被使用，用来表示此buffer对象是立即准备的。
    //
    // 一些值将被通过output结构体被返回：默认宽高相关的当前设置，当前的转换点，
    // 以及入队的buffer对象的号码。
    virtual status_t queueBuffer(int slot,
            const QueueBufferInput& input, QueueBufferOutput* output);

    // cancelBuffer returns a dequeued buffer to the BufferQueue, but doesn't
    // queue it for use by the consumer.
    //
    // The buffer will not be overwritten until the fence signals.  The fence
    // will usually be the one obtained from dequeueBuffer.
    // cancelBuffer方法将一个已经出队的buffer对象返回给BufferQueue对象，但是不需要入队此buffer对象。
    // 此buffer的内容将不能重写，除非栅栏发出了一个信号；一般而言，栅栏对象与从dequeueBuffer方法获取的
    // 栅栏是一致的
    virtual status_t cancelBuffer(int slot, const sp<Fence>& fence);

    // Query native window attributes.  The "what" values are enumerated in
    // window.h (e.g. NATIVE_WINDOW_FORMAT).
    virtual int query(int what, int* outValue);

    // connect attempts to connect a producer API to the BufferQueue.  This
    // must be called before any other IGraphicBufferProducer methods are
    // called except for getAllocator.  A consumer must already be connected.
    //
    // This method will fail if connect was previously called on the
    // BufferQueue and no corresponding disconnect call was made (i.e. if
    // it's still connected to a producer).
    //
    // APIs are enumerated in window.h (e.g. NATIVE_WINDOW_API_CPU).
    //
    // connect方法试图将一个生产者连接至BufferQueue对象。
    // 此方法的调用，必须在除了getAllocator方法之外的其他方法调用前，被调用。
    // 一个消耗者必须已经连接了。
    // 如果connect方法之前被调用了，并且没有相应的disconnect方法被调用，
    // 那么再次调用connect方法，将会引起一个失败。
    // api参数的值在window.h文件中被枚举。
    virtual status_t connect(const sp<IProducerListener>& listener,
            int api, bool producerControlledByApp, QueueBufferOutput* output);

    // See IGraphicBufferProducer::disconnect
    virtual status_t disconnect(int api, DisconnectMode mode = DisconnectMode::Api);

    // Attaches a sideband buffer stream to the IGraphicBufferProducer.
    //
    // A sideband stream is a device-specific mechanism for passing buffers
    // from the producer to the consumer without using dequeueBuffer/
    // queueBuffer. If a sideband stream is present, the consumer can choose
    // whether to acquire buffers from the sideband stream or from the queued
    // buffers.
    //
    // Passing NULL or a different stream handle will detach the previous
    // handle if any.
    virtual status_t setSidebandStream(const sp<NativeHandle>& stream);

    // See IGraphicBufferProducer::allocateBuffers
    virtual void allocateBuffers(uint32_t width, uint32_t height,
            PixelFormat format, uint32_t usage);

    // See IGraphicBufferProducer::allowAllocation
    virtual status_t allowAllocation(bool allow);

    // See IGraphicBufferProducer::setGenerationNumber
    virtual status_t setGenerationNumber(uint32_t generationNumber);

    // See IGraphicBufferProducer::getConsumerName
    virtual String8 getConsumerName() const override;

    // See IGraphicBufferProducer::setSharedBufferMode
    virtual status_t setSharedBufferMode(bool sharedBufferMode) override;

    // See IGraphicBufferProducer::setAutoRefresh
    virtual status_t setAutoRefresh(bool autoRefresh) override;

    // See IGraphicBufferProducer::setDequeueTimeout
    virtual status_t setDequeueTimeout(nsecs_t timeout) override;

    // See IGraphicBufferProducer::getLastQueuedBuffer
    virtual status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence, float outTransformMatrix[16]) override;

    // See IGraphicBufferProducer::getFrameTimestamps
    virtual bool getFrameTimestamps(uint64_t frameNumber,
            FrameTimestamps* outTimestamps) const override;

    // See IGraphicBufferProducer::getUniqueId
    virtual status_t getUniqueId(uint64_t* outId) const override;

private:
    // This is required by the IBinder::DeathRecipient interface
    virtual void binderDied(const wp<IBinder>& who);

    // Returns the slot of the next free buffer if one is available or
    // BufferQueueCore::INVALID_BUFFER_SLOT otherwise
    int getFreeBufferLocked() const;

    // Returns the next free slot if one is available or
    // BufferQueueCore::INVALID_BUFFER_SLOT otherwise
    int getFreeSlotLocked() const;

    // waitForFreeSlotThenRelock finds the oldest slot in the FREE state. It may
    // block if there are no available slots and we are not in non-blocking
    // mode (producer and consumer controlled by the application). If it blocks,
    // it will release mCore->mMutex while blocked so that other operations on
    // the BufferQueue may succeed.
    enum class FreeSlotCaller {
        Dequeue,
        Attach,
    };
    status_t waitForFreeSlotThenRelock(FreeSlotCaller caller, int* found) const;

    sp<BufferQueueCore> mCore;

    // This references mCore->mSlots. Lock mCore->mMutex while accessing.
    BufferQueueDefs::SlotsType& mSlots;

    // This is a cached copy of the name stored in the BufferQueueCore.
    // It's updated during connect and dequeueBuffer (which should catch
    // most updates).
    String8 mConsumerName;

    uint32_t mStickyTransform;

    // This saves the fence from the last queueBuffer, such that the
    // next queueBuffer call can throttle buffer production. The prior
    // queueBuffer's fence is not nessessarily available elsewhere,
    // since the previous buffer might have already been acquired.
    sp<Fence> mLastQueueBufferFence;

    Rect mLastQueuedCrop;
    uint32_t mLastQueuedTransform;

    // Take-a-ticket system for ensuring that onFrame* callbacks are called in
    // the order that frames are queued. While the BufferQueue lock
    // (mCore->mMutex) is held, a ticket is retained by the producer. After
    // dropping the BufferQueue lock, the producer must wait on the condition
    // variable until the current callback ticket matches its retained ticket.
    Mutex mCallbackMutex;
    int mNextCallbackTicket; // Protected by mCore->mMutex
    int mCurrentCallbackTicket; // Protected by mCallbackMutex
    Condition mCallbackCondition;

    // Sets how long dequeueBuffer or attachBuffer will block if a buffer or
    // slot is not yet available.
    nsecs_t mDequeueTimeout;

}; // class BufferQueueProducer

} // namespace android

#endif
