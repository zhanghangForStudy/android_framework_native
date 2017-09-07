/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef ANDROID_GUI_SURFACE_H
#define ANDROID_GUI_SURFACE_H

#include <gui/IGraphicBufferProducer.h>
#include <gui/BufferQueue.h>

#include <ui/ANativeObjectBase.h>
#include <ui/Region.h>

#include <binder/Parcelable.h>

#include <utils/RefBase.h>
#include <utils/threads.h>
#include <utils/KeyedVector.h>

struct ANativeWindow_Buffer;

namespace android {

/*
 * An implementation of ANativeWindow that feeds graphics buffers into a
 * BufferQueue.
 *
 * This is typically used by programs that want to render frames through
 * some means (maybe OpenGL, a software renderer, or a hardware decoder)
 * and have the frames they create forwarded to SurfaceFlinger for
 * compositing.  For example, a video decoder could render a frame and call
 * eglSwapBuffers(), which invokes ANativeWindow callbacks defined by
 * Surface.  Surface then forwards the buffers through Binder IPC
 * to the BufferQueue's producer interface, providing the new frame to a
 * consumer such as GLConsumer.
 *
 * Surface是一个提供BufferQueue之中的GraphicsBuffer对象的本地窗口实现者/
 *
 * 此类一般而言，是被，期望使用某些方式渲染帧（可能是OpenGL,一个软件渲染器,或者一个硬件解码器）
 * 的程序，使用；同时，此类拥有，被其创建，并传递给SurfaceFlinger合成的帧。例如，一个视频解码器
 * 能够渲染一个帧并调用eglSwapBuffers()方法，eglSwapBuffers()方法能够执行有Surface类定义的，
 * 本地窗口回调。Surface类，还能通过binder跨进程同学，传递buffer对象给BufferQueue的生产者接口，
 * 由此为类似于GLConsumer的消耗者，提供一张新的帧
 *
 *　具体而言，Surface类主要有两大职能：
 *  1.作为应用进程端的本地窗口；
 *  2.管理IGraphicBufferProducer对象
 */
class Surface
    : public ANativeObjectBase<ANativeWindow, Surface, RefBase>
{
public:

    /*
     * creates a Surface from the given IGraphicBufferProducer (which concrete
     * implementation is a BufferQueue).
     *
     * Surface is mainly state-less while it's disconnected, it can be
     * viewed as a glorified IGraphicBufferProducer holder. It's therefore
     * safe to create other Surfaces from the same IGraphicBufferProducer.
     *
     * However, once a Surface is connected, it'll prevent other Surfaces
     * referring to the same IGraphicBufferProducer to become connected and
     * therefore prevent them to be used as actual producers of buffers.
     *
     * the controlledByApp flag indicates that this Surface (producer) is
     * controlled by the application. This flag is used at connect time.
     * 根据给定的IGraphicBufferProducer（具体的实现是BufferQueue）入参，创建一个Surface对象.
     *
     * Surface对象在没连接的时候，是少状态的；它能够被当做一个被优化的IGraphicBufferProducer
     * 对象持有者。因此从一个相同的IGraphicBufferProducer对象上创建其他的surface对象是安全的。
     *
     * 然而，一旦一个surface被连接了，它将阻止其他的surface对象引用此相同的IGraphicBufferProducer对象，
     * 从而它们被当做一个真正的buffer生产者。
     *
     * controlledByApp参数表示此Surface是否被application控制。 此标识在连接的时候，被使用。
     */
    Surface(const sp<IGraphicBufferProducer>& bufferProducer, bool controlledByApp = false);

    /* getIGraphicBufferProducer() returns the IGraphicBufferProducer this
     * Surface was created with. Usually it's an error to use the
     * IGraphicBufferProducer while the Surface is connected.
     *
     * getIGraphicBufferProducer方法返回，此surface创建时，关联的IGraphicBufferProducer对象。
     * 当Surface对象被连接的时候，使用IGraphicBufferProducer对象是一个错误
     */
    sp<IGraphicBufferProducer> getIGraphicBufferProducer() const;

    /* convenience function to check that the given surface is non NULL as
     * well as its IGraphicBufferProducer
     *
     * 检测给定的surface对象，以及它对应的IGraphicBufferProducer对象是否为空，的一个遍历方法
     */
    static bool isValid(const sp<Surface>& surface) {
        return surface != NULL && surface->getIGraphicBufferProducer() != NULL;
    }

    /* Attaches a sideband buffer stream to the Surface's IGraphicBufferProducer.
     *
     * A sideband stream is a device-specific mechanism for passing buffers
     * from the producer to the consumer without using dequeueBuffer/
     * queueBuffer. If a sideband stream is present, the consumer can choose
     * whether to acquire buffers from the sideband stream or from the queued
     * buffers.
     *
     * Passing NULL or a different stream handle will detach the previous
     * handle if any.
     *
     * 向此surface对象的IGraphicBufferProducer对象，粘合一道边界缓冲流。
     *
     * 一道边界缓冲刘是一个特定于设备的机制，用来将buffer对象，以非dequeBuffer及queueBuffer的方式，
     * 从生产者传递给消耗者。
     * 如果一个边界流存在，消耗者能够选择是否从边界流获取buffer，还是从queued的buffer中获取buffer.
     *
     * 传递NULL或者一个不同的流句柄，将会使得上一个被传递而来的边界流脱落
     */
    void setSidebandStream(const sp<NativeHandle>& stream);

    /* Allocates buffers based on the current dimensions/format.
     *
     * This function will allocate up to the maximum number of buffers
     * permitted by the current BufferQueue configuration. It will use the
     * default format and dimensions. This is most useful to avoid an allocation
     * delay during dequeueBuffer. If there are already the maximum number of
     * buffers allocated, this function has no effect.
     *
     * 根据当前的尺寸和格式，分配buffer对象。
     *
     * 此方法将分配到buffer对象最大数量，此最大数量被当前BufferQueue对象的配置所限制。
     * 它将使用默认的格式和尺寸。而此方法最有用的一点是，在dequeueBuffer期间，避免一个分配延迟。
     * 如果已经达到了最大分配buffer对象数，则此方法将不会产生任何影响。
     */
    void allocateBuffers();

    /* Sets the generation number on the IGraphicBufferProducer and updates the
     * generation number on any buffers attached to the Surface after this call.
     * See IGBP::setGenerationNumber for more information.
     *
     * 将生产ID设置到IGraphicBufferProducer对象之上，并且更新此生产ID到任何粘贴到此Surface对象
     * 之上
     */
    status_t setGenerationNumber(uint32_t generationNumber);

    // See IGraphicBufferProducer::getConsumerName
    String8 getConsumerName() const;

    // See IGraphicBufferProducer::getNextFrameNumber
    uint64_t getNextFrameNumber() const;

    /* Set the scaling mode to be used with a Surface.
     * See NATIVE_WINDOW_SET_SCALING_MODE and its parameters
     * in <system/window.h>. */
    int setScalingMode(int mode);

    // See IGraphicBufferProducer::setDequeueTimeout
    status_t setDequeueTimeout(nsecs_t timeout);

    /*
     * Wait for frame number to increase past lastFrame for at most
     * timeoutNs. Useful for one thread to wait for another unknown
     * thread to queue a buffer.
     *
     * 等待增加上一个帧数后的帧数。对于一个线程等待另一个未知线程入队一个buffer对象
     * 很有用
     */
    bool waitForNextFrame(uint64_t lastFrame, nsecs_t timeout);

    // See IGraphicBufferProducer::getLastQueuedBuffer
    // See GLConsumer::getTransformMatrix for outTransformMatrix format
    status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence, float outTransformMatrix[16]);

