#ifndef PC_TEXTURE_PACK_H
#define PC_TEXTURE_PACK_H

#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Porpoise does not currently support ACPC HD texture packs. */
int pc_texture_pack_active(void);
GLuint pc_texture_pack_lookup(const void* data, int data_size, int w, int h, unsigned int fmt,
                              const void* tlut_data, int tlut_entries, int tlut_is_be, int* out_w,
                              int* out_h);

#ifdef __cplusplus
}
#endif

#endif /* PC_TEXTURE_PACK_H */
