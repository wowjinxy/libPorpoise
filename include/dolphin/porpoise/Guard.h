#ifndef DOLPHIN_PORPOISE_GUARD_H
#define DOLPHIN_PORPOISE_GUARD_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PORPOISE_GUARD_MODE_OFF        0
#define PORPOISE_GUARD_MODE_LOG_RETURN 1
#define PORPOISE_GUARD_MODE_PANIC      2

#ifndef PORPOISE_GUARD_MODE
#define PORPOISE_GUARD_MODE PORPOISE_GUARD_MODE_OFF
#endif

void PPGuardFailure(const char* file, int line, const char* expr, const char* detail);

#ifdef __cplusplus
}
#endif

#if PORPOISE_GUARD_MODE == PORPOISE_GUARD_MODE_OFF
#define PP_GUARD_NOTIFY(expr, detail) ((void)0)
#else
#define PP_GUARD_NOTIFY(expr, detail) PPGuardFailure(__FILE__, __LINE__, (expr), (detail))
#endif

#define PP_GUARD_RET(cond, retval, detail) \
    do { \
        if (!(cond)) { \
            PP_GUARD_NOTIFY(#cond, (detail)); \
            return (retval); \
        } \
    } while (0)

#define PP_GUARD_VOID(cond, detail) \
    do { \
        if (!(cond)) { \
            PP_GUARD_NOTIFY(#cond, (detail)); \
            return; \
        } \
    } while (0)

#define PP_GUARD_PTR_RET(ptr, retval) \
    PP_GUARD_RET((ptr) != NULL, (retval), "null pointer")

#define PP_GUARD_NONNEG_RET(value, retval) \
    PP_GUARD_RET((value) >= 0, (retval), "negative value")

#define PP_GUARD_RANGE_RET(offset, length, limit, retval) \
    do { \
        s64 _pp_off = (s64)(offset); \
        s64 _pp_len = (s64)(length); \
        s64 _pp_lim = (s64)(limit); \
        if (_pp_off < 0 || _pp_len < 0 || _pp_off > _pp_lim || _pp_len > (_pp_lim - _pp_off)) { \
            PP_GUARD_NOTIFY("valid range", "offset/length outside bounds"); \
            return (retval); \
        } \
    } while (0)

#endif /* DOLPHIN_PORPOISE_GUARD_H */