    // See IGraphicBufferProducer::getFrameTimestamps
    bool getFrameTimestamps(uint64_t frameNumber, nsecs_t* outPostedTime,
            nsecs_t* outAcquireTime, nsecs_t* outRefreshStartTime,
            nsecs_t* outGlCompositionDoneTime, nsecs_t* outDisplayRetireTime,
            nsecs_t* outReleaseTime);

    status_t getUniqueId(uint64_t* outId) const;

protected:
    virtual ~Surface();

private:
    // can't be copied
    Surface& operator = (const Surface& rhs);
    Surface(const Surface& rhs);

    // ANativeWindow hooks
    static int hook_cancelBuffer(ANativeWindow* window,
            ANativeWindowBuffer* buffer, int fenceFd);
    static int hook_dequeueBuffer(ANativeWindow* window,
            ANativeWindowBuffer** buffer, int* fenceFd);
    static int hook_perform(ANativeWindow* window, int operation, ...);
    static int hook_query(const ANativeWindow* window, int what, int* value);
    static int hook_queueBuffer(ANativeWindow* window,
            ANativeWindowBuffer* buffer, int fenceFd);
    static int hook_setSwapInterval(ANativeWindow* window, int interval);

    static int hook_cancelBuffer_DEPRECATED(ANativeWindow* window,
            ANativeWindowBuffer* buffer);
    static int hook_dequeueBuffer_DEPRECATED(ANativeWindow* window,
            ANativeWindowBuffer** buffer);
    static int hook_lockBuffer_DEPRECATED(ANativeWindow* window,
            ANativeWindowBuffer* buffer);
    static int hook_queueBuffer_DEPRECATED(ANativeWindow* window,
            ANativeWindowBuffer* buffer);

