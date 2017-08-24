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

#ifndef ANDROID_GUI_BUFFERQUEUECONSUMER_H
#define ANDROID_GUI_BUFFERQUEUECONSUMER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <gui/BufferQueueDefs.h>
#include <gui/IGraphicBufferConsumer.h>

namespace android {

class BufferQueueCore;

class BufferQueueConsumer : public BnGraphicBufferConsumer {

public:
    BufferQueueConsumer(const sp<BufferQueueCore>& core);
    virtual ~BufferQueueConsumer();

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the BufferQueue. If no buffer is pending then it returns
    // NO_BUFFER_AVAILABLE. If a buffer is successfully acquired, the
    // information about the buffer is returned in BufferItem.  If the buffer
    // returned had previously been acquired then the BufferItem::mGraphicBuffer
    // field of buffer is set to NULL and it is assumed that the consumer still
    // holds a reference to the buffer.
    //
    // If expectedPresent is nonzero, it indicates the time when the buffer
    // will be displayed on screen. If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    //
    // acquireBuffer方法视图获取BufferQueue对象中，下一个缓冲区对象的控制权。
    // 如果没有缓冲区是即将可用的，则此方法将返回NO_BUFFER_AVAILABLE。
    // 如果一个缓冲区被成功获取，则关于此缓冲区的信息将存储在BufferItem对象之中被返回。
    // 如果返回的缓冲区之前已经被获取了，则缓冲区的mGraphicBuffer属性将被设置为NULL
    // 并且它假定消费者将依旧持有此缓冲区的一个引用。
    // 如果expectedPresent不为0，则它表示缓冲区被展示到屏幕之上的时间。
    // 如果此缓冲区的时间戳是未来的未来，则此缓冲区将无法被获取，并且将返回PRESENT_LATER。
    // expectedPresent是纳秒级别的，并且时间是基于CLOCK_MONOTONIC。
    virtual status_t acquireBuffer(BufferItem* outBuffer,
            nsecs_t expectedPresent, uint64_t maxFrameNumber = 0) override;

    // See IGraphicBufferConsumer::detachBuffer
    virtual status_t detachBuffer(int slot);

    // See IGraphicBufferConsumer::attachBuffer
    virtual status_t attachBuffer(int* slot, const sp<GraphicBuffer>& buffer);

    // releaseBuffer releases a buffer slot from the consumer back to the
    // BufferQueue.  This may be done while the buffer's contents are still
    // being accessed.  The fence will signal when the buffer is no longer
    // in use. frameNumber is used to indentify the exact buffer returned.
    //
    // If releaseBuffer returns STALE_BUFFER_SLOT, then the consumer must free
    // any references to the just-released buffer that it might have, as if it
    // had received a onBuffersReleased() call with a mask set for the released
    // buffer.
    //
    // Note that the dependencies on EGL will be removed once we switch to using
    // the Android HW Sync HAL.
    // releaseBuffer将一个缓冲区槽位释放会BufferQueue对象之中。当缓冲区的内容依旧被访问的时候，
    // 此方法执行的操作也可能完成。fence将在此缓冲区不在使用的时候，发出一个新型号。frameNumber
    // 被用来鉴定被返回的缓冲区对象。
    //
    // 如果releaseBuffer方法返回STALE_BUFFER_SLOT，则消费者必须释放所有的引用。
    // 注意，基于EGL的依赖将被移除，一旦我们切换使用Android硬件同步抽象层。
    virtual status_t releaseBuffer(int slot, uint64_t frameNumber,
            const sp<Fence>& releaseFence, EGLDisplay display,
            EGLSyncKHR fence);

    // connect connects a consumer to the BufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // BufferQueue is placed into the "abandoned" state, causing most
    // interactions with the BufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumerListener may not be NULL.
    // connect方法连接一个消费者到BufferQueue对象之上。只有一个消费者可能被连接。
    // 并且当消费者断开连接的时候，BufferQueue对象将被切换至丢弃状态，而这将引起
    // 大部分生产者与BufferQueue对象的交互失败。
    // @param controlledByApp标识是否消费者被APP控制；
    // @param consumerListener不能为NULL。
    virtual status_t connect(const sp<IConsumerListener>& consumerListener,
            bool controlledByApp);

