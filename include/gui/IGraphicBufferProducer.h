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

#ifndef ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H
#define ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>

#include <binder/IInterface.h>

#include <ui/Fence.h>
#include <ui/GraphicBuffer.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#include <gui/FrameTimestamps.h>

namespace android {
// ----------------------------------------------------------------------------

class IProducerListener;
class NativeHandle;
class Surface;

/*
 * This class defines the Binder IPC interface for the producer side of
 * a queue of graphics buffers.  It's used to send graphics data from one
 * component to another.  For example, a class that decodes video for
 * playback might use this to provide frames.  This is typically done
 * indirectly, through Surface.
 * 此类定义了一个关于图形缓存队列的生产者的Binder跨进程通信接口。
 * 此类被用来将图形数据从一个组件发送至另一个组件。
 * 例如，一个为播放而解码视频的类，可能使用此类来提供帧。
 * 而一个代表性的间接使用，就是Surface
 * The underlying mechanism is a BufferQueue, which implements
 * BnGraphicBufferProducer.  In normal operation, the producer calls
 * dequeueBuffer() to get an empty buffer, fills it with data, then
 * calls queueBuffer() to make it available to the consumer.
 * 潜在的一个机制是一个BufferQueue类，此类实现了BnGraphicBufferProducer?
 * 在一般的操作之中，生产者调用dequeueBuffer()方法来获取一个空的buffer，
 * 然后将此buffer填满数据，然后调用queueBuffer()方法使得此buffer能够被
 * 消费者使用
 * This class was previously called ISurfaceTexture.
 * 此类在ISurfaceTexture之前被调用
 */
class IGraphicBufferProducer : public IInterface
{
public:
    DECLARE_META_INTERFACE(GraphicBufferProducer);

    enum {
        // A flag returned by dequeueBuffer when the client needs to call
        // requestBuffer immediately thereafter.
        // 当客户端需要立即调用requestBuffer方法时，由dequeueBuffer方法返回的一个标识
        BUFFER_NEEDS_REALLOCATION = 0x1,
        // A flag returned by dequeueBuffer when all mirrored slots should be
        // released by the client. This flag should always be processed first.
        // 当所有映射的slot应该被客户端释放的时候，由dequeueBuffer方法返回的一个标识
        // 此标识应该总是被优先执行
        RELEASE_ALL_BUFFERS       = 0x2,
    };

    // requestBuffer requests a new buffer for the given index. The server (i.e.
    // the IGraphicBufferProducer implementation) assigns the newly created
    // buffer to the given slot index, and the client is expected to mirror the
    // slot->buffer mapping so that it's not necessary to transfer a
    // GraphicBuffer for every dequeue operation.
    //
    // The slot must be in the range of [0, NUM_BUFFER_SLOTS).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - one of the two conditions occurred:
    //              * slot was out of range (see above)
    //              * buffer specified by the slot is not dequeued
    // requestBuffer方法为传递而来的index,请求了一个新的buffer。
    // 服务端重新分配了新的buffer给传递而来的index，并且客户端期望引用
    // slot->buffer，以便它无需为每次的dequeue操作转移此图形缓存
    // @param slot   此入参的范围必须在[0,NUM_BUFFER_SLOTS)之间
    // @param return 如果返回非NO_ERROR值，则表示一个错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：以下两种情况，发生了任意一种：slot参数超出了范围；或者slot对应的buffer还未出队
    virtual status_t requestBuffer(int slot, sp<GraphicBuffer>* buf) = 0;

    // setMaxDequeuedBufferCount sets the maximum number of buffers that can be
    // dequeued by the producer at one time. If this method succeeds, any new
    // buffer slots will be both unallocated and owned by the BufferQueue object
    // (i.e. they are not owned by the producer or consumer). Calling this may
    // also cause some buffer slots to be emptied. If the caller is caching the
    // contents of the buffer slots, it should empty that cache after calling
    // this method.
    //
    // This function should not be called with a value of maxDequeuedBuffers
    // that is less than the number of currently dequeued buffer slots. Doing so
    // will result in a BAD_VALUE error.
    //
    // The buffer count should be at least 1 (inclusive), but at most
    // (NUM_BUFFER_SLOTS - the minimum undequeued buffer count) (exclusive). The
    // minimum undequeued buffer count can be obtained by calling
    // query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned.
    // * BAD_VALUE - one of the below conditions occurred:
    //     * bufferCount was out of range (see above).
    //     * client would have more than the requested number of dequeued
    //       buffers after this call.
    //     * this call would cause the maxBufferCount value to be exceeded.
    //     * failure to adjust the number of available slots.
    //
    // setMaxDequeuedBufferCount方法设置了同一时刻，能够被生产者出队的最大buffer数量。
    // 如果此方法成功了，任何新的buffer将不会被分配，且被BufferQueue对象所拥有。
    // 调用此方法也可能引起一些buffer被清空。
    // 如果调用者正在缓存此buffer的内容，则在调用了此方法后，会清空缓存。
    //
    // 此方法不应该以maxDequeuedBuffers值为参数，来调用此方法。如果这样做，将返回一个BAD_VALUE错误
    // buffer数量的范围为[1,NUM_BUFFER_SLOTS - 最小未出队buffer数)
    // 最小未出队buffer数能够通过调用query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS)方法来获取
    //
    // @param return 如果返回非NO_ERROR值，则表示一个错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：以下情况中，发生了任意一种：
    //                          * buffer数超出了范围；
    //                          * 在此调用后，客户端有更多的请求出队buffer数
    //                          * 此调用引起最大buffer数溢出的情况
    //                          * 调整可用buffer数目失败
    virtual status_t setMaxDequeuedBufferCount(int maxDequeuedBuffers) = 0;

