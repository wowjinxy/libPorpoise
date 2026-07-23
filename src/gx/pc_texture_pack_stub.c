#include "pc_texture_pack.h"

int pc_texture_pack_active(void) { return 0; }

GLuint pc_texture_pack_lookup(const void* data, int data_size, int w, int h, unsigned int fmt,
                              const void* tlut_data, int tlut_entries, int tlut_is_be, int* out_w,
                              int* out_h) {
  (void)data;
  (void)data_size;
  (void)w;
  (void)h;
  (void)fmt;
  (void)tlut_data;
  (void)tlut_entries;
  (void)tlut_is_be;
  if (out_w) *out_w = 0;
  if (out_h) *out_h = 0;
  return 0;
}