    int dispatchConnect(va_list args);
    int dispatchDisconnect(va_list args);
    int dispatchSetBufferCount(va_list args);
    int dispatchSetBuffersGeometry(va_list args);
    int dispatchSetBuffersDimensions(va_list args);
    int dispatchSetBuffersUserDimensions(va_list args);
    int dispatchSetBuffersFormat(va_list args);
    int dispatchSetScalingMode(va_list args);
    int dispatchSetBuffersTransform(va_list args);
    int dispatchSetBuffersStickyTransform(va_list args);
    int dispatchSetBuffersTimestamp(va_list args);
    int dispatchSetCrop(va_list args);
    int dispatchSetPostTransformCrop(va_list args);
    int dispatchSetUsage(va_list args);
    int dispatchLock(va_list args);
    int dispatchUnlockAndPost(va_list args);
    int dispatchSetSidebandStream(va_list args);
    int dispatchSetBuffersDataSpace(va_list args);
    int dispatchSetSurfaceDamage(va_list args);
    int dispatchSetSharedBufferMode(va_list args);
    int dispatchSetAutoRefresh(va_list args);
    int dispatchGetFrameTimestamps(va_list args);

protected:
    virtual int dequeueBuffer(ANativeWindowBuffer** buffer, int* fenceFd);
    virtual int cancelBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int queueBuffer(ANativeWindowBuffer* buffer, int fenceFd);
    virtual int perform(int operation, va_list args);
    virtual int setSwapInterval(int interval);

    virtual int lockBuffer_DEPRECATED(ANativeWindowBuffer* buffer);

    virtual int connect(int api);
    virtual int setBufferCount(int bufferCount);
    virtual int setBuffersDimensions(uint32_t width, uint32_t height);
    virtual int setBuffersUserDimensions(uint32_t width, uint32_t height);
    virtual int setBuffersFormat(PixelFormat format);
    virtual int setBuffersTransform(uint32_t transform);
    virtual int setBuffersStickyTransform(uint32_t transform);
    virtual int setBuffersTimestamp(int64_t timestamp);
    virtual int setBuffersDataSpace(android_dataspace dataSpace);
    virtual int setCrop(Rect const* rect);
    virtual int setUsage(uint32_t reqUsage);
    virtual void setSurfaceDamage(android_native_rect_t* rects, size_t numRects);

public:
    virtual int disconnect(int api,
            IGraphicBufferProducer::DisconnectMode mode =
                    IGraphicBufferProducer::DisconnectMode::Api);

    virtual int setMaxDequeuedBufferCount(int maxDequeuedBuffers);
    virtual int setAsyncMode(bool async);
    virtual int setSharedBufferMode(bool sharedBufferMode);
    virtual int setAutoRefresh(bool autoRefresh);
    virtual int lock(ANativeWindow_Buffer* outBuffer, ARect* inOutDirtyBounds);
    virtual int unlockAndPost();
    virtual int query(int what, int* value) const;

    virtual int connect(int api, const sp<IProducerListener>& listener);
    virtual int detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence);
    virtual int attachBuffer(ANativeWindowBuffer*);

protected:
    enum { NUM_BUFFER_SLOTS = BufferQueue::NUM_BUFFER_SLOTS };
    enum { DEFAULT_FORMAT = PIXEL_FORMAT_RGBA_8888 };

