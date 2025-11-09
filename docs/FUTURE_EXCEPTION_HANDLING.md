# Future Feature: Platform Exception Handler Integration

**Status:** Planned for v0.2.0+  
**Priority:** Medium  
**Complexity:** High  
**Benefit:** Enables games with custom error handlers to work without modification

---

## Overview

Currently, Porpoise SDK tracks error handlers registered via `OSSetErrorHandler()` but doesn't actually call them when exceptions occur. This is because modern operating systems handle exceptions before user code can intercept them.

This document outlines how we could integrate platform-specific exception handling to call user error handlers, maintaining full compatibility with games that use custom exception handling.

---

## Current Behavior

```c
// Game registers a handler
OSSetErrorHandler(OS_ERROR_DSI, MyDSIHandler);

// Bad memory access occurs
int* ptr = NULL;
*ptr = 42;

// What happens:
// - Windows: Shows "Access Violation" dialog
// - Linux: Segmentation fault
// - Handler is NOT called
```

---

## Proposed Implementation

### Architecture

```
Platform Exception → Signal/SEH Handler → Convert to OSContext → Call User Handler
                                                                        ↓
                                                                   Recover or Crash
```

### Windows Implementation (Structured Exception Handling)

```c
#ifdef _WIN32
#include <windows.h>

static LONG WINAPI PorpoiseExceptionFilter(EXCEPTION_POINTERS* exInfo) {
    OSContext context;
    OSError error;
    u32 dsisr = 0, dar = 0;
    
    // Convert Windows exception to OS error type
    switch (exInfo->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            error = OS_ERROR_DSI;
            dar = exInfo->ExceptionRecord->ExceptionInformation[1];
            dsisr = exInfo->ExceptionRecord->ExceptionInformation[0] ? 
                    0x0A000000 : 0x48000000; // Write vs Read
            break;
            
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            error = OS_ERROR_PROGRAM;
            break;
            
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INVALID_OPERATION:
            error = OS_ERROR_FPE;
            break;
            
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            error = OS_ERROR_ALIGNMENT;
            dar = (u32)exInfo->ExceptionRecord->ExceptionAddress;
            break;
            
        default:
            return EXCEPTION_CONTINUE_SEARCH;
    }
    
    // Convert Windows context to OSContext
    ConvertWindowsContextToOSContext(exInfo->ContextRecord, &context);
    
    // Check if user installed a handler
    OSErrorHandler handler = __OSGetErrorHandler(error);
    if (handler) {
        // Call user handler
        handler(error, &context, dsisr, dar);
        
        // If handler returns, it wants to continue
        // Convert modified OSContext back to Windows context
        ConvertOSContextToWindowsContext(&context, exInfo->ContextRecord);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    
    // No handler - let OS handle it or call __OSUnhandledException
    __OSUnhandledException(error, &context, dsisr, dar);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Install during OSInit()
void InstallExceptionHandler(void) {
    SetUnhandledExceptionFilter(PorpoiseExceptionFilter);
}

// Context conversion helpers
void ConvertWindowsContextToOSContext(CONTEXT* winCtx, OSContext* osCtx) {
    memset(osCtx, 0, sizeof(OSContext));
    
    #ifdef _WIN64
        osCtx->srr0 = (u32)winCtx->Rip;  // Program counter
        osCtx->gpr[1] = (u32)winCtx->Rsp; // Stack pointer
        osCtx->gpr[3] = (u32)winCtx->Rcx; // First param
        osCtx->gpr[4] = (u32)winCtx->Rdx; // Second param
        // ... map other registers ...
    #else
        osCtx->srr0 = (u32)winCtx->Eip;
        osCtx->gpr[1] = (u32)winCtx->Esp;
        osCtx->gpr[3] = (u32)winCtx->Eax;
        // ... map other registers ...
    #endif
}
```

### Linux Implementation (POSIX Signals)