    // Set the async flag if the producer intends to asynchronously queue
    // buffers without blocking. Typically this is used for triple-buffering
    // and/or when the swap interval is set to zero.
    //
    // Enabling async mode will internally allocate an additional buffer to
    // allow for the asynchronous behavior. If it is not enabled queue/dequeue
    // calls may block.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned.
    // * BAD_VALUE - one of the following has occurred:
    //             * this call would cause the maxBufferCount value to be
    //               exceeded
    //             * failure to adjust the number of available slots.
    // 设置异步标识，如果生产者期望异步非阻塞的方式入队buffer。
    // 一般而言，这被三缓冲模式，在缓冲交换间隔被设置为0的时候，使用。
    // 启用异步模式，将在内部分配一个额外的buffer用来允许异步行为。
    // 如果异步模式不被启用，则入队/出队调用可能阻塞。
    // @param return 如果返回非NO_ERROR值，则表示一个错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：以下情况中，发生了任意一种：
    //                          * 此调用引起maxBufferCount溢出
    //                          * 调整可用槽数量失败
    virtual status_t setAsyncMode(bool async) = 0;

    // dequeueBuffer requests a new buffer slot for the client to use. Ownership
    // of the slot is transfered to the client, meaning that the server will not
    // use the contents of the buffer associated with that slot.
    //
    // The slot index returned may or may not contain a buffer (client-side).
    // If the slot is empty the client should call requestBuffer to assign a new
    // buffer to that slot.
    //
    // Once the client is done filling this buffer, it is expected to transfer
    // buffer ownership back to the server with either cancelBuffer on
    // the dequeued slot or to fill in the contents of its associated buffer
    // contents and call queueBuffer.
    //
    // If dequeueBuffer returns the BUFFER_NEEDS_REALLOCATION flag, the client is
    // expected to call requestBuffer immediately.
    //
    // If dequeueBuffer returns the RELEASE_ALL_BUFFERS flag, the client is
    // expected to release all of the mirrored slot->buffer mappings.
    //
    // The fence parameter will be updated to hold the fence associated with
    // the buffer. The contents of the buffer must not be overwritten until the
    // fence signals. If the fence is Fence::NO_FENCE, the buffer may be written
    // immediately.
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
    // are enumerated in <gralloc.h>, e.g. GRALLOC_USAGE_HW_RENDER.  These
    // will be merged with the usage flags specified by
    // IGraphicBufferConsumer::setConsumerUsageBits.
    //
    // This call will block until a buffer is available to be dequeued. If
    // both the producer and consumer are controlled by the app, then this call
    // can never block and will return WOULD_BLOCK if no buffer is available.
    //
    // A non-negative value with flags set (see above) will be returned upon
    // success.
    //
    // Return of a negative means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - both in async mode and buffer count was less than the
    //               max numbers of buffers that can be allocated at once.
    // * INVALID_OPERATION - cannot attach the buffer because it would cause
    //                       too many buffers to be dequeued, either because
    //                       the producer already has a single buffer dequeued
    //                       and did not set a buffer count, or because a
    //                       buffer count was set and this call would cause
    //                       it to be exceeded.
    // * WOULD_BLOCK - no buffer is currently available, and blocking is disabled
    //                 since both the producer/consumer are controlled by app
    // * NO_MEMORY - out of memory, cannot allocate the graphics buffer.
    // * TIMED_OUT - the timeout set by setDequeueTimeout was exceeded while
    //               waiting for a buffer to become available.
    //
    // All other negative values are an unknown error returned downstream
    // from the graphics allocator (typically errno).
    //
    // dequeueBuffer方法会请求一个新的buffer槽给客户端使用。并且此buffer槽的所有权
    // 被转交给客户端，这也意味这服务端将无权使用与此槽位相关联的buffer的内容了。
    // 被返回的槽位索引，既可能包含一个(客户端)的buffer,也可能不包含一个(客户端)的buffer。
    // 如果返回的槽位是空的，则客户端应该调用requestBuffer来请求分配一个新的buffer。
    // 一旦客户端完成了对此buffer的填充，则客户端可以以两种方式来将此buffer槽的所有权交回
    // 给服务端，这两种方式是：cancelBuffer方法和将其内容填充至buffer中并调用queueBuffer方法
    // 如果此方法的返回值之中包含了BUFFER_NEEDS_REALLOCATION，则表示服务端期望客户端立即调用
    // requestBuffer方法。
    // 如果此方法的返回值之中包含了RELEASE_ALL_BUFFERS，则表示服务端期望客户端释放所有slot->buffer的镜像映射。
    // 围栏参数将被更新，用以持有与此buffer相关的围栏。buffer的内容禁止重写，直到围栏信号来临。
    // 如果buffer的围栏参数是Fence::NO_FENCE，则此buffer能够立即被重写。
    // 宽度和高度参数禁止大于GL_MAX_VIEWPORT_DIMS 和 GL_MAX_TEXTURE_SIZE 的最小值（具体可查看glGetIntegerv）。
    // 由于无效尺寸引起的错误可能不会被报告，直到updateTexImage()方法被调用。
    // 如果宽度和高度都为0，则由方法setDefaultBufferSize()指定的默认值将被使用。
    // 如果格式也被设置为0了，则默认的格式将被使用。
    // 用途参数指定gralloc缓存的使用标识。此值在<gralloc.h>文件之中被枚举，例如GRALLOC_USAGE_HW_RENDER
    // 这些被指定的用途，将会被IGraphicBufferConsumer::setConsumerUsageBits方法合并。
    // 此方法的调用将被阻塞，直到一个buffer可以出队了。
    // 如果生产者与消费者都被APP所控制，则此调用将永不阻塞并将将返回WOULD_BLOCK，如果没有buffer可用。
    // 一个包含了标识设置的正值将被返回，标识调用成功
    // 而一个负值的返回则标识，一些错误发生了：
    //               NO_INIT          ：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE        ：处于异步模式之中，或者buffer数量少于可分配的buffer最大值
    //               INVALID_OPERATION:不能粘贴buffer，应为这引起了太多buffer出队，或者
    //                                 是因为生产者在没有设置buffer数量的情况下，
    //                                 已经有一个单一的buffer出队，或这因为一个buffer数量被设置了
    //                                 而此次调用引起了一个溢出。
    //                     WOULD_BLOCK:当前没有buffer可用，并且因为生产者和消费者都被app控制的原因，
    //                                 而不允许阻塞
    //                       NO_MEMORY:内存不足，不能够分配graphics buffer.
    //                       TIMED_OUT:被setDequeueTimeout方法指定的超时被溢出了，当等待一个buffer变得可用的时候
    // 所有的其他负值都是来至于底层graphics allocator的未知错误
    virtual status_t dequeueBuffer(int* slot, sp<Fence>* fence, uint32_t w,
            uint32_t h, PixelFormat format, uint32_t usage) = 0;

