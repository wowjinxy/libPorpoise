# Porpoise SDK API Reference

This document provides a reference for the Porpoise SDK API, organized by module.

## OS Module (dolphin/os.h)

The OS module provides core operating system functionality including initialization, threading, timing, and synchronization.

### Initialization

#### `void OSInit(void)`
Initializes the operating system. Must be called before using any other OS functions.

**Example:**
```c
int main(void) {
    OSInit();
    // Your code here
    return 0;
}
```

#### `BOOL OSIsInitialized(void)`
Checks if the OS has been initialized.

**Returns:** TRUE if initialized, FALSE otherwise.

---

### Thread Management

#### `void OSCreateThread(OSThread* thread, OSThreadFunc func, void* arg, void* stack, u32 stackSize, OSPriority priority, u32 attr)`

Creates a new thread.

**Parameters:**
- `thread` - Pointer to thread structure
- `func` - Thread function to execute
- `arg` - Argument passed to thread function
- `stack` - Pointer to thread stack memory
- `stackSize` - Size of stack in bytes (recommended: 16384+)
- `priority` - Thread priority (0-31, higher = more important)
- `attr` - Thread attributes (usually 0)

**Example:**
```c
static u8 threadStack[16384];
static OSThread myThread;

void MyThreadFunc(void* arg) {
    OSReport("Thread running!\n");
}

OSCreateThread(&myThread, MyThreadFunc, NULL,
               threadStack + 16384, 16384, 16, 0);
OSResumeThread(&myThread);
```

#### `s32 OSResumeThread(OSThread* thread)`
Starts or resumes a thread.

**Returns:** 0 on success, -1 on error.

#### `s32 OSSuspendThread(OSThread* thread)`
Suspends a thread.

**Returns:** 0 on success, -1 on error.

#### `void OSYieldThread(void)`
Yields the current thread's time slice.

#### `void OSSleepThread(void)`
Puts the current thread to sleep briefly (~1ms).

#### `BOOL OSIsThreadTerminated(OSThread* thread)`
Checks if a thread has terminated.

**Returns:** TRUE if terminated, FALSE otherwise.

#### `OSThread* OSGetCurrentThread(void)`
Gets the currently executing thread.

**Returns:** Pointer to current thread.

---

### Mutexes

#### `void OSInitMutex(OSMutex* mutex)`
Initializes a mutex for synchronization.

#### `void OSLockMutex(OSMutex* mutex)`
Locks a mutex. Blocks if already locked.

#### `void OSUnlockMutex(OSMutex* mutex)`
Unlocks a mutex.

#### `BOOL OSTryLockMutex(OSMutex* mutex)`
Attempts to lock a mutex without blocking.

**Returns:** TRUE if locked successfully, FALSE if already locked.

**Example:**
```c
static OSMutex dataMutex;
static int sharedData = 0;

OSInitMutex(&dataMutex);

// In thread code:
OSLockMutex(&dataMutex);
sharedData++;
OSUnlockMutex(&dataMutex);
```

---

### Time Functions

#### `OSTime OSGetTime(void)`
Gets the current system time in ticks.

**Returns:** Time in OS ticks (40.5 MHz equivalent).

#### `OSTick OSGetTick(void)`
Gets the lower 32 bits of the current time.

**Returns:** Time tick value.

#### `void OSTicksToCalendarTime(OSTime ticks, u16* year, u8* month, u8* day, u8* hour, u8* minute, u8* second)`

Converts OS time to calendar time.

**Example:**
```c
OSTime time = OSGetTime();
u16 year;
u8 month, day, hour, minute, second;

OSTicksToCalendarTime(time, &year, &month, &day, 
                     &hour, &minute, &second);
OSReport("Time: %04d-%02d-%02d %02d:%02d:%02d\n",
         year, month, day, hour, minute, second);
```

---

### Alarms

#### `void OSCreateAlarm(OSAlarm* alarm)`
Initializes an alarm structure.

#### `void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)`
Sets a one-shot alarm.

**Parameters:**
- `alarm` - Alarm structure
- `tick` - Ticks until alarm fires
- `handler` - Function called when alarm fires

#### `void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler)`
Sets a repeating alarm.

#### `void OSCancelAlarm(OSAlarm* alarm)`
Cancels a pending alarm.

---

### Utility Functions

#### `void OSReport(const char* fmt, ...)`
Prints formatted output to stdout.

**Example:**
```c
OSReport("Value: %d\n", 42);
OSReport("String: %s\n", "Hello");
```

#### `u32 OSGetConsoleType(void)`
Gets the console type identifier.

**Returns:** Console type (0x10000000 for PC port).

---

## Types Module (dolphin/types.h)

Provides standard type definitions matching GC/Wii SDK conventions.

### Integer Types
- `u8`, `u16`, `u32`, `u64` - Unsigned integers
- `s8`, `s16`, `s32`, `s64` - Signed integers

### Floating Point Types
- `f32` - 32-bit float
- `f64` - 64-bit double

### Volatile Types
- `vu8`, `vu16`, `vu32`, `vu64` - Volatile unsigned
- `vs8`, `vs16`, `vs32`, `vs64` - Volatile signed

### Macros
- `ROUND_UP(x, align)` - Round up to alignment
- `ROUND_DOWN(x, align)` - Round down to alignment
- `ALIGN(x, align)` - Align value
- `ARRAY_LENGTH(arr)` - Get array element count
- `MIN(a, b)` - Minimum value
- `MAX(a, b)` - Maximum value
- `CLAMP(x, min, max)` - Clamp value to range

---

## Platform Compatibility Notes

### Threading
- Maps to Windows threads (Win32) or POSIX threads (Unix)
- Priority mapping may differ from original hardware
- Stack sizes should be generous (16KB minimum recommended)

### Timing
- Uses high-resolution timers on all platforms
- Time base approximates GC timer frequency (40.5 MHz)
- Actual resolution depends on platform capabilities

### Mutex Behavior
- Uses critical sections (Windows) or pthread mutexes (Unix)
- Behavior matches original SDK expectations
- Recursive locking not currently supported

---

## Coming Soon

The following modules are planned for future releases:

- **GX** - Graphics subsystem
- **PAD** - Controller input
- **CARD** - Memory card operations
- **DVD** - Disc I/O
- **AX** - Audio subsystem

Stay tuned for updates!