private:
    void freeAllBuffers();
    int getSlotFromBufferLocked(android_native_buffer_t* buffer) const;

    struct BufferSlot {
        sp<GraphicBuffer> buffer;
        Region dirtyRegion;
    };

    // mSurfaceTexture is the interface to the surface texture server. All
    // operations on the surface texture client ultimately translate into
    // interactions with the server using this interface.
    // TODO: rename to mBufferProducer
    //
    // mSurfaceTexture是surface纹理服务的接口。所有在surface纹理客户端上的操作，
    // 最终都将通过此接口，转移到服务端的实现者之上
    sp<IGraphicBufferProducer> mGraphicBufferProducer;

    // mSlots stores the buffers that have been allocated for each buffer slot.
    // It is initialized to null pointers, and gets filled in with the result of
    // IGraphicBufferProducer::requestBuffer when the client dequeues a buffer from a
    // slot that has not yet been used. The buffer allocated to a slot will also
    // be replaced if the requested buffer usage or geometry differs from that
    // of the buffer allocated to a slot.
    //
    // mSlotes存储已经为每个buffer槽位分配了的buffer对象。
    // 此属性被初始化为空指针，并且在IGraphicBufferProducer::requestBuffer方法之中被填满，
    // 当客户端从一个还未被使用的槽位中，出队一个buffer对象的时候。
    // 而一个已经被分配了一个槽位的buffer对象，也将被替换，如果请求的用途或者结合构造不同于
    // 已经被分配的buffer对象
    BufferSlot mSlots[NUM_BUFFER_SLOTS];

    // mReqWidth is the buffer width that will be requested at the next dequeue
    // operation. It is initialized to 1.
    // 在下一次出队操作过程之中，将被请求的buffer使用的宽度；默认为1
    uint32_t mReqWidth;

    // mReqHeight is the buffer height that will be requested at the next
    // dequeue operation. It is initialized to 1.
    //
    // 在下一次出队操作过程之中，将将被请求的buffer使用的高度；默认为1
    uint32_t mReqHeight;

    // mReqFormat is the buffer pixel format that will be requested at the next
    // deuque operation. It is initialized to PIXEL_FORMAT_RGBA_8888.
    // 在下一次出队操作过程之中，将被请求的buffer使用的格式；默认为PIXEL_FORMAT_RGBA_8888
    PixelFormat mReqFormat;

    // mReqUsage is the set of buffer usage flags that will be requested
    // at the next deuque operation. It is initialized to 0.
    //
    // 在下一次出队操作过程之中，将被请求的buffer使用的用途；默认为0
    uint32_t mReqUsage;

    // mTimestamp is the timestamp that will be used for the next buffer queue
    // operation. It defaults to NATIVE_WINDOW_TIMESTAMP_AUTO, which means that
    // a timestamp is auto-generated when queueBuffer is called.
    //
    // 在下一次出队操作过程之中，将被请求的buffer使用的时间戳；默认为NATIVE_WINDOW_TIMESTAMP_AUTO
    // 此默认值意味者，当前queueBuffer被调用的时候，一个时间戳将被自动分配
    int64_t mTimestamp;

    // mDataSpace is the buffer dataSpace that will be used for the next buffer
    // queue operation. It defaults to HAL_DATASPACE_UNKNOWN, which
    // means that the buffer contains some type of color data.
    // 在下一次出队操作过程之中，将被请求的buffer使用的数据空间。
    // 默认为HAL_DATASPACE_UNKNOWN，此默认值意味着buffer包含了一些类型的颜色值
    android_dataspace mDataSpace;

    // mCrop is the crop rectangle that will be used for the next buffer
    // that gets queued. It is set by calling setCrop.
    //
    // 在下一次入队操作过程之中，将被请求的buffer使用的裁剪矩形。
    Rect mCrop;

    // mScalingMode is the scaling mode that will be used for the next
    // buffers that get queued. It is set by calling setScalingMode.
    //
    // 在下一次入队操作过程之中，将被请求的buffer使用的缩放模式
    int mScalingMode;

    // mTransform is the transform identifier that will be used for the next
    // buffer that gets queued. It is set by calling setTransform.
    //
    // 在下一次入队操作过程之中，将被请求的buffer使用的变换单位矩阵。
    uint32_t mTransform;

    // mStickyTransform is a transform that is applied on top of mTransform
    // in each buffer that is queued.  This is typically used to force the
    // compositor to apply a transform, and will prevent the transform hint
    // from being set by the compositor.
    //
    // mStickyTransform是一个应用在mTransform之上，对每个入队的buffer对象作用的变换。
    // 此属性一般被用来推动排序者应用一个转换，并且将阻止来自于排序中的变化点
    uint32_t mStickyTransform;

    // mDefaultWidth is default width of the buffers, regardless of the
    // native_window_set_buffers_dimensions call.
    uint32_t mDefaultWidth;

    // mDefaultHeight is default height of the buffers, regardless of the
    // native_window_set_buffers_dimensions call.
    uint32_t mDefaultHeight;

    // mUserWidth, if non-zero, is an application-specified override
    // of mDefaultWidth.  This is lower priority than the width set by
    // native_window_set_buffers_dimensions.
    //
    // 用户宽度，如果非0，表示特定于application的默认宽度。
    // 此属性的优先级小于由native_window_set_buffers_dimensions设置的宽度
    uint32_t mUserWidth;

    // mUserHeight, if non-zero, is an application-specified override
    // of mDefaultHeight.  This is lower priority than the height set
    // by native_window_set_buffers_dimensions.
    //
    // 用户高度，如果非0，表示特定于application的默认高度。
    // 此属性的优先级小于由native_window_set_buffers_dimensions设置的高度
    uint32_t mUserHeight;

    // mTransformHint is the transform probably applied to buffers of this
    // window. this is only a hint, actual transform may differ.
    // mTransformHint是一个可能应用此窗口的buffer的变换。此值只表示一个建议，
    // 实际的变换可能不同
    uint32_t mTransformHint;

    // mProducerControlledByApp whether this buffer producer is controlled
    // by the application
    // 此buffer生产者是否由application控制
    bool mProducerControlledByApp;

    // mSwapIntervalZero set if we should drop buffers at queue() time to
    // achieve an asynchronous swap interval
    // 是否我们应该在一个异步交换间隔的入队时间上丢弃buffer对象
    bool mSwapIntervalZero;

    // mConsumerRunningBehind whether the consumer is running more than
    // one buffer behind the producer.
    //
    // 是否消耗者正在运行的，且落后于生产者的buffer对象个数大于1
    mutable bool mConsumerRunningBehind;

    // mMutex is the mutex used to prevent concurrent access to the member
    // variables of Surface objects. It must be locked whenever the
    // member variables are accessed.
    //
    // mMutex互斥量，被用来阻止并发访问Surface对象的成员变量。
    // 成员变量无论什么时候被访问，都应该被锁住
    mutable Mutex mMutex;

    // must be used from the lock/unlock thread
    sp<GraphicBuffer>           mLockedBuffer;
    sp<GraphicBuffer>           mPostedBuffer;
    bool                        mConnectedToCpu;

    // When a CPU producer is attached, this reflects the region that the
    // producer wished to update as well as whether the Surface was able to copy
    // the previous buffer back to allow a partial update.
    //
    // When a non-CPU producer is attached, this reflects the surface damage
    // (the change since the previous frame) passed in by the producer.
    //
    // 当一个CPU生产者被粘贴了，此属性引用了一个生产者希望更新的区域；该区域
    // 也决定了是否当前Surface对象能否从上一个buffer对象中copy内容，一次来进行一个局部的更新。
    Region mDirtyRegion;

    // Stores the current generation number. See setGenerationNumber and
    // IGraphicBufferProducer::setGenerationNumber for more information.
    // 保存当前的生成号
    uint32_t mGenerationNumber;

    // Caches the values that have been passed to the producer.
    bool mSharedBufferMode;
    bool mAutoRefresh;

    // If in shared buffer mode and auto refresh is enabled, store the shared
    // buffer slot and return it for all calls to queue/dequeue without going
    // over Binder.
    int mSharedBufferSlot;

    // This is true if the shared buffer has already been queued/canceled. It's
    // used to prevent a mismatch between the number of queue/dequeue calls.
    bool mSharedBufferHasBeenQueued;

    // These are used to satisfy the NATIVE_WINDOW_LAST_*_DURATION queries
    nsecs_t mLastDequeueDuration = 0;
    nsecs_t mLastQueueDuration = 0;

    Condition mQueueBufferCondition;

    uint64_t mNextFrameNumber;
};

namespace view {

/**
 * A simple holder for an IGraphicBufferProducer, to match the managed-side
 * android.view.Surface parcelable behavior.
 *
 * This implements android/view/Surface.aidl
 *
 * TODO: Convert IGraphicBufferProducer into AIDL so that it can be directly
 * used in managed Binder calls.
 */
class Surface : public Parcelable {
  public:

    String16 name;
    sp<IGraphicBufferProducer> graphicBufferProducer;

    virtual status_t writeToParcel(Parcel* parcel) const override;
    virtual status_t readFromParcel(const Parcel* parcel) override;

    // nameAlreadyWritten set to true by Surface.java, because it splits
    // Parceling itself between managed and native code, so it only wants a part
    // of the full parceling to happen on its native side.
    status_t writeToParcel(Parcel* parcel, bool nameAlreadyWritten) const;

    // nameAlreadyRead set to true by Surface.java, because it splits
    // Parceling itself between managed and native code, so it only wants a part
    // of the full parceling to happen on its native side.
    status_t readFromParcel(const Parcel* parcel, bool nameAlreadyRead);

  private:

    static String16 readMaybeEmptyString16(const Parcel* parcel);
};

} // namespace view

}; // namespace android

#endif  // ANDROID_GUI_SURFACE_H