    // detachBuffer attempts to remove all ownership of the buffer in the given
    // slot from the buffer queue. If this call succeeds, the slot will be
    // freed, and there will be no way to obtain the buffer from this interface.
    // The freed slot will remain unallocated until either it is selected to
    // hold a freshly allocated buffer in dequeueBuffer or a buffer is attached
    // to the slot. The buffer must have already been dequeued, and the caller
    // must already possesses the sp<GraphicBuffer> (i.e., must have called
    // requestBuffer).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - the given slot number is invalid, either because it is
    //               out of the range [0, NUM_BUFFER_SLOTS), or because the slot
    //               it refers to is not currently dequeued and requested.
    // detachBuffer方法视图从buffer队列之中，删除给定槽位对应的buffer的所有权。
    // 如果此调用成功，此槽位将被释放，并且此接口将不会有任何方式来获取此buffer.
    // 被释放的槽位仍未被范培，知道它被选择持有一个刚被分配的buffer，或者一个buffer被粘贴至了此槽位。
    // buffer必须已经被出队了，并且调用者必须已经占有了sp<GraphicBuffer>（例如，必须调用requestBuffer）
    // @param return 如果返回非NO_ERROR值，则表示一个错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：给定的slot索引是无效的，或者因为slot草除了其范围，
    //                          或者因为此槽位引用的buffer当前没有被出队或者没有被请求
    virtual status_t detachBuffer(int slot) = 0;

    // detachNextBuffer is equivalent to calling dequeueBuffer, requestBuffer,
    // and detachBuffer in sequence, except for two things:
    //
    // 1) It is unnecessary to know the dimensions, format, or usage of the
    //    next buffer.
    // 2) It will not block, since if it cannot find an appropriate buffer to
    //    return, it will return an error instead.
    //
    // Only slots that are free but still contain a GraphicBuffer will be
    // considered, and the oldest of those will be returned. outBuffer is
    // equivalent to outBuffer from the requestBuffer call, and outFence is
    // equivalent to fence from the dequeueBuffer call.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - either outBuffer or outFence were NULL.
    // * NO_MEMORY - no slots were found that were both free and contained a
    //               GraphicBuffer.
    // detachNextBuffer方法等价于串行调用dequeueBuffer方法、requestBuffer方法、
    // detachBuffer方法，除了一下两件事：
    //  1) 此方法不需要知道尺寸、格式或者buffer的用途。
    //  2）此方法非阻塞，因为如果它不能发现一个合适的buffer返回，则它将返回一个错误。
    // 只有虽然被释放，但是依旧含有一个GraphicBuffer对象的槽位将被重视，并且将返回最古老的。
    // outBuffer等价于调用requestBuffer方法返回的outBuffer,同时outFence等价与调用
    // dequeueBuffer方法返回的outFence.
    // @param return 如果返回非NO_ERROR值，则表示一个错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：outBuffer或者outFence为空
    //               NO_MEMORY:没有被释放且包含了一个GraphicsBuffer对象的槽位被发现。
    virtual status_t detachNextBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence) = 0;

