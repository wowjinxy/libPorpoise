#include <dolphin/os.h>

#include <atomic>
#include <cstdint>

namespace {

constexpr int MessageCount = 20000;

OSMessageQueue Queue = {};
OSMessage Messages[1] = {};
OSThread Worker = {};
std::atomic<int> Received = 0;
std::atomic<bool> Failed = false;

void* ReceiveMessages(void*) {
    for (int expected = 1; expected <= MessageCount; ++expected) {
        OSMessage message = nullptr;
        if (!OSReceiveMessage(&Queue, &message, OS_MESSAGE_BLOCK)) {
            Failed.store(true, std::memory_order_release);
            return nullptr;
        }
        if (reinterpret_cast<uintptr_t>(message) !=
            static_cast<uintptr_t>(expected)) {
            Failed.store(true, std::memory_order_release);
            return nullptr;
        }
        Received.store(expected, std::memory_order_release);
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x5678));
}

}

extern "C" void SIM_VIInit(void) {
}

extern "C" void SIM_Render(void) {
}

int main() {
    OSMessageQueue orderQueue = {};
    OSMessage orderMessages[3] = {};
    OSInitMessageQueue(&orderQueue, orderMessages, 3);

    if (!OSSendMessage(
            &orderQueue,
            reinterpret_cast<OSMessage>(static_cast<uintptr_t>(1)),
            OS_MESSAGE_NOBLOCK) ||
        !OSSendMessage(
            &orderQueue,
            reinterpret_cast<OSMessage>(static_cast<uintptr_t>(2)),
            OS_MESSAGE_NOBLOCK) ||
        !OSJamMessage(
            &orderQueue,
            reinterpret_cast<OSMessage>(static_cast<uintptr_t>(3)),
            OS_MESSAGE_NOBLOCK)) {
        return 1;
    }
    if (OSSendMessage(
            &orderQueue,
            reinterpret_cast<OSMessage>(static_cast<uintptr_t>(4)),
            OS_MESSAGE_NOBLOCK)) {
        return 2;
    }

    const uintptr_t expectedOrder[] = {3u, 1u, 2u};
    for (uintptr_t expected : expectedOrder) {
        OSMessage message = nullptr;
        if (!OSReceiveMessage(
                &orderQueue,
                &message,
                OS_MESSAGE_NOBLOCK) ||
            reinterpret_cast<uintptr_t>(message) != expected) {
            return 3;
        }
    }
    if (OSReceiveMessage(
            &orderQueue,
            nullptr,
            OS_MESSAGE_NOBLOCK)) {
        return 4;
    }

    OSInitMessageQueue(&Queue, Messages, 1);
    if (!OSCreateThread(
            &Worker,
            ReceiveMessages,
            nullptr,
            nullptr,
            0,
            16,
            0)) {
        return 5;
    }
    if (OSResumeThread(&Worker) != 1) {
        return 6;
    }

    for (int value = 1; value <= MessageCount; ++value) {
        if (!OSSendMessage(
                &Queue,
                reinterpret_cast<OSMessage>(
                    static_cast<uintptr_t>(value)),
                OS_MESSAGE_BLOCK)) {
            return 7;
        }
    }

    void* result = nullptr;
    if (!OSJoinThread(&Worker, &result)) {
        return 8;
    }

    return !Failed.load(std::memory_order_acquire) &&
                   Received.load(std::memory_order_acquire) ==
                       MessageCount &&
                   result ==
                       reinterpret_cast<void*>(
                           static_cast<uintptr_t>(0x5678))
               ? 0
               : 9;
}
