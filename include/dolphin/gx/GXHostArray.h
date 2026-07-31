#ifndef LIBPORPOISE_DOLPHIN_GX_HOST_ARRAY_H
#define LIBPORPOISE_DOLPHIN_GX_HOST_ARRAY_H

#ifdef LIBPORPOISE_PORT

BEGIN_SCOPE_EXTERN_C

void GXSetArrayU32(GXAttr attr, void* base_ptr, u8 stride);

END_SCOPE_EXTERN_C

#ifdef __cplusplus

#define LIBPORPOISE_GX_ARRAY_IS_U32(base_ptr) \
    (__is_same(decltype((base_ptr) + 0), u32*) || \
     __is_same(decltype((base_ptr) + 0), const u32*) || \
     __is_same(decltype((base_ptr) + 0), volatile u32*) || \
     __is_same(decltype((base_ptr) + 0), const volatile u32*))

#define GXSetArray(attr, base_ptr, stride) \
    (LIBPORPOISE_GX_ARRAY_IS_U32(base_ptr) \
         ? GXSetArrayU32((attr), (void*)(base_ptr), (stride)) \
         : GXSetArray((attr), (void*)(base_ptr), (stride)))

#elif defined(__GNUC__) || defined(__clang__)

#define LIBPORPOISE_GX_ARRAY_IS_U32(base_ptr) \
    (__builtin_types_compatible_p(__typeof__(*(base_ptr)), u32) || \
     __builtin_types_compatible_p(__typeof__(*(base_ptr)), const u32) || \
     __builtin_types_compatible_p(__typeof__(*(base_ptr)), volatile u32) || \
     __builtin_types_compatible_p(__typeof__(*(base_ptr)), const volatile u32))

#define GXSetArray(attr, base_ptr, stride) \
    (LIBPORPOISE_GX_ARRAY_IS_U32(base_ptr) \
         ? GXSetArrayU32((attr), (void*)(base_ptr), (stride)) \
         : GXSetArray((attr), (void*)(base_ptr), (stride)))

#endif

#endif

#endif