    // attachBuffer attempts to transfer ownership of a buffer to the buffer
    // queue. If this call succeeds, it will be as if this buffer was dequeued
    // from the returned slot number. As such, this call will fail if attaching
    // this buffer would cause too many buffers to be simultaneously dequeued.
    //
    // If attachBuffer returns the RELEASE_ALL_BUFFERS flag, the caller is
    // expected to release all of the mirrored slot->buffer mappings.
    //
    // A non-negative value with flags set (see above) will be returned upon
    // success.
    //
    // Return of a negative value means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - outSlot or buffer were NULL, invalid combination of
    //               async mode and buffer count override, or the generation
    //               number of the buffer did not match the buffer queue.
    // * INVALID_OPERATION - cannot attach the buffer because it would cause
    //                       too many buffers to be dequeued, either because
    //                       the producer already has a single buffer dequeued
    //                       and did not set a buffer count, or because a
    //                       buffer count was set and this call would cause
    //                       it to be exceeded.
    // * WOULD_BLOCK - no buffer slot is currently available, and blocking is
    //                 disabled since both the producer/consumer are
    //                 controlled by the app.
    // * TIMED_OUT - the timeout set by setDequeueTimeout was exceeded while
    //               waiting for a slot to become available.
    // attachBuffer方法试图转移buffer的所有权给buffer queue。
    // 如果此调用成功，则将如同此buffer从返回的槽位索引之中出队一般。
    // 同样的，如果粘贴此buffer将会引起太多buffer被同时出队，则此调用将会失败。
    //
    // 如果attachBuffer方法返回RELEASE_ALL_BUFFERS标识，则表示服务端期望客户端释放所有slot->buffer的镜像映射。
    // 一个非负数的返回，表示此方法调用成功；
    // 而一个负数的返回，则表示一些错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：outSlot或者buffer是空的，异步模式与buffer重写的无效组合，
    //                          或者buffer生成的数组并不匹配buffer queue.
    //               INVALID_OPERATION:不能粘贴buffer，应为这引起了太多buffer出队，或者
    //                                 是因为生产者在没有设置buffer数量的情况下，
    //                                 已经有一个单一的buffer出队，或这因为一个buffer数量被设置了
    //                                 而此次调用引起了一个溢出。
    //                     WOULD_BLOCK:当前没有buffer可用，并且因为生产者和消费者都被app控制的原因，
    //                                 而不允许阻塞
    //                       TIMED_OUT:被setDequeueTimeout方法指定的超时被溢出了，当等待一个buffer变得可用的时候
    virtual status_t attachBuffer(int* outSlot,
            const sp<GraphicBuffer>& buffer) = 0;



    // queueBuffer indicates that the client has finished filling in the
    // contents of the buffer associated with slot and transfers ownership of
    // that slot back to the server.
    //
    // It is not valid to call queueBuffer on a slot that is not owned
    // by the client or one for which a buffer associated via requestBuffer
    // (an attempt to do so will fail with a return value of BAD_VALUE).
    //
    // In addition, the input must be described by the client (as documented
    // below). Any other properties (zero point, etc)
    // are client-dependent, and should be documented by the client.
    //
    // The slot must be in the range of [0, NUM_BUFFER_SLOTS).
    //
    // Upon success, the output will be filled with meaningful values
    // (refer to the documentation below).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - one of the below conditions occurred:
    //              * fence was NULL
    //              * scaling mode was unknown
    //              * both in async mode and buffer count was less than the
    //                max numbers of buffers that can be allocated at once
    //              * slot index was out of range (see above).
    //              * the slot was not in the dequeued state
    //              * the slot was enqueued without requesting a buffer
    //              * crop rect is out of bounds of the buffer dimensions
    //
    // queueBuffer方法表示客户端完成了对槽位相关的buffer的内容填充，并且将slot的所有权
    // 交还给了服务端。
    //
    // 对一个不属于客户端，或者没有通过requestBuffer方法与一个buffer关联起来的buffer
    // 槽位调用queueBuffer方法是无效的。（任何这样的尝试都将得到一个BAD_VALUE的返回值）
    //
    // 此外，QueueBufferInput必须被客户端所描述。任何其他的属性（0点等）都是依赖客户端的
    // 应该被客户端记录。
    //
    // 槽位索引的范围必须是【0，NUM_BUFFER_SLOTS)
    //
    // 对于成功的情况，output将被有意义的数据填充。
    //
    // 而一个负数的返回，则表示一些错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：下面任意情况，只要满足一种：
    //                          * fence为空
    //                          * scalingMode未知
    //                          * 处于异步模式，同时buffer数量少于同一时刻能够被分配的buffe最大数量
    //                          * 槽位索引溢出
    //                          * 此槽位不在出队状态
    //                          * 此槽位虽然出队，但是还未请求一个buffer对象
    //                          * 裁剪矩形已经超出了buffer尺寸范围
    struct QueueBufferInput : public Flattenable<QueueBufferInput> {
        friend class Flattenable<QueueBufferInput>;
        inline QueueBufferInput(const Parcel& parcel);
        // timestamp - a monotonically increasing value in nanoseconds
        // isAutoTimestamp - if the timestamp was synthesized at queue time
        // dataSpace - description of the contents, interpretation depends on format
        // crop - a crop rectangle that's used as a hint to the consumer
        // scalingMode - a set of flags from NATIVE_WINDOW_SCALING_* in <window.h>
        // transform - a set of flags from NATIVE_WINDOW_TRANSFORM_* in <window.h>
        // fence - a fence that the consumer must wait on before reading the buffer,
        //         set this to Fence::NO_FENCE if the buffer is ready immediately
        // sticky - the sticky transform set in Surface (only used by the LEGACY
        //          camera mode).