```c
#ifndef _WIN32
#include <signal.h>
#include <ucontext.h>

static void PorpoiseSignalHandler(int sig, siginfo_t* info, void* ctx) {
    ucontext_t* uctx = (ucontext_t*)ctx;
    OSContext context;
    OSError error;
    u32 dsisr = 0, dar = 0;
    
    // Convert signal to OS error type
    switch (sig) {
        case SIGSEGV:
            error = OS_ERROR_DSI;
            dar = (u32)info->si_addr;
            break;
            
        case SIGFPE:
            error = OS_ERROR_FPE;
            break;
            
        case SIGILL:
            error = OS_ERROR_PROGRAM;
            break;
            
        case SIGBUS:
            error = OS_ERROR_ALIGNMENT;
            dar = (u32)info->si_addr;
            break;
            
        default:
            return;
    }
    
    // Convert Linux context to OSContext
    ConvertLinuxContextToOSContext(uctx, &context);
    
    // Check for user handler
    OSErrorHandler handler = __OSGetErrorHandler(error);
    if (handler) {
        handler(error, &context, dsisr, dar);
        
        // If handler returns, convert context back
        ConvertOSContextToLinuxContext(&context, uctx);
        return; // Continue execution
    }
    
    // No handler
    __OSUnhandledException(error, &context, dsisr, dar);
    abort();
}

void InstallExceptionHandler(void) {
    struct sigaction sa;
    sa.sa_sigaction = PorpoiseSignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}

void ConvertLinuxContextToOSContext(ucontext_t* linuxCtx, OSContext* osCtx) {
    memset(osCtx, 0, sizeof(OSContext));
    
    #ifdef __x86_64__
        osCtx->srr0 = (u32)linuxCtx->uc_mcontext.gregs[REG_RIP];
        osCtx->gpr[1] = (u32)linuxCtx->uc_mcontext.gregs[REG_RSP];
        osCtx->gpr[3] = (u32)linuxCtx->uc_mcontext.gregs[REG_RDI];
        // ... map registers ...
    #elif defined(__i386__)
        osCtx->srr0 = (u32)linuxCtx->uc_mcontext.gregs[REG_EIP];
        osCtx->gpr[1] = (u32)linuxCtx->uc_mcontext.gregs[REG_ESP];
        // ... map registers ...
    #endif
}
```

---

## Register Mapping Strategy

### x86-64 → PowerPC Register Mapping

| PowerPC | x86-64 | Purpose |
|---------|--------|---------|
| r0 | (temp) | Volatile |
| r1 | rsp | Stack pointer |
| r2 | (TOC) | RTOC pointer |
| r3 | rdi | First param / return value |
| r4 | rsi | Second param |
| r5 | rdx | Third param |
| r6 | rcx | Fourth param |
| r7 | r8 | Fifth param |
| r8 | r9 | Sixth param |
| ... | ... | Best effort mapping |
| PC (srr0) | rip | Program counter |
| LR | (stack) | Link register (from stack) |

**Note:** Not all registers map cleanly. We do best-effort conversion.

---

## Implementation Plan

### Phase 1: Basic Structure (v0.2.0)
- [ ] Create exception filter/handler stubs
- [ ] Implement context conversion functions
- [ ] Add CMake option: `PORPOISE_ENABLE_EXCEPTION_HANDLERS`
- [ ] Test with simple cases (divide by zero, null pointer)

### Phase 2: Full Integration (v0.3.0)
- [ ] Handle all exception types
- [ ] Support exception recovery (CONTINUE_EXECUTION)
- [ ] Test with games that use error handlers
- [ ] Document limitations and caveats

### Phase 3: Polish (v0.4.0)
- [ ] Improve context conversion accuracy
- [ ] Add unit tests for exception handling
- [ ] Performance optimization
- [ ] Cross-platform testing

---

## Challenges & Considerations

