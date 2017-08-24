/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_GUI_IGRAPHICBUFFERCONSUMER_H
#define ANDROID_GUI_IGRAPHICBUFFERCONSUMER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Timers.h>

#include <binder/IInterface.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <gui/OccupancyTracker.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace android {
// ----------------------------------------------------------------------------

class BufferItem;
class Fence;
class GraphicBuffer;
class IConsumerListener;
class NativeHandle;

class IGraphicBufferConsumer : public IInterface {

public:
    enum {
        // Returned by releaseBuffer, after which the consumer must
        // free any references to the just-released buffer that it might have.
        // 被releaseBuffer返回，在releaseBuffer方法之后，消耗者必须释放所有的引用，用以释放
        // 此消耗着可能拥有的buffer对象。
        STALE_BUFFER_SLOT = 1,
        // Returned by dequeueBuffer if there are no pending buffers available.
        // 如果releaseBuffer方法中并没有可用的buffer，则返回此值
        NO_BUFFER_AVAILABLE,
        // Returned by dequeueBuffer if it's too early for the buffer to be acquired.
        // 对于此buffer而言，获取此buffer时机太早的话，就会返回此值
        PRESENT_LATER,
    };

    // acquireBuffer attempts to acquire ownership of the next pending buffer in
    // the BufferQueue.  If no buffer is pending then it returns
    // NO_BUFFER_AVAILABLE.  If a buffer is successfully acquired, the
    // information about the buffer is returned in BufferItem.
    //
    // If the buffer returned had previously been
    // acquired then the BufferItem::mGraphicBuffer field of buffer is set to
    // NULL and it is assumed that the consumer still holds a reference to the
    // buffer.
    //
    // If presentWhen is non-zero, it indicates the time when the buffer will
    // be displayed on screen.  If the buffer's timestamp is farther in the
    // future, the buffer won't be acquired, and PRESENT_LATER will be
    // returned.  The presentation time is in nanoseconds, and the time base
    // is CLOCK_MONOTONIC.
    //
    // If maxFrameNumber is non-zero, it indicates that acquireBuffer should
    // only return a buffer with a frame number less than or equal to
    // maxFrameNumber. If no such frame is available (such as when a buffer has
    // been replaced but the consumer has not received the onFrameReplaced
    // callback), then PRESENT_LATER will be returned.
    //
    // Return of NO_ERROR means the operation completed as normal.
    //
    // Return of a positive value means the operation could not be completed
    //    at this time, but the user should try again later:
    // * NO_BUFFER_AVAILABLE - no buffer is pending (nothing queued by producer)
    // * PRESENT_LATER - the buffer's timestamp is farther in the future
    //
    // Return of a negative value means an error has occurred:
    // * INVALID_OPERATION - too many buffers have been acquired
    //
    // acquireBuffer方法试图，在bufferqueue之中获取下一个即将使用的buffer的使用权。
    // 如果没有可立即使用的buffer，则此方法返回NO_BUFFER_AVAILABLE。
    // 如果一个buffer对象被成功获取，则关于此buffer的信息，将会在入参BufferItem之中
    // 被返回。
    //
    // 如果被返回的buffer已经被获取了，则BufferItem类中的mGraphicBuffer属性会被设置为null
    // 并且假定消耗着依然持有此buffer的引用。
    //
    // 如果presentWhen不为0，则它表示buffer即将被展示到屏幕上的事件。如果此buffer的时间戳
    // 在未来的未来，则此buffer将不会允许被获取，同时此方法将返回PRESENT_LATER值。presentWhen
    // 是毫秒级别的。
    // 如果maxFrameNumber参数是非0的，则它表示acquireBuffer方法应该只能返回frame num小于或者等于
    // maxFrameNumber的buffer对象。如果没有这样的frame可用，则PRESENT_LATER将被返回。
    // NO_ERROR值的返回，则表示操作按照一般预料的那样被执行。
    // 一个正数值被返回，则意味着此方法的操作在此时无法完成，用户应该在稍后尝试：
    // NO_BUFFER_AVAILABLE：没有buffer对象是即将可用的（生产者没有入队任何东西）。
    //       PRESENT_LATER：buffer的时间戳，在未来的未来
    // 一个负数的返回，则表示此方法发生了一些错误：
    //   INVALID_OPERATION：请求了太多的buffer对象
    virtual status_t acquireBuffer(BufferItem* buffer, nsecs_t presentWhen,
            uint64_t maxFrameNumber = 0) = 0;