        // timestamp - 一个单调递增的纳秒级数值
        // isAutoTimestamp - 时间戳是否在队列时间上合成
        // dataSpace - 内容的描述，其解释依赖于格式
        // crop - 裁剪矩形，被用来作为一个与消耗相关的提示
        // scalingMode - 一系列来至于<window.h>文件中的标志
        // transform - 一系列来至于<window.h>文件中的标志
        // fence - 一个消费者在读取buffer之前，必须等待此围栏；如果buffer是可以立即读取，则将fence设置为Fence::NO_FENCE
        // sticky - surface之中的转换集合
        inline QueueBufferInput(int64_t timestamp, bool isAutoTimestamp,
                android_dataspace dataSpace, const Rect& crop, int scalingMode,
                uint32_t transform, const sp<Fence>& fence, uint32_t sticky = 0)
                : timestamp(timestamp), isAutoTimestamp(isAutoTimestamp),
                  dataSpace(dataSpace), crop(crop), scalingMode(scalingMode),
                  transform(transform), stickyTransform(sticky), fence(fence),
                  surfaceDamage() { }
        inline void deflate(int64_t* outTimestamp, bool* outIsAutoTimestamp,
                android_dataspace* outDataSpace,
                Rect* outCrop, int* outScalingMode,
                uint32_t* outTransform, sp<Fence>* outFence,
                uint32_t* outStickyTransform = NULL) const {
            *outTimestamp = timestamp;
            *outIsAutoTimestamp = bool(isAutoTimestamp);
            *outDataSpace = dataSpace;
            *outCrop = crop;
            *outScalingMode = scalingMode;
            *outTransform = transform;
            *outFence = fence;
            if (outStickyTransform != NULL) {
                *outStickyTransform = stickyTransform;
            }
        }

        // Flattenable protocol
        size_t getFlattenedSize() const;
        size_t getFdCount() const;
        status_t flatten(void*& buffer, size_t& size, int*& fds, size_t& count) const;
        status_t unflatten(void const*& buffer, size_t& size, int const*& fds, size_t& count);

        const Region& getSurfaceDamage() const { return surfaceDamage; }
        void setSurfaceDamage(const Region& damage) { surfaceDamage = damage; }

    private:
        int64_t timestamp;
        int isAutoTimestamp;
        android_dataspace dataSpace;
        Rect crop;
        int scalingMode;
        uint32_t transform;
        uint32_t stickyTransform;
        sp<Fence> fence;
        Region surfaceDamage;
    };

    // QueueBufferOutput must be a POD structure
    // GCC使用__attribute__关键字来描述函数，变量和数据类型的属性，用于编译器对源代码的优化
    // __attribute__ ((packed)) 的作用就是告诉编译器，取消结构在编译过程中的优化对齐，按照实际占用字节数进行对齐，是GCC特有的语法
    struct __attribute__ ((__packed__)) QueueBufferOutput {
        inline QueueBufferOutput() { }
        // outWidth - filled with default width applied to the buffer
        // outHeight - filled with default height applied to the buffer
        // outTransformHint - filled with default transform applied to the buffer
        // outNumPendingBuffers - num buffers queued that haven't yet been acquired
        //                        (counting the currently queued buffer)
        // outWidth　－　应用到此缓冲区上的默认填充宽度；
        // outHeight － 应用到此缓冲区上的默认填充高度；
        // outTransformHint －　应用到此缓冲区之上的默认填充转换
        // outNumPendingBuffers　－　当前并没有获取的，已经入队的缓冲区数
        inline void deflate(uint32_t* outWidth,
                uint32_t* outHeight,
                uint32_t* outTransformHint,
                uint32_t* outNumPendingBuffers,
                uint64_t* outNextFrameNumber) const {
            *outWidth = width;
            *outHeight = height;
            *outTransformHint = transformHint;
            *outNumPendingBuffers = numPendingBuffers;
            *outNextFrameNumber = nextFrameNumber;
        }
        inline void inflate(uint32_t inWidth, uint32_t inHeight,
                uint32_t inTransformHint, uint32_t inNumPendingBuffers,
                uint64_t inNextFrameNumber) {
            width = inWidth;
            height = inHeight;
            transformHint = inTransformHint;
            numPendingBuffers = inNumPendingBuffers;
            nextFrameNumber = inNextFrameNumber;
        }
    private:
        uint32_t width;
        uint32_t height;
        uint32_t transformHint;
        uint32_t numPendingBuffers;
        uint64_t nextFrameNumber{0};
    };

