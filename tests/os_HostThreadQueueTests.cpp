#include <dolphin/os.h>
#include <SDL2/SDL_timer.h>

#include <atomic>
#include <cstdint>

namespace {

OSThreadQueue Queue = {};
OSThread Worker = {};
std::atomic<bool> WorkerEntered = false;
std::atomic<bool> WorkerWoke = false;

void* WaitForWake(void*) {
    WorkerEntered.store(true, std::memory_order_release);
    OSSleepThread(&Queue);
    WorkerWoke.store(true, std::memory_order_release);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234));
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    OSInitThreadQueue(&Queue);
    if (!OSCreateThread(&Worker, WaitForWake, nullptr, nullptr, 0, 16, 0)) {
        return 1;
    }
    if (OSResumeThread(&Worker) != 1) {
        return 2;
    }

    for (int elapsed = 0;
         elapsed < 100 && !WorkerEntered.load(std::memory_order_acquire);
         ++elapsed) {
        SDL_Delay(1);
    }
    if (!WorkerEntered.load(std::memory_order_acquire)) {
        return 3;
    }

    SDL_Delay(10);
    OSWakeupThread(&Queue);

    void* result = nullptr;
    if (!OSJoinThread(&Worker, &result)) {
        return 4;
    }

    return WorkerWoke.load(std::memory_order_acquire) &&
                   result == reinterpret_cast<void*>(static_cast<uintptr_t>(0x1234))
               ? 0
               : 5;
}