    // detachBuffer attempts to remove all ownership of the buffer in the given
    // slot from the buffer queue. If this call succeeds, the slot will be
    // freed, and there will be no way to obtain the buffer from this interface.
    // The freed slot will remain unallocated until either it is selected to
    // hold a freshly allocated buffer in dequeueBuffer or a buffer is attached
    // to the slot. The buffer must have already been acquired.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - the given slot number is invalid, either because it is
    //               out of the range [0, NUM_BUFFER_SLOTS) or because the slot
    //               it refers to is not currently acquired.
    // detachBuffer试图移除，被给定的，来自于bufferqueue的槽位之中的buffer对象的所有权。
    // 如果此方法调用成功，槽位将被释放，并且将没有任何办法，从此接口之中获取buffer对象。
    // 此被释放调的槽位将继续保持无分配的状态，直到它在dequeueBuffer方法中，被选择持有
    // 一个新鲜的buffer对象；或者一个buffer对象被粘贴到了此槽位之中。
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // BAD_VALUE:被改订的槽位号是无效的，无效则表示槽位号即可能溢出了[0,NUM_BUFFER_SLOTS)
    //           的范围，或者是因为它引用的槽位当前并没有处于获取状态之中
    virtual status_t detachBuffer(int slot) = 0;

    // attachBuffer attempts to transfer ownership of a buffer to the buffer
    // queue. If this call succeeds, it will be as if this buffer was acquired
    // from the returned slot number. As such, this call will fail if attaching
    // this buffer would cause too many buffers to be simultaneously acquired.
    //
    // If the buffer is successfully attached, its frameNumber is initialized
    // to 0. This must be passed into the releaseBuffer call or else the buffer
    // will be deallocated as stale.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - outSlot or buffer were NULL, or the generation number of
    //               the buffer did not match the buffer queue.
    // * INVALID_OPERATION - cannot attach the buffer because it would cause too
    //                       many buffers to be acquired.
    // * NO_MEMORY - no free slots available
    // attachBuffer方法视图将buffer的所有权，转移到bufferqueue对象之。
    // 如果此方法调用成功，则犹如此buffer对象从被返回的槽位之中获取了。
    // 同样的，如果粘贴此buffer对象将引起过多的buffer对象被同时获取，则此方法的调用将失败。
    // 如果此buffer对象被成功粘贴，则它的frameNumber将被初始化为0.
    // 此值，将一定会被传递给relaseBuffer方法调用，或者其他此buffer对象将被释放的方法调用中。
    // NO_ERROR值的返回，则表示操作按照一般预料的那样被执行。
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // BAD_VALUE：outSlot或者buffer对象为空，或者此buffer对象的生成ID与bufferqueue之中的
    // 生成ID不一致。
    // INVALID_OPERATION:无法粘贴此buffer对象，因为此操作将引起太多的buffer对象被获取。
    // NO_MEMORY:没有可用的槽位
    virtual status_t attachBuffer(int *outSlot,
            const sp<GraphicBuffer>& buffer) = 0;

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
    //
    // Return of NO_ERROR means the operation completed as normal.
    //
    // Return of a positive value means the operation could not be completed
    //    at this time, but the user should try again later:
    // * STALE_BUFFER_SLOT - see above (second paragraph)
    //
    // Return of a negative value means an error has occurred:
    // * BAD_VALUE - one of the following could've happened:
    //               * the buffer slot was invalid
    //               * the fence was NULL
    //               * the buffer slot specified is not in the acquired state
    // releaseBuffer方法从消耗者一端，释放一个buffer槽位给bufferQueue对象。
    // 当buffer对象中的内容依旧被使用的时候，此方法也可能被调用。
    // 而当此buffer对象不在使用的时候，此frence对象将会发送此信号。
    // frameNumber被用来精确的标识buffer对象。
    //
    // 如果releaseBuffer方法返回STALE_BUFFER_SLOT，则消耗者必须释放
    // 所有的引用，就好像它收到了一个onBuffersReleased回调。
    //
    // 注意，一旦我们切换使用android硬件同步，则关于EGL的依赖将被移除。
    //
    // NO_ERROR值的返回，则表示操作按照一般预料的那样被执行。
    //
    // 一个负值的返回意味着一个错误的发生：
    // BAD_VALUE : 下面的情况，发生了：
    //              buffer槽位是无效的；
    //              fence是空的；
    //              buffer槽位并没有被指定为获取状态
    virtual status_t releaseBuffer(int buf, uint64_t frameNumber,
            EGLDisplay display, EGLSyncKHR fence,
            const sp<Fence>& releaseFence) = 0;