    virtual status_t queueBuffer(int slot, const QueueBufferInput& input,
            QueueBufferOutput* output) = 0;

    // cancelBuffer indicates that the client does not wish to fill in the
    // buffer associated with slot and transfers ownership of the slot back to
    // the server.
    //
    // The buffer is not queued for use by the consumer.
    //
    // The slot must be in the range of [0, NUM_BUFFER_SLOTS).
    //
    // The buffer will not be overwritten until the fence signals.  The fence
    // will usually be the one obtained from dequeueBuffer.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned or the producer is not
    //             connected.
    // * BAD_VALUE - one of the below conditions occurred:
    //              * fence was NULL
    //              * slot index was out of range (see above).
    //              * the slot was not in the dequeued state
    // cancelBuffer表明客户端不期望填充与此slot相关的槽位，并且将槽位的所有权归还给服务端
    // buffer对象没有被消费者入队使用
    // slot的范围必须是[0,NUM_BUFFER_SLOTS)。
    // buffer对象在fence信号之前，是不能被重写的。fence通常将是从dequeueBuffer之中获取的。
    // 而一个负数的返回，则表示一些错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：下面任意情况，只要满足一种：
    //                          * fence为空
    //                          * 槽位索引溢出
    //                          * 此槽位不在出队状态
    virtual status_t cancelBuffer(int slot, const sp<Fence>& fence) = 0;

    // query retrieves some information for this surface
    // 'what' tokens allowed are that of NATIVE_WINDOW_* in <window.h>
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - the buffer queue has been abandoned.
    // * BAD_VALUE - what was out of range
    // 查询当前surface的一些信息
    virtual int query(int what, int* value) = 0;

    // connect attempts to connect a client API to the IGraphicBufferProducer.
    // This must be called before any other IGraphicBufferProducer methods are
    // called except for getAllocator. A consumer must be already connected.
    //
    // This method will fail if the connect was previously called on the
    // IGraphicBufferProducer and no corresponding disconnect call was made.
    //
    // The listener is an optional binder callback object that can be used if
    // the producer wants to be notified when the consumer releases a buffer
    // back to the BufferQueue. It is also used to detect the death of the
    // producer. If only the latter functionality is desired, there is a
    // DummyProducerListener class in IProducerListener.h that can be used.
    //
    // The api should be one of the NATIVE_WINDOW_API_* values in <window.h>
    //
    // The producerControlledByApp should be set to true if the producer is hosted
    // by an untrusted process (typically app_process-forked processes). If both
    // the producer and the consumer are app-controlled then all buffer queues
    // will operate in async mode regardless of the async flag.
    //
    // Upon success, the output will be filled with meaningful data
    // (refer to QueueBufferOutput documentation above).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * NO_INIT - one of the following occurred:
    //             * the buffer queue was abandoned
    //             * no consumer has yet connected
    // * BAD_VALUE - one of the following has occurred:
    //             * the producer is already connected
    //             * api was out of range (see above).
    //             * output was NULL.
    //             * Failure to adjust the number of available slots. This can
    //               happen because of trying to allocate/deallocate the async
    //               buffer in response to the value of producerControlledByApp.
    // * DEAD_OBJECT - the token is hosted by an already-dead process
    //
    // Additional negative errors may be returned by the internals, they
    // should be treated as opaque fatal unrecoverable errors.
    // connect方法视图连接一个客户端API到IGraphicBufferProducer。
    // 此调用，必须在IGraphicBufferProducer类中，除了getAllocator方法之外，的其他所有方法之前调用。
    // 一个消耗着必须已经被连接了。
    // 如果此IGraphicBufferProducer已经被连接了，同时disconnect方法并没有被调用，则再次调用此方法将失败
    // listner参数是一个可选的binder回调;当comsumer将一个buffer释放会bufferqueue队列的时候，如果produce
    // 期望发送一个通知，则会使用listener参数对应的对象。
    // 此listener对象也被用来探测producer是否死亡。
    // api参数应该是<window.h>文件中的NATIVE_WINDOW_API_*相关值
    // 如果produncer被一个不信任的进程持有，则producerControlledByApp参数应该被设置为true
    // 如果produncer和comsumer都是APP控制的，那么所有的buffer队列将在异步模式中操作
    // 对于成功的情况，output将被有意义的数据填充。
    //
    // 而一个负数的返回，则表示一些错误发生了：
    //               NO_INIT：buffer queue已经被废弃；或者生产者没有被连接了
    //               BAD_VALUE：下面任意情况，只要满足一种：
    //                          * producer已经被连接
    //                          * api超出范围
    //                          * output为空
    //                          * 调整可用槽位数目失败；
    //             DEAD_OBJECT：已经被一个已经死亡的进程持有
    virtual status_t connect(const sp<IProducerListener>& listener,
            int api, bool producerControlledByApp, QueueBufferOutput* output) = 0;