### 1. Context Conversion
**Problem:** x86 and PowerPC have completely different register sets.

**Solution:**
- Map common registers (PC, SP, params, return value)
- Leave other registers as 0
- Document which registers are valid

### 2. Recovery After Exception
**Problem:** Recovering from exceptions is platform-specific and dangerous.

**Solution:**
- On Windows: Return `EXCEPTION_CONTINUE_EXECUTION`
- On Linux: Use `siglongjmp()` or modify `ucontext_t`
- Warn developers that recovery is limited

### 3. Debugger Interference
**Problem:** Our exception handler might interfere with debuggers.

**Solution:**
- Only install if `PORPOISE_ENABLE_EXCEPTION_HANDLERS` is ON
- Check if debugger is attached (`IsDebuggerPresent()`)
- Let debugger have first chance at exceptions

### 4. Nested Exceptions
**Problem:** Exception in exception handler = bad.

**Solution:**
- Disable our handler before calling user handler
- Re-enable after handler returns
- If nested exception occurs, let OS handle it

---

## Benefits vs Costs

### Benefits ✅
- Games with custom error handlers work unchanged
- Better crash reporting (shows error type)
- Potential for error recovery
- More faithful to original SDK behavior

### Costs ⚠️
- Platform-specific code (Windows vs Linux)
- Maintenance burden (two code paths)
- Potential debugger conflicts
- Limited register mapping accuracy
- Most games don't need it (~95% never install handlers)

---

## Decision Matrix

### Implement if:
- ✅ Porting a game that uses custom error handlers
- ✅ Need better crash diagnostics
- ✅ Want to study exception behavior

### Skip if:
- ✅ Game doesn't install error handlers (most don't)
- ✅ Platform debugger is sufficient
- ✅ Want to keep SDK simple

---

## Testing Strategy

### Test Cases
1. **Null Pointer Dereference** → Should trigger OS_ERROR_DSI
2. **Divide by Zero** → Should trigger OS_ERROR_PROGRAM
3. **Unaligned Access** → Should trigger OS_ERROR_ALIGNMENT (if detected)
4. **Invalid Instruction** → Should trigger OS_ERROR_PROGRAM
5. **FPU Exception** → Should trigger OS_ERROR_FPE
6. **Handler Recovery** → Handler modifies context and continues

### Platforms to Test
- Windows 10/11 (x64)
- Windows 10/11 (x86)
- Linux (x86_64)
- Linux (ARM64)
- macOS (x86_64, ARM64)

---

## Code Location

When implemented, code would go in:
- `src/os/OSError.c` - Main implementation
- `src/os/OSError_Win32.c` - Windows-specific
- `src/os/OSError_Posix.c` - POSIX-specific
- `include/dolphin/os/OSError.h` - Public API (no changes needed)

---

## References

### Windows SEH Documentation
- [SetUnhandledExceptionFilter](https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setunhandledexceptionfilter)
- [Structured Exception Handling](https://docs.microsoft.com/en-us/cpp/cpp/structured-exception-handling-c-cpp)

### POSIX Signal Documentation
- [sigaction(2)](https://man7.org/linux/man-pages/man2/sigaction.2.html)
- [Signal-safety](https://man7.org/linux/man-pages/man7/signal-safety.7.html)

### Related Projects
- Dolphin Emulator - Full exception emulation
- RPCS3 - PS3 exception handling on PC
- Wine - Windows exception handling on Linux

---

## Conclusion

This feature would make Porpoise SDK more complete but is **not critical** for most ports. It's a **nice-to-have** rather than essential.

**Recommendation:** Implement only if:
1. Specific game needs it
2. Community requests it
3. Developer has time and expertise

For now, the current implementation (tracking handlers but not calling them) provides **API compatibility** which is sufficient for 95%+ of games.

---

**Last Updated:** 2025-11-09  
**Assigned To:** TBD  
**Estimated Effort:** 40-60 hours