    // consumerConnect connects a consumer to the BufferQueue.  Only one
    // consumer may be connected, and when that consumer disconnects the
    // BufferQueue is placed into the "abandoned" state, causing most
    // interactions with the BufferQueue by the producer to fail.
    // controlledByApp indicates whether the consumer is controlled by
    // the application.
    //
    // consumer may not be NULL.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned
    // * BAD_VALUE - a NULL consumer was provided
    //
    // consumerConnect连接一个消耗着到BufferQueue对象之中。
    // 只有一个消耗者可以被连接，而当此消耗者断开连接时，BufferQueue对象被移至丢弃状态，
    // 而此状态将会引起大部分生产者与BufferQueue对象的交互失败。
    // controlledByApp标识是否此消耗者被应用端控制。
    // consumer参数不可能为NULL。
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // NO_INIT : BufferQueue对象被丢弃了
    // BAD_VALUE：一个空的消耗者被提供
    virtual status_t consumerConnect(const sp<IConsumerListener>& consumer, bool controlledByApp) = 0;

    // consumerDisconnect disconnects a consumer from the BufferQueue. All
    // buffers will be freed and the BufferQueue is placed in the "abandoned"
    // state, causing most interactions with the BufferQueue by the producer to
    // fail.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - no consumer is currently connected
    // consumerDisconnect方法将断开一个消耗者与BufferQueue对象的连接。
    // 所有的buffer对象将被释放并且BufferQueue对象将被转移至丢弃状态，
    // 而此状态将会引起大部分生产者与BufferQueue对象的交互失败。
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // BAD_VALUE：当前并没有消耗者被连接
    virtual status_t consumerDisconnect() = 0;

    // getReleasedBuffers sets the value pointed to by slotMask to a bit set.
    // Each bit index with a 1 corresponds to a released buffer slot with that
    // index value.  In particular, a released buffer is one that has
    // been released by the BufferQueue but have not yet been released by the consumer.
    //
    // This should be called from the onBuffersReleased() callback.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned.
    // getReleasedBuffers方法将被slotMask指向的值到一个bit集合之上。
    // 每个bit位上，1表示一个释放了的buffer槽位。
    // 特别的，一个释放的buffer对象，是指一个被bufferqueue对象释放了的，但是还没有
    // 被消耗者释放的buffer对象。
    //
    // 此方法应该在onBuffersReleased方法之中被回调。
    //
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // NO_INIT:bufferqueue对象被丢弃了。
    virtual status_t getReleasedBuffers(uint64_t* slotMask) = 0;

    // setDefaultBufferSize is used to set the size of buffers returned by
    // dequeueBuffer when a width and height of zero is requested.  Default
    // is 1x1.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - either w or h was zero
    // setDefaultBufferSize被用来设置被bufferqueue对象返回的buffer对象的默认尺寸
    //
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    // BAD_VALUE:要么w参数为0，要么h参数为0
    virtual status_t setDefaultBufferSize(uint32_t w, uint32_t h) = 0;

    // setMaxBufferCount sets the maximum value for the number of buffers used
    // in the buffer queue (the initial default is NUM_BUFFER_SLOTS). If a call
    // to setMaxAcquiredBufferCount (by the consumer), or a call to setAsyncMode
    // or setMaxDequeuedBufferCount (by the producer), would cause this value to
    // be exceeded then that call will fail. This call will fail if a producer
    // is connected to the BufferQueue.
    //
    // The count must be between 1 and NUM_BUFFER_SLOTS, inclusive. The count
    // cannot be less than maxAcquiredBufferCount.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - one of the below conditions occurred:
    //             * bufferCount was out of range (see above).
    //             * failure to adjust the number of available slots.
    // * INVALID_OPERATION - attempting to call this after a producer connected.
    //
    // setMaxBufferCount方法设置了在bufferqueue对象中使用的buffer对象数目最大值。
    // 如果一个来自于消耗者一端的setMaxAcquiredBufferCount调用，或者一个
    // 来至于生产者一端的setMaxDequeuedBufferCount调用或者一个setAsyncMode的调用，
    // 都将引起此值溢出，从而引起此调用的失败。
    // 如果一个生产者被连接到了BufferQueue对象之上，则此调用将会失败。
    //
    // count值必须在1到NUM_BUFFER_SLOTS之间。count值不能小于maxAcquiredBufferCount
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    //         BAD_VALUE:下面情况中的一种发生了：
    //                   bufferCount超出了范围；
    //                   调整可用槽位数失败。
    // INVALID_OPERATION：试图在一个生产者被连接后，调用此方法
    virtual status_t setMaxBufferCount(int bufferCount) = 0;