    enum class DisconnectMode {
        // Disconnect only the specified API.
        Api,
        // Disconnect any API originally connected from the process calling disconnect.
        AllLocal
    };

    // disconnect attempts to disconnect a client API from the
    // IGraphicBufferProducer.  Calling this method will cause any subsequent
    // calls to other IGraphicBufferProducer methods to fail except for
    // getAllocator and connect.  Successfully calling connect after this will
    // allow the other methods to succeed again.
    //
    // The api should be one of the NATIVE_WINDOW_API_* values in <window.h>
    //
    // Alternatively if mode is AllLocal, then the API value is ignored, and any API
    // connected from the same PID calling disconnect will be disconnected.
    //
    // Disconnecting from an abandoned IGraphicBufferProducer is legal and
    // is considered a no-op.
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - one of the following has occurred:
    //             * the api specified does not match the one that was connected
    //             * api was out of range (see above).
    // * DEAD_OBJECT - the token is hosted by an already-dead process
    // disconnect试图断掉一个来自于IGraphicBufferProducer的客户端API。
    // 如果调用了此方法，那么对IGraphicBufferProducer类中除了getAllocator和connect方法外，的其他方法的后续调用
    // 都将失败；而此方法调用后，能够再次成功调用connect方法，则将运行所有其他的方法被调用成功。
    // api参数应该是<window.h>文件中的NATIVE_WINDOW_API_*相关值
    // 如果mode是AllLocal，则api参数值将被忽略，并且所有来自于相同PID的任何API连接将被断开。
    // 来至于一个丢弃的IGraphicBufferProducer的断开操作都是合法的，只是这种操作将会触发一个空操作。
    // 一个负数的返回，则表示一些错误发生了：
    //               BAD_VALUE：下面任意情况，只要满足一种：
    //                          * producer已经被连接
    //                          * 指定的API并不匹配连接时指定的API
    //                          * output为空
    //                          * 调整可用槽位数目失败；
    //             DEAD_OBJECT：已经被一个已经死亡的进程持有
    virtual status_t disconnect(int api, DisconnectMode mode = DisconnectMode::Api) = 0;

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
    // 一个sideband缓存流归属于此IGraphicBufferProducer对象。
    // 一个sideband刘是一个具体于设备(实现)机制的流，用将图像缓存从生产者传递给消费者，
    // 在传递的过程中，无需使用dequeueBuffer或者queueBuffer方法。
    // 一个sideband流是实时的，消费者能够选择是从sideband流之中，还是从bufferqueue之中，获取buffer对象
    // 传递NULL或者一个不同的句柄，将脱落之前的句柄，如果存在
    virtual status_t setSidebandStream(const sp<NativeHandle>& stream) = 0;

    // Allocates buffers based on the given dimensions/format.
    //
    // This function will allocate up to the maximum number of buffers
    // permitted by the current BufferQueue configuration. It will use the
    // given format, dimensions, and usage bits, which are interpreted in the
    // same way as for dequeueBuffer, and the async flag must be set the same
    // way as for dequeueBuffer to ensure that the correct number of buffers are
    // allocated. This is most useful to avoid an allocation delay during
    // dequeueBuffer. If there are already the maximum number of buffers
    // allocated, this function has no effect.
    //
    // 根据给定的尺寸或和格式，分配buffer缓冲区
    //
    // 此方法将分配被当前bufferqueue配置允许的最大缓冲区数量。
    // 此方法将使用指定的格式、尺寸以及用途，这些参数在dequeueBuffer的时候，也会以同一种方式被解释。
    // 此外异步标识也必须以dequeueBuffer相同的方式来设置，用以确保被分配缓冲区的正确数量。
    // 为了避免在dequeuebuffer期间，分配延迟，此方法是最有用的。
    // 如果bufferqueue之中被分配buffer的数量超过了最大值，则此方法无效
    virtual void allocateBuffers(uint32_t width, uint32_t height,
            PixelFormat format, uint32_t usage) = 0;

