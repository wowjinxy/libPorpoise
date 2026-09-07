#ifndef LIBPORPOISE_SIM_AI_HPP
#define LIBPORPOISE_SIM_AI_HPP

#include <dolphin/types.h>
#include <SDL2/SDL_mutex.h>

#include "simulator/sim_ai.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace SIM::AI {

struct StereoFrame {
    u16 left;
    u16 right;
};

class RingBuffer {
public:
    explicit RingBuffer(u32 capacityFrames)
        : mCapacity(capacityFrames),
          mBuffer(std::make_unique<StereoFrame[]>(capacityFrames))
    {
        mReadIndex.store(0, std::memory_order_relaxed);
        mWriteIndex.store(0, std::memory_order_relaxed);
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /*
     * Number of frames currently available for reading.
     */
    u32 AvailableToRead() const
    {
        const u32 read =
            mReadIndex.load(std::memory_order_acquire);

        const u32 write =
            mWriteIndex.load(std::memory_order_acquire);

        return write - read;
    }

    /*
     * Number of frames that can currently be written.
     */
    u32 AvailableToWrite() const
    {
        return mCapacity - AvailableToRead();
    }

    u32 Write(const StereoFrame* frames, u32 count)
    {
        const u32 read =
            mReadIndex.load(std::memory_order_acquire);

        const u32 write =
            mWriteIndex.load(std::memory_order_relaxed);

        const u32 available = mCapacity - (write - read);
        const u32 toWrite = count < available ? count : available;

        for (u32 i = 0; i < toWrite; ++i)
            mBuffer[(write + i) % mCapacity] = frames[i];

        mWriteIndex.store(
            write + toWrite,
            std::memory_order_release);

        return toWrite;
    }

    u32 Read(StereoFrame* frames, u32 count)
    {
        const u32 write =
            mWriteIndex.load(std::memory_order_acquire);

        const u32 read =
            mReadIndex.load(std::memory_order_relaxed);

        const u32 available = write - read;
        const u32 toRead = count < available ? count : available;

        for (u32 i = 0; i < toRead; ++i)
            frames[i] = mBuffer[(read + i) % mCapacity];

        mReadIndex.store(
            read + toRead,
            std::memory_order_release);

        return toRead;
    }

    void Clear()
    {
        const size_t write =
            mWriteIndex.load(std::memory_order_relaxed);

        mReadIndex.store(write, std::memory_order_release);
    }

    u32 Capacity() const
    {
        return mCapacity;
    }

private:
    const u32 mCapacity;

    std::unique_ptr<StereoFrame[]> mBuffer;

    alignas(64) std::atomic<u32> mReadIndex;
    alignas(64) std::atomic<u32> mWriteIndex;
};

enum class ThreadMessageType {
 SetRegValue,
 InitDma,
 StartDma,
 StopDma,
 Count
};

struct SetRegValue {
 u32 reg;
 u32 newVal;
 SDL_sem * semaphore;
};

struct InitDmaMessage {
    StereoFrame * startAddr;
    u32 length;
};

struct ThreadMessage {
 ThreadMessage(){};
 ThreadMessageType mType;
 union {
    SetRegValue mSetRegValue;
    InitDmaMessage mInitDma;
 };
};

typedef union {
    u32 raw;
    struct {
        u32 playingStatus:1;
        u32 auxFrequency:1;
        u32 interruptMask:1;
        u32 interruptStatus:1;
        u32 interruptValid:1;
        u32 sampleCounterReset:1;
        u32 dspSampleRate:1;
        u32 unused:25;
    };
} ControlRegister;

typedef union {
    u32 raw;
    struct {
        u8 left;
        u8 right;
        u16 unused; 
    };
} VolumeRegister;

void Init();
int MainThread(void * arg);
void CallDmaInterrupt();
u32 ConsumeAudio(u32 numFrames, StereoFrame * outputBuffer);
void InitDma(u32 startMemHndl, u32 length);

}

#endif