    // setMaxAcquiredBufferCount sets the maximum number of buffers that can
    // be acquired by the consumer at one time (default 1). If this method
    // succeeds, any new buffer slots will be both unallocated and owned by the
    // BufferQueue object (i.e. they are not owned by the producer or consumer).
    // Calling this may also cause some buffer slots to be emptied.
    //
    // This function should not be called with a value of maxAcquiredBuffers
    // that is less than the number of currently acquired buffer slots. Doing so
    // will result in a BAD_VALUE error.
    //
    // maxAcquiredBuffers must be (inclusive) between 1 and
    // MAX_MAX_ACQUIRED_BUFFERS. It also cannot cause the maxBufferCount value
    // to be exceeded.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned
    // * BAD_VALUE - one of the below conditions occurred:
    //             * maxAcquiredBuffers was out of range (see above).
    //             * failure to adjust the number of available slots.
    //             * client would have more than the requested number of
    //               acquired buffers after this call
    // * INVALID_OPERATION - attempting to call this after a producer connected.
    // setMaxAcquiredBufferCount方法设置，在同一时刻，能够被消耗者获取的buffer对象的最大数量（默认为1.
    // 如果此方法成功了，则任何新的buffer槽位对象将被释放，并且其使用权由BufferQueue对象拥有。
    // 调用此方法，可能引起一些buffer槽位为空。
    // 此方法不应该以少于当前已经被获取的buffer槽位数量的值，为入参进行调用，如果这样做，
    // 将会返回一个BAD_VALUE。
    // maxAcquiredBuffers的范围必须在[1,MAX_MAX_ACQUIRED_BUFFERS)之间。
    //
    // 返回一个与NO_ERROR不同的值，则意味这一个错误发生了：
    //   NO_INIT：BufferQueue对象处于被丢弃状态；
    // BAD_VALUE:发生了下面任意一种情况：
    //          maxAcquiredBuffers溢出；
    //          调整可用槽位数量失败；
    //          当调用了此方法之后，客户端拥有的可用buffer对象数，大于了调整后的maxAcquiredBuffers值
    virtual status_t setMaxAcquiredBufferCount(int maxAcquiredBuffers) = 0;

    // setConsumerName sets the name used in logging
    virtual void setConsumerName(const String8& name) = 0;

    // setDefaultBufferFormat allows the BufferQueue to create
    // GraphicBuffers of a defaultFormat if no format is specified
    // in dequeueBuffer.
    // The initial default is PIXEL_FORMAT_RGBA_8888.
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    // setDefaultBufferFormat方法允许BufferQueue以一个默认的buffer格式，来创建GraphicBuffers，
    // 如果在dequeueBuffer的时候，没有指定具体的格式；
    // 初始的默认值为PIXEL_FORMAT_RGBA_8888
    //
    // 返回一个与NO_ERROR不同的值，则意味着一个未知错误发生了
    virtual status_t setDefaultBufferFormat(PixelFormat defaultFormat) = 0;

    // setDefaultBufferDataSpace is a request to the producer to provide buffers
    // of the indicated dataSpace. The producer may ignore this request.
    // The initial default is HAL_DATASPACE_UNKNOWN.
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    // setDefaultBufferDataSpace方法是一个让生产者提供标识数据空间的请求。
    // 但是生产者可能忽略这个请求。
    virtual status_t setDefaultBufferDataSpace(
            android_dataspace defaultDataSpace) = 0;

    // setConsumerUsageBits will turn on additional usage bits for dequeueBuffer.
    // These are merged with the bits passed to dequeueBuffer.  The values are
    // enumerated in gralloc.h, e.g. GRALLOC_USAGE_HW_RENDER; the default is 0.
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    //
    // setConsumerUsageBits方法将为deuqueBuffer方法，开启一个额外的用途bit集合。
    // 这些值，通过bit位集进行合并，并传递给dequeueBuffer方法。
    // 这些值，在gralloc.h文件中被枚举。
    //
    // 返回一个与NO_ERROR不同的值，则意味着一个未知错误发生了
    virtual status_t setConsumerUsageBits(uint32_t usage) = 0;

    // setTransformHint bakes in rotation to buffers so overlays can be used.
    // The values are enumerated in window.h, e.g.
    // NATIVE_WINDOW_TRANSFORM_ROT_90.  The default is 0 (no transform).
    //
    // Return of a value other than NO_ERROR means an unknown error has occurred.
    virtual status_t setTransformHint(uint32_t hint) = 0;

    // Retrieve the sideband buffer stream, if any.
    virtual sp<NativeHandle> getSidebandStream() const = 0;

    // Retrieves any stored segments of the occupancy history of this
    // BufferQueue and clears them. Optionally closes out the pending segment if
    // forceFlush is true.
    virtual status_t getOccupancyHistory(bool forceFlush,
            std::vector<OccupancyTracker::Segment>* outHistory) = 0;

    // discardFreeBuffers releases all currently-free buffers held by the queue,
    // in order to reduce the memory consumption of the queue to the minimum
    // possible without discarding data.
    virtual status_t discardFreeBuffers() = 0;

    // dump state into a string
    virtual void dump(String8& result, const char* prefix) const = 0;

public:
    DECLARE_META_INTERFACE(GraphicBufferConsumer);
};

// ----------------------------------------------------------------------------

class BnGraphicBufferConsumer : public BnInterface<IGraphicBufferConsumer>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_IGRAPHICBUFFERCONSUMER_H
