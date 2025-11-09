#include <dolphin/os.h>
#include <stdlib.h>

#define THREAD_STACK_SIZE 16384

static u8 threadStack[THREAD_STACK_SIZE];
static OSThread testThread;
static OSMutex testMutex;
static int sharedCounter = 0;

void* ThreadFunction(void* arg) {
    int threadId = *(int*)arg;
    
    OSReport("Thread %d started!\n", threadId);
    
    for (int i = 0; i < 5; i++) {
        OSLockMutex(&testMutex);
        sharedCounter++;
        OSReport("Thread %d: counter = %d\n", threadId, sharedCounter);
        OSUnlockMutex(&testMutex);
        
        OSSleepThread(NULL);
    }
    
    OSReport("Thread %d finished!\n", threadId);
    return NULL;
}

int main(void) {
    OSReport("Thread test example\n");
    OSReport("===================\n\n");
    
    // Initialize OS
    OSInit();
    
    // Initialize mutex
    OSInitMutex(&testMutex);
    OSReport("Mutex initialized\n");
    
    // Create thread
    int threadId = 1;
    OSCreateThread(&testThread, ThreadFunction, &threadId,
                   threadStack + THREAD_STACK_SIZE, THREAD_STACK_SIZE,
                   16, 0);
    OSReport("Thread created\n");
    
    // Start the thread
    OSResumeThread(&testThread);
    OSReport("Thread started\n\n");
    
    // Main thread also increments counter
    for (int i = 0; i < 5; i++) {
        OSLockMutex(&testMutex);
        sharedCounter++;
        OSReport("Main thread: counter = %d\n", sharedCounter);
        OSUnlockMutex(&testMutex);
        
        OSSleepThread(NULL);
    }
    
    // Wait for thread to complete
    while (!OSIsThreadTerminated(&testThread)) {
        OSSleepThread(NULL);
    }
    
    OSReport("\nThread test completed!\n");
    OSReport("Final counter value: %d\n", sharedCounter);
    
    return 0;
}