    // Sets whether dequeueBuffer is allowed to allocate new buffers.
    //
    // Normally dequeueBuffer does not discriminate between free slots which
    // already have an allocated buffer and those which do not, and will
    // allocate a new buffer if the slot doesn't have a buffer or if the slot's
    // buffer doesn't match the requested size, format, or usage. This method
    // allows the producer to restrict the eligible slots to those which already
    // have an allocated buffer of the correct size, format, and usage. If no
    // eligible slot is available, dequeueBuffer will block or return an error
    // as usual.
    // 设置是否允许dequeueBuffer分配新的缓冲区。
    // 一般来说，dequeueBuffer并不辨别一个槽位是否已经分配了一个图形缓冲，也不关心在这个槽位没有分配缓冲区或者
    // 它分配的缓冲区的格式、尺寸或者用途与期望的不同时，是否分配一个新的图形缓冲区。
    // 此方法允许生产者限制合适的槽位，即已经分配了正确尺寸、格式以及用途的缓冲区。
    // 如果没有合适的槽位可用，dequeueBuffer方法将阻塞护着返回一个错误码。
    virtual status_t allowAllocation(bool allow) = 0;

    // Sets the current generation number of the BufferQueue.
    //
    // This generation number will be inserted into any buffers allocated by the
    // BufferQueue, and any attempts to attach a buffer with a different
    // generation number will fail. Buffers already in the queue are not
    // affected and will retain their current generation number. The generation
    // number defaults to 0.
    // 设置BufferQueue当前的生成ID。
    // 生成ID将插入到所有被BufferQueue分配了的缓冲之中，并且所有的以不同ID粘贴一个buffer的尝试都将失败。
    // 已经在队列之中的缓冲区不会收到此方法的影响，并且将保持当前它们持有的生成ID
    // 生成ID的默认值是0.
    virtual status_t setGenerationNumber(uint32_t generationNumber) = 0;

    // Returns the name of the connected consumer.
    // 返回已连接的消耗着的名称
    virtual String8 getConsumerName() const = 0;

    // Used to enable/disable shared buffer mode.
    //
    // When shared buffer mode is enabled the first buffer that is queued or
    // dequeued will be cached and returned to all subsequent calls to
    // dequeueBuffer and acquireBuffer. This allows the producer and consumer to
    // simultaneously access the same buffer.
    // 被用来启用/禁用共享缓冲区模式。
    //
    // 当共享缓冲区模式被启用，第一个出队或者入队的缓冲区将被缓存，并且返回给后续所有的dequeueBuffer和
    // acquireBuffer调用。
    // 此方法允许生产者和消耗者同时访问一块相同的缓冲区
    virtual status_t setSharedBufferMode(bool sharedBufferMode) = 0;

    // Used to enable/disable auto-refresh.
    //
    // Auto refresh has no effect outside of shared buffer mode. In shared
    // buffer mode, when enabled, it indicates to the consumer that it should
    // attempt to acquire buffers even if it is not aware of any being
    // available.
    // 被用来启用/禁用自动刷新。
    //
    // 自动刷新对除了，共享缓冲区模式之外的模式，并无效果。
    // 在共享缓冲区模式之中，当启用了自动刷新功能，则将指示消耗者，
    // 即便它不知道任何当前可用的缓冲区，它也应该视图获取缓冲区
    virtual status_t setAutoRefresh(bool autoRefresh) = 0;

    // Sets how long dequeueBuffer will wait for a buffer to become available
    // before returning an error (TIMED_OUT).
    //
    // This timeout also affects the attachBuffer call, which will block if
    // there is not a free slot available into which the attached buffer can be
    // placed.
    //
    // By default, the BufferQueue will wait forever, which is indicated by a
    // timeout of -1. If set (to a value other than -1), this will disable
    // non-blocking mode and its corresponding spare buffer (which is used to
    // ensure a buffer is always available).
    //
    // Return of a value other than NO_ERROR means an error has occurred:
    // * BAD_VALUE - Failure to adjust the number of available slots. This can
    //               happen because of trying to allocate/deallocate the async
    //               buffer.
    virtual status_t setDequeueTimeout(nsecs_t timeout) = 0;

    // Returns the last queued buffer along with a fence which must signal
    // before the contents of the buffer are read. If there are no buffers in
    // the queue, outBuffer will be populated with nullptr and outFence will be
    // populated with Fence::NO_FENCE
    //
    // outTransformMatrix is not modified if outBuffer is null.
    //
    // Returns NO_ERROR or the status of the Binder transaction
    virtual status_t getLastQueuedBuffer(sp<GraphicBuffer>* outBuffer,
            sp<Fence>* outFence, float outTransformMatrix[16]) = 0;

    // Attempts to retrieve timestamp information for the given frame number.
    // If information for the given frame number is not found, returns false.
    // Returns true otherwise.
    //
    // If a fence has not yet signaled the timestamp returned will be 0;
    virtual bool getFrameTimestamps(uint64_t /*frameNumber*/,
            FrameTimestamps* /*outTimestamps*/) const { return false; }

    // Returns a unique id for this BufferQueue
    // 为BufferQueue对象返回一个唯一的ID
    virtual status_t getUniqueId(uint64_t* outId) const = 0;
};

// ----------------------------------------------------------------------------

class BnGraphicBufferProducer : public BnInterface<IGraphicBufferProducer>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------
}; // namespace android

#endif // ANDROID_GUI_IGRAPHICBUFFERPRODUCER_H
