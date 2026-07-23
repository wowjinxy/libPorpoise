/* pc_gx_internal.h - GX state machine, vertex format, TEV config, GL objects */
#ifndef PC_GX_INTERNAL_H
#define PC_GX_INTERNAL_H

#include "pc_platform.h"

/* Define PC_GL_DEBUG to check for GL errors after significant calls */
#ifdef PC_GL_DEBUG
#define PC_GL_CHECK(label) do { \
    GLenum err_ = glGetError(); \
    if (err_ != GL_NO_ERROR) \
        printf("[GL ERR] %s: 0x%04X at %s:%d\n", label, err_, __FILE__, __LINE__); \
} while(0)
#else
#define PC_GL_CHECK(label) ((void)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- Dirty flags for conditional uniform upload --- */
#define PC_GX_DIRTY_PROJECTION  (1u << 0)
#define PC_GX_DIRTY_MODELVIEW   (1u << 1)
#define PC_GX_DIRTY_TEV_COLORS  (1u << 2)
#define PC_GX_DIRTY_TEV_STAGES  (1u << 3)
#define PC_GX_DIRTY_SWAP_TABLES (1u << 4)
#define PC_GX_DIRTY_KONST       (1u << 5)
#define PC_GX_DIRTY_ALPHA_CMP   (1u << 6)
#define PC_GX_DIRTY_LIGHTING    (1u << 7)
#define PC_GX_DIRTY_TEXGEN      (1u << 8)
#define PC_GX_DIRTY_TEXTURES    (1u << 9)
#define PC_GX_DIRTY_INDIRECT    (1u << 10)
#define PC_GX_DIRTY_FOG         (1u << 11)
#define PC_GX_DIRTY_DEPTH       (1u << 12)
#define PC_GX_DIRTY_COLOR_MASK  (1u << 13)
#define PC_GX_DIRTY_CULL        (1u << 14)
#define PC_GX_DIRTY_BLEND       (1u << 15)
#define PC_GX_DIRTY_ALL         0xFFFFu
#define PC_GX_DIRTY_SET(flag) (g_gx.dirty |= (flag))
#define DIRTY(flag) PC_GX_DIRTY_SET(flag)

/* --- Vertex buffer --- */
#define PC_GX_MAX_VERTS       65536
#define PC_GX_MAX_ATTRIB_SIZE 64
#define PC_GX_MAX_ATTR        26
#define PC_GX_MAX_VTXFMT      8
#define PC_GX_MAX_TEV_STAGES  16
#define PC_GX_MAX_TEXGENS     8

typedef struct {
    int has_position;
    int has_normal;
    int has_color0;
    int has_color1;
    int has_texcoord[8];
    int texcoord_frac[8];
    int position_size;
    int color_size;
    int texcoord_size;
    int stride;
    u8 attr_cnt[PC_GX_MAX_ATTR];
    u8 attr_type[PC_GX_MAX_ATTR];
    u8 attr_frac[PC_GX_MAX_ATTR];
} PCGXVertexFormat;

typedef struct {
    float position[3];
    float normal[3];
    float binormal[3];
    float tangent[3];
    unsigned char color0[4];
    unsigned char color1[4];
    float texcoord[8][2];
    float pn_mtx_idx;
    float tex_mtx_idx[8];
} PCGXVertex;

typedef struct {
    int color_a, color_b, color_c, color_d;
    int alpha_a, alpha_b, alpha_c, alpha_d;
    int color_op, color_bias, color_scale, color_clamp, color_out;
    int alpha_op, alpha_bias, alpha_scale, alpha_clamp, alpha_out;
    int clamp_mode;
    int tex_coord, tex_map, color_chan;
    int tex_lookup_enable;
    int k_color_sel, k_alpha_sel;
    int ras_swap, tex_swap;
    int ind_stage, ind_format, ind_bias, ind_mtx, ind_wrap_s, ind_wrap_t;
    int ind_add_prev, ind_lod, ind_alpha;
} PCGXTevStage;