    // disconnect disconnects a consumer from the BufferQueue. All
    // buffers will be freed and the BufferQueue is placed in the "abandoned"
    // state, causing most interactions with the BufferQueue by the producer to
    // fail.
    // disconnect方法将从BufferQueue对象之中断开一个消费者。此方法将引起所有的缓冲区被释放，并
    // 将BufferQueue对象切换值丢弃状态，而这将引起大部分生产者与BufferQueue对象的交互失败。
    virtual status_t disconnect();

    // getReleasedBuffers sets the value pointed to by outSlotMask to a bit mask
    // indicating which buffer slots have been released by the BufferQueue
    // but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    // getReleasedBuffers方法设置被outSlotMask指向的值，此值是一个bit掩码
    // 用来标识哪一个缓冲区槽位被BufferQueue释放，但是还未被消费者释放。
    //
    // 此方法应该从onBuffersReleased回调之中被调用。
    virtual status_t getReleasedBuffers(uint64_t* outSlotMask);

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    // setDefaultBufferSize方法被用来设置被dequeueBuffer方法返回的缓冲区尺寸，当
    // 一个宽高皆为0的请求发生时。默认的值是1*1.
    virtual status_t setDefaultBufferSize(uint32_t width, uint32_t height);

    // see IGraphicBufferConsumer::setMaxBufferCount
    virtual status_t setMaxBufferCount(int bufferCount);

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1).  This call will
    // fail if a producer is connected to the BufferQueue.
    // setMaxAcquiredBufferCount方法设置，能够被消费者在同一时间下，最大的，可被获取的缓冲区数目（默认为1）.
    // 如果一个生产者被连接至BufferQueue对象，则此方法的调用将会失败
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers);

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name);

    // setDefaultBufferFormat allows the BufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer. The initial default is HAL_PIXEL_FORMAT_RGBA_8888.
    // setDefaultBufferFormat允许BufferQueue对象创建一个默认格式的图形缓冲区，如果
    // 没有格式被指定。初始默认的值为HAL_PIXEL_FORMAT_RGBA_8888
    virtual status_t setDefaultBufferFormat(PixelFormat defaultFormat);

    // setDefaultBufferDataSpace allows the BufferQueue to create
    // GraphicBuffers of a defaultDataSpace if no data space is specified
    // in queueBuffer.
    // The initial default is HAL_DATASPACE_UNKNOWN
    // setDefaultBufferDataSpace允许BufferQueue对象创建一个默认数据空间的图形缓冲区
    // 如果没有数据空间被指定
    // 默认的初始值是HAL_DATASPACE_UNKNOWN
    virtual status_t setDefaultBufferDataSpace(
            android_dataspace defaultDataSpace);

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    // setConsumerUsageBits将为dequeueBuffer方法发动一个额外的用途。
    virtual status_t setConsumerUsageBits(uint32_t usage);

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    // setTransformHint用以旋转缓冲区，以便遮盖图能够不饿使用。
    virtual status_t setTransformHint(uint32_t hint);

    // Retrieve the sideband buffer stream, if any.
    // 如果存在，获取sideband缓冲区流
    virtual sp<NativeHandle> getSidebandStream() const;

    // See IGraphicBufferConsumer::getOccupancyHistory
    virtual status_t getOccupancyHistory(bool forceFlush,
            std::vector<OccupancyTracker::Segment>* outHistory) override;

    // See IGraphicBufferConsumer::discardFreeBuffers
    virtual status_t discardFreeBuffers() override;

    // dump our state in a String
    virtual void dump(String8& result, const char* prefix) const;

    // Functions required for backwards compatibility.
    // These will be modified/renamed in IGraphicBufferConsumer and will be
    // removed from this class at that time. See b/13306289.

    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
            EGLDisplay display, EGLSyncKHR fence,
            const sp<Fence>& releaseFence) {
        return releaseBuffer(buf, frameNumber, releaseFence, display, fence);
    }

    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer,
            bool controlledByApp) {
        return connect(consumer, controlledByApp);
    }

    virtual status_t consumerDisconnect() { return disconnect(); }

    // End functions required for backwards compatibility

private:
    sp<BufferQueueCore> mCore;

    // This references mCore->mSlots. Lock mCore->mMutex while accessing.
    BufferQueueDefs::SlotsType& mSlots;

    // This is a cached copy of the name stored in the BufferQueueCore.
    // It's updated during setConsumerName.
    String8 mConsumerName;

}; // class BufferQueueConsumer

} // namespace android

#endif