typedef struct {
    int r, g, b, a;  /* channel indices: 0=R, 1=G, 2=B, 3=A */
} PCGXTevSwapTable;

typedef struct {
    /* Primitive assembly */
    int current_primitive;
    int current_vtxfmt;
    int vertex_count;
    int expected_vertex_count;
    int in_begin;
    PCGXVertex vertex_buffer[PC_GX_MAX_VERTS];
    int current_vertex_idx;
    PCGXVertex current_vertex;
    u8 nbt_normal_phase;
    u8 color_write_phase;
    u8 texcoord_write_phase;
    u8 matrix_index_write_phase;
    int current_tex_mtx_idx[8];

    /* Vertex descriptor */
    int vtx_desc[PC_GX_MAX_ATTR];
    PCGXVertexFormat vtx_fmt[PC_GX_MAX_VTXFMT];
    u8 line_width;
    u8 point_size;
    u32 line_tex_offset;
    u32 point_tex_offset;
    u8 tex_offset_line_enable[8];
    u8 tex_offset_point_enable[8];

    /* Transforms */
    float projection_mtx[4][4];
    int projection_type;
    float pos_mtx[10][3][4];
    float nrm_mtx[10][3][3];
    float tex_mtx[10][3][4];
    float post_tex_mtx[20][3][4];
    int current_mtx;

    /* Viewport & scissor */
    float viewport[6];  /* x, y, w, h, near, far */
    int scissor[4];     /* left, top, w, h */
    int scissor_offset[2]; /* x, y */

    /* TEV */
    int num_tev_stages;
    PCGXTevStage tev_stages[16];
    float tev_colors[4][4];    /* PREV, REG0, REG1, REG2 */
    float tev_k_colors[4][4];
    PCGXTevSwapTable tev_swap_table[4];

    /* Textures */
    int num_tex_gens;
    int tex_gen_type[8];
    int tex_gen_src[8];
    int tex_gen_mtx[8];
    int tex_gen_post_mtx[8];
    int tex_gen_normalize[8];
    u8 texcoord_cyl_wrap_s[8];
    u8 texcoord_cyl_wrap_t[8];
    u8 texcoord_manual_scale_enable[8];
    u16 texcoord_manual_scale_s[8];
    u16 texcoord_manual_scale_t[8];
    GLuint gl_textures[8];
    int tex_obj_w[8];
    int tex_obj_h[8];
    int tex_obj_fmt[8];

    /* Lighting */
    int num_chans;
    float chan_amb_color[2][4];
    float chan_mat_color[2][4];
    int chan_ctrl_enable[4];
    int chan_ctrl_amb_src[4];
    int chan_ctrl_mat_src[4];
    int chan_ctrl_light_mask[4];
    int chan_ctrl_diff_fn[4];
    int chan_ctrl_attn_fn[4];

    struct {
        float pos[3];
        float dir[3];
        float color[4];
        float a0, a1, a2;  /* angular attenuation */
        float k0, k1, k2;  /* distance attenuation */
    } lights[8];

    /* Blend & depth */
    int blend_mode;
    int blend_src;
    int blend_dst;
    int blend_logic_op;
    int z_compare_enable;
    int z_compare_func;
    int z_update_enable;
    int z_comp_loc_before_tex;
    int dither_enable;
    int color_update_enable;
    int alpha_update_enable;
    int dst_alpha_enable;
    int dst_alpha_value;
    int field_mode;
    int field_half_aspect;
    int field_mask_odd;
    int field_mask_even;
    int pixel_fmt;
    int z_fmt;

    /* CPU Direct EFB Access (GXPoke/GXPeek state) */
    int poke_alpha_func;
    int poke_alpha_threshold;
    int poke_alpha_read_mode;
    int poke_alpha_update_enable;
    int poke_color_update_enable;
    int poke_dst_alpha_enable;
    int poke_dst_alpha_value;
    int poke_dither_enable;
    int poke_z_compare_enable;
    int poke_z_compare_func;
    int poke_z_update_enable;
    int poke_blend_mode;
    int poke_blend_src;
    int poke_blend_dst;
    int poke_blend_logic_op;

    /* Alpha compare */
    int alpha_comp0;
    int alpha_ref0;
    int alpha_op;
    int alpha_comp1;
    int alpha_ref1;

    int cull_mode;
    int clip_mode;
    int coplanar_enable;
    int ztex_op;
    int ztex_fmt;
    int ztex_bias;

    /* Fog */
    int fog_type;
    float fog_start, fog_end, fog_near, fog_far;
    float fog_color[4];

    /* TLUT palette storage for CI4/CI8/CI14X2 textures */
    struct {
        const void* data;
        int format;      /* GX_TL_IA8=0, GX_TL_RGB5A3=1 */
        int n_entries;
        int is_be;       /* 1=big-endian (ROM/JSystem), 0=native LE (emu64 tlutconv) */
    } tlut[20];

    /* Indirect textures */
    int num_ind_stages;
    struct {
        int tex_coord;
        int tex_map;
        int scale_s;
        int scale_t;
    } ind_order[4];
    float ind_mtx[3][2][3];
    int   ind_mtx_scale[3];

    /* Deferred vertex commit: position starts vertex, commit on next position or GXEnd */
    int vertex_pending;

    /* GL objects */
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    GLuint current_shader;

    /* Uniform locations (looked up once per shader change) */
    struct {
        GLint projection, modelview, normal_mtx;
        GLint modelview_arr, normal_mtx_arr, use_matrix_array;
        GLint tev_prev, tev_reg0, tev_reg1, tev_reg2;
        GLint num_tev_stages;
        GLint tev_color_in[PC_GX_MAX_TEV_STAGES], tev_alpha_in[PC_GX_MAX_TEV_STAGES];
        GLint tev_color_op[PC_GX_MAX_TEV_STAGES], tev_alpha_op[PC_GX_MAX_TEV_STAGES];
        GLint tev_color_chan[PC_GX_MAX_TEV_STAGES];
        GLint kcolor, tev_ksel;
        GLint alpha_comp0, alpha_ref0, alpha_op, alpha_comp1, alpha_ref1;
        GLint lighting_enabled, mat_color, amb_color;
        GLint chan_mat_src, chan_amb_src, num_chans;
        GLint alpha_lighting_enabled, alpha_mat_src;
        GLint lighting_enabled1, mat_color1, amb_color1;
        GLint chan_mat_src1, chan_amb_src1;
        GLint alpha_lighting_enabled1, alpha_mat_src1;
        GLint chan_diff_fn, chan_attn_fn;
        GLint chan_diff_fn1, chan_attn_fn1;
        GLint light_mask, light_pos[8], light_dir[8], light_color[8];
        GLint light_cos_att[8], light_dist_att[8];
        GLint light_mask1;
        GLint texmtx_enable[PC_GX_MAX_TEXGENS], texmtx_row0[PC_GX_MAX_TEXGENS], texmtx_row1[PC_GX_MAX_TEXGENS],
              texmtx_row2[PC_GX_MAX_TEXGENS];
        GLint texmtxidx_enable[PC_GX_MAX_TEXGENS];
        GLint texmtx_all_row0[10], texmtx_all_row1[10], texmtx_all_row2[10];
        GLint postmtx_enable[PC_GX_MAX_TEXGENS], postmtx_row0[PC_GX_MAX_TEXGENS], postmtx_row1[PC_GX_MAX_TEXGENS],
              postmtx_row2[PC_GX_MAX_TEXGENS];
        GLint texgen_src[PC_GX_MAX_TEXGENS], texgen_type[PC_GX_MAX_TEXGENS], texgen_normalize[PC_GX_MAX_TEXGENS];
        GLint texgen_qt_notcalc[PC_GX_MAX_TEXGENS];
        GLint tcs_cyl_wrap_enable[PC_GX_MAX_TEXGENS];
        GLint tcs_manual_enable[PC_GX_MAX_TEXGENS];
        GLint tcs_manual_scale[PC_GX_MAX_TEXGENS];
        GLint use_texture[PC_GX_MAX_TEV_STAGES];
        GLint texture[PC_GX_MAX_TEV_STAGES];
        GLint tev_tc_src[PC_GX_MAX_TEV_STAGES];
        GLint tev_tex_map[PC_GX_MAX_TEV_STAGES];
        GLint num_ind_stages;
        GLint ind_tex[4], ind_scale[4];
        GLint ind_tc_src[4];
        GLint ind_mtx_r0[PC_GX_MAX_TEV_STAGES], ind_mtx_r1[PC_GX_MAX_TEV_STAGES];
        GLint tev_ind_cfg[PC_GX_MAX_TEV_STAGES], tev_ind_wrap[PC_GX_MAX_TEV_STAGES];
        GLint fog_type, fog_start, fog_end, fog_color;
        GLint ztex_enable, ztex_op, ztex_fmt, ztex_bias, ztex_bias24;
        GLint tev_bsc[PC_GX_MAX_TEV_STAGES], tev_out[PC_GX_MAX_TEV_STAGES];
        GLint tev_clamp_mode[PC_GX_MAX_TEV_STAGES];
        GLint swap_table;
        GLint tev_swap[PC_GX_MAX_TEV_STAGES];
    } uloc;

    float clear_color[4];
    float clear_depth;

    /* Copy/framebuffer */
    int copy_src[4];       /* left, top, w, h */
    int copy_dst[2];       /* w, h */
    int copy_clamp;
    int copy_gamma;
    int copy_frame2field;
    float copy_yscale;
    int copy_yscale_reg;
    u8  copy_sample_pattern[12][2];
    int tex_copy_src[4];
    int tex_copy_dst[2];
    unsigned int tex_copy_fmt;
    int tex_copy_mipmap;
    int copy_aa_enable;
    int copy_vf_enable;
    u8  copy_vfilter[7];
    int bbox_valid;
    u16 bbox_left;
    u16 bbox_top;
    u16 bbox_right;
    u16 bbox_bottom;

    /* Indexed vertex data */
    const void* array_base[PC_GX_MAX_ATTR];
    unsigned char array_stride[PC_GX_MAX_ATTR];

    unsigned int dirty;

} PCGXState;

extern PCGXState g_gx;

typedef struct PCGXShaderCacheEntry {
    uint64_t key;
    GLuint program;
} PCGXShaderCacheEntry;

/* --- Internal functions --- */
void pc_gx_init(void);
void pc_gx_shutdown(void);
void pc_gx_flush_vertices(void);
void pc_gx_flush_if_begin_complete(void);

/* TEV shader */
GLuint pc_gx_tev_get_shader(PCGXState* state);
void   pc_gx_tev_init(void);
void   pc_gx_tev_shutdown(void);

/* Texture cache */
GLuint pc_gx_texture_upload(void* data, int width, int height, int format, int ci_format,
                            void* tlut, int tlut_format, int tlut_count);
void   pc_gx_texture_init(void);
void   pc_gx_texture_shutdown(void);
void   pc_gx_texture_cache_invalidate(void);

#ifdef PC_ENHANCEMENTS
/* EFB capture: store full-res GL texture from GXCopyTex, retrieve on texture load */
void   pc_gx_efb_capture_store(uintptr_t dest_ptr, GLuint gl_tex);
GLuint pc_gx_efb_capture_find(uintptr_t data_ptr);
void   pc_gx_efb_capture_cleanup(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PC_GX_INTERNAL_H */
