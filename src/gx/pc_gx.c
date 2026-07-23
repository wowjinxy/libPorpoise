/* pc_gx.c - GX API â†’ OpenGL 3.3: state management, vertex submission, draw dispatch */
#include "pc_gx_internal.h"
#include <stddef.h>
static GLushort quad_index_buf[(PC_GX_MAX_VERTS / 4) * 6];
#include <math.h>
#include <dolphin/gx/GXCommandList.h>
#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXManage.h>
#include <dolphin/demo/DEMOInit.h>
#include <dolphin/os/OSThread.h>
#include <dolphin/vi.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* Forward declarations for internal cross-calls in this translation unit. */
void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d);
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d);
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg);
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg);
void GXSetNumIndStages(u8 n);
void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map);
void GXSetIndTexCoordScale(GXIndTexStageID ind_stage, GXIndTexScale scale_s, GXIndTexScale scale_t);
u32 GXCompressZ16(u32 z24, GXZFmt16 zfmt);
u32 GXDecompressZ16(u32 z16, GXZFmt16 zfmt);
static inline u16 pc_gx_pack_rgb565(const u8* p);
static void pc_gx_bbox_touch_draw(void);
static void pc_gx_apply_viewport(void);
static void pc_gx_apply_scissor(void);
static float pc_gx_quantize_ind_mtx_coeff(float v);

/* --- Global GX State --- */
PCGXState g_gx;
static int g_pc_gx_initialized = 0;
static GXDrawSyncCallback g_draw_sync_cb = NULL;
static GXDrawDoneCallback g_draw_done_cb = NULL;
static GXVerifyCallback g_verify_cb = NULL;
static GXVerifyLevel g_verify_level = GX_WARN_NONE;
static GXBreakPtCallback g_breakpt_cb = NULL;
static void* g_breakpt_addr = NULL;
static OSThread* g_current_gx_thread = NULL;
static u16 g_draw_sync_token = 0;
static GXBool g_draw_done_pending = GX_FALSE;
static volatile void* g_write_gather_redirect = NULL;
static volatile void* g_write_gather_prev = NULL;
static PCGXVertex g_pc_gx_expanded_prim[PC_GX_MAX_VERTS * 3];
/* BP register decode shadow for PE control/cmode1 pixel-format coupling. */
static int g_bp_pe_pixfmt_bits = 0;
static int g_bp_cmode1_pix_subfmt = 0;

/* Triangle strip CPU-expansion policy:
 * Default preserves GL/GX strip face semantics by swapping odd triangles.
 * For debugging, disable the odd-triangle swap with:
 *   PC_GX_DISABLE_STRIP_ODD_SWAP=1
 */
static int pc_gx_disable_strip_odd_swap(void) {
    static int init = 0;
    static int disable_swap = 0;
    if (!init) {
        const char* v = getenv("PC_GX_DISABLE_STRIP_ODD_SWAP");
        disable_swap = (v && v[0] != '\0' && v[0] != '0') ? 1 : 0;
        init = 1;
    }
    return disable_swap;
}

/* GX raster/front-face convention matches clockwise front faces in window space.
 * Allow override for debugging:
 *   PC_GX_FRONT_FACE_CCW=1
 *   PC_GX_FRONT_FACE_CW=1
 */
static int pc_gx_front_face_is_cw(void) {
    static int init = 0;
    static int use_cw = 1;
    if (!init) {
        const char* ccw = getenv("PC_GX_FRONT_FACE_CCW");
        const char* cw = getenv("PC_GX_FRONT_FACE_CW");
        if (ccw && ccw[0] != '\0' && ccw[0] != '0') {
            use_cw = 0;
        }
        if (cw && cw[0] != '\0' && cw[0] != '0') {
            use_cw = 1;
        }
        init = 1;
    }
    return use_cw;
}

static int pc_gx_should_use_matrix_array(const PCGXVertex* verts, int count) {
    int base_idx;

    if (!verts || count <= 0) return 0;

    base_idx = (int)(verts[0].pn_mtx_idx + 0.5f);
    if (base_idx < 0) base_idx = 0;
    if (base_idx > 9) base_idx = 9;

    for (int i = 1; i < count; ++i) {
        int idx = (int)(verts[i].pn_mtx_idx + 0.5f);
        if (idx < 0) idx = 0;
        if (idx > 9) idx = 9;
        if (idx != base_idx) {
            return 1;
        }
    }

    return base_idx != g_gx.current_mtx;
}

/* Common render modes expected by SDK demos. */
GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
};
GXRenderModeObj GXNtsc480IntAa = {
    VI_TVMODE_NTSC_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 1,
};
GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT, 704, 528, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
};
GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_PAL_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
};

static int pc_gx_debug_enabled(void) {
    static int init = 0;
    static int enabled = 0;
    if (!init) {
        const char* v = getenv("PC_GX_DEBUG");
        enabled = (v && v[0] != '\0' && v[0] != '0') ? 1 : 0;
        init = 1;
    }
    return enabled;
}

static void pc_gx_debug_log(const char* msg) {
    FILE* f = fopen("pcgx_debug.log", "ab");
    if (!f) return;
    fputs(msg, f);
    fputc('\n', f);
    fclose(f);
}

static float pc_gx_quantize_ind_mtx_coeff(float v) {
    /* SDK/HW stores indirect matrix coefficients as signed 11-bit with 10 fractional bits. */
    int q = ((int)(v * 1024.0f)) & 0x7FF;
    if (q & 0x400) q -= 0x800;
    return (float)q / 1024.0f;
}

/* Convert to TEV register signed-11 storage domain (-1024..1023). */
static inline s16 pc_gx_wrap_s10_to_tev11(s32 value) {
    u16 raw = (u16)((u32)value & 0x07FFu);
    return (raw & 0x0400u) ? (s16)(raw | 0xF800u) : (s16)raw;
}

#ifdef PC_ENHANCEMENTS
/* Aspect correction: factor = gc_aspect/actual_aspect, offset = content left edge in GC coords */
static float g_aspect_factor = 1.0f;
static float g_aspect_offset = 0.0f;
static int   g_aspect_active = 0;

static void pc_gx_update_aspect(void) {
    float gc_aspect = (float)PC_GC_WIDTH / (float)PC_GC_HEIGHT;
    float win_aspect = (float)g_pc_window_w / (float)g_pc_window_h;
    if (win_aspect > gc_aspect + 0.01f) {
        g_aspect_factor = gc_aspect / win_aspect;
        g_aspect_offset = (1.0f - g_aspect_factor) / 2.0f * (float)PC_GC_WIDTH;
        g_aspect_active = 1;
    } else {
        g_aspect_factor = 1.0f;
        g_aspect_offset = 0.0f;
        g_aspect_active = 0;
    }
}

/* EFB capture: keep full-res GL textures from GXCopyTex instead of downsampling to 640x480 */
#define MAX_EFB_CAPTURES 4
static struct {
    uintptr_t dest_ptr;
    GLuint gl_tex;
} s_efb_captures[MAX_EFB_CAPTURES];
static int s_efb_capture_count = 0;

void pc_gx_efb_capture_store(uintptr_t dest_ptr, GLuint gl_tex) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].dest_ptr == dest_ptr) {
            if (s_efb_captures[i].gl_tex)
                glDeleteTextures(1, &s_efb_captures[i].gl_tex);
            s_efb_captures[i].gl_tex = gl_tex;
            return;
        }
    }
    if (s_efb_capture_count >= MAX_EFB_CAPTURES) {
        if (s_efb_captures[0].gl_tex)
            glDeleteTextures(1, &s_efb_captures[0].gl_tex);
        memmove(&s_efb_captures[0], &s_efb_captures[1],
                (MAX_EFB_CAPTURES - 1) * sizeof(s_efb_captures[0]));
        s_efb_capture_count = MAX_EFB_CAPTURES - 1;
    }
    s_efb_captures[s_efb_capture_count].dest_ptr = dest_ptr;
    s_efb_captures[s_efb_capture_count].gl_tex = gl_tex;
    s_efb_capture_count++;
}

GLuint pc_gx_efb_capture_find(uintptr_t data_ptr) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].dest_ptr == data_ptr)
            return s_efb_captures[i].gl_tex;
    }
    return 0;
}

void pc_gx_efb_capture_cleanup(void) {
    for (int i = 0; i < s_efb_capture_count; i++) {
        if (s_efb_captures[i].gl_tex)
            glDeleteTextures(1, &s_efb_captures[i].gl_tex);
    }
    s_efb_capture_count = 0;
}
#endif

typedef struct {
    int active;
    u8* buf;
    u32 size;
    u32 off;
    int overflow;
} PCGXDLBuildState;

static PCGXDLBuildState g_pc_gx_dl = {0};

enum {
    PCGX_DL_OP_TEXCOPY_SRC = 0x1001,
    PCGX_DL_OP_TEXCOPY_DST = 0x1002,
    PCGX_DL_OP_COPY_FILTER = 0x1003,
    PCGX_DL_OP_COPY_TEX = 0x1004,
    PCGX_DL_OP_DRAW_BATCH = 0x1005,
};

static void pc_gx_dl_write(const void* data, u32 len) {
    if (!g_pc_gx_dl.active || g_pc_gx_dl.overflow) return;
    if (g_pc_gx_dl.off + len > g_pc_gx_dl.size) {
        g_pc_gx_dl.overflow = 1;
        return;
    }
    memcpy(g_pc_gx_dl.buf + g_pc_gx_dl.off, data, len);
    g_pc_gx_dl.off += len;
}

static void pc_gx_dl_record_draw_batch(u32 primitive, const PCGXVertex* verts, u32 count) {
    u32 op;
    u32 bytes;
    if (!g_pc_gx_dl.active || g_pc_gx_dl.overflow) return;
    if (!verts || count == 0 || count > PC_GX_MAX_VERTS) return;

    op = PCGX_DL_OP_DRAW_BATCH;
    bytes = count * (u32)sizeof(PCGXVertex);

    pc_gx_dl_write(&op, sizeof(op));
    pc_gx_dl_write(&primitive, sizeof(primitive));
    pc_gx_dl_write(&count, sizeof(count));
    pc_gx_dl_write(verts, bytes);
}

static u16 pc_gx_be16(const u8* p) {
    return (u16)(((u16)p[0] << 8) | (u16)p[1]);
}

static u32 pc_gx_be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static float pc_gx_be_f32(const u8* p) {
    union {
        u32 u;
        float f;
    } cvt;
    cvt.u = pc_gx_be32(p);
    return cvt.f;
}

static void pc_unpack_rgba8f(u32 packed, float* out_rgba) {
    /* Shift-based: works for colors packed as (r<<24|g<<16|b<<8|a) from N64 DL data */
    out_rgba[0] = ((packed >> 24) & 0xFF) / 255.0f;
    out_rgba[1] = ((packed >> 16) & 0xFF) / 255.0f;
    out_rgba[2] = ((packed >> 8) & 0xFF) / 255.0f;
    out_rgba[3] = (packed & 0xFF) / 255.0f;
}

/* Byte-based: reads RGBA from memory order. Use for GXColor structs (not N64 DL data). */
static void pc_unpack_gxcolor_f(u32 color_as_u32, float* out_rgba) {
    const u8* bytes = (const u8*)&color_as_u32;
    out_rgba[0] = bytes[0] / 255.0f;
    out_rgba[1] = bytes[1] / 255.0f;
    out_rgba[2] = bytes[2] / 255.0f;
    out_rgba[3] = bytes[3] / 255.0f;
}

static u32 pc_pack_gxcolor_u32(GXColor color) {
    return ((u32)color.r) | ((u32)color.g << 8) | ((u32)color.b << 16) | ((u32)color.a << 24);
}

/* Map first-pass tex matrix ID to slot: raw 0..9, GX enum 30..57 (stride 3), or 60=identity */
static int pc_tex_mtx_id_to_slot(int id) {
    if (id == GX_IDENTITY) return -1;
    if (id >= 0 && id < 10) return id;
    if (id >= GX_TEXMTX0 && id < GX_IDENTITY) return (id - GX_TEXMTX0) / 3;
    return -1;
}

/* Map post-transform tex matrix ID to slot: raw 0..19, GX enum 64..121 (stride 3), or 125=identity. */
static int pc_pt_tex_mtx_id_to_slot(int id) {
    if (id == GX_PTIDENTITY) return -1;
    if (id >= 0 && id < 20) return id;
    if (id >= GX_PTTEXMTX0 && id < GX_PTIDENTITY) return (id - GX_PTTEXMTX0) / 3;
    return -1;
}

static int pc_gx_texgen_func_valid(int func) {
    return (func >= GX_TG_MTX3x4 && func <= GX_TG_SRTG);
}

static int pc_gx_texgen_is_regular(int func) {
    return (func == GX_TG_MTX3x4 || func == GX_TG_MTX2x4);
}

static int pc_gx_texgen_is_bump(int func) {
    return (func >= GX_TG_BUMP0 && func <= GX_TG_BUMP7);
}

static int pc_gx_texgen_src_valid(int func, int src) {
    if (pc_gx_texgen_is_regular(func)) {
        return (src >= GX_TG_POS && src <= GX_TG_TEX7);
    }
    if (pc_gx_texgen_is_bump(func)) {
        return (src >= GX_TG_TEXCOORD0 && src <= GX_TG_TEXCOORD6);
    }
    if (func == GX_TG_SRTG) {
        return (src == GX_TG_COLOR0 || src == GX_TG_COLOR1);
    }
    return 0;
}

static int pc_gx_texgen_mtx_valid(int func, int mtx) {
    if (!pc_gx_texgen_is_regular(func)) {
        return 1; /* Unused for bump/SRTG. */
    }
    return (pc_tex_mtx_id_to_slot(mtx) >= 0 || mtx == GX_IDENTITY);
}

static int pc_gx_texgen_post_mtx_valid(int postmtx) {
    return (pc_pt_tex_mtx_id_to_slot(postmtx) >= 0 || postmtx == GX_PTIDENTITY);
}

static int pc_gx_texgen_only_pos_and_single_texcoord(void) {
    int tex_count = 0;

    if (g_gx.vtx_desc[GX_VA_POS] == GX_NONE) {
        return 0;
    }

    if (g_gx.vtx_desc[GX_VA_NRM] != GX_NONE || g_gx.vtx_desc[GX_VA_NBT] != GX_NONE ||
        g_gx.vtx_desc[GX_VA_CLR0] != GX_NONE || g_gx.vtx_desc[GX_VA_CLR1] != GX_NONE) {
        return 0;
    }

    for (int i = GX_VA_TEX0; i <= GX_VA_TEX7; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            tex_count++;
        }
    }
    if (tex_count != 1) {
        return 0;
    }

    for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7MTXIDX; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            return 0;
        }
    }

    return 1;
}

/* Commit pending vertex + flush batch to GL. Used by GXBegin/GXEnd/GXCopyDisp/etc. */
static void pc_gx_commit_pending_and_flush(void) {
    if (!g_gx.in_begin) return;
    if (g_gx.vertex_pending && g_gx.current_vertex_idx < PC_GX_MAX_VERTS) {
        g_gx.vertex_buffer[g_gx.current_vertex_idx] = g_gx.current_vertex;
        g_gx.current_vertex_idx++;
        g_gx.vertex_pending = 0;
    }
    g_gx.in_begin = 0;
    if (g_gx.current_vertex_idx > 0)
        pc_gx_flush_vertices();
}

/* emu64 omits GXEnd() â€” flush when expected vertex count is reached so the
 * batch renders with the state it was built with, not subsequent state changes. */
void pc_gx_flush_if_begin_complete(void) {
    if (!g_gx.in_begin || g_gx.expected_vertex_count <= 0) return;

    int submitted = g_gx.current_vertex_idx + (g_gx.vertex_pending ? 1 : 0);
    if (submitted < g_gx.expected_vertex_count) return;

    pc_gx_commit_pending_and_flush();
}

int pc_emu64_frame_cmds = 0;
int pc_emu64_frame_crashes = 0;
int pc_emu64_frame_noop_cmds = 0;
int pc_emu64_frame_tri_cmds = 0;
int pc_emu64_frame_vtx_cmds = 0;
int pc_emu64_frame_dl_cmds = 0;
int pc_emu64_frame_cull_visible = 0;
int pc_emu64_frame_cull_rejected = 0;

static int pc_gx_ensure_gl_context(void) {
    SDL_Window* win = VIGetSDLWindow();
    SDL_GLContext ctx = VIGetGLContext();

    if (!win || !ctx) {
        /* Some games call GXInit before VIInit (e.g. Pikmin). */
        VIInit();
        win = VIGetSDLWindow();
        ctx = VIGetGLContext();
    }

    if (!win || !ctx) {
        fprintf(stderr, "pc_gx_init: no SDL window/GL context available\n");
        return 0;
    }

    VIMakeContextCurrent();
    if (SDL_GL_GetCurrentContext() != ctx) {
        fprintf(stderr, "pc_gx_init: GL context is not current after VIMakeContextCurrent (%s)\n", SDL_GetError());
        return 0;
    }

    return 1;
}

void pc_gx_init(void) {
    if (g_pc_gx_initialized) return;

    if (!pc_gx_ensure_gl_context()) {
        return;
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "pc_gx_init: gladLoadGL failed (current_ctx=%p, glGetString=%p, sdl_err=%s)\n",
                SDL_GL_GetCurrentContext(), SDL_GL_GetProcAddress("glGetString"), SDL_GetError());
        return;
    }

    memset(&g_gx, 0, sizeof(g_gx));

    g_gx.projection_type = GX_PERSPECTIVE;
    g_gx.num_tev_stages = 1;
    g_gx.num_chans = 0;
    g_gx.num_tex_gens = 1;
    for (int i = 0; i < 8; i++) {
        g_gx.tex_gen_type[i] = GX_TG_MTX2x4;
        g_gx.tex_gen_src[i] = GX_TG_TEX0 + i;
        g_gx.tex_gen_mtx[i] = GX_IDENTITY;
        g_gx.tex_gen_post_mtx[i] = GX_PTIDENTITY;
        g_gx.tex_gen_normalize[i] = GX_FALSE;
        g_gx.texcoord_cyl_wrap_s[i] = 0;
        g_gx.texcoord_cyl_wrap_t[i] = 0;
        g_gx.texcoord_manual_scale_enable[i] = 0;
        g_gx.texcoord_manual_scale_s[i] = 1;
        g_gx.texcoord_manual_scale_t[i] = 1;
    }
    g_gx.cull_mode = GX_CULL_BACK;
    g_gx.clip_mode = GX_CLIP_ENABLE;
    g_gx.coplanar_enable = GX_FALSE;
    g_gx.ztex_op = GX_ZT_DISABLE;
    g_gx.ztex_fmt = GX_TF_Z24X8;
    g_gx.ztex_bias = 0;
    g_gx.z_compare_enable = 1;
    g_gx.z_compare_func = GX_LEQUAL;
    g_gx.z_update_enable = 1;
    g_gx.z_comp_loc_before_tex = 1;
    g_gx.dither_enable = 1;
    g_gx.color_update_enable = 1;
    g_gx.alpha_update_enable = 1;
    g_gx.dst_alpha_enable = 0;
    g_gx.dst_alpha_value = 0;
    g_gx.field_mode = 0;
    g_gx.field_half_aspect = 0;
    g_gx.field_mask_odd = 1;
    g_gx.field_mask_even = 1;
    g_gx.pixel_fmt = GX_PF_RGB8_Z24;
    g_gx.z_fmt = GX_ZC_LINEAR;
    g_bp_pe_pixfmt_bits = 0;
    g_bp_cmode1_pix_subfmt = 0;
    g_gx.poke_alpha_func = GX_ALWAYS;
    g_gx.poke_alpha_threshold = 0;
    g_gx.poke_alpha_read_mode = GX_READ_FF;
    g_gx.poke_alpha_update_enable = 1;
    g_gx.poke_color_update_enable = 1;
    g_gx.poke_dst_alpha_enable = 0;
    g_gx.poke_dst_alpha_value = 0;
    g_gx.poke_dither_enable = 0;
    g_gx.poke_z_compare_enable = 1;
    g_gx.poke_z_compare_func = GX_ALWAYS;
    g_gx.poke_z_update_enable = 1;
    g_gx.poke_blend_mode = GX_BM_NONE;
    g_gx.poke_blend_src = GX_BL_ONE;
    g_gx.poke_blend_dst = GX_BL_ZERO;
    g_gx.poke_blend_logic_op = GX_LO_SET;
    g_gx.alpha_comp0 = GX_ALWAYS;
    g_gx.alpha_ref0 = 0;
    g_gx.alpha_op = GX_AOP_AND;
    g_gx.alpha_comp1 = GX_ALWAYS;
    g_gx.alpha_ref1 = 0;
    g_gx.blend_mode = GX_BM_NONE;
    g_gx.blend_src = GX_BL_ONE;
    g_gx.blend_dst = GX_BL_ZERO;
    g_gx.copy_clamp = GX_CLAMP_TOP | GX_CLAMP_BOTTOM;
    g_gx.copy_gamma = GX_GM_1_0;
    g_gx.copy_frame2field = GX_COPY_PROGRESSIVE;
    g_gx.copy_yscale = 1.0f;
    g_gx.copy_yscale_reg = 0x100;
    g_gx.copy_aa_enable = 0;
    g_gx.copy_vf_enable = 0;
    memset(g_gx.copy_vfilter, 0, sizeof(g_gx.copy_vfilter));
    for (int i = 0; i < 12; ++i) {
        g_gx.copy_sample_pattern[i][0] = 6;
        g_gx.copy_sample_pattern[i][1] = 6;
    }
    g_gx.bbox_valid = 0;
    g_gx.bbox_left = 0;
    g_gx.bbox_top = 0;
    g_gx.bbox_right = 0;
    g_gx.bbox_bottom = 0;
    g_gx.line_width = 6;   /* 1 px in GX's 1/6-pixel units */
    g_gx.point_size = 6;   /* 1 px in GX's 1/6-pixel units */
    g_gx.line_tex_offset = GX_TO_ZERO;
    g_gx.point_tex_offset = GX_TO_ZERO;
    memset(g_gx.tex_offset_line_enable, 0, sizeof(g_gx.tex_offset_line_enable));
    memset(g_gx.tex_offset_point_enable, 0, sizeof(g_gx.tex_offset_point_enable));
    g_gx.scissor_offset[0] = 0;
    g_gx.scissor_offset[1] = 0;
    g_gx.clear_color[3] = 0.0f;
    g_gx.clear_depth = 1.0f;

    for (int i = 0; i < 4; i++)
        g_gx.projection_mtx[i][i] = 1.0f;

    for (int i = 0; i < 10; i++) {
        g_gx.pos_mtx[i][0][0] = 1.0f;
        g_gx.pos_mtx[i][1][1] = 1.0f;
        g_gx.pos_mtx[i][2][2] = 1.0f;
    }

    for (int i = 0; i < 10; i++) {
        g_gx.nrm_mtx[i][0][0] = 1.0f;
        g_gx.nrm_mtx[i][1][1] = 1.0f;
        g_gx.nrm_mtx[i][2][2] = 1.0f;
    }

    for (int i = 0; i < 20; i++) {
        g_gx.post_tex_mtx[i][0][0] = 1.0f;
        g_gx.post_tex_mtx[i][1][1] = 1.0f;
        g_gx.post_tex_mtx[i][2][2] = 1.0f;
    }

    g_gx.tev_swap_table[0] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[1] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[2] = (PCGXTevSwapTable){0, 1, 2, 3};
    g_gx.tev_swap_table[3] = (PCGXTevSwapTable){0, 1, 2, 3};

    for (int i = 0; i < 2; i++) {
        g_gx.chan_mat_color[i][0] = 1.0f;
        g_gx.chan_mat_color[i][1] = 1.0f;
        g_gx.chan_mat_color[i][2] = 1.0f;
        g_gx.chan_mat_color[i][3] = 1.0f;
    }
    for (int i = 0; i < 4; i++) {
        g_gx.chan_ctrl_enable[i] = GX_FALSE;
        g_gx.chan_ctrl_amb_src[i] = GX_SRC_REG;
        g_gx.chan_ctrl_mat_src[i] = GX_SRC_VTX;
        g_gx.chan_ctrl_light_mask[i] = GX_LIGHT_NULL;
        g_gx.chan_ctrl_diff_fn[i] = GX_DF_NONE;
        g_gx.chan_ctrl_attn_fn[i] = GX_AF_NONE;
    }

    /* Hardware-like TEV stage 0 defaults: pass rasterized color/alpha. */
    g_gx.tev_stages[0].color_a = GX_CC_ZERO;
    g_gx.tev_stages[0].color_b = GX_CC_ZERO;
    g_gx.tev_stages[0].color_c = GX_CC_ZERO;
    g_gx.tev_stages[0].color_d = GX_CC_RASC;
    g_gx.tev_stages[0].alpha_a = GX_CA_ZERO;
    g_gx.tev_stages[0].alpha_b = GX_CA_ZERO;
    g_gx.tev_stages[0].alpha_c = GX_CA_ZERO;
    g_gx.tev_stages[0].alpha_d = GX_CA_RASA;
    g_gx.tev_stages[0].color_op = GX_TEV_ADD;
    g_gx.tev_stages[0].alpha_op = GX_TEV_ADD;
    g_gx.tev_stages[0].color_bias = GX_TB_ZERO;
    g_gx.tev_stages[0].alpha_bias = GX_TB_ZERO;
    g_gx.tev_stages[0].color_scale = GX_CS_SCALE_1;
    g_gx.tev_stages[0].alpha_scale = GX_CS_SCALE_1;
    g_gx.tev_stages[0].color_clamp = GX_TRUE;
    g_gx.tev_stages[0].alpha_clamp = GX_TRUE;
    g_gx.tev_stages[0].clamp_mode = GX_TC_LINEAR;
    g_gx.tev_stages[0].color_out = GX_TEVPREV;
    g_gx.tev_stages[0].alpha_out = GX_TEVPREV;
    g_gx.tev_stages[0].tex_coord = GX_TEXCOORD0;
    g_gx.tev_stages[0].tex_map = GX_TEXMAP0;
    g_gx.tev_stages[0].color_chan = GX_COLOR0A0;
    g_gx.tev_stages[0].tex_lookup_enable = 0;

    /* Quad-to-triangle index buffer */
    for (int q = 0; q < PC_GX_MAX_VERTS / 4; q++) {
        int base = q * 4;
        quad_index_buf[q * 6 + 0] = base + 0;
        quad_index_buf[q * 6 + 1] = base + 1;
        quad_index_buf[q * 6 + 2] = base + 2;
        quad_index_buf[q * 6 + 3] = base + 0;
        quad_index_buf[q * 6 + 4] = base + 2;
        quad_index_buf[q * 6 + 5] = base + 3;
    }

    glGenVertexArrays(1, &g_gx.vao);
    glGenBuffers(1, &g_gx.vbo);
    glGenBuffers(1, &g_gx.ebo);

    /* VAO setup: attrib pointers persist since PCGXVertex layout and VBO ID never change */
    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);
    glBufferData(GL_ARRAY_BUFFER, PC_GX_MAX_VERTS * sizeof(PCGXVertex), NULL, GL_STREAM_DRAW);
    {
        size_t stride = sizeof(PCGXVertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, normal));
        glEnableVertexAttribArray(15);
        glVertexAttribPointer(15, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, binormal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(PCGXVertex, color0));
        glEnableVertexAttribArray(12);
        glVertexAttribPointer(12, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(PCGXVertex, color1));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, pn_mtx_idx));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[1]));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[2]));
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[3]));
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[4]));
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[5]));
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[6]));
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, texcoord[7]));
        glEnableVertexAttribArray(13);
        glVertexAttribPointer(13, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, tex_mtx_idx[0]));
        glEnableVertexAttribArray(14);
        glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(PCGXVertex, tex_mtx_idx[4]));
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gx.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_index_buf), quad_index_buf, GL_STATIC_DRAW);

    glBindVertexArray(0);

    pc_gx_tev_init();
    pc_gx_texture_init();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glFrontFace(pc_gx_front_face_is_cw() ? GL_CW : GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
#ifdef GL_DEPTH_CLAMP
    glDisable(GL_DEPTH_CLAMP);
#endif
    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, 0.0f);

    g_gx.dirty = PC_GX_DIRTY_ALL;
    g_pc_gx_initialized = 1;
}

void pc_gx_begin_frame(void) {
    if (!g_pc_gx_initialized) {
        pc_gx_init();
        if (!g_pc_gx_initialized) return;
    }

    pc_emu64_frame_cmds = 0;
    pc_emu64_frame_crashes = 0;
    pc_emu64_frame_noop_cmds = 0;
    pc_emu64_frame_tri_cmds = 0;
    pc_emu64_frame_vtx_cmds = 0;
    pc_emu64_frame_dl_cmds = 0;
    pc_emu64_frame_cull_visible = 0;
    pc_emu64_frame_cull_rejected = 0;
    pc_gx_draw_call_count = 0;
    g_pc_widescreen_stretch = 0;
    /* glClear respects write masks â€” must enable all before clearing */
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    /* Always clear the full backbuffer regardless of prior GX scissor/viewport state. */
    glDisable(GL_SCISSOR_TEST);
#ifdef PC_ENHANCEMENTS
    pc_gx_update_aspect();
    glViewport(0, 0, g_pc_window_w, g_pc_window_h);
#else
    glViewport(0, 0, PC_GC_WIDTH, PC_GC_HEIGHT);
#endif
    glClearDepth(g_gx.clear_depth);
    glClearColor(g_gx.clear_color[0], g_gx.clear_color[1], g_gx.clear_color[2], g_gx.clear_color[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Restore GX viewport/scissor after VI present path overwrote GL viewport state.
     * Viewport/scissor are immediate-state APIs (not part of dirty-upload groups). */
    if (g_gx.viewport[2] > 0.0f && g_gx.viewport[3] > 0.0f) {
        pc_gx_apply_viewport();
    }
    pc_gx_apply_scissor();

    /* VI present path mutates global GL state (depth/blend/cull/masks/viewport/scissor).
     * Force a full GX->GL resync on next draw so stale GL state cannot leak across frames. */
    g_gx.dirty = PC_GX_DIRTY_ALL;
}

void pc_gx_shutdown(void) {
    pc_gx_tev_shutdown();
    pc_gx_texture_shutdown();
#ifdef PC_ENHANCEMENTS
    pc_gx_efb_capture_cleanup();
#endif

    if (g_gx.ebo) glDeleteBuffers(1, &g_gx.ebo);
    if (g_gx.vbo) glDeleteBuffers(1, &g_gx.vbo);
    if (g_gx.vao) glDeleteVertexArrays(1, &g_gx.vao);
    g_pc_gx_initialized = 0;
}

/* --- Vertex Submission --- */
void GXBegin(GXPrimitive primitive, GXVtxFmt vtxfmt, u16 nverts) {
    /* Auto-flush previous batch if GXEnd was omitted (normal on real HW) */
    pc_gx_commit_pending_and_flush();

    switch (primitive) {
        case GX_QUADS:
        case GX_TRIANGLES:
        case GX_TRIANGLESTRIP:
        case GX_TRIANGLEFAN:
        case GX_LINES:
        case GX_LINESTRIP:
        case GX_POINTS:
            break;
        default:
            /* SDK would assert here; fail soft on PC backend. */
            primitive = GX_TRIANGLES;
            break;
    }
    if ((u32)vtxfmt >= (u32)GX_MAX_VTXFMT) {
        /* SDK would assert here; fail soft on PC backend. */
        vtxfmt = GX_VTXFMT0;
    }

    g_gx.current_primitive = (u32)primitive;
    g_gx.current_vtxfmt = (u32)vtxfmt;
    /* Manual documents a maximum of 65536 vertices for the u16 count field. */
    g_gx.expected_vertex_count = (nverts == 0) ? 65536 : (int)nverts;
    g_gx.current_vertex_idx = 0;
    g_gx.in_begin = 1;
    g_gx.vertex_pending = 0;
    g_gx.nbt_normal_phase = 0;
    g_gx.color_write_phase = 0;
    g_gx.texcoord_write_phase = 0;
    g_gx.matrix_index_write_phase = 0;
    memset(&g_gx.current_vertex, 0, sizeof(PCGXVertex));
    g_gx.current_vertex.color0[0] = 255;
    g_gx.current_vertex.color0[1] = 255;
    g_gx.current_vertex.color0[2] = 255;
    g_gx.current_vertex.color0[3] = 255;
    g_gx.current_vertex.color1[0] = 255;
    g_gx.current_vertex.color1[1] = 255;
    g_gx.current_vertex.color1[2] = 255;
    g_gx.current_vertex.color1[3] = 255;
    g_gx.current_vertex.pn_mtx_idx = (float)g_gx.current_mtx;
    for (int i = 0; i < 8; ++i) {
        g_gx.current_vertex.tex_mtx_idx[i] = (float)g_gx.current_tex_mtx_idx[i];
    }
}

void GXEnd(void) {
    pc_gx_commit_pending_and_flush();
}

static float pc_gx_current_position_frac_scale(void);

void GXPosition3f32(f32 x, f32 y, f32 z) {
    /* Deferred commit: position call commits the previous vertex */
    if (g_gx.vertex_pending && g_gx.current_vertex_idx < PC_GX_MAX_VERTS) {
        g_gx.vertex_buffer[g_gx.current_vertex_idx] = g_gx.current_vertex;
        g_gx.current_vertex_idx++;
    }

    /* Reset vertex but carry last colors forward */
    u8 c0r = g_gx.current_vertex.color0[0];
    u8 c0g = g_gx.current_vertex.color0[1];
    u8 c0b = g_gx.current_vertex.color0[2];
    u8 c0a = g_gx.current_vertex.color0[3];
    u8 c1r = g_gx.current_vertex.color1[0];
    u8 c1g = g_gx.current_vertex.color1[1];
    u8 c1b = g_gx.current_vertex.color1[2];
    u8 c1a = g_gx.current_vertex.color1[3];
    memset(&g_gx.current_vertex, 0, sizeof(PCGXVertex));
    g_gx.nbt_normal_phase = 0;
    g_gx.color_write_phase = 0;
    g_gx.texcoord_write_phase = 0;
    g_gx.matrix_index_write_phase = 0;
    g_gx.current_vertex.color0[0] = c0r;
    g_gx.current_vertex.color0[1] = c0g;
    g_gx.current_vertex.color0[2] = c0b;
    g_gx.current_vertex.color0[3] = c0a;
    g_gx.current_vertex.color1[0] = c1r;
    g_gx.current_vertex.color1[1] = c1g;
    g_gx.current_vertex.color1[2] = c1b;
    g_gx.current_vertex.color1[3] = c1a;
    g_gx.current_vertex.pn_mtx_idx = (float)g_gx.current_mtx;
    for (int i = 0; i < 8; ++i) {
        g_gx.current_vertex.tex_mtx_idx[i] = (float)g_gx.current_tex_mtx_idx[i];
    }

    g_gx.current_vertex.position[0] = x;
    g_gx.current_vertex.position[1] = y;
    g_gx.current_vertex.position[2] = z;
    g_gx.vertex_pending = 1;
}

void GXPosition3u16(u16 x, u16 y, u16 z) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, (f32)z * scale);
}
void GXPosition3s16(s16 x, s16 y, s16 z) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, (f32)z * scale);
}
void GXPosition3u8(u8 x, u8 y, u8 z) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, (f32)z * scale);
}
void GXPosition3s8(s8 x, s8 y, s8 z) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, (f32)z * scale);
}

void GXPosition2f32(f32 x, f32 y) { GXPosition3f32(x, y, 0.0f); }
void GXPosition2u16(u16 x, u16 y) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, 0.0f);
}
void GXPosition2s16(s16 x, s16 y) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, 0.0f);
}
void GXPosition2u8(u8 x, u8 y) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, 0.0f);
}
void GXPosition2s8(s8 x, s8 y) {
    float scale = pc_gx_current_position_frac_scale();
    GXPosition3f32((f32)x * scale, (f32)y * scale, 0.0f);
}

static float pc_gx_frac_scale(u8 frac) {
    return (frac != 0) ? (1.0f / (float)(1u << frac)) : 1.0f;
}

static const PCGXVertexFormat* pc_gx_current_vertex_format(void) {
    if (g_gx.current_vtxfmt >= 0 && g_gx.current_vtxfmt < GX_MAX_VTXFMT) {
        return &g_gx.vtx_fmt[g_gx.current_vtxfmt];
    }
    return &g_gx.vtx_fmt[0];
}

static float pc_gx_current_position_frac_scale(void) {
    const PCGXVertexFormat* fmt = pc_gx_current_vertex_format();
    return pc_gx_frac_scale(fmt->attr_frac[GX_VA_POS]);
}

static float pc_gx_current_texcoord_frac_scale(u32 attr) {
    const PCGXVertexFormat* fmt = pc_gx_current_vertex_format();
    if (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) {
        return pc_gx_frac_scale(fmt->attr_frac[attr]);
    }
    return pc_gx_frac_scale(fmt->attr_frac[GX_VA_TEX0]);
}

static u32 pc_gx_current_texcoord_attr(void) {
    int phase = (int)g_gx.texcoord_write_phase;
    int i;

    for (i = GX_VA_TEX0; i <= GX_VA_TEX7; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            if (phase == 0) {
                return (u32)i;
            }
            phase--;
        }
    }
    return GX_VA_TEX0;
}

static u32 pc_gx_current_matrix_index_attr(void) {
    int phase = (int)g_gx.matrix_index_write_phase;
    int i;

    for (i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7MTXIDX; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            if (phase == 0) {
                return (u32)i;
            }
            phase--;
        }
    }

    return GX_VA_PNMTXIDX;
}

static void pc_gx_advance_matrix_index_phase(void) {
    int enabled = 0;
    int i;

    for (i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7MTXIDX; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            enabled++;
        }
    }

    if (enabled <= 1) {
        g_gx.matrix_index_write_phase = 0;
        return;
    }

    if ((int)g_gx.matrix_index_write_phase < (enabled - 1)) {
        g_gx.matrix_index_write_phase++;
    }
}

static void pc_gx_advance_texcoord_phase(void) {
    int enabled = 0;
    int i;

    for (i = GX_VA_TEX0; i <= GX_VA_TEX7; ++i) {
        if (g_gx.vtx_desc[i] != GX_NONE) {
            enabled++;
        }
    }

    if (enabled <= 1) {
        g_gx.texcoord_write_phase = 0;
        return;
    }

    if ((int)g_gx.texcoord_write_phase < (enabled - 1)) {
        g_gx.texcoord_write_phase++;
    }
}

static u32 pc_gx_current_color_attr(void) {
    int clr0 = g_gx.vtx_desc[GX_VA_CLR0];
    int clr1 = g_gx.vtx_desc[GX_VA_CLR1];

    if (clr0 != GX_NONE && clr1 != GX_NONE) {
        return (g_gx.color_write_phase == 0) ? GX_VA_CLR0 : GX_VA_CLR1;
    }
    if (clr0 != GX_NONE) return GX_VA_CLR0;
    if (clr1 != GX_NONE) return GX_VA_CLR1;
    return GX_VA_CLR0;
}

static void pc_gx_advance_color_phase(void) {
    int clr0 = g_gx.vtx_desc[GX_VA_CLR0];
    int clr1 = g_gx.vtx_desc[GX_VA_CLR1];

    if (clr0 != GX_NONE && clr1 != GX_NONE) {
        if (g_gx.color_write_phase == 0) {
            g_gx.color_write_phase = 1;
        }
    } else {
        g_gx.color_write_phase = 0;
    }
}

static void pc_gx_write_vertex_color(u32 attr, u8 r, u8 g, u8 b, u8 a) {
    if (attr == GX_VA_CLR1) {
        g_gx.current_vertex.color1[0] = r;
        g_gx.current_vertex.color1[1] = g;
        g_gx.current_vertex.color1[2] = b;
        g_gx.current_vertex.color1[3] = a;
    } else {
        g_gx.current_vertex.color0[0] = r;
        g_gx.current_vertex.color0[1] = g;
        g_gx.current_vertex.color0[2] = b;
        g_gx.current_vertex.color0[3] = a;
    }
}

static u8 pc_gx_current_color_type(u32 attr) {
    const PCGXVertexFormat* fmt = pc_gx_current_vertex_format();
    return fmt->attr_type[attr];
}

static void pc_gx_decode_color_u16(u16 clr, u8 type, u8* out_rgba) {
    out_rgba[0] = 255;
    out_rgba[1] = 255;
    out_rgba[2] = 255;
    out_rgba[3] = 255;

    switch (type) {
        case GX_RGB565:
            out_rgba[0] = (u8)(((clr >> 11) & 0x1F) * 255 / 31);
            out_rgba[1] = (u8)(((clr >> 5) & 0x3F) * 255 / 63);
            out_rgba[2] = (u8)((clr & 0x1F) * 255 / 31);
            out_rgba[3] = 255;
            break;
        case GX_RGBA4:
            out_rgba[0] = (u8)(((clr >> 12) & 0x0F) * 17);
            out_rgba[1] = (u8)(((clr >> 8) & 0x0F) * 17);
            out_rgba[2] = (u8)(((clr >> 4) & 0x0F) * 17);
            out_rgba[3] = (u8)((clr & 0x0F) * 17);
            break;
        default:
            /* Fallback keeps prior behavior for odd format/function pairs. */
            out_rgba[0] = (u8)((clr >> 8) & 0xFF);
            out_rgba[1] = (u8)(clr & 0xFF);
            out_rgba[2] = 0;
            out_rgba[3] = 255;
            break;
    }
}

static void pc_gx_decode_color_u32(u32 clr, u8 type, u8* out_rgba) {
    u32 v;
    out_rgba[0] = 255;
    out_rgba[1] = 255;
    out_rgba[2] = 255;
    out_rgba[3] = 255;

    switch (type) {
        case GX_RGB8:
        case GX_RGBX8:
            out_rgba[0] = (u8)((clr >> 24) & 0xFF);
            out_rgba[1] = (u8)((clr >> 16) & 0xFF);
            out_rgba[2] = (u8)((clr >> 8) & 0xFF);
            out_rgba[3] = 255;
            break;
        case GX_RGBA6:
            /* 24-bit packed: RRRRRRGG GGGGBBBB BBBAAAAAA */
            v = (clr >> 8) & 0x00FFFFFFu;
            out_rgba[0] = (u8)((((v >> 18) & 0x3F) * 255 + 31) / 63);
            out_rgba[1] = (u8)((((v >> 12) & 0x3F) * 255 + 31) / 63);
            out_rgba[2] = (u8)((((v >> 6) & 0x3F) * 255 + 31) / 63);
            out_rgba[3] = (u8)((((v >> 0) & 0x3F) * 255 + 31) / 63);
            break;
        case GX_RGBA8:
        default:
            out_rgba[0] = (u8)((clr >> 24) & 0xFF);
            out_rgba[1] = (u8)((clr >> 16) & 0xFF);
            out_rgba[2] = (u8)((clr >> 8) & 0xFF);
            out_rgba[3] = (u8)(clr & 0xFF);
            break;
    }
}

static void pc_gx_read_position_indexed(const u8* src, u8 cnt, u8 type, u8 frac,
                                        float* x, float* y, float* z) {
    float scale = pc_gx_frac_scale(frac);
    *x = 0.0f;
    *y = 0.0f;
    *z = 0.0f;

    switch (type) {
        case GX_F32: {
            const float* p = (const float*)src;
            *x = p[0];
            *y = p[1];
            *z = (cnt == GX_POS_XYZ) ? p[2] : 0.0f;
            break;
        }
        case GX_S16: {
            const s16* p = (const s16*)src;
            *x = p[0] * scale;
            *y = p[1] * scale;
            *z = (cnt == GX_POS_XYZ) ? (p[2] * scale) : 0.0f;
            break;
        }
        case GX_U16: {
            const u16* p = (const u16*)src;
            *x = p[0] * scale;
            *y = p[1] * scale;
            *z = (cnt == GX_POS_XYZ) ? (p[2] * scale) : 0.0f;
            break;
        }
        case GX_S8: {
            const s8* p = (const s8*)src;
            *x = p[0] * scale;
            *y = p[1] * scale;
            *z = (cnt == GX_POS_XYZ) ? (p[2] * scale) : 0.0f;
            break;
        }
        case GX_U8:
        default: {
            const u8* p = (const u8*)src;
            *x = p[0] * scale;
            *y = p[1] * scale;
            *z = (cnt == GX_POS_XYZ) ? (p[2] * scale) : 0.0f;
            break;
        }
    }
}

static void pc_gx_read_normal_indexed(const u8* src, u8 type, u8 frac,
                                      float* x, float* y, float* z) {
    (void)frac;
    *x = 0.0f;
    *y = 0.0f;
    *z = 1.0f;

    switch (type) {
        case GX_F32: {
            const float* p = (const float*)src;
            *x = p[0];
            *y = p[1];
            *z = p[2];
            break;
        }
        case GX_S16: {
            const s16* p = (const s16*)src;
            /* GXSetVtxAttrFmt: implied fractional bits for normals (S16 => 14). */
            *x = p[0] / 16384.0f;
            *y = p[1] / 16384.0f;
            *z = p[2] / 16384.0f;
            break;
        }
        case GX_U16: {
            const u16* p = (const u16*)src;
            *x = (float)p[0];
            *y = (float)p[1];
            *z = (float)p[2];
            break;
        }
        case GX_S8: {
            const s8* p = (const s8*)src;
            /* GXSetVtxAttrFmt: implied fractional bits for normals (S8 => 6). */
            *x = p[0] / 64.0f;
            *y = p[1] / 64.0f;
            *z = p[2] / 64.0f;
            break;
        }
        case GX_U8:
        default: {
            const u8* p = (const u8*)src;
            *x = (float)p[0];
            *y = (float)p[1];
            *z = (float)p[2];
            break;
        }
    }
}

static void pc_gx_read_texcoord_indexed(const u8* src, u8 cnt, u8 type, u8 frac,
                                        float* s, float* t) {
    float scale = pc_gx_frac_scale(frac);
    *s = 0.0f;
    *t = 0.0f;

    switch (type) {
        case GX_F32: {
            const float* p = (const float*)src;
            *s = p[0];
            *t = (cnt == GX_TEX_ST) ? p[1] : 0.0f;
            break;
        }
        case GX_S16: {
            const s16* p = (const s16*)src;
            *s = p[0] * scale;
            *t = (cnt == GX_TEX_ST) ? (p[1] * scale) : 0.0f;
            break;
        }
        case GX_U16: {
            const u16* p = (const u16*)src;
            *s = p[0] * scale;
            *t = (cnt == GX_TEX_ST) ? (p[1] * scale) : 0.0f;
            break;
        }
        case GX_S8: {
            const s8* p = (const s8*)src;
            *s = p[0] * scale;
            *t = (cnt == GX_TEX_ST) ? (p[1] * scale) : 0.0f;
            break;
        }
        case GX_U8:
        default: {
            const u8* p = (const u8*)src;
            *s = p[0] * scale;
            *t = (cnt == GX_TEX_ST) ? (p[1] * scale) : 0.0f;
            break;
        }
    }
}

static void pc_gx_read_color_indexed(const u8* src, u8 cnt, u8 type, u8* out_rgba) {
    out_rgba[0] = 255;
    out_rgba[1] = 255;
    out_rgba[2] = 255;
    out_rgba[3] = 255;

    switch (type) {
        case GX_RGB565: {
            u16 c = 0;
            memcpy(&c, src, sizeof(c));
            out_rgba[0] = (u8)(((c >> 11) & 0x1F) * 255 / 31);
            out_rgba[1] = (u8)(((c >> 5) & 0x3F) * 255 / 63);
            out_rgba[2] = (u8)((c & 0x1F) * 255 / 31);
            out_rgba[3] = 255;
            break;
        }
        case GX_RGB8: {
            out_rgba[0] = src[0];
            out_rgba[1] = src[1];
            out_rgba[2] = src[2];
            out_rgba[3] = 255;
            break;
        }
        case GX_RGBX8: {
            out_rgba[0] = src[0];
            out_rgba[1] = src[1];
            out_rgba[2] = src[2];
            out_rgba[3] = 255;
            break;
        }
        case GX_RGBA4: {
            u16 c = 0;
            memcpy(&c, src, sizeof(c));
            out_rgba[0] = (u8)(((c >> 12) & 0x0F) * 17);
            out_rgba[1] = (u8)(((c >> 8) & 0x0F) * 17);
            out_rgba[2] = (u8)(((c >> 4) & 0x0F) * 17);
            out_rgba[3] = (u8)((c & 0x0F) * 17);
            break;
        }
        case GX_RGBA6: {
            u32 c24 = ((u32)src[0] << 16) | ((u32)src[1] << 8) | (u32)src[2];
            out_rgba[0] = (u8)((((c24 >> 18) & 0x3F) * 255 + 31) / 63);
            out_rgba[1] = (u8)((((c24 >> 12) & 0x3F) * 255 + 31) / 63);
            out_rgba[2] = (u8)((((c24 >> 6) & 0x3F) * 255 + 31) / 63);
            out_rgba[3] = (u8)((((c24 >> 0) & 0x3F) * 255 + 31) / 63);
            break;
        }
        case GX_RGBA8:
        default: {
            out_rgba[0] = src[0];
            out_rgba[1] = src[1];
            out_rgba[2] = src[2];
            out_rgba[3] = (cnt == GX_CLR_RGBA) ? src[3] : 255;
            break;
        }
    }
}

void GXPosition1x16(u16 index) {
    if (g_gx.array_base[GX_VA_POS]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_POS];
        const u8* src = base + index * g_gx.array_stride[GX_VA_POS];
        const PCGXVertexFormat* fmt = &g_gx.vtx_fmt[g_gx.current_vtxfmt];
        u8 cnt = fmt->attr_cnt[GX_VA_POS];
        u8 type = fmt->attr_type[GX_VA_POS];
        u8 frac = fmt->attr_frac[GX_VA_POS];
        float x, y, z;

        if (cnt != GX_POS_XY && cnt != GX_POS_XYZ) cnt = GX_POS_XYZ;
        pc_gx_read_position_indexed(src, cnt, type, frac, &x, &y, &z);
        GXPosition3f32(x, y, z);
    }
}
void GXPosition1x8(u8 index) { GXPosition1x16(index); }
static void pc_gx_set_matrix_index_attr(u32 attr, u8 index) {
    u32 slot = (u32)index / 3u;

    if (attr == GX_VA_PNMTXIDX) {
        if (slot >= 10u) return;
        g_gx.current_mtx = (int)slot;
        if (g_gx.vertex_pending) {
            g_gx.current_vertex.pn_mtx_idx = (float)slot;
        }
        DIRTY(PC_GX_DIRTY_MODELVIEW);
        return;
    }

    if (attr >= GX_VA_TEX0MTXIDX && attr <= GX_VA_TEX7MTXIDX) {
        u32 tex = attr - GX_VA_TEX0MTXIDX;
        if (tex >= 8u) return;
        if (slot >= 10u) return;
        g_gx.current_tex_mtx_idx[tex] = (int)slot;
        if (g_gx.vertex_pending) {
            g_gx.current_vertex.tex_mtx_idx[tex] = (float)slot;
        }
    }
}

static u32 pc_gx_normal_vec_bytes(u8 type) {
    switch (type) {
        case GX_F32:
            return 12u;
        case GX_S16:
        case GX_U16:
            return 6u;
        case GX_S8:
        case GX_U8:
        default:
            return 3u;
    }
}

void GXMatrixIndex1x8(u8 index) {
    u32 attr = pc_gx_current_matrix_index_attr();
    pc_gx_set_matrix_index_attr(attr, index);
    pc_gx_advance_matrix_index_phase();
}
void GXMatrixIndex1u8(u8 index) { GXMatrixIndex1x8(index); }

void GXNormal3f32(f32 x, f32 y, f32 z) {
    const PCGXVertexFormat* fmt = pc_gx_current_vertex_format();
    u8 ncnt = fmt->attr_cnt[GX_VA_NRM];

    if (ncnt == GX_NRM_NBT || ncnt == GX_NRM_NBT3) {
        if (g_gx.nbt_normal_phase == 0) {
            g_gx.current_vertex.normal[0] = x;
            g_gx.current_vertex.normal[1] = y;
            g_gx.current_vertex.normal[2] = z;
        } else if (g_gx.nbt_normal_phase == 1) {
            g_gx.current_vertex.binormal[0] = x;
            g_gx.current_vertex.binormal[1] = y;
            g_gx.current_vertex.binormal[2] = z;
        } else {
            g_gx.current_vertex.tangent[0] = x;
            g_gx.current_vertex.tangent[1] = y;
            g_gx.current_vertex.tangent[2] = z;
        }
        g_gx.nbt_normal_phase = (u8)((g_gx.nbt_normal_phase + 1u) % 3u);
        return;
    }

    g_gx.nbt_normal_phase = 0;
    g_gx.current_vertex.normal[0] = x;
    g_gx.current_vertex.normal[1] = y;
    g_gx.current_vertex.normal[2] = z;
}
void GXNormal3s16(s16 x, s16 y, s16 z) {
    /* GXSetVtxAttrFmt: implied fractional bits for normals (S16 => 14). */
    GXNormal3f32(x / 16384.0f, y / 16384.0f, z / 16384.0f);
}
void GXNormal3s8(s8 x, s8 y, s8 z) {
    /* GXSetVtxAttrFmt: implied fractional bits for normals (S8 => 6). */
    GXNormal3f32(x / 64.0f, y / 64.0f, z / 64.0f);
}
void GXNormal1x16(u16 index) {
    if (g_gx.array_base[GX_VA_NRM]) {
        const u8* base = (const u8*)g_gx.array_base[GX_VA_NRM];
        const u8* src = base + index * g_gx.array_stride[GX_VA_NRM];
        const PCGXVertexFormat* fmt = &g_gx.vtx_fmt[g_gx.current_vtxfmt];
        u8 ncnt = fmt->attr_cnt[GX_VA_NRM];
        u8 type = fmt->attr_type[GX_VA_NRM];
        u8 frac = fmt->attr_frac[GX_VA_NRM];
        float x, y, z;

        if (ncnt == GX_NRM_NBT) {
            u32 vec_bytes = pc_gx_normal_vec_bytes(type);
            for (u32 i = 0; i < 3; ++i) {
                pc_gx_read_normal_indexed(src + i * vec_bytes, type, frac, &x, &y, &z);
                GXNormal3f32(x, y, z);
            }
        } else {
            pc_gx_read_normal_indexed(src, type, frac, &x, &y, &z);
            GXNormal3f32(x, y, z);
        }
    }
}
void GXNormal1x8(u8 index) { GXNormal1x16(index); }

void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    u32 attr = pc_gx_current_color_attr();
    pc_gx_write_vertex_color(attr, r, g, b, a);
    pc_gx_advance_color_phase();
}
void GXColor3u8(u8 r, u8 g, u8 b) { GXColor4u8(r, g, b, 255); }
void GXColor1u32(u32 clr) {
    u32 attr = pc_gx_current_color_attr();
    u8 rgba[4];
    pc_gx_decode_color_u32(clr, pc_gx_current_color_type(attr), rgba);
    pc_gx_write_vertex_color(attr, rgba[0], rgba[1], rgba[2], rgba[3]);
    pc_gx_advance_color_phase();
}
void GXColor1u16(u16 clr) {
    u32 attr = pc_gx_current_color_attr();
    u8 rgba[4];
    pc_gx_decode_color_u16(clr, pc_gx_current_color_type(attr), rgba);
    pc_gx_write_vertex_color(attr, rgba[0], rgba[1], rgba[2], rgba[3]);
    pc_gx_advance_color_phase();
}
void GXColor1x16(u16 index) {
    u32 attr = pc_gx_current_color_attr();

    if (g_gx.array_base[attr]) {
        const u8* base = (const u8*)g_gx.array_base[attr];
        const u8* src = base + index * g_gx.array_stride[attr];
        const PCGXVertexFormat* fmt = &g_gx.vtx_fmt[g_gx.current_vtxfmt];
        u8 cnt = fmt->attr_cnt[attr];
        u8 type = fmt->attr_type[attr];
        u8 rgba[4];

        if (cnt != GX_CLR_RGB && cnt != GX_CLR_RGBA) cnt = GX_CLR_RGBA;
        pc_gx_read_color_indexed(src, cnt, type, rgba);
        pc_gx_write_vertex_color(attr, rgba[0], rgba[1], rgba[2], rgba[3]);
        pc_gx_advance_color_phase();
    }
}
void GXColor1x8(u8 index) { GXColor1x16(index); }

void GXColor4f32(float r, float g, float b, float a) {
    GXColor4u8((u8)(r * 255.0f + 0.5f), (u8)(g * 255.0f + 0.5f),
               (u8)(b * 255.0f + 0.5f), (u8)(a * 255.0f + 0.5f));
}

static void pc_gx_set_texcoord_channel(u32 channel, f32 s, f32 t) {
    if (channel < 8) {
        g_gx.current_vertex.texcoord[channel][0] = s;
        g_gx.current_vertex.texcoord[channel][1] = t;
    }
}

void GXTexCoord2f32(f32 s, f32 t) {
    u32 attr = pc_gx_current_texcoord_attr();
    u32 channel = (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) ? (attr - GX_VA_TEX0) : 0;
    pc_gx_set_texcoord_channel(channel, s, t);
    pc_gx_advance_texcoord_phase();
}

void GXTexCoord2u16(u16 s, u16 t) {
    float scale = pc_gx_current_texcoord_frac_scale(pc_gx_current_texcoord_attr());
    GXTexCoord2f32((f32)s * scale, (f32)t * scale);
}
void GXTexCoord2s16(s16 s, s16 t) {
    float scale = pc_gx_current_texcoord_frac_scale(pc_gx_current_texcoord_attr());
    GXTexCoord2f32((f32)s * scale, (f32)t * scale);
}
void GXTexCoord2u8(u8 s, u8 t) {
    float scale = pc_gx_current_texcoord_frac_scale(pc_gx_current_texcoord_attr());
    GXTexCoord2f32((f32)s * scale, (f32)t * scale);
}
void GXTexCoord2s8(s8 s, s8 t) {
    float scale = pc_gx_current_texcoord_frac_scale(pc_gx_current_texcoord_attr());
    GXTexCoord2f32((f32)s * scale, (f32)t * scale);
}

void GXTexCoord1f32(f32 s) { GXTexCoord2f32(s, 0.0f); }
void GXTexCoord1u16(u16 s) { GXTexCoord2u16(s, 0); }
void GXTexCoord1s16(s16 s) { GXTexCoord2s16(s, 0); }
void GXTexCoord1u8(u8 s) { GXTexCoord2u8(s, 0); }
void GXTexCoord1s8(s8 s) { GXTexCoord2s8(s, 0); }

void GXTexCoord1x16(u16 index) {
    u32 attr = pc_gx_current_texcoord_attr();

    if (g_gx.array_base[attr]) {
        const u8* base = (const u8*)g_gx.array_base[attr];
        const u8* src = base + index * g_gx.array_stride[attr];
        const PCGXVertexFormat* fmt = pc_gx_current_vertex_format();
        u8 cnt = fmt->attr_cnt[attr];
        u8 type = fmt->attr_type[attr];
        u8 frac = fmt->attr_frac[attr];
        float s, t;
        u32 channel = (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7) ? (attr - GX_VA_TEX0) : 0;

        if (cnt != GX_TEX_S && cnt != GX_TEX_ST) cnt = GX_TEX_ST;
        pc_gx_read_texcoord_indexed(src, cnt, type, frac, &s, &t);
        pc_gx_set_texcoord_channel(channel, s, t);
        pc_gx_advance_texcoord_phase();
    }
}
void GXTexCoord1x8(u8 index) { GXTexCoord1x16(index); }

/* --- Uniform Location Cache --- */
static void pc_gx_cache_uniform_locations(GLuint shader) {
    char name[48];
    int i;
    #define UL(n) glGetUniformLocation(shader, n)

    g_gx.uloc.projection = UL("u_projection");
    g_gx.uloc.modelview  = UL("u_modelview");
    g_gx.uloc.normal_mtx = UL("u_normal_mtx");
    g_gx.uloc.modelview_arr = UL("u_modelview_arr[0]");
    g_gx.uloc.normal_mtx_arr = UL("u_normal_mtx_arr[0]");
    g_gx.uloc.use_matrix_array = UL("u_use_matrix_array");

    g_gx.uloc.tev_prev = UL("u_tev_prev");
    g_gx.uloc.tev_reg0 = UL("u_tev_reg0");
    g_gx.uloc.tev_reg1 = UL("u_tev_reg1");
    g_gx.uloc.tev_reg2 = UL("u_tev_reg2");

    g_gx.uloc.num_tev_stages = UL("u_num_tev_stages");
    for (i = 0; i < PC_GX_MAX_TEV_STAGES; i++) {
        snprintf(name, sizeof(name), "u_tev_color_in[%d]", i);
        g_gx.uloc.tev_color_in[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_alpha_in[%d]", i);
        g_gx.uloc.tev_alpha_in[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_color_op[%d]", i);
        g_gx.uloc.tev_color_op[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_alpha_op[%d]", i);
        g_gx.uloc.tev_alpha_op[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_color_chan[%d]", i);
        g_gx.uloc.tev_color_chan[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_tc_src[%d]", i);
        g_gx.uloc.tev_tc_src[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_ind_cfg[%d]", i);
        g_gx.uloc.tev_ind_cfg[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_ind_wrap[%d]", i);
        g_gx.uloc.tev_ind_wrap[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_clamp_mode[%d]", i);
        g_gx.uloc.tev_clamp_mode[i] = UL(name);
    }

    g_gx.uloc.kcolor   = UL("u_kcolor[0]");
    g_gx.uloc.tev_ksel = UL("u_tev_ksel[0]");

    g_gx.uloc.alpha_comp0 = UL("u_alpha_comp0");
    g_gx.uloc.alpha_ref0  = UL("u_alpha_ref0");
    g_gx.uloc.alpha_op    = UL("u_alpha_op");
    g_gx.uloc.alpha_comp1 = UL("u_alpha_comp1");
    g_gx.uloc.alpha_ref1  = UL("u_alpha_ref1");

    g_gx.uloc.lighting_enabled = UL("u_lighting_enabled");
    g_gx.uloc.mat_color  = UL("u_mat_color");
    g_gx.uloc.amb_color  = UL("u_amb_color");
    g_gx.uloc.chan_mat_src = UL("u_chan_mat_src");
    g_gx.uloc.chan_amb_src = UL("u_chan_amb_src");
    g_gx.uloc.lighting_enabled1 = UL("u_lighting_enabled1");
    g_gx.uloc.mat_color1  = UL("u_mat_color1");
    g_gx.uloc.amb_color1  = UL("u_amb_color1");
    g_gx.uloc.chan_mat_src1 = UL("u_chan_mat_src1");
    g_gx.uloc.chan_amb_src1 = UL("u_chan_amb_src1");
    g_gx.uloc.chan_diff_fn = UL("u_chan_diff_fn");
    g_gx.uloc.chan_attn_fn = UL("u_chan_attn_fn");
    g_gx.uloc.chan_diff_fn1 = UL("u_chan_diff_fn1");
    g_gx.uloc.chan_attn_fn1 = UL("u_chan_attn_fn1");
    g_gx.uloc.num_chans  = UL("u_num_chans");
    g_gx.uloc.alpha_lighting_enabled = UL("u_alpha_lighting_enabled");
    g_gx.uloc.alpha_mat_src = UL("u_alpha_mat_src");
    g_gx.uloc.alpha_lighting_enabled1 = UL("u_alpha_lighting_enabled1");
    g_gx.uloc.alpha_mat_src1 = UL("u_alpha_mat_src1");

    g_gx.uloc.light_mask = UL("u_light_mask");
    g_gx.uloc.light_mask1 = UL("u_light_mask1");
    for (i = 0; i < 8; i++) {
        snprintf(name, sizeof(name), "u_light_pos[%d]", i);
        g_gx.uloc.light_pos[i] = UL(name);
        snprintf(name, sizeof(name), "u_light_dir[%d]", i);
        g_gx.uloc.light_dir[i] = UL(name);
        snprintf(name, sizeof(name), "u_light_color[%d]", i);
        g_gx.uloc.light_color[i] = UL(name);
        snprintf(name, sizeof(name), "u_light_cos_att[%d]", i);
        g_gx.uloc.light_cos_att[i] = UL(name);
        snprintf(name, sizeof(name), "u_light_dist_att[%d]", i);
        g_gx.uloc.light_dist_att[i] = UL(name);
    }

    for (i = 0; i < PC_GX_MAX_TEXGENS; i++) {
        snprintf(name, sizeof(name), "u_texmtx_enable[%d]", i);
        g_gx.uloc.texmtx_enable[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtx_row0[%d]", i);
        g_gx.uloc.texmtx_row0[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtx_row1[%d]", i);
        g_gx.uloc.texmtx_row1[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtx_row2[%d]", i);
        g_gx.uloc.texmtx_row2[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtxidx_enable[%d]", i);
        g_gx.uloc.texmtxidx_enable[i] = UL(name);
        snprintf(name, sizeof(name), "u_postmtx_enable[%d]", i);
        g_gx.uloc.postmtx_enable[i] = UL(name);
        snprintf(name, sizeof(name), "u_postmtx_row0[%d]", i);
        g_gx.uloc.postmtx_row0[i] = UL(name);
        snprintf(name, sizeof(name), "u_postmtx_row1[%d]", i);
        g_gx.uloc.postmtx_row1[i] = UL(name);
        snprintf(name, sizeof(name), "u_postmtx_row2[%d]", i);
        g_gx.uloc.postmtx_row2[i] = UL(name);
        snprintf(name, sizeof(name), "u_texgen_src[%d]", i);
        g_gx.uloc.texgen_src[i] = UL(name);
        snprintf(name, sizeof(name), "u_texgen_type[%d]", i);
        g_gx.uloc.texgen_type[i] = UL(name);
        snprintf(name, sizeof(name), "u_texgen_normalize[%d]", i);
        g_gx.uloc.texgen_normalize[i] = UL(name);
        snprintf(name, sizeof(name), "u_texgen_qt_notcalc[%d]", i);
        g_gx.uloc.texgen_qt_notcalc[i] = UL(name);
        snprintf(name, sizeof(name), "u_tcs_cyl_wrap_enable[%d]", i);
        g_gx.uloc.tcs_cyl_wrap_enable[i] = UL(name);
        snprintf(name, sizeof(name), "u_tcs_manual_enable[%d]", i);
        g_gx.uloc.tcs_manual_enable[i] = UL(name);
        snprintf(name, sizeof(name), "u_tcs_manual_scale[%d]", i);
        g_gx.uloc.tcs_manual_scale[i] = UL(name);
    }
    for (i = 0; i < 10; i++) {
        snprintf(name, sizeof(name), "u_texmtx_all_row0[%d]", i);
        g_gx.uloc.texmtx_all_row0[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtx_all_row1[%d]", i);
        g_gx.uloc.texmtx_all_row1[i] = UL(name);
        snprintf(name, sizeof(name), "u_texmtx_all_row2[%d]", i);
        g_gx.uloc.texmtx_all_row2[i] = UL(name);
    }

    for (i = 0; i < PC_GX_MAX_TEV_STAGES; i++) {
        snprintf(name, sizeof(name), "u_use_texture[%d]", i);
        g_gx.uloc.use_texture[i] = UL(name);
        snprintf(name, sizeof(name), "u_texture[%d]", i);
        g_gx.uloc.texture[i] = UL(name);
    }

    g_gx.uloc.num_ind_stages = UL("u_num_ind_stages");
    for (i = 0; i < 4; i++) {
        snprintf(name, sizeof(name), "u_ind_tex[%d]", i);
        g_gx.uloc.ind_tex[i] = UL(name);
        snprintf(name, sizeof(name), "u_ind_scale[%d]", i);
        g_gx.uloc.ind_scale[i] = UL(name);
        snprintf(name, sizeof(name), "u_ind_tc_src[%d]", i);
        g_gx.uloc.ind_tc_src[i] = UL(name);
    }
    for (i = 0; i < PC_GX_MAX_TEV_STAGES; i++) {
        snprintf(name, sizeof(name), "u_ind_mtx_r0[%d]", i);
        g_gx.uloc.ind_mtx_r0[i] = UL(name);
        snprintf(name, sizeof(name), "u_ind_mtx_r1[%d]", i);
        g_gx.uloc.ind_mtx_r1[i] = UL(name);
    }

    g_gx.uloc.fog_type  = UL("u_fog_type");
    g_gx.uloc.fog_start = UL("u_fog_start");
    g_gx.uloc.fog_end   = UL("u_fog_end");
    g_gx.uloc.fog_color = UL("u_fog_color");
    g_gx.uloc.ztex_enable = UL("u_ztex_enable");
    g_gx.uloc.ztex_op = UL("u_ztex_op");
    g_gx.uloc.ztex_fmt = UL("u_ztex_fmt");
    g_gx.uloc.ztex_bias = UL("u_ztex_bias");
    g_gx.uloc.ztex_bias24 = UL("u_ztex_bias24");

    /* Per-stage bias/scale/clamp/output */
    for (i = 0; i < PC_GX_MAX_TEV_STAGES; i++) {
        snprintf(name, sizeof(name), "u_tev_bsc[%d]", i);
        g_gx.uloc.tev_bsc[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_out[%d]", i);
        g_gx.uloc.tev_out[i] = UL(name);
        snprintf(name, sizeof(name), "u_tev_swap[%d]", i);
        g_gx.uloc.tev_swap[i] = UL(name);
    }
    g_gx.uloc.swap_table = UL("u_swap_table");

    #undef UL
}

/* --- Vertex Flush --- */
int pc_gx_draw_call_count = 0;

void pc_gx_flush_vertices(void) {
    int count = g_gx.current_vertex_idx;
    if (count == 0) return;

    if (g_pc_gx_dl.active) {
        pc_gx_dl_record_draw_batch((u32)g_gx.current_primitive, g_gx.vertex_buffer, (u32)count);
    }

    pc_gx_draw_call_count++;

    GLuint shader = pc_gx_tev_get_shader(&g_gx);

    if (pc_gx_debug_enabled()) {
        static int debug_prints = 0;
        if (debug_prints < 40) {
            const PCGXVertex* v0 = &g_gx.vertex_buffer[0];
            const float* mv = (const float*)g_gx.pos_mtx[g_gx.current_mtx];
            const float* p = (const float*)g_gx.projection_mtx;
            float pos[4] = { v0->position[0], v0->position[1], v0->position[2], 1.0f };
            float eye[4];
            float clip[4];
            float ndc[3] = { 0.0f, 0.0f, 0.0f };

            eye[0] = mv[0] * pos[0] + mv[1] * pos[1] + mv[2] * pos[2] + mv[3] * pos[3];
            eye[1] = mv[4] * pos[0] + mv[5] * pos[1] + mv[6] * pos[2] + mv[7] * pos[3];
            eye[2] = mv[8] * pos[0] + mv[9] * pos[1] + mv[10] * pos[2] + mv[11] * pos[3];
            eye[3] = 1.0f;

            clip[0] = p[0] * eye[0] + p[1] * eye[1] + p[2] * eye[2] + p[3] * eye[3];
            clip[1] = p[4] * eye[0] + p[5] * eye[1] + p[6] * eye[2] + p[7] * eye[3];
            clip[2] = p[8] * eye[0] + p[9] * eye[1] + p[10] * eye[2] + p[11] * eye[3];
            clip[3] = p[12] * eye[0] + p[13] * eye[1] + p[14] * eye[2] + p[15] * eye[3];

            if (fabsf(clip[3]) > 1e-6f) {
                ndc[0] = clip[0] / clip[3];
                ndc[1] = clip[1] / clip[3];
                ndc[2] = clip[2] / clip[3];
            }

            {
                char line[512];
                snprintf(line, sizeof(line),
                         "[PCGX] draw prim=%u cnt=%d shader=%u pos=(%.2f,%.2f,%.2f) eye=(%.2f,%.2f,%.2f,%.2f) clip=(%.2f,%.2f,%.2f,%.2f) ndc=(%.2f,%.2f,%.2f) mv_t=(%.2f,%.2f,%.2f) alpha=(c0:%d r0:%d op:%d c1:%d r1:%d) chan=(n:%d mat:%d)",
                         (unsigned)g_gx.current_primitive, count, (unsigned)shader,
                         pos[0], pos[1], pos[2],
                         eye[0], eye[1], eye[2], eye[3],
                         clip[0], clip[1], clip[2], clip[3],
                         ndc[0], ndc[1], ndc[2],
                         mv[3], mv[7], mv[11],
                         g_gx.alpha_comp0, g_gx.alpha_ref0, g_gx.alpha_op, g_gx.alpha_comp1, g_gx.alpha_ref1,
                         g_gx.num_chans, g_gx.chan_ctrl_mat_src[0]);
                pc_gx_debug_log(line);
            }
            debug_prints++;
        }
    }

    if (shader && shader != g_gx.current_shader) {
        glUseProgram(shader);
        PC_GL_CHECK("glUseProgram");
        g_gx.current_shader = shader;
        pc_gx_cache_uniform_locations(shader);
        g_gx.dirty = PC_GX_DIRTY_ALL;
    }

    const PCGXVertex* submit_vertices = g_gx.vertex_buffer;
    int submit_count = count;

    /* Some desktop drivers are flaky with strip/fan on dynamic streams.
     * Expand to plain triangles on CPU while preserving winding rules. */
    if (g_gx.current_primitive == GX_TRIANGLESTRIP && count >= 3) {
        int disable_strip_odd_swap = pc_gx_disable_strip_odd_swap();
        int out = 0;
        for (int i = 0; i < count - 2; i++) {
            int i0, i1, i2;
            if (!disable_strip_odd_swap && ((i & 1) != 0)) {
                i0 = i + 1;
                i1 = i;
                i2 = i + 2;
            } else {
                i0 = i;
                i1 = i + 1;
                i2 = i + 2;
            }
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[i0];
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[i1];
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[i2];
        }
        submit_vertices = g_pc_gx_expanded_prim;
        submit_count = out;
    } else if (g_gx.current_primitive == GX_TRIANGLEFAN && count >= 3) {
        int out = 0;
        for (int i = 1; i < count - 1; i++) {
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[0];
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[i];
            g_pc_gx_expanded_prim[out++] = g_gx.vertex_buffer[i + 1];
        }
        submit_vertices = g_pc_gx_expanded_prim;
        submit_count = out;
    }

    glBindVertexArray(g_gx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx.vbo);
    glBufferData(GL_ARRAY_BUFFER, submit_count * sizeof(PCGXVertex), submit_vertices, GL_STREAM_DRAW);

    /* Upload only dirty state groups */
    if (shader) {
        GLint loc;
        unsigned int dirty = g_gx.dirty;
        #define UL(field) g_gx.uloc.field

        if (dirty & PC_GX_DIRTY_PROJECTION) {
            loc = UL(projection);
            if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_TRUE, (float*)g_gx.projection_mtx);
        }

        if (dirty & PC_GX_DIRTY_MODELVIEW) {
            loc = UL(modelview);
            if (loc >= 0) {
                float mv44[16];
                const float* src = (const float*)g_gx.pos_mtx[g_gx.current_mtx];
                mv44[ 0] = src[0]; mv44[ 1] = src[1]; mv44[ 2] = src[2]; mv44[ 3] = src[3];
                mv44[ 4] = src[4]; mv44[ 5] = src[5]; mv44[ 6] = src[6]; mv44[ 7] = src[7];
                mv44[ 8] = src[8]; mv44[ 9] = src[9]; mv44[10] = src[10]; mv44[11] = src[11];
                mv44[12] = 0.0f;   mv44[13] = 0.0f;   mv44[14] = 0.0f;    mv44[15] = 1.0f;
                glUniformMatrix4fv(loc, 1, GL_TRUE, mv44);
            }
            loc = UL(normal_mtx);
            if (loc >= 0) glUniformMatrix3fv(loc, 1, GL_TRUE, (const float*)g_gx.nrm_mtx[g_gx.current_mtx]);

            loc = UL(modelview_arr);
            if (loc >= 0) {
                float mv44_arr[10][16];
                int m;
                for (m = 0; m < 10; ++m) {
                    const float* src = (const float*)g_gx.pos_mtx[m];
                    mv44_arr[m][0] = src[0];   mv44_arr[m][1] = src[1];   mv44_arr[m][2] = src[2];   mv44_arr[m][3] = src[3];
                    mv44_arr[m][4] = src[4];   mv44_arr[m][5] = src[5];   mv44_arr[m][6] = src[6];   mv44_arr[m][7] = src[7];
                    mv44_arr[m][8] = src[8];   mv44_arr[m][9] = src[9];   mv44_arr[m][10] = src[10]; mv44_arr[m][11] = src[11];
                    mv44_arr[m][12] = 0.0f;    mv44_arr[m][13] = 0.0f;    mv44_arr[m][14] = 0.0f;     mv44_arr[m][15] = 1.0f;
                }
                glUniformMatrix4fv(loc, 10, GL_TRUE, &mv44_arr[0][0]);
            }
            loc = UL(normal_mtx_arr);
            if (loc >= 0) {
                glUniformMatrix3fv(loc, 10, GL_TRUE, (const float*)&g_gx.nrm_mtx[0][0][0]);
            }
            loc = UL(use_matrix_array);
            if (loc >= 0) glUniform1i(loc, pc_gx_should_use_matrix_array(submit_vertices, submit_count));
        }

        if (dirty & PC_GX_DIRTY_TEV_COLORS) {
            loc = UL(tev_prev); if (loc >= 0) glUniform4fv(loc, 1, g_gx.tev_colors[0]);
            loc = UL(tev_reg0); if (loc >= 0) glUniform4fv(loc, 1, g_gx.tev_colors[1]);
            loc = UL(tev_reg1); if (loc >= 0) glUniform4fv(loc, 1, g_gx.tev_colors[2]);
            loc = UL(tev_reg2); if (loc >= 0) glUniform4fv(loc, 1, g_gx.tev_colors[3]);
        }

        if (dirty & PC_GX_DIRTY_TEV_STAGES) {
            loc = UL(num_tev_stages); if (loc >= 0) glUniform1i(loc, g_gx.num_tev_stages);
            for (int s = 0; s < PC_GX_MAX_TEV_STAGES && s < g_gx.num_tev_stages; s++) {
                PCGXTevStage* ts = &g_gx.tev_stages[s];
                loc = UL(tev_color_in[s]); if (loc >= 0) glUniform4i(loc, ts->color_a, ts->color_b, ts->color_c, ts->color_d);
                loc = UL(tev_alpha_in[s]); if (loc >= 0) glUniform4i(loc, ts->alpha_a, ts->alpha_b, ts->alpha_c, ts->alpha_d);
                loc = UL(tev_color_op[s]); if (loc >= 0) glUniform1i(loc, ts->color_op);
                loc = UL(tev_alpha_op[s]); if (loc >= 0) glUniform1i(loc, ts->alpha_op);
                loc = UL(tev_color_chan[s]); if (loc >= 0) glUniform1i(loc, ts->color_chan);
                loc = UL(tev_bsc[s]);  if (loc >= 0) glUniform4i(loc, ts->color_bias, ts->color_scale, ts->alpha_bias, ts->alpha_scale);
                loc = UL(tev_out[s]);  if (loc >= 0) glUniform4i(loc, ts->color_clamp, ts->alpha_clamp, ts->color_out, ts->alpha_out);
                loc = UL(tev_clamp_mode[s]); if (loc >= 0) glUniform1i(loc, ts->clamp_mode);
                loc = UL(tev_swap[s]); if (loc >= 0) glUniform2i(loc, ts->ras_swap, ts->tex_swap);
            }
            loc = UL(tev_ksel);
            if (loc >= 0) {
                int ksel[PC_GX_MAX_TEV_STAGES * 3];
                for (int s = 0; s < PC_GX_MAX_TEV_STAGES; s++) {
                    ksel[s * 3 + 0] = g_gx.tev_stages[s].k_color_sel;
                    ksel[s * 3 + 1] = g_gx.tev_stages[s].k_alpha_sel;
                    ksel[s * 3 + 2] = s;
                }
                glUniform3iv(loc, PC_GX_MAX_TEV_STAGES, ksel);
            }
            for (int s = 0; s < PC_GX_MAX_TEV_STAGES; s++) {
                int tc_src = 0;
                if (s < g_gx.num_tev_stages) {
                    int tc = g_gx.tev_stages[s].tex_coord;
                    if (tc >= GX_TEXCOORD0 && tc <= GX_TEXCOORD7 &&
                        tc < (int)g_gx.num_tex_gens) tc_src = tc;
                    else tc_src = 0;
                }
                loc = UL(tev_tc_src[s]); if (loc >= 0) glUniform1i(loc, tc_src);
            }
        }

        if (dirty & PC_GX_DIRTY_SWAP_TABLES) {
            loc = UL(swap_table);
            if (loc >= 0) {
                int sw[16];
                for (int t = 0; t < 4; t++) {
                    sw[t*4+0] = g_gx.tev_swap_table[t].r;
                    sw[t*4+1] = g_gx.tev_swap_table[t].g;
                    sw[t*4+2] = g_gx.tev_swap_table[t].b;
                    sw[t*4+3] = g_gx.tev_swap_table[t].a;
                }
                glUniform4iv(loc, 4, sw);
            }
        }

        if (dirty & PC_GX_DIRTY_KONST) {
            loc = UL(kcolor); if (loc >= 0) glUniform4fv(loc, 4, (const float*)g_gx.tev_k_colors);
        }

        if (dirty & PC_GX_DIRTY_ALPHA_CMP) {
            loc = UL(alpha_comp0); if (loc >= 0) glUniform1i(loc, g_gx.alpha_comp0);
            loc = UL(alpha_ref0);  if (loc >= 0) glUniform1i(loc, g_gx.alpha_ref0);
            loc = UL(alpha_op);    if (loc >= 0) glUniform1i(loc, g_gx.alpha_op);
            loc = UL(alpha_comp1); if (loc >= 0) glUniform1i(loc, g_gx.alpha_comp1);
            loc = UL(alpha_ref1);  if (loc >= 0) glUniform1i(loc, g_gx.alpha_ref1);
        }

        if (dirty & PC_GX_DIRTY_LIGHTING) {
            loc = UL(lighting_enabled); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_enable[0]);
            loc = UL(mat_color);  if (loc >= 0) glUniform4fv(loc, 1, g_gx.chan_mat_color[0]);
            loc = UL(amb_color);  if (loc >= 0) glUniform4fv(loc, 1, g_gx.chan_amb_color[0]);
            loc = UL(chan_mat_src); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_mat_src[0]);
            loc = UL(chan_amb_src); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_amb_src[0]);
            loc = UL(chan_diff_fn); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_diff_fn[0]);
            loc = UL(chan_attn_fn); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_attn_fn[0]);

            loc = UL(lighting_enabled1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_enable[2]);
            loc = UL(mat_color1);  if (loc >= 0) glUniform4fv(loc, 1, g_gx.chan_mat_color[1]);
            loc = UL(amb_color1);  if (loc >= 0) glUniform4fv(loc, 1, g_gx.chan_amb_color[1]);
            loc = UL(chan_mat_src1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_mat_src[2]);
            loc = UL(chan_amb_src1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_amb_src[2]);
            loc = UL(chan_diff_fn1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_diff_fn[2]);
            loc = UL(chan_attn_fn1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_attn_fn[2]);

            loc = UL(num_chans);  if (loc >= 0) glUniform1i(loc, g_gx.num_chans);
            loc = UL(alpha_lighting_enabled); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_enable[1]);
            loc = UL(alpha_mat_src); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_mat_src[1]);
            loc = UL(alpha_lighting_enabled1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_enable[3]);
            loc = UL(alpha_mat_src1); if (loc >= 0) glUniform1i(loc, g_gx.chan_ctrl_mat_src[3]);
            {
                int color_light_mask = g_gx.chan_ctrl_light_mask[0];
                int color_light_mask1 = g_gx.chan_ctrl_light_mask[2];
                loc = UL(light_mask); if (loc >= 0) glUniform1i(loc, color_light_mask);
                loc = UL(light_mask1); if (loc >= 0) glUniform1i(loc, color_light_mask1);
                float lpos[8][3], ldir[8][3], lcol[8][4], lcos_att[8][3], ldist_att[8][3];
                for (int i = 0; i < 8; i++) {
                    memcpy(lpos[i], g_gx.lights[i].pos, sizeof(lpos[i]));
                    memcpy(ldir[i], g_gx.lights[i].dir, sizeof(ldir[i]));
                    memcpy(lcol[i], g_gx.lights[i].color, sizeof(lcol[i]));
                    lcos_att[i][0] = g_gx.lights[i].a0;
                    lcos_att[i][1] = g_gx.lights[i].a1;
                    lcos_att[i][2] = g_gx.lights[i].a2;
                    ldist_att[i][0] = g_gx.lights[i].k0;
                    ldist_att[i][1] = g_gx.lights[i].k1;
                    ldist_att[i][2] = g_gx.lights[i].k2;
                }
                loc = UL(light_pos[0]);   if (loc >= 0) glUniform3fv(loc, 8, &lpos[0][0]);
                loc = UL(light_dir[0]);   if (loc >= 0) glUniform3fv(loc, 8, &ldir[0][0]);
                loc = UL(light_color[0]); if (loc >= 0) glUniform4fv(loc, 8, &lcol[0][0]);
                loc = UL(light_cos_att[0]);  if (loc >= 0) glUniform3fv(loc, 8, &lcos_att[0][0]);
                loc = UL(light_dist_att[0]); if (loc >= 0) glUniform3fv(loc, 8, &ldist_att[0][0]);
            }
        }

        if (dirty & PC_GX_DIRTY_TEXGEN) {
            for (int slot = 0; slot < 10; ++slot) {
                const float* tm = (const float*)g_gx.tex_mtx[slot];
                loc = g_gx.uloc.texmtx_all_row0[slot];
                if (loc >= 0) glUniform4f(loc, tm[0], tm[1], tm[2], tm[3]);
                loc = g_gx.uloc.texmtx_all_row1[slot];
                if (loc >= 0) glUniform4f(loc, tm[4], tm[5], tm[6], tm[7]);
                loc = g_gx.uloc.texmtx_all_row2[slot];
                if (loc >= 0) glUniform4f(loc, tm[8], tm[9], tm[10], tm[11]);
            }

            for (int tg = 0; tg < PC_GX_MAX_TEXGENS; tg++) {
                int texgen_func = g_gx.tex_gen_type[tg];
                int texgen_norm = g_gx.tex_gen_normalize[tg];
                int texgen_qt_notcalc = 0;
                int mtx_id = g_gx.tex_gen_mtx[tg];
                int slot = pc_tex_mtx_id_to_slot(mtx_id);
                int has_mtx = (slot >= 0 && slot < 10);
                int has_idx_mtx = (g_gx.vtx_desc[GX_VA_TEX0MTXIDX + tg] != GX_NONE);
                int post_mtx_id = g_gx.tex_gen_post_mtx[tg];
                int post_slot = pc_pt_tex_mtx_id_to_slot(post_mtx_id);
                int has_post_mtx = (post_slot >= 0 && post_slot < 20);

                /*
                 * HW2 fast-path behavior:
                 * when a vertex has only position + one texcoord and texgen is MTX2x4,
                 * normalization is suppressed even if requested.
                 */
                if (texgen_norm && texgen_func == GX_TG_MTX2x4 &&
                    pc_gx_texgen_only_pos_and_single_texcoord()) {
                    texgen_norm = 0;
                    texgen_qt_notcalc = 1;
                }

                loc = g_gx.uloc.texmtx_enable[tg]; if (loc >= 0) glUniform1i(loc, has_mtx);
                loc = g_gx.uloc.texmtxidx_enable[tg]; if (loc >= 0) glUniform1i(loc, has_idx_mtx);
                if (has_mtx) {
                    const float* tm = (const float*)g_gx.tex_mtx[slot];
                    loc = g_gx.uloc.texmtx_row0[tg]; if (loc >= 0) glUniform4f(loc, tm[0], tm[1], tm[2], tm[3]);
                    loc = g_gx.uloc.texmtx_row1[tg]; if (loc >= 0) glUniform4f(loc, tm[4], tm[5], tm[6], tm[7]);
                    loc = g_gx.uloc.texmtx_row2[tg]; if (loc >= 0) glUniform4f(loc, tm[8], tm[9], tm[10], tm[11]);
                }
                loc = g_gx.uloc.postmtx_enable[tg]; if (loc >= 0) glUniform1i(loc, has_post_mtx);
                if (has_post_mtx) {
                    const float* ptm = (const float*)g_gx.post_tex_mtx[post_slot];
                    loc = g_gx.uloc.postmtx_row0[tg]; if (loc >= 0) glUniform4f(loc, ptm[0], ptm[1], ptm[2], ptm[3]);
                    loc = g_gx.uloc.postmtx_row1[tg]; if (loc >= 0) glUniform4f(loc, ptm[4], ptm[5], ptm[6], ptm[7]);
                    loc = g_gx.uloc.postmtx_row2[tg]; if (loc >= 0) glUniform4f(loc, ptm[8], ptm[9], ptm[10], ptm[11]);
                }
                loc = g_gx.uloc.texgen_src[tg]; if (loc >= 0) glUniform1i(loc, g_gx.tex_gen_src[tg]);
                loc = g_gx.uloc.texgen_type[tg]; if (loc >= 0) glUniform1i(loc, g_gx.tex_gen_type[tg]);
                loc = g_gx.uloc.texgen_normalize[tg]; if (loc >= 0) glUniform1i(loc, texgen_norm);
                loc = g_gx.uloc.texgen_qt_notcalc[tg]; if (loc >= 0) glUniform1i(loc, texgen_qt_notcalc);
                loc = g_gx.uloc.tcs_cyl_wrap_enable[tg];
                if (loc >= 0) glUniform2i(loc, g_gx.texcoord_cyl_wrap_s[tg] ? 1 : 0, g_gx.texcoord_cyl_wrap_t[tg] ? 1 : 0);
                loc = g_gx.uloc.tcs_manual_enable[tg]; if (loc >= 0) glUniform1i(loc, g_gx.texcoord_manual_scale_enable[tg] ? 1 : 0);
                loc = g_gx.uloc.tcs_manual_scale[tg];
                if (loc >= 0) {
                    float ss = (float)g_gx.texcoord_manual_scale_s[tg];
                    float ts = (float)g_gx.texcoord_manual_scale_t[tg];
                    if (ss < 1.0f) ss = 1.0f;
                    if (ts < 1.0f) ts = 1.0f;
                    glUniform2f(loc, ss, ts);
                }
            }
        }

        if (dirty & (PC_GX_DIRTY_TEXTURES | PC_GX_DIRTY_TEV_STAGES)) {
            int use_tex_stage[PC_GX_MAX_TEV_STAGES] = { 0 };
            GLuint tex_obj_stage[PC_GX_MAX_TEV_STAGES] = { 0 };
            for (int s = 0; s < PC_GX_MAX_TEV_STAGES; s++) {
                if (s < g_gx.num_tev_stages) {
                    int tc = g_gx.tev_stages[s].tex_coord;
                    int tex_map = g_gx.tev_stages[s].tex_map;
                    int tex_lookup_enable = g_gx.tev_stages[s].tex_lookup_enable;
                    int tc_valid = (tc >= GX_TEXCOORD0 && tc <= GX_TEXCOORD7 &&
                                    tc < (int)g_gx.num_tex_gens);
                    if (tex_lookup_enable && tc_valid && tex_map >= 0 && tex_map < 8)
                        tex_obj_stage[s] = g_gx.gl_textures[tex_map];
                }
                if (tex_obj_stage[s] != 0) {
                    use_tex_stage[s] = 1;
                    glActiveTexture(GL_TEXTURE0 + s);
                    glBindTexture(GL_TEXTURE_2D, tex_obj_stage[s]);
                }
            }
            for (int s = 0; s < PC_GX_MAX_TEV_STAGES; s++) {
                loc = UL(use_texture[s]);
                if (loc >= 0) glUniform1i(loc, use_tex_stage[s]);
                loc = UL(texture[s]);
                if (loc >= 0) glUniform1i(loc, s);
            }
        }

        /* Indirect textures on units after regular TEV stage samplers. */
        if (dirty & (PC_GX_DIRTY_INDIRECT | PC_GX_DIRTY_TEXTURES)) {
            loc = UL(num_ind_stages); if (loc >= 0) glUniform1i(loc, g_gx.num_ind_stages);
            for (int i = 0; i < 4; i++) {
                int ind_tex_map = g_gx.ind_order[i].tex_map;
                if (i < g_gx.num_ind_stages && ind_tex_map >= 0 && ind_tex_map < 8) {
                    GLuint ind_tex = g_gx.gl_textures[ind_tex_map];
                    if (ind_tex) {
                        glActiveTexture(GL_TEXTURE0 + PC_GX_MAX_TEV_STAGES + i);
                        glBindTexture(GL_TEXTURE_2D, ind_tex);
                    }
                }
                loc = UL(ind_tex[i]); if (loc >= 0) glUniform1i(loc, PC_GX_MAX_TEV_STAGES + i);
                loc = UL(ind_tc_src[i]); if (loc >= 0) glUniform1i(loc, g_gx.ind_order[i].tex_coord);
                loc = UL(ind_scale[i]);
                if (loc >= 0) {
                    float s_scale = 1.0f / (float)(1 << g_gx.ind_order[i].scale_s);
                    float t_scale = 1.0f / (float)(1 << g_gx.ind_order[i].scale_t);
                    glUniform2f(loc, s_scale, t_scale);
                }
            }
            for (int i = 0; i < 3; i++) {
                float scale_val = ldexpf(1.0f, g_gx.ind_mtx_scale[i]);
                float packed[6];
                for (int j = 0; j < 6; j++)
                    packed[j] = ((float*)g_gx.ind_mtx[i])[j] * scale_val;
                loc = UL(ind_mtx_r0[i]); if (loc >= 0) glUniform3f(loc, packed[0], packed[1], packed[2]);
                loc = UL(ind_mtx_r1[i]); if (loc >= 0) glUniform3f(loc, packed[3], packed[4], packed[5]);
            }
            for (int s = 0; s < g_gx.num_tev_stages && s < PC_GX_MAX_TEV_STAGES; s++) {
                PCGXTevStage* ts = &g_gx.tev_stages[s];
                loc = UL(tev_ind_cfg[s]);
                if (loc >= 0) glUniform4i(loc, ts->ind_stage, ts->ind_mtx, ts->ind_bias,
                                          ((ts->ind_format & 0x3) << 2) | (ts->ind_alpha & 0x3));
                loc = UL(tev_ind_wrap[s]);
                if (loc >= 0) glUniform3i(loc, ts->ind_wrap_s, ts->ind_wrap_t,
                                          (ts->ind_add_prev ? 1 : 0) | ((ts->ind_lod ? 1 : 0) << 1));
            }
        }
        glActiveTexture(GL_TEXTURE0);

        if (dirty & PC_GX_DIRTY_FOG) {
            loc = UL(fog_type);  if (loc >= 0) glUniform1i(loc, g_gx.fog_type);
            loc = UL(fog_start); if (loc >= 0) glUniform1f(loc, g_gx.fog_start);
            loc = UL(fog_end);   if (loc >= 0) glUniform1f(loc, g_gx.fog_end);
            loc = UL(fog_color);  if (loc >= 0) glUniform4fv(loc, 1, g_gx.fog_color);
        }

        if (dirty & PC_GX_DIRTY_DEPTH) {
            float z_bias_nrm = (float)(g_gx.ztex_bias & 0x00FFFFFF) / 16777215.0f;
            int z_fmt_mode = 2; /* 0=Z8, 1=Z16, 2=Z24X8 */
            if (g_gx.ztex_fmt == GX_TF_Z8) {
                z_fmt_mode = 0;
            } else if (g_gx.ztex_fmt == GX_TF_Z16) {
                z_fmt_mode = 1;
            }

            loc = UL(ztex_enable); if (loc >= 0) glUniform1i(loc, g_gx.ztex_op != GX_ZT_DISABLE);
            loc = UL(ztex_op);     if (loc >= 0) glUniform1i(loc, g_gx.ztex_op);
            loc = UL(ztex_fmt);    if (loc >= 0) glUniform1i(loc, z_fmt_mode);
            loc = UL(ztex_bias);   if (loc >= 0) glUniform1f(loc, z_bias_nrm);
            loc = UL(ztex_bias24); if (loc >= 0) glUniform1i(loc, (g_gx.ztex_bias & 0x00FFFFFF));
        }

        #undef UL
    }

    GLenum gl_prim;
    switch (g_gx.current_primitive) {
        case GX_QUADS:         gl_prim = GL_TRIANGLES; break;
        case GX_TRIANGLES:     gl_prim = GL_TRIANGLES; break;
        case GX_TRIANGLESTRIP: gl_prim = GL_TRIANGLE_STRIP; break;
        case GX_TRIANGLEFAN:   gl_prim = GL_TRIANGLE_FAN; break;
        case GX_LINES:         gl_prim = GL_LINES; break;
        case GX_LINESTRIP:     gl_prim = GL_LINE_STRIP; break;
        case GX_POINTS:        gl_prim = GL_POINTS; break;
        default:               gl_prim = GL_TRIANGLES; break;
    }

    if (g_gx.dirty & PC_GX_DIRTY_DEPTH) {
        GLboolean depth_write_enable = GL_FALSE;
        if (g_gx.z_compare_enable) {
            glEnable(GL_DEPTH_TEST);
            GLenum zfunc;
            switch (g_gx.z_compare_func) {
                case GX_NEVER:   zfunc = GL_NEVER; break;
                case GX_LESS:    zfunc = GL_LESS; break;
                case GX_EQUAL:   zfunc = GL_EQUAL; break;
                case GX_LEQUAL:  zfunc = GL_LEQUAL; break;
                case GX_GREATER: zfunc = GL_GREATER; break;
                case GX_NEQUAL:  zfunc = GL_NOTEQUAL; break;
                case GX_GEQUAL:  zfunc = GL_GEQUAL; break;
                case GX_ALWAYS:  zfunc = GL_ALWAYS; break;
                default:         zfunc = GL_LEQUAL; break;
            }
            glDepthFunc(zfunc);
            depth_write_enable = g_gx.z_update_enable ? GL_TRUE : GL_FALSE;
        } else {
            /* SDK docs: compare disabled => Z buffering disabled and no Z updates. */
            glDisable(GL_DEPTH_TEST);
            depth_write_enable = GL_FALSE;
        }
        glDepthMask(depth_write_enable);
    }

    if (g_gx.dirty & PC_GX_DIRTY_COLOR_MASK) {
        int depth_only_color_target = (g_gx.pixel_fmt == GX_PF_Z24);
        glColorMask(
            (g_gx.color_update_enable && !depth_only_color_target) ? GL_TRUE : GL_FALSE,
            (g_gx.color_update_enable && !depth_only_color_target) ? GL_TRUE : GL_FALSE,
            (g_gx.color_update_enable && !depth_only_color_target) ? GL_TRUE : GL_FALSE,
            (g_gx.alpha_update_enable && !depth_only_color_target) ? GL_TRUE : GL_FALSE
        );
        if (g_gx.dither_enable) {
            glEnable(GL_DITHER);
        } else {
            glDisable(GL_DITHER);
        }
    }

    if (g_gx.dirty & PC_GX_DIRTY_CULL) {
        glFrontFace(pc_gx_front_face_is_cw() ? GL_CW : GL_CCW);
        switch (g_gx.cull_mode) {
            case GX_CULL_NONE:  glDisable(GL_CULL_FACE); break;
            case GX_CULL_FRONT: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
            case GX_CULL_BACK:  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
            case GX_CULL_ALL:   glEnable(GL_CULL_FACE); glCullFace(GL_FRONT_AND_BACK); break;
        }
    }

    if (g_gx.dirty & PC_GX_DIRTY_BLEND) {
        switch (g_gx.blend_mode) {
            case GX_BM_NONE:
                glDisable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                break;
            case GX_BM_BLEND:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                {
                    GLenum src, dst;
                    switch (g_gx.blend_src) {
                        case GX_BL_ZERO:        src = GL_ZERO; break;
                        case GX_BL_ONE:         src = GL_ONE; break;
                        case GX_BL_DSTCLR:      src = GL_DST_COLOR; break;
                        case GX_BL_INVDSTCLR:   src = GL_ONE_MINUS_DST_COLOR; break;
                        case GX_BL_SRCALPHA:    src = GL_SRC_ALPHA; break;
                        case GX_BL_INVSRCALPHA: src = GL_ONE_MINUS_SRC_ALPHA; break;
                        case GX_BL_DSTALPHA:    src = GL_DST_ALPHA; break;
                        case GX_BL_INVDSTALPHA: src = GL_ONE_MINUS_DST_ALPHA; break;
                        default:                src = GL_ONE; break;
                    }
                    switch (g_gx.blend_dst) {
                        case GX_BL_ZERO:        dst = GL_ZERO; break;
                        case GX_BL_ONE:         dst = GL_ONE; break;
                        case GX_BL_SRCCLR:      dst = GL_SRC_COLOR; break;
                        case GX_BL_INVSRCCLR:   dst = GL_ONE_MINUS_SRC_COLOR; break;
                        case GX_BL_SRCALPHA:    dst = GL_SRC_ALPHA; break;
                        case GX_BL_INVSRCALPHA: dst = GL_ONE_MINUS_SRC_ALPHA; break;
                        case GX_BL_DSTALPHA:    dst = GL_DST_ALPHA; break;
                        case GX_BL_INVDSTALPHA: dst = GL_ONE_MINUS_DST_ALPHA; break;
                        default:                dst = GL_ZERO; break;
                    }
                    /* DST_ALPHAâ†’SRC_ALPHA: GC EFB alpha semantics differ from GL */
                    if (g_gx.blend_src == GX_BL_DSTALPHA && g_gx.blend_dst == GX_BL_INVDSTALPHA) {
                        src = GL_SRC_ALPHA;
                        dst = GL_ONE_MINUS_SRC_ALPHA;
                    }
                    glBlendFunc(src, dst);
                }
                break;
            case GX_BM_LOGIC:
                glDisable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                break;
            case GX_BM_SUBTRACT:
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                glBlendFunc(GL_ONE, GL_ONE);
                break;
        }
    }

    if (g_gx.copy_aa_enable) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }

    if (submit_vertices == g_pc_gx_expanded_prim) {
        gl_prim = GL_TRIANGLES;
    }

    /* GXSetZCompLoc(GX_TRUE): depth compare/write occurs before texturing.
     * To emulate this with shader alpha test (discard), run a depth-only prepass
     * when alpha compare is active so depth updates are not blocked by discard. */
    int used_z_prepass = 0;
    GLint z_prepass_saved_depth_func = GL_LEQUAL;
    GLboolean z_prepass_saved_depth_mask = GL_TRUE;
    {
        int alpha_compare_active = !(
            g_gx.alpha_comp0 == GX_ALWAYS &&
            g_gx.alpha_comp1 == GX_ALWAYS
        );
        int do_z_prepass = (shader != 0) &&
                           g_gx.z_compare_enable &&
                           g_gx.z_update_enable &&
                           g_gx.z_comp_loc_before_tex &&
                           alpha_compare_active;

        if (do_z_prepass) {
            GLboolean old_color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
            GLboolean old_depth_mask = GL_TRUE;
            GLboolean old_blend = glIsEnabled(GL_BLEND);
            GLint loc;

            glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
            glGetIntegerv(GL_DEPTH_FUNC, &z_prepass_saved_depth_func);
            z_prepass_saved_depth_mask = old_depth_mask;

            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            loc = g_gx.uloc.alpha_comp0; if (loc >= 0) glUniform1i(loc, GX_ALWAYS);
            loc = g_gx.uloc.alpha_ref0;  if (loc >= 0) glUniform1i(loc, 0);
            loc = g_gx.uloc.alpha_op;    if (loc >= 0) glUniform1i(loc, GX_AOP_AND);
            loc = g_gx.uloc.alpha_comp1; if (loc >= 0) glUniform1i(loc, GX_ALWAYS);
            loc = g_gx.uloc.alpha_ref1;  if (loc >= 0) glUniform1i(loc, 0);

            if (g_gx.current_primitive == GX_QUADS) {
                int num_quads = count / 4;
                int num_indices = num_quads * 6;
                glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, 0);
                PC_GL_CHECK("glDrawElements(z_prepass)");
            } else {
                glDrawArrays(gl_prim, 0, submit_count);
                PC_GL_CHECK("glDrawArrays(z_prepass)");
            }

            loc = g_gx.uloc.alpha_comp0; if (loc >= 0) glUniform1i(loc, g_gx.alpha_comp0);
            loc = g_gx.uloc.alpha_ref0;  if (loc >= 0) glUniform1i(loc, g_gx.alpha_ref0);
            loc = g_gx.uloc.alpha_op;    if (loc >= 0) glUniform1i(loc, g_gx.alpha_op);
            loc = g_gx.uloc.alpha_comp1; if (loc >= 0) glUniform1i(loc, g_gx.alpha_comp1);
            loc = g_gx.uloc.alpha_ref1;  if (loc >= 0) glUniform1i(loc, g_gx.alpha_ref1);

            glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
            if (old_blend) glEnable(GL_BLEND);
            else glDisable(GL_BLEND);

            /*
             * Main color pass should hit exactly the fragments that survived
             * prepass regardless of original compare func (e.g. GX_LESS).
             */
            glDepthFunc(GL_EQUAL);
            glDepthMask(GL_FALSE);
            used_z_prepass = 1;
        }
    }

    if (g_gx.current_primitive == GX_QUADS) {
        int num_quads = count / 4;
        int num_indices = num_quads * 6;
        glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, 0);
        PC_GL_CHECK("glDrawElements");
    } else {
        glDrawArrays(gl_prim, 0, submit_count);
        PC_GL_CHECK("glDrawArrays");
    }

    if (used_z_prepass) {
        glDepthFunc((GLenum)z_prepass_saved_depth_func);
        glDepthMask(z_prepass_saved_depth_mask);
    }

    if (g_gx.blend_mode == GX_BM_SUBTRACT)
        glBlendEquation(GL_FUNC_ADD);

    if (submit_count > 0) {
        pc_gx_bbox_touch_draw();
    }

    g_gx.dirty = 0;
}

/* --- Vertex Descriptor / Format --- */
void GXSetVtxDesc(GXAttr attr, GXAttrType type) {
    if (attr < GX_VA_PNMTXIDX || attr >= GX_VA_MAX_ATTR || attr >= PC_GX_MAX_ATTR) return;
    if (type < GX_NONE || type > GX_INDEX16) return;

    /* SDK behavior: NRM and NBT share one descriptor slot; setting one enabled
     * disables the other unless explicitly set to GX_NONE. */
    if (attr == GX_VA_NRM) {
        g_gx.vtx_desc[GX_VA_NRM] = (int)type;
        if (type != GX_NONE) g_gx.vtx_desc[GX_VA_NBT] = GX_NONE;
        return;
    }
    if (attr == GX_VA_NBT) {
        g_gx.vtx_desc[GX_VA_NBT] = (int)type;
        if (type != GX_NONE) g_gx.vtx_desc[GX_VA_NRM] = GX_NONE;
        return;
    }

    g_gx.vtx_desc[attr] = (int)type;
}

void GXSetVtxDescv(GXVtxDescList* list) {
    if (!list) return;
    while (list->attr != GX_VA_NULL) {
        GXSetVtxDesc(list->attr, list->type);
        ++list;
    }
}

void GXClearVtxDesc(void) {
    memset(g_gx.vtx_desc, 0, sizeof(g_gx.vtx_desc));
    /* SDK default keeps position enabled as direct input after clear. */
    g_gx.vtx_desc[GX_VA_POS] = GX_DIRECT;
}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {
    PCGXVertexFormat* fmt;
    int tc;

    if ((u32)vtxfmt >= GX_MAX_VTXFMT) return;
    if ((u32)attr < GX_VA_POS || (u32)attr > GX_VA_MAX_ATTR) return;
    if (frac >= 32) return;

    /* SDK VAT entries are only defined for POS/NRM-NBT/CLR0-1/TEX0-7. */
    if (!(
        attr == GX_VA_POS || attr == GX_VA_NRM || attr == GX_VA_NBT ||
        attr == GX_VA_CLR0 || attr == GX_VA_CLR1 ||
        (attr >= GX_VA_TEX0 && attr <= GX_VA_TEX7)
    )) {
        return;
    }

    fmt = &g_gx.vtx_fmt[vtxfmt];

    if (attr == GX_VA_POS) {
        fmt->attr_cnt[GX_VA_POS] = (u8)((cnt == GX_POS_XY) ? GX_POS_XY : GX_POS_XYZ);
        fmt->attr_type[GX_VA_POS] = (u8)type;
        fmt->attr_frac[GX_VA_POS] = frac;
        fmt->has_position = 1;
        return;
    }

    if (attr == GX_VA_NRM || attr == GX_VA_NBT) {
        /* SDK behavior: NRM and NBT share VAT cnt/type/frac storage. */
        u8 nrm_cnt = (u8)cnt;
        if (attr == GX_VA_NRM) {
            nrm_cnt = (u8)GX_NRM_XYZ;
        } else if (nrm_cnt != GX_NRM_NBT && nrm_cnt != GX_NRM_NBT3) {
            nrm_cnt = (u8)GX_NRM_NBT;
        }

        fmt->attr_cnt[GX_VA_NRM] = nrm_cnt;
        fmt->attr_cnt[GX_VA_NBT] = nrm_cnt;
        fmt->attr_type[GX_VA_NRM] = (u8)type;
        fmt->attr_type[GX_VA_NBT] = (u8)type;
        /* Normal VAT uses implied fixed-point fractions (S8=6, S16=14). */
        fmt->attr_frac[GX_VA_NRM] = 0;
        fmt->attr_frac[GX_VA_NBT] = 0;
        fmt->has_normal = 1;
        return;
    }

    if (attr == GX_VA_CLR0 || attr == GX_VA_CLR1) {
        fmt->attr_cnt[attr] = (u8)((cnt == GX_CLR_RGB) ? GX_CLR_RGB : GX_CLR_RGBA);
        fmt->attr_type[attr] = (u8)type;
        /* Color VAT has no fractional field in hardware. */
        fmt->attr_frac[attr] = 0;
        if (attr == GX_VA_CLR0) fmt->has_color0 = 1;
        else fmt->has_color1 = 1;
        return;
    }

    /* TEX0-7 */
    tc = (int)attr - GX_VA_TEX0;
    fmt->attr_cnt[attr] = (u8)((cnt == GX_TEX_S) ? GX_TEX_S : GX_TEX_ST);
    fmt->attr_type[attr] = (u8)type;
    fmt->attr_frac[attr] = frac;
    fmt->has_texcoord[tc] = 1;
    fmt->texcoord_frac[tc] = frac;
}

void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, const GXVtxAttrFmtList* list) {
    if (!list) return;
    if ((u32)vtxfmt >= GX_MAX_VTXFMT) return;

    while (list->attr != GX_VA_NULL) {
        if ((u32)list->attr < GX_VA_POS || (u32)list->attr > GX_VA_MAX_ATTR) return;
        if (list->frac >= 32) return;
        GXSetVtxAttrFmt(vtxfmt, list->attr, list->cnt, list->type, list->frac);
        ++list;
    }
}

void GXSetArray(GXAttr attr, const void* data, u8 stride) {
    GXAttr cp_attr = attr;

    /* SDK behavior: NBT shares NRM attribute array slot. */
    if (cp_attr == GX_VA_NBT) {
        cp_attr = GX_VA_NRM;
    }

    /* SDK accepts GX_VA_POS .. GX_LIGHT_ARRAY for GXSetArray. */
    if ((u32)cp_attr < GX_VA_POS || (u32)cp_attr > GX_LIGHT_ARRAY) return;
    if ((u32)cp_attr >= PC_GX_MAX_ATTR) return;

    g_gx.array_base[cp_attr] = data;
    g_gx.array_stride[cp_attr] = stride;
}

void GXInvalidateVtxCache(void) { }

/* --- Transforms --- */
static int pc_gx_pos_nrm_mtx_id_to_slot(u32 id) {
    if ((id % 3u) != 0u) return -1;
    if (id > GX_PNMTX9) return -1;
    return (int)(id / 3u);
}

void GXSetProjection(const void* mtx, u32 type) {
    const float (*src)[4];
    float p0, p1, p2, p3, p4, p5;

    if (!mtx) return;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_PROJECTION);
    g_gx.projection_type = (type == GX_ORTHOGRAPHIC) ? GX_ORTHOGRAPHIC : GX_PERSPECTIVE;

    src = (const float (*)[4])mtx;
    if (g_gx.projection_type == GX_PERSPECTIVE) {
        /* GXSetProjection docs: only p0..p5 are consumed for perspective. */
        p0 = src[0][0];
        p1 = src[0][2];
        p2 = src[1][1];
        p3 = src[1][2];
        p4 = src[2][2];
        p5 = src[2][3];

        memset(g_gx.projection_mtx, 0, sizeof(g_gx.projection_mtx));
        g_gx.projection_mtx[0][0] = p0;
        g_gx.projection_mtx[0][2] = p1;
        g_gx.projection_mtx[1][1] = p2;
        g_gx.projection_mtx[1][2] = p3;
        g_gx.projection_mtx[2][2] = p4;
        g_gx.projection_mtx[2][3] = p5;
        g_gx.projection_mtx[3][2] = -1.0f;
    } else {
        /* GXSetProjection docs: only p0..p5 are consumed for orthographic. */
        p0 = src[0][0];
        p1 = src[0][3];
        p2 = src[1][1];
        p3 = src[1][3];
        p4 = src[2][2];
        p5 = src[2][3];

        memset(g_gx.projection_mtx, 0, sizeof(g_gx.projection_mtx));
        g_gx.projection_mtx[0][0] = p0;
        g_gx.projection_mtx[0][3] = p1;
        g_gx.projection_mtx[1][1] = p2;
        g_gx.projection_mtx[1][3] = p3;
        g_gx.projection_mtx[2][2] = p4;
        g_gx.projection_mtx[2][3] = p5;
        g_gx.projection_mtx[3][3] = 1.0f;
    }

#ifdef PC_ENHANCEMENTS
    /* Widescreen: 0=hor+ (both), 1=stretch (none), 2=UI (ortho only) */
    if (g_pc_widescreen_stretch == 0 ||
        (g_pc_widescreen_stretch == 2 && type == GX_ORTHOGRAPHIC)) {
        if (g_aspect_active) {
            g_gx.projection_mtx[0][0] *= g_aspect_factor;
        }
    }
#endif
}

void GXSetProjectionv(const f32* ptr) {
    float mtx[4][4];
    u32 type;

    if (!ptr) return;

    type = (u32)ptr[0];
    memset(mtx, 0, sizeof(mtx));

    mtx[0][0] = ptr[1];
    mtx[1][1] = ptr[3];
    mtx[2][2] = ptr[5];
    mtx[2][3] = ptr[6];

    if (type == GX_ORTHOGRAPHIC) {
        mtx[0][3] = ptr[2];
        mtx[1][3] = ptr[4];
        mtx[3][3] = 1.0f;
    } else {
        mtx[0][2] = ptr[2];
        mtx[1][2] = ptr[4];
        mtx[3][2] = -1.0f;
    }

    GXSetProjection(mtx, type);
}

void GXLoadPosMtxImm(const void* mtx, u32 id) {
    int slot;
    if (!mtx) return;
    slot = pc_gx_pos_nrm_mtx_id_to_slot(id);
    if (slot < 0 || slot >= 10) return;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    memcpy(g_gx.pos_mtx[slot], mtx, sizeof(float) * 12);
}

void GXLoadNrmMtxImm(const void* mtx, u32 id) {
    int slot;
    if (!mtx) return;
    slot = pc_gx_pos_nrm_mtx_id_to_slot(id);
    if (slot < 0 || slot >= 10) return;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    /* Extract upper-left 3x3 from 3x4 row-major Mtx (stride 4, not contiguous) */
    {
        const float* src = (const float*)mtx;
        g_gx.nrm_mtx[slot][0][0] = src[0]; g_gx.nrm_mtx[slot][0][1] = src[1]; g_gx.nrm_mtx[slot][0][2] = src[2];
        g_gx.nrm_mtx[slot][1][0] = src[4]; g_gx.nrm_mtx[slot][1][1] = src[5]; g_gx.nrm_mtx[slot][1][2] = src[6];
        g_gx.nrm_mtx[slot][2][0] = src[8]; g_gx.nrm_mtx[slot][2][1] = src[9]; g_gx.nrm_mtx[slot][2][2] = src[10];
    }
}

void GXLoadNrmMtxImm3x3(const void* mtx, u32 id) {
    int slot;
    const float* src;
    if (!mtx) return;
    slot = pc_gx_pos_nrm_mtx_id_to_slot(id);
    if (slot < 0 || slot >= 10) return;
    src = (const float*)mtx;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    g_gx.nrm_mtx[slot][0][0] = src[0]; g_gx.nrm_mtx[slot][0][1] = src[1]; g_gx.nrm_mtx[slot][0][2] = src[2];
    g_gx.nrm_mtx[slot][1][0] = src[3]; g_gx.nrm_mtx[slot][1][1] = src[4]; g_gx.nrm_mtx[slot][1][2] = src[5];
    g_gx.nrm_mtx[slot][2][0] = src[6]; g_gx.nrm_mtx[slot][2][1] = src[7]; g_gx.nrm_mtx[slot][2][2] = src[8];
}

void GXLoadTexMtxImm(const void* mtx, u32 id, u32 type) {
    int slot;
    int post_slot;
    float (*dst)[4];
    const float* src;
    if (!mtx) return;
    src = (const float*)mtx;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEXGEN);
    slot = pc_tex_mtx_id_to_slot((int)id);
    post_slot = pc_pt_tex_mtx_id_to_slot((int)id);
    if (slot < 0 && post_slot < 0) return;
    dst = (slot >= 0) ? g_gx.tex_mtx[slot] : g_gx.post_tex_mtx[post_slot];

    if (type == GX_MTX2x4) {
        /* For 2x4 tex matrices, hardware effectively uses q=1. */
        dst[0][0] = src[0]; dst[0][1] = src[1]; dst[0][2] = src[2]; dst[0][3] = src[3];
        dst[1][0] = src[4]; dst[1][1] = src[5]; dst[1][2] = src[6]; dst[1][3] = src[7];
        dst[2][0] = 0.0f;   dst[2][1] = 0.0f;   dst[2][2] = 0.0f;   dst[2][3] = 1.0f;
        return;
    }

    /* GX_MTX3x4 (and unknown values for compatibility): copy all 12 floats. */
    memcpy(dst, mtx, sizeof(float) * 12);
}

void GXLoadPosMtxIndx(u16 index, u32 id) {
    const u8* base = (const u8*)g_gx.array_base[GX_POS_MTX_ARRAY];
    u8 stride = g_gx.array_stride[GX_POS_MTX_ARRAY];
    const void* src;

    if (!base) return;
    if (stride == 0) stride = (u8)(sizeof(float) * 12);
    src = base + ((size_t)index * (size_t)stride);
    GXLoadPosMtxImm(src, id);
}

void GXLoadNrmMtxIndx3x3(u16 index, u32 id) {
    const u8* nrm_base = (const u8*)g_gx.array_base[GX_NRM_MTX_ARRAY];
    u8 nrm_stride = g_gx.array_stride[GX_NRM_MTX_ARRAY];
    int slot = pc_gx_pos_nrm_mtx_id_to_slot(id);

    if (slot < 0 || slot >= 10) return;
    if (!nrm_base) return;

    {
        const float* src;
        if (nrm_stride == 0) nrm_stride = (u8)(sizeof(float) * 9);
        src = (const float*)(nrm_base + ((size_t)index * (size_t)nrm_stride));
        g_gx.nrm_mtx[slot][0][0] = src[0];
        g_gx.nrm_mtx[slot][0][1] = src[1];
        g_gx.nrm_mtx[slot][0][2] = src[2];
        g_gx.nrm_mtx[slot][1][0] = src[3];
        g_gx.nrm_mtx[slot][1][1] = src[4];
        g_gx.nrm_mtx[slot][1][2] = src[5];
        g_gx.nrm_mtx[slot][2][0] = src[6];
        g_gx.nrm_mtx[slot][2][1] = src[7];
        g_gx.nrm_mtx[slot][2][2] = src[8];
        DIRTY(PC_GX_DIRTY_MODELVIEW);
    }
}

void GXLoadTexMtxIndx(u16 index, u32 id, u32 type) {
    const u8* base = (const u8*)g_gx.array_base[GX_TEX_MTX_ARRAY];
    u8 stride = g_gx.array_stride[GX_TEX_MTX_ARRAY];
    const void* src;

    if (!base) return;
    if (stride == 0) stride = (u8)((type == GX_MTX2x4) ? (sizeof(float) * 8) : (sizeof(float) * 12));
    src = base + ((size_t)index * (size_t)stride);
    GXLoadTexMtxImm(src, id, type);
}

void GXSetCurrentMtx(u32 id) {
    int slot = pc_gx_pos_nrm_mtx_id_to_slot(id);
    if (slot < 0 || slot >= 10) return;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_MODELVIEW);
    g_gx.current_mtx = (u32)slot;
}

static void pc_gx_apply_viewport(void) {
    float left = g_gx.viewport[0] - (float)g_gx.scissor_offset[0];
    float top = g_gx.viewport[1] - (float)g_gx.scissor_offset[1];
    float wd = g_gx.viewport[2];
    float ht = g_gx.viewport[3];
    float nearz = g_gx.viewport[4];
    float farz = g_gx.viewport[5];

#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_window_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_window_h / (float)PC_GC_HEIGHT;
        float adj_left = left;
        float adj_wd = wd;

        /* UI mode: remap sub-viewports to match aspect-corrected content */
        if (g_pc_widescreen_stretch == 2 && g_aspect_active) {
            int is_full = (g_gx.viewport[0] < 1.0f && g_gx.viewport[1] < 1.0f &&
                           g_gx.viewport[2] > (float)(PC_GC_WIDTH - 1) &&
                           g_gx.viewport[3] > (float)(PC_GC_HEIGHT - 1));
            if (!is_full) {
                adj_left = g_aspect_offset + left * g_aspect_factor;
                adj_wd = wd * g_aspect_factor;
            }
        }

        int gl_x = (int)(adj_left * sx);
        int gl_w = (int)(adj_wd * sx);
        int gl_h = (int)(ht * sy);
        int gl_y = g_pc_window_h - (int)(top * sy) - gl_h;
        glViewport(gl_x, gl_y, gl_w, gl_h);
    }
#else
    /* GX is Y-down, GL is Y-up */
    glViewport((int)left, PC_GC_HEIGHT - (int)top - (int)ht, (int)wd, (int)ht);
#endif
    glDepthRange((double)nearz, (double)farz);
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    g_gx.viewport[0] = left;
    g_gx.viewport[1] = top;
    g_gx.viewport[2] = wd;
    g_gx.viewport[3] = ht;
    g_gx.viewport[4] = nearz;
    g_gx.viewport[5] = farz;
    pc_gx_apply_viewport();
}

void GXSetViewportv(const f32* vp) {
    GXSetViewport(vp[0], vp[1], vp[2], vp[3], vp[4], vp[5]);
}

void GXGetViewportv(f32* vp) {
    memcpy(vp, g_gx.viewport, sizeof(f32) * 6);
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    f32 jittered_top = top;
    if (field == VI_FIELD_BELOW) {
        jittered_top -= 0.5f;
    }
    GXSetViewport(left, jittered_top, wd, ht, nearz, farz);
}

static void pc_gx_apply_scissor(void) {
    int left = g_gx.scissor[0] - g_gx.scissor_offset[0];
    int top = g_gx.scissor[1] - g_gx.scissor_offset[1];
    int wd = g_gx.scissor[2];
    int ht = g_gx.scissor[3];

    if (wd <= 0 || ht <= 0) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    if (left < 0) {
        wd += left;
        left = 0;
    }
    if (top < 0) {
        ht += top;
        top = 0;
    }

    if (left >= PC_GC_WIDTH || top >= PC_GC_HEIGHT) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    if (left + wd > PC_GC_WIDTH) wd = PC_GC_WIDTH - left;
    if (top + ht > PC_GC_HEIGHT) ht = PC_GC_HEIGHT - top;

    if (wd <= 0 || ht <= 0) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    glEnable(GL_SCISSOR_TEST);
#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_window_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_window_h / (float)PC_GC_HEIGHT;
        int gl_x = (int)(left * sx);
        int gl_w = (int)(wd * sx);
        int gl_h = (int)(ht * sy);
        int gl_y = g_pc_window_h - (int)(top * sy) - gl_h;
        glScissor(gl_x, gl_y, gl_w, gl_h);
    }
#else
    /* GX is Y-down, GL is Y-up */
    glScissor(left, PC_GC_HEIGHT - top - ht, wd, ht);
#endif
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    pc_gx_flush_if_begin_complete();
    g_gx.scissor[0] = left;
    g_gx.scissor[1] = top;
    g_gx.scissor[2] = wd;
    g_gx.scissor[3] = ht;
    pc_gx_apply_scissor();
}

void GXGetScissor(u32* xOrig, u32* yOrig, u32* wd, u32* ht) {
    *xOrig = (u32)g_gx.scissor[0];
    *yOrig = (u32)g_gx.scissor[1];
    *wd    = (u32)g_gx.scissor[2];
    *ht    = (u32)g_gx.scissor[3];
}

void GXSetScissorBoxOffset(s32 x, s32 y) {
    pc_gx_flush_if_begin_complete();
    /* The GP works on 2x2 regions; offset values must be even. */
    x &= ~1;
    y &= ~1;
    g_gx.scissor_offset[0] = x;
    g_gx.scissor_offset[1] = y;
    pc_gx_apply_viewport();
    pc_gx_apply_scissor();
}
void GXSetClipMode(GXClipMode mode) {
    pc_gx_flush_if_begin_complete();
    g_gx.clip_mode = (mode == GX_CLIP_DISABLE) ? GX_CLIP_DISABLE : GX_CLIP_ENABLE;
#ifdef GL_DEPTH_CLAMP
    /*
     * Keep clip-disable as a no-op for now.
     * Direct GX clip-disable emulation does not map cleanly to GL state.
     */
    glDisable(GL_DEPTH_CLAMP);
#endif
}

void GXGetProjectionv(f32* p) {
    p[0] = (f32)g_gx.projection_type;
    p[1] = g_gx.projection_mtx[0][0];
    p[3] = g_gx.projection_mtx[1][1];
    p[5] = g_gx.projection_mtx[2][2];
    p[6] = g_gx.projection_mtx[2][3];

    if (g_gx.projection_type == GX_ORTHOGRAPHIC) {
        p[2] = g_gx.projection_mtx[0][3];
        p[4] = g_gx.projection_mtx[1][3];
    } else {
        p[2] = g_gx.projection_mtx[0][2];
        p[4] = g_gx.projection_mtx[1][2];
    }
}

void GXProject(f32 x, f32 y, f32 z, f32 mtx[3][4], f32* pm, f32* vp, f32* sx, f32* sy, f32* sz) {
    f32 eye_x;
    f32 eye_y;
    f32 eye_z;
    f32 xc;
    f32 yc;
    f32 zc;
    f32 wc;

    if (!mtx || !pm || !vp || !sx || !sy || !sz) return;

    eye_x = mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    eye_y = mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    eye_z = mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));

    if (pm[0] == 0.0f) {
        xc = (eye_x * pm[1]) + (eye_z * pm[2]);
        yc = (eye_y * pm[3]) + (eye_z * pm[4]);
        zc = pm[6] + (eye_z * pm[5]);
        wc = 1.0f / -eye_z;
    } else {
        xc = pm[2] + (eye_x * pm[1]);
        yc = pm[4] + (eye_y * pm[3]);
        zc = pm[6] + (eye_z * pm[5]);
        wc = 1.0f;
    }

    *sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
    *sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
    *sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));
}

void GXGetVtxDesc(GXAttr attr, GXAttrType* type) {
    if (!type) return;
    if (attr < PC_GX_MAX_ATTR) {
        *type = (GXAttrType)g_gx.vtx_desc[attr];
    } else {
        *type = GX_NONE;
    }
}

void GXGetVtxDescv(GXVtxDescList* out) {
    if (!out) return;
    {
        u32 n = 0;
        u32 attr;
        for (attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7 && n < (GX_MAX_VTXDESCLIST_SZ - 1); ++attr) {
            out[n].attr = (GXAttr)attr;
            GXGetVtxDesc((GXAttr)attr, &out[n].type);
            ++n;
        }
        if (n < (GX_MAX_VTXDESCLIST_SZ - 1)) {
            out[n].attr = GX_VA_NBT;
            GXGetVtxDesc(GX_VA_NBT, &out[n].type);
            ++n;
        }
        out[n].attr = GX_VA_NULL;
        out[n].type = GX_NONE;
    }
}

void GXGetVtxAttrFmt(GXVtxFmt idx, GXAttr attr, GXCompCnt* compCnt, GXCompType* compType, u8* shift) {
    const PCGXVertexFormat* fmt;
    GXCompType nrmType;

    if ((u32)idx >= GX_MAX_VTXFMT) {
        if (compCnt) *compCnt = (GXCompCnt)1;
        if (compType) *compType = GX_U8;
        if (shift) *shift = 0;
        return;
    }

    fmt = &g_gx.vtx_fmt[idx];
    switch (attr) {
        case GX_VA_POS:
            if (compCnt) *compCnt = (GXCompCnt)fmt->attr_cnt[GX_VA_POS];
            if (compType) *compType = (GXCompType)fmt->attr_type[GX_VA_POS];
            if (shift) *shift = fmt->attr_frac[GX_VA_POS];
            return;
        case GX_VA_NRM:
        case GX_VA_NBT:
            if (compCnt) *compCnt = (GXCompCnt)fmt->attr_cnt[GX_VA_NRM];
            nrmType = (GXCompType)fmt->attr_type[GX_VA_NRM];
            if (compType) *compType = nrmType;
            if (shift) {
                switch (nrmType) {
                    case GX_S8:
                        *shift = 6;
                        break;
                    case GX_S16:
                        *shift = 14;
                        break;
                    default:
                        *shift = 0;
                        break;
                }
            }
            return;
        case GX_VA_CLR0:
        case GX_VA_CLR1:
            if (compCnt) *compCnt = (GXCompCnt)fmt->attr_cnt[attr];
            if (compType) *compType = (GXCompType)fmt->attr_type[attr];
            if (shift) *shift = 0;
            return;
        case GX_VA_TEX0:
        case GX_VA_TEX1:
        case GX_VA_TEX2:
        case GX_VA_TEX3:
        case GX_VA_TEX4:
        case GX_VA_TEX5:
        case GX_VA_TEX6:
        case GX_VA_TEX7:
            if (compCnt) *compCnt = (GXCompCnt)fmt->attr_cnt[attr];
            if (compType) *compType = (GXCompType)fmt->attr_type[attr];
            if (shift) *shift = fmt->attr_frac[attr];
            return;
        default:
            /* Mirrors SDK default branch for array/meta attributes. */
            if (compCnt) *compCnt = (GXCompCnt)1;
            if (compType) *compType = GX_U8;
            if (shift) *shift = 0;
            return;
    }
}

void GXGetVtxAttrFmtv(GXVtxFmt idx, GXVtxAttrFmtList* out) {
    GXAttr attr;

    if (!out) return;
    if ((u32)idx >= GX_MAX_VTXFMT) {
        out[0].attr = GX_VA_NULL;
        out[0].cnt = (GXCompCnt)1;
        out[0].type = GX_U8;
        out[0].frac = 0;
        return;
    }

    for (attr = GX_VA_POS; attr <= GX_VA_TEX7; ++attr) {
        out->attr = attr;
        GXGetVtxAttrFmt(idx, attr, &out->cnt, &out->type, &out->frac);
        ++out;
    }
    out->attr = GX_VA_NULL;
    out->cnt = (GXCompCnt)1;
    out->type = GX_U8;
    out->frac = 0;
}

void GXGetArray(GXAttr attr, void** base_ptr, u8* stride) {
    GXAttr cp_attr = attr;
    if (base_ptr) *base_ptr = NULL;
    if (stride) *stride = 0;

    if (cp_attr == GX_VA_NBT) {
        cp_attr = GX_VA_NRM;
    }

    if ((u32)cp_attr < GX_VA_POS || (u32)cp_attr > GX_LIGHT_ARRAY) return;
    if ((u32)cp_attr >= PC_GX_MAX_ATTR) return;

    if (base_ptr) *base_ptr = (void*)g_gx.array_base[cp_attr];
    if (stride) *stride = g_gx.array_stride[cp_attr];
}

/* --- TEV Configuration --- */
void GXSetNumTevStages(u8 nStages) {
    u8 maxStages = (u8)PC_GX_MAX_TEV_STAGES;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (nStages < 1) nStages = 1;
    if (nStages > maxStages) nStages = maxStages;
    g_gx.num_tev_stages = nStages;
}

void GXSetTevOp(GXTevStageID stage, GXTevMode mode) {
    GXTevColorArg input_color = GX_CC_RASC;
    GXTevAlphaArg input_alpha = GX_CA_RASA;

    pc_gx_flush_if_begin_complete();
    if (stage >= 16) return;

    /* Per SDK docs, stage0 uses raster color/alpha; later stages use prev stage output. */
    if (stage != GX_TEVSTAGE0) {
        input_color = GX_CC_CPREV;
        input_alpha = GX_CA_APREV;
    }

    /* TEV formula: out = (d + ((1-c)*a + c*b) + bias) * scale */
    switch (mode) {
    case GX_MODULATE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, input_color, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, input_alpha, GX_CA_ZERO);
        break;
    case GX_DECAL:
        GXSetTevColorIn(stage, input_color, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, input_alpha);
        break;
    case GX_BLEND:
        GXSetTevColorIn(stage, input_color, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, input_alpha, GX_CA_ZERO);
        break;
    case GX_REPLACE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        break;
    case GX_PASSCLR:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, input_color);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, input_alpha);
        break;
    default:
        return;
    }
    GXSetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].color_a = a;
        g_gx.tev_stages[stage].color_b = b;
        g_gx.tev_stages[stage].color_c = c;
        g_gx.tev_stages[stage].color_d = d;
    }
}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].alpha_a = a;
        g_gx.tev_stages[stage].alpha_b = b;
        g_gx.tev_stages[stage].alpha_c = c;
        g_gx.tev_stages[stage].alpha_d = d;
    }
}

void GXSetTevColorOp(
    GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].color_op = op;
        g_gx.tev_stages[stage].color_bias = bias;
        g_gx.tev_stages[stage].color_scale = scale;
        g_gx.tev_stages[stage].color_clamp = clamp;
        g_gx.tev_stages[stage].color_out = out_reg;
    }
}

void GXSetTevAlphaOp(
    GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].alpha_op = op;
        g_gx.tev_stages[stage].alpha_bias = bias;
        g_gx.tev_stages[stage].alpha_scale = scale;
        g_gx.tev_stages[stage].alpha_clamp = clamp;
        g_gx.tev_stages[stage].alpha_out = out_reg;
    }
}

void GXSetTevClampMode(GXTevStageID stage, GXTevClampMode mode) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].clamp_mode = mode;
    }
}

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES | PC_GX_DIRTY_TEXTURES);
    if (stage < 16) {
        /*
         * SDK behavior (GXTev.c):
         * - map is masked with ~GX_TEX_DISABLE, then clamped into [0..GX_MAX_TEXMAP-1]
         * - coord falls back to GX_TEXCOORD0 when coord is NULL/invalid
         * - texture lookup enable is controlled only by map != GX_TEXMAP_NULL and no disable flag
         */
        u32 tmap = ((u32)map) & ~(u32)GX_TEX_DISABLE;
        u32 tcoord = (coord >= GX_MAX_TEXCOORD) ? (u32)GX_TEXCOORD0 : (u32)coord;
        if (tmap >= GX_MAX_TEXMAP) {
            tmap = (u32)GX_TEXMAP0;
        }

        g_gx.tev_stages[stage].tex_coord = (int)tcoord;
        g_gx.tev_stages[stage].tex_map = (int)tmap;
        g_gx.tev_stages[stage].color_chan = color;
        g_gx.tev_stages[stage].tex_lookup_enable =
            ((map != GX_TEXMAP_NULL) && ((((u32)map) & (u32)GX_TEX_DISABLE) == 0u)) ? 1 : 0;

        /*
         * GX demos commonly use:
         *   stage0 = REPLACE (tex0 color),
         *   stage1 = PASSCLR with COLOR_NULL while using tex1 only for Z-texture.
         * On hardware this effectively preserves previous color on stage1.
         * If we keep PASSCLR routed through RASC with COLOR_NULL, the stage resolves
         * to white and overwrites stage0, producing solid white billboards.
         */
        if (color == GX_COLOR_NULL) {
            PCGXTevStage* tev = &g_gx.tev_stages[stage];
            if (tev->color_a == GX_CC_ZERO && tev->color_b == GX_CC_ZERO &&
                tev->color_c == GX_CC_ZERO && tev->color_d == GX_CC_RASC &&
                tev->alpha_a == GX_CA_ZERO && tev->alpha_b == GX_CA_ZERO &&
                tev->alpha_c == GX_CA_ZERO && tev->alpha_d == GX_CA_RASA) {
                tev->color_d = GX_CC_CPREV;
                tev->alpha_d = GX_CA_APREV;
            }
        }
    }
}

void GXSetTevColor(GXTevRegID id, GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_COLORS);
    if (id < GX_MAX_TEVREG) {
        g_gx.tev_colors[id][0] = (float)color.r / 255.0f;
        g_gx.tev_colors[id][1] = (float)color.g / 255.0f;
        g_gx.tev_colors[id][2] = (float)color.b / 255.0f;
        g_gx.tev_colors[id][3] = (float)color.a / 255.0f;
    }
}

void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_COLORS);
    if (id < GX_MAX_TEVREG) {
        const s16 r = pc_gx_wrap_s10_to_tev11((s32)color.r);
        const s16 g = pc_gx_wrap_s10_to_tev11((s32)color.g);
        const s16 b = pc_gx_wrap_s10_to_tev11((s32)color.b);
        const s16 a = pc_gx_wrap_s10_to_tev11((s32)color.a);
        g_gx.tev_colors[id][0] = (float)r / 255.0f;
        g_gx.tev_colors[id][1] = (float)g / 255.0f;
        g_gx.tev_colors[id][2] = (float)b / 255.0f;
        g_gx.tev_colors[id][3] = (float)a / 255.0f;
    }
}

void GXSetTevKColor(GXTevKColorID id, GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_KONST);
    if (id < 4) {
        g_gx.tev_k_colors[id][0] = (float)color.r / 255.0f;
        g_gx.tev_k_colors[id][1] = (float)color.g / 255.0f;
        g_gx.tev_k_colors[id][2] = (float)color.b / 255.0f;
        g_gx.tev_k_colors[id][3] = (float)color.a / 255.0f;
    }
}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) g_gx.tev_stages[stage].k_color_sel = ((int)sel) & 0x1F;
}
void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) g_gx.tev_stages[stage].k_alpha_sel = ((int)sel) & 0x1F;
}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEV_STAGES);
    if (stage < 16) {
        g_gx.tev_stages[stage].ras_swap = ((int)ras_sel) & 0x3;
        g_gx.tev_stages[stage].tex_swap = ((int)tex_sel) & 0x3;
    }
}

void GXSetTevSwapModeTable(
    GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_SWAP_TABLES);
    if (table < 4) {
        g_gx.tev_swap_table[table].r = ((int)red) & 0x3;
        g_gx.tev_swap_table[table].g = ((int)green) & 0x3;
        g_gx.tev_swap_table[table].b = ((int)blue) & 0x3;
        g_gx.tev_swap_table[table].a = ((int)alpha) & 0x3;
    }
}

/* --- Alpha / Depth / Blend --- */
void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_ALPHA_CMP);
    g_gx.alpha_comp0 = ((int)comp0) & 0x7;
    g_gx.alpha_ref0 = ref0;
    g_gx.alpha_op = ((int)op) & 0x3;
    g_gx.alpha_comp1 = ((int)comp1) & 0x7;
    g_gx.alpha_ref1 = ref1;
}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst, GXLogicOp logic_op) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_BLEND);
    g_gx.blend_mode = type;
    g_gx.blend_src = src;
    g_gx.blend_dst = dst;
    g_gx.blend_logic_op = logic_op;
}

void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_DEPTH);
    g_gx.z_compare_enable = compare_enable;
    g_gx.z_compare_func = func;
    g_gx.z_update_enable = update_enable;
}

void GXSetColorUpdate(GXBool enable) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    g_gx.color_update_enable = enable;
}
void GXSetAlphaUpdate(GXBool enable) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    g_gx.alpha_update_enable = enable;
}
void GXSetZCompLoc(GXBool before_tex) {
    pc_gx_flush_if_begin_complete();
    /* Compare-before-texture vs compare-after-texture is not fully emulated yet. */
    g_gx.z_comp_loc_before_tex = before_tex ? 1 : 0;
}
void GXSetDither(GXBool dither) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    g_gx.dither_enable = dither ? 1 : 0;
}
void GXSetDstAlpha(GXBool enable, u8 alpha) {
    /* EFB constant destination alpha behavior is tracked but only partially emulated. */
    g_gx.dst_alpha_enable = enable ? 1 : 0;
    g_gx.dst_alpha_value = alpha;
}
void GXSetFieldMask(GXBool odd, GXBool even) {
    g_gx.field_mask_odd = odd ? 1 : 0;
    g_gx.field_mask_even = even ? 1 : 0;
}
void GXSetFieldMode(GXBool field_mode, GXBool half_aspect) {
    g_gx.field_mode = field_mode ? 1 : 0;
    g_gx.field_half_aspect = half_aspect ? 1 : 0;
}

static u32 pc_gx_pixfmt_to_pe_bits(GXPixelFmt pix_fmt) {
    static const u32 p2f[8] = { 0, 1, 2, 3, 4, 4, 4, 5 };
    if ((u32)pix_fmt < 8u) {
        return p2f[(u32)pix_fmt];
    }
    return 0u;
}

static GXPixelFmt pc_gx_bp_decode_pixel_fmt(u32 pe_fmt_bits, u32 cmode1_subfmt) {
    switch (pe_fmt_bits & 0x7u) {
    case 0: return GX_PF_RGB8_Z24;
    case 1: return GX_PF_RGBA6_Z24;
    case 2: return GX_PF_RGB565_Z16;
    case 3: return GX_PF_Z24;
    case 4:
        switch (cmode1_subfmt & 0x3u) {
        case 0: return GX_PF_Y8;
        case 1: return GX_PF_U8;
        case 2: return GX_PF_V8;
        default: return GX_PF_Y8;
        }
    case 5: return GX_PF_YUV420;
    default: return GX_PF_RGB8_Z24;
    }
}

void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) {
    u32 pe_fmt_bits;
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_COLOR_MASK);
    /* Track requested EFB format; backend currently uses a fixed GL render target format. */
    g_gx.pixel_fmt = pix_fmt;
    g_gx.z_fmt = z_fmt;
    pe_fmt_bits = pc_gx_pixfmt_to_pe_bits(pix_fmt);
    g_bp_pe_pixfmt_bits = (int)pe_fmt_bits;
    if (pe_fmt_bits == 4u) {
        g_bp_cmode1_pix_subfmt = ((int)pix_fmt - (int)GX_PF_Y8) & 0x3;
    }
}

static int pc_gx_compare_u32(GXCompare func, u32 lhs, u32 rhs) {
    switch (func) {
    case GX_NEVER: return 0;
    case GX_LESS: return lhs < rhs;
    case GX_EQUAL: return lhs == rhs;
    case GX_LEQUAL: return lhs <= rhs;
    case GX_GREATER: return lhs > rhs;
    case GX_NEQUAL: return lhs != rhs;
    case GX_GEQUAL: return lhs >= rhs;
    case GX_ALWAYS:
    default:
        return 1;
    }
}

static int pc_gx_efb_has_alpha(void) {
    return g_gx.pixel_fmt == GX_PF_RGBA6_Z24;
}

static int pc_gx_map_efb_xy(u16 x, u16 y, int* wx, int* wy_gl) {
    int px = (int)x;
    int py = (int)y;

#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_window_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_window_h / (float)PC_GC_HEIGHT;
        px = (int)((float)x * sx);
        py = (int)((float)y * sy);
    }
#endif

    if (px < 0 || py < 0 || px >= g_pc_window_w || py >= g_pc_window_h) {
        return 0;
    }

    if (wx) *wx = px;
    if (wy_gl) *wy_gl = g_pc_window_h - 1 - py;
    return 1;
}

static int pc_gx_read_rgba_pixel(int wx, int wy_gl, u8* out_rgba) {
    if (!out_rgba) return 0;
    if (wx < 0 || wy_gl < 0 || wx >= g_pc_window_w || wy_gl >= g_pc_window_h) return 0;
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(wx, wy_gl, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out_rgba);
    return 1;
}

static int pc_gx_read_depth_pixel(int wx, int wy_gl, float* out_depth) {
    if (!out_depth) return 0;
    if (wx < 0 || wy_gl < 0 || wx >= g_pc_window_w || wy_gl >= g_pc_window_h) return 0;
    glReadPixels(wx, wy_gl, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, out_depth);
    return 1;
}

static void pc_gx_write_rgba_pixel(int wx, int wy_gl, const u8* rgba, GXBool color_update, GXBool alpha_update) {
    GLboolean had_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint old_scissor[4] = {0};
    GLboolean old_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLfloat old_clear_color[4] = {0};

    if (!rgba) return;

    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear_color);

    glEnable(GL_SCISSOR_TEST);
    glScissor(wx, wy_gl, 1, 1);
    glColorMask(color_update ? GL_TRUE : GL_FALSE,
                color_update ? GL_TRUE : GL_FALSE,
                color_update ? GL_TRUE : GL_FALSE,
                alpha_update ? GL_TRUE : GL_FALSE);
    glClearColor(rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glClearColor(old_clear_color[0], old_clear_color[1], old_clear_color[2], old_clear_color[3]);
    glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
    if (had_scissor) {
        glScissor(old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

static void pc_gx_write_depth_pixel(int wx, int wy_gl, float depth) {
    GLboolean had_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint old_scissor[4] = {0};
    GLboolean old_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLboolean old_depth_mask = GL_TRUE;
    GLdouble old_clear_depth = 1.0;

    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;

    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &old_clear_depth);

    glEnable(GL_SCISSOR_TEST);
    glScissor(wx, wy_gl, 1, 1);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glClearDepth((GLdouble)depth);
    glClear(GL_DEPTH_BUFFER_BIT);

    glClearDepth(old_clear_depth);
    glDepthMask(old_depth_mask);
    glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
    if (had_scissor) {
        glScissor(old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

static void pc_gx_unpack_argb(u32 argb, u8* rgba) {
    if (!rgba) return;
    rgba[3] = (u8)((argb >> 24) & 0xFF);
    rgba[0] = (u8)((argb >> 16) & 0xFF);
    rgba[1] = (u8)((argb >> 8) & 0xFF);
    rgba[2] = (u8)(argb & 0xFF);
}

static u32 pc_gx_pack_argb(const u8* rgba) {
    if (!rgba) return 0;
    return ((u32)rgba[3] << 24) | ((u32)rgba[0] << 16) | ((u32)rgba[1] << 8) | (u32)rgba[2];
}

static u8 pc_gx_logic_byte(GXLogicOp op, u8 src, u8 dst) {
    switch (op) {
    case GX_LO_CLEAR: return 0x00;
    case GX_LO_AND: return (u8)(src & dst);
    case GX_LO_REVAND: return (u8)(src & (u8)~dst);
    case GX_LO_COPY: return src;
    case GX_LO_INVAND: return (u8)(((u8)~src) & dst);
    case GX_LO_NOOP: return dst;
    case GX_LO_XOR: return (u8)(src ^ dst);
    case GX_LO_OR: return (u8)(src | dst);
    case GX_LO_NOR: return (u8)~(src | dst);
    case GX_LO_EQUIV: return (u8)~(src ^ dst);
    case GX_LO_INV: return (u8)~dst;
    case GX_LO_REVOR: return (u8)(src | (u8)~dst);
    case GX_LO_INVCOPY: return (u8)~src;
    case GX_LO_INVOR: return (u8)(((u8)~src) | dst);
    case GX_LO_NAND: return (u8)~(src & dst);
    case GX_LO_SET: return 0xFF;
    default: return src;
    }
}

static float pc_gx_blend_factor(GXBlendFactor factor, const u8* src, const u8* dst, int chan, int is_src_factor) {
    float s = src ? (src[chan] / 255.0f) : 0.0f;
    float d = dst ? (dst[chan] / 255.0f) : 0.0f;
    float sa = src ? (src[3] / 255.0f) : 0.0f;
    float da = dst ? (dst[3] / 255.0f) : 0.0f;

    switch (factor) {
    case GX_BL_ZERO: return 0.0f;
    case GX_BL_ONE: return 1.0f;
    case GX_BL_SRCCLR:
        return is_src_factor ? s : d;
    case GX_BL_INVSRCCLR:
        return 1.0f - (is_src_factor ? s : d);
    case GX_BL_SRCALPHA: return sa;
    case GX_BL_INVSRCALPHA: return 1.0f - sa;
    case GX_BL_DSTALPHA: return da;
    case GX_BL_INVDSTALPHA: return 1.0f - da;
    default:
        return 1.0f;
    }
}

static int pc_gx_poke_alpha_pass(u8 alpha) {
    return pc_gx_compare_u32((GXCompare)g_gx.poke_alpha_func, (u32)alpha, (u32)g_gx.poke_alpha_threshold);
}

void GXPokeAlphaMode(GXCompare func, u8 threshold) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_alpha_func = func;
    g_gx.poke_alpha_threshold = threshold;
}

void GXPokeAlphaRead(GXAlphaReadMode mode) {
    pc_gx_flush_if_begin_complete();
    if (mode != GX_READ_00 && mode != GX_READ_FF && mode != GX_READ_NONE) {
        mode = GX_READ_NONE;
    }
    g_gx.poke_alpha_read_mode = mode;
}

void GXPokeAlphaUpdate(GXBool update_enable) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_alpha_update_enable = update_enable ? 1 : 0;
}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_blend_mode = type;
    g_gx.poke_blend_src = src_factor;
    g_gx.poke_blend_dst = dst_factor;
    g_gx.poke_blend_logic_op = op;
}

void GXPokeColorUpdate(GXBool update_enable) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_color_update_enable = update_enable ? 1 : 0;
}

void GXPokeDstAlpha(GXBool enable, u8 alpha) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_dst_alpha_enable = enable ? 1 : 0;
    g_gx.poke_dst_alpha_value = alpha;
}

void GXPokeDither(GXBool dither) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_dither_enable = dither ? 1 : 0;
}

void GXPokeZMode(GXBool compare_enable, GXCompare func, GXBool update_enable) {
    pc_gx_flush_if_begin_complete();
    g_gx.poke_z_compare_enable = compare_enable ? 1 : 0;
    g_gx.poke_z_compare_func = func;
    g_gx.poke_z_update_enable = update_enable ? 1 : 0;
}

void GXPeekARGB(u16 x, u16 y, u32* color) {
    int wx, wy;
    u8 rgba[4] = {0, 0, 0, 0};

    if (!color) return;
    *color = 0;

    pc_gx_commit_pending_and_flush();
    if (!pc_gx_map_efb_xy(x, y, &wx, &wy)) return;
    if (!pc_gx_read_rgba_pixel(wx, wy, rgba)) return;

    if (!pc_gx_efb_has_alpha()) {
        if (g_gx.poke_alpha_read_mode == GX_READ_00) rgba[3] = 0x00;
        else if (g_gx.poke_alpha_read_mode == GX_READ_FF) rgba[3] = 0xFF;
    }

    *color = pc_gx_pack_argb(rgba);
}

void GXPokeARGB(u16 x, u16 y, u32 color) {
    int wx, wy;
    u8 src_rgba[4], dst_rgba[4], out_rgba[4];

    pc_gx_commit_pending_and_flush();
    if (!pc_gx_map_efb_xy(x, y, &wx, &wy)) return;

    pc_gx_unpack_argb(color, src_rgba);
    if (g_gx.poke_dst_alpha_enable) {
        src_rgba[3] = (u8)g_gx.poke_dst_alpha_value;
    }

    if (!pc_gx_poke_alpha_pass(src_rgba[3])) return;
    if (!pc_gx_read_rgba_pixel(wx, wy, dst_rgba)) {
        memset(dst_rgba, 0, sizeof(dst_rgba));
    }

    memcpy(out_rgba, src_rgba, sizeof(out_rgba));
    if (g_gx.poke_blend_mode == GX_BM_BLEND) {
        for (int c = 0; c < 4; ++c) {
            float sf = pc_gx_blend_factor((GXBlendFactor)g_gx.poke_blend_src, src_rgba, dst_rgba, c, 1);
            float df = pc_gx_blend_factor((GXBlendFactor)g_gx.poke_blend_dst, src_rgba, dst_rgba, c, 0);
            float v = (src_rgba[c] / 255.0f) * sf + (dst_rgba[c] / 255.0f) * df;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            out_rgba[c] = (u8)(v * 255.0f + 0.5f);
        }
    } else if (g_gx.poke_blend_mode == GX_BM_SUBTRACT) {
        for (int c = 0; c < 4; ++c) {
            int v = (int)dst_rgba[c] - (int)src_rgba[c];
            if (v < 0) v = 0;
            out_rgba[c] = (u8)v;
        }
    } else if (g_gx.poke_blend_mode == GX_BM_LOGIC) {
        for (int c = 0; c < 4; ++c) {
            out_rgba[c] = pc_gx_logic_byte((GXLogicOp)g_gx.poke_blend_logic_op, src_rgba[c], dst_rgba[c]);
        }
    }

    if (g_gx.poke_dither_enable && g_gx.pixel_fmt == GX_PF_RGB565_Z16) {
        u16 rgb565 = pc_gx_pack_rgb565(out_rgba);
        out_rgba[0] = (u8)((((rgb565 >> 11) & 0x1F) * 255 + 15) / 31);
        out_rgba[1] = (u8)((((rgb565 >> 5) & 0x3F) * 255 + 31) / 63);
        out_rgba[2] = (u8)(((rgb565 & 0x1F) * 255 + 15) / 31);
    }

    pc_gx_write_rgba_pixel(wx, wy, out_rgba,
                           g_gx.poke_color_update_enable ? GX_TRUE : GX_FALSE,
                           g_gx.poke_alpha_update_enable ? GX_TRUE : GX_FALSE);
}

void GXPeekZ(u16 x, u16 y, u32* z) {
    int wx, wy;
    float depth = 1.0f;
    u32 z24;

    if (!z) return;
    *z = 0;

    pc_gx_commit_pending_and_flush();
    if (!pc_gx_map_efb_xy(x, y, &wx, &wy)) return;
    if (!pc_gx_read_depth_pixel(wx, wy, &depth)) return;

    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    z24 = (u32)(depth * 16777215.0f + 0.5f) & 0x00FFFFFFu;

    if (g_gx.pixel_fmt == GX_PF_RGB565_Z16) {
        *z = GXCompressZ16(z24, (GXZFmt16)g_gx.z_fmt) & 0xFFFFu;
    } else {
        *z = z24;
    }
}

void GXPokeZ(u16 x, u16 y, u32 z) {
    int wx, wy;
    float dst_depth = 1.0f;
    u32 src_z24;
    u32 dst_z24;

    pc_gx_commit_pending_and_flush();
    if (!pc_gx_map_efb_xy(x, y, &wx, &wy)) return;

    if (g_gx.pixel_fmt == GX_PF_RGB565_Z16) {
        src_z24 = GXDecompressZ16(z & 0xFFFFu, (GXZFmt16)g_gx.z_fmt) & 0x00FFFFFFu;
    } else {
        src_z24 = z & 0x00FFFFFFu;
    }

    if (g_gx.poke_z_compare_enable) {
        if (!pc_gx_read_depth_pixel(wx, wy, &dst_depth)) {
            dst_depth = 1.0f;
        }
        if (dst_depth < 0.0f) dst_depth = 0.0f;
        if (dst_depth > 1.0f) dst_depth = 1.0f;
        dst_z24 = (u32)(dst_depth * 16777215.0f + 0.5f) & 0x00FFFFFFu;
        if (!pc_gx_compare_u32((GXCompare)g_gx.poke_z_compare_func, src_z24, dst_z24)) {
            return;
        }
    }

    if (!g_gx.poke_z_update_enable) return;
    pc_gx_write_depth_pixel(wx, wy, (float)src_z24 / 16777215.0f);
}

u32 GXCompressZ16(u32 z24, GXZFmt16 zfmt) {
    u32 z16 = 0;
    u32 z24n;
    s32 exp;
    s32 shift;

    z24 &= 0x00FFFFFFu;
    z24n = ~(z24 << 8);
    exp = 0;
    while (exp < 32 && ((z24n & 0x80000000u) == 0u)) {
        z24n <<= 1;
        ++exp;
    }

    switch (zfmt) {
    case GX_ZC_LINEAR:
        z16 = (z24 >> 8) & 0xFFFFu;
        break;
    case GX_ZC_NEAR:
        if (exp > 3) exp = 3;
        shift = (exp == 3) ? 7 : (9 - exp);
        z16 = ((z24 >> shift) & 0x3FFFu) | ((u32)exp << 14);
        break;
    case GX_ZC_MID:
        if (exp > 7) exp = 7;
        shift = (exp == 7) ? 4 : (10 - exp);
        z16 = ((z24 >> shift) & 0x1FFFu) | ((u32)exp << 13);
        break;
    case GX_ZC_FAR:
        if (exp > 12) exp = 12;
        shift = (exp == 12) ? 0 : (11 - exp);
        z16 = ((z24 >> shift) & 0x0FFFu) | ((u32)exp << 12);
        break;
    default:
        z16 = (z24 >> 8) & 0xFFFFu;
        break;
    }
    return z16;
}

u32 GXDecompressZ16(u32 z16, GXZFmt16 zfmt) {
    u32 z24 = 0;
    u32 prefix;
    s32 exp;
    s32 shift;

    z16 &= 0xFFFFu;
    switch (zfmt) {
    case GX_ZC_LINEAR:
        z24 = (z16 << 8) & 0x00FFFF00u;
        break;
    case GX_ZC_NEAR:
        exp = (s32)((z16 >> 14) & 0x3);
        shift = (exp == 3) ? 7 : (9 - exp);
        prefix = (exp > 0) ? (~0u << (24 - exp)) : 0u;
        z24 = (prefix | ((z16 & 0x3FFFu) << shift)) & 0x00FFFFFFu;
        break;
    case GX_ZC_MID:
        exp = (s32)((z16 >> 13) & 0x7);
        shift = (exp == 7) ? 4 : (10 - exp);
        prefix = (exp > 0) ? (~0u << (24 - exp)) : 0u;
        z24 = (prefix | ((z16 & 0x1FFFu) << shift)) & 0x00FFFFFFu;
        break;
    case GX_ZC_FAR:
        exp = (s32)((z16 >> 12) & 0xF);
        shift = (exp == 12) ? 0 : (11 - exp);
        prefix = (exp > 0) ? (~0u << (24 - exp)) : 0u;
        z24 = (prefix | ((z16 & 0x0FFFu) << shift)) & 0x00FFFFFFu;
        break;
    default:
        z24 = (z16 << 8) & 0x00FFFF00u;
        break;
    }
    return z24;
}

void GXSetCullMode(GXCullMode mode) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_CULL);
    g_gx.cull_mode = g_pc_model_viewer_no_cull ? GX_CULL_NONE : mode;
}
void GXGetCullMode(GXCullMode* mode) {
    *mode = (GXCullMode)g_gx.cull_mode;
}

void GXSetCoPlanar(GXBool enable) {
    pc_gx_flush_if_begin_complete();
    g_gx.coplanar_enable = (enable != GX_FALSE) ? GX_TRUE : GX_FALSE;
    if (g_gx.coplanar_enable) {
        /* Approximate coplanar/decal behavior to reduce Z-fighting on overlays. */
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f, 0.0f);
    }
}

/* --- Fog --- */
void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_FOG);
    g_gx.fog_type = type;
    g_gx.fog_start = startz;
    g_gx.fog_end = endz;
    g_gx.fog_near = nearz;
    g_gx.fog_far = farz;
    g_gx.fog_color[0] = color.r / 255.0f;
    g_gx.fog_color[1] = color.g / 255.0f;
    g_gx.fog_color[2] = color.b / 255.0f;
    g_gx.fog_color[3] = color.a / 255.0f;
}

void GXSetFogColor(GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_FOG);
    g_gx.fog_color[0] = color.r / 255.0f;
    g_gx.fog_color[1] = color.g / 255.0f;
    g_gx.fog_color[2] = color.b / 255.0f;
    g_gx.fog_color[3] = color.a / 255.0f;
}

void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projmtx[4][4]) {
    (void)table; (void)width; (void)projmtx;
}
void GXSetFogRangeAdj(GXBool enable, u16 center, GXFogAdjTable* table) {
    (void)enable; (void)center; (void)table;
}

/* --- Lighting --- */
static int pc_gx_chan_index(u32 chan) {
    switch (chan) {
        case GX_COLOR0:
        case GX_ALPHA0:
        case GX_COLOR0A0:
            return 0;
        case GX_COLOR1:
        case GX_ALPHA1:
        case GX_COLOR1A1:
            return 1;
        default:
            return -1;
    }
}

void GXSetNumChans(u8 nChans) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_LIGHTING);
    /* SDK contract is 0..2; clamp in release-style PC path. */
    g_gx.num_chans = (nChans <= 2) ? nChans : 2;
}

void GXSetChanCtrl(u32 chan, GXBool enable, u32 amb_src, u32 mat_src,
                   u32 light_mask, u32 diff_fn, u32 attn_fn) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_LIGHTING);
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0) {
        int is_combined = (chan == GX_COLOR0A0 || chan == GX_COLOR1A1);
        int is_alpha = (chan == GX_ALPHA0 || chan == GX_ALPHA1);

        if (!is_alpha || is_combined) {
            g_gx.chan_ctrl_enable[idx * 2] = enable;
            g_gx.chan_ctrl_amb_src[idx * 2] = amb_src;
            g_gx.chan_ctrl_mat_src[idx * 2] = mat_src;
            g_gx.chan_ctrl_light_mask[idx * 2] = light_mask;
            /* Match GX HW register behavior: spec attenuation forces DF_NONE. */
            g_gx.chan_ctrl_diff_fn[idx * 2] = (attn_fn == GX_AF_SPEC) ? GX_DF_NONE : diff_fn;
            g_gx.chan_ctrl_attn_fn[idx * 2] = attn_fn;
        }
        if (is_alpha || is_combined) {
            g_gx.chan_ctrl_enable[idx * 2 + 1] = enable;
            g_gx.chan_ctrl_amb_src[idx * 2 + 1] = amb_src;
            g_gx.chan_ctrl_mat_src[idx * 2 + 1] = mat_src;
            g_gx.chan_ctrl_light_mask[idx * 2 + 1] = light_mask;
            /* Match GX HW register behavior: spec attenuation forces DF_NONE. */
            g_gx.chan_ctrl_diff_fn[idx * 2 + 1] = (attn_fn == GX_AF_SPEC) ? GX_DF_NONE : diff_fn;
            g_gx.chan_ctrl_attn_fn[idx * 2 + 1] = attn_fn;
        }
    }
}

void GXSetChanAmbColor(u32 chan, GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_LIGHTING);
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0 && idx < 2) {
        float r = color.r / 255.0f;
        float g = color.g / 255.0f;
        float b = color.b / 255.0f;
        float a = color.a / 255.0f;
        if (chan == GX_COLOR0 || chan == GX_COLOR1) {
            g_gx.chan_amb_color[idx][0] = r;
            g_gx.chan_amb_color[idx][1] = g;
            g_gx.chan_amb_color[idx][2] = b;
        } else if (chan == GX_ALPHA0 || chan == GX_ALPHA1) {
            g_gx.chan_amb_color[idx][3] = a;
        } else {
            g_gx.chan_amb_color[idx][0] = r;
            g_gx.chan_amb_color[idx][1] = g;
            g_gx.chan_amb_color[idx][2] = b;
            g_gx.chan_amb_color[idx][3] = a;
        }
    }
}

void GXSetChanMatColor(u32 chan, GXColor color) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_LIGHTING);
    int idx = pc_gx_chan_index(chan);
    if (idx >= 0 && idx < 2) {
        float r = color.r / 255.0f;
        float g = color.g / 255.0f;
        float b = color.b / 255.0f;
        float a = color.a / 255.0f;
        if (chan == GX_COLOR0 || chan == GX_COLOR1) {
            g_gx.chan_mat_color[idx][0] = r;
            g_gx.chan_mat_color[idx][1] = g;
            g_gx.chan_mat_color[idx][2] = b;
        } else if (chan == GX_ALPHA0 || chan == GX_ALPHA1) {
            g_gx.chan_mat_color[idx][3] = a;
        } else {
            g_gx.chan_mat_color[idx][0] = r;
            g_gx.chan_mat_color[idx][1] = g;
            g_gx.chan_mat_color[idx][2] = b;
            g_gx.chan_mat_color[idx][3] = a;
        }
    }
}

/* GXLightObj internal layout (from GXPriv.h) */
typedef struct {
    u32 padding[3];
    u32 color;
    f32 a0, a1, a2;
    f32 k0, k1, k2;
    f32 px, py, pz;
    f32 nx, ny, nz;
} PCGXLightObjInternal;

static void pc_gx_lightobj_defaults(PCGXLightObjInternal* l) {
    if (!l) return;
    l->a0 = 1.0f; l->a1 = 0.0f; l->a2 = 0.0f;
    l->k0 = 1.0f; l->k1 = 0.0f; l->k2 = 0.0f;
    l->nx = 0.0f; l->ny = 0.0f; l->nz = -1.0f;
}

static int pc_gx_lightobj_coeffs_valid(const PCGXLightObjInternal* l) {
    float spot_sum;
    float dist_sum;
    const float lim = 1.0e6f;
    if (!l) return 0;
    if (!isfinite(l->a0) || !isfinite(l->a1) || !isfinite(l->a2)) return 0;
    if (!isfinite(l->k0) || !isfinite(l->k1) || !isfinite(l->k2)) return 0;
    if (fabsf(l->a0) > lim || fabsf(l->a1) > lim || fabsf(l->a2) > lim) return 0;
    if (fabsf(l->k0) > lim || fabsf(l->k1) > lim || fabsf(l->k2) > lim) return 0;
    /* Zeroed GXLightObj is common on PC; treat zero coeff blocks as uninitialized. */
    spot_sum = fabsf(l->a0) + fabsf(l->a1) + fabsf(l->a2);
    dist_sum = fabsf(l->k0) + fabsf(l->k1) + fabsf(l->k2);
    if (spot_sum < 1.0e-8f) return 0;
    if (dist_sum < 1.0e-8f) return 0;
    return 1;
}

static void pc_gx_lightobj_sanitize(PCGXLightObjInternal* l) {
    if (!l) return;
    if (!pc_gx_lightobj_coeffs_valid(l)) {
        pc_gx_lightobj_defaults(l);
    }
    if (!isfinite(l->nx) || !isfinite(l->ny) || !isfinite(l->nz)) {
        l->nx = 0.0f; l->ny = 0.0f; l->nz = -1.0f;
    }
}

void GXInitLightSpot(void* lt, f32 cutoff, u32 spot_func) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    f32 a0, a1, a2, r, cr, d;

    if (cutoff <= 0.0f || cutoff > 90.0f)
        spot_func = GX_SP_OFF;

    r = PC_PIf * cutoff / 180.0f;
    cr = cosf(r);
    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0f * cr;
        a1 = 1000.0f;
        a2 = 0.0f;
        break;
    case GX_SP_COS:
        a0 = -cr / (1.0f - cr);
        a1 = 1.0f / (1.0f - cr);
        a2 = 0.0f;
        break;
    case GX_SP_COS2:
        a0 = 0.0f;
        a1 = -cr / (1.0f - cr);
        a2 = 1.0f / (1.0f - cr);
        break;
    case GX_SP_SHARP:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (cr * (cr - 2.0f)) / d;
        a1 = 2.0f / d;
        a2 = -1.0f / d;
        break;
    case GX_SP_RING1:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = (-4.0f * cr) / d;
        a1 = (4.0f * (1.0f + cr)) / d;
        a2 = -4.0f / d;
        break;
    case GX_SP_RING2:
        d = (1.0f - cr) * (1.0f - cr);
        a0 = 1.0f - ((2.0f * cr * cr) / d);
        a1 = (4.0f * cr) / d;
        a2 = -2.0f / d;
        break;
    case GX_SP_OFF:
    default:
        a0 = 1.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        break;
    }
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
}
void GXInitLightDistAttn(void* lt, f32 ref_dist, f32 ref_bright, u32 dist_func) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    f32 k0, k1, k2;

    if (ref_dist < 0.0f)
        dist_func = GX_DA_OFF;
    if (ref_bright <= 0.0f || ref_bright >= 1.0f)
        dist_func = GX_DA_OFF;

    switch (dist_func) {
    case GX_DA_GENTLE:
        k0 = 1.0f;
        k1 = (1.0f - ref_bright) / (ref_bright * ref_dist);
        k2 = 0.0f;
        break;
    case GX_DA_MEDIUM:
        k0 = 1.0f;
        k1 = (0.5f * (1.0f - ref_bright)) / (ref_bright * ref_dist);
        k2 = (0.5f * (1.0f - ref_bright)) / (ref_bright * ref_dist * ref_dist);
        break;
    case GX_DA_STEEP:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = (1.0f - ref_bright) / (ref_bright * ref_dist * ref_dist);
        break;
    case GX_DA_OFF:
    default:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = 0.0f;
        break;
    }
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXInitLightPos(void* lt, f32 x, f32 y, f32 z) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    pc_gx_lightobj_sanitize(l);
    l->px = x; l->py = y; l->pz = z;
}
void GXInitLightDir(void* lt, f32 nx, f32 ny, f32 nz) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    pc_gx_lightobj_sanitize(l);
    l->nx = -nx; l->ny = -ny; l->nz = -nz;
}
void GXInitSpecularDir(void* lt, f32 nx, f32 ny, f32 nz) {
    const f32 gx_large_number = -1.0e18f;
    f32 hx = -nx;
    f32 hy = -ny;
    f32 hz = -nz + 1.0f;
    f32 mag = hx * hx + hy * hy + hz * hz;
    if (mag != 0.0f) {
        mag = 1.0f / sqrtf(mag);
    }

    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->px = nx * gx_large_number;
    l->py = ny * gx_large_number;
    l->pz = nz * gx_large_number;
    l->nx = hx * mag;
    l->ny = hy * mag;
    l->nz = hz * mag;
}
void GXInitSpecularDirHA(void* lt, f32 nx, f32 ny, f32 nz, f32 hx, f32 hy, f32 hz) {
    const f32 gx_large_number = -1.0e18f;
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    pc_gx_lightobj_sanitize(l);
    l->px = nx * gx_large_number;
    l->py = ny * gx_large_number;
    l->pz = nz * gx_large_number;
    l->nx = hx;
    l->ny = hy;
    l->nz = hz;
}
void GXInitLightColor(void* lt, GXColor color) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    pc_gx_lightobj_sanitize(l);
    l->color = pc_pack_gxcolor_u32(color);
}
void GXInitLightAttn(void* lt, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXInitLightAttnA(void* lt, f32 a0, f32 a1, f32 a2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->a0 = a0; l->a1 = a1; l->a2 = a2;
}
void GXInitLightAttnK(void* lt, f32 k0, f32 k1, f32 k2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    l->k0 = k0; l->k1 = k1; l->k2 = k2;
}
void GXLoadLightObjImm(void* lt, u32 light) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_LIGHTING);
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    int slot = -1;
    for (int i = 0; i < 8; i++) {
        if (light == (1u << i)) { slot = i; break; }
    }
    if (slot < 0) return;
    pc_gx_lightobj_sanitize(l);

    g_gx.lights[slot].pos[0] = l->px;
    g_gx.lights[slot].pos[1] = l->py;
    g_gx.lights[slot].pos[2] = l->pz;
    g_gx.lights[slot].dir[0] = l->nx;
    g_gx.lights[slot].dir[1] = l->ny;
    g_gx.lights[slot].dir[2] = l->nz;
    g_gx.lights[slot].a0 = l->a0;
    g_gx.lights[slot].a1 = l->a1;
    g_gx.lights[slot].a2 = l->a2;
    g_gx.lights[slot].k0 = l->k0;
    g_gx.lights[slot].k1 = l->k1;
    g_gx.lights[slot].k2 = l->k2;
    pc_unpack_gxcolor_f(l->color, g_gx.lights[slot].color);
}
void GXLoadLightObjIndx(u32 lt_obj_indx, u32 light) {
    const u8* base;
    u8 stride;
    const PCGXLightObjInternal* l;
    if (GX_LIGHT_ARRAY >= PC_GX_MAX_ATTR) return;
    base = (const u8*)g_gx.array_base[GX_LIGHT_ARRAY];
    if (!base) return;
    stride = g_gx.array_stride[GX_LIGHT_ARRAY];
    if (stride == 0) stride = (u8)sizeof(PCGXLightObjInternal);
    l = (const PCGXLightObjInternal*)(base + ((size_t)lt_obj_indx * (size_t)stride));
    GXLoadLightObjImm((void*)l, light);
}
void GXGetLightPos(void* lt, f32* x, f32* y, f32* z) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    *x = l->px; *y = l->py; *z = l->pz;
}
void GXGetLightAttnA(void* lt, f32* a0, f32* a1, f32* a2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    if (a0) *a0 = l->a0;
    if (a1) *a1 = l->a1;
    if (a2) *a2 = l->a2;
}
void GXGetLightAttnK(void* lt, f32* k0, f32* k1, f32* k2) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    if (k0) *k0 = l->k0;
    if (k1) *k1 = l->k1;
    if (k2) *k2 = l->k2;
}
void GXGetLightDir(void* lt, f32* nx, f32* ny, f32* nz) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    if (nx) *nx = -l->nx;
    if (ny) *ny = -l->ny;
    if (nz) *nz = -l->nz;
}
void GXGetLightColor(void* lt, GXColor* color) {
    PCGXLightObjInternal* l = (PCGXLightObjInternal*)lt;
    if (color) {
        const u8* bytes = (const u8*)&l->color;
        color->r = bytes[0];
        color->g = bytes[1];
        color->b = bytes[2];
        color->a = bytes[3];
    }
}

/* --- Texture Coordinate Generation --- */
void GXSetNumTexGens(u8 n) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEXGEN | PC_GX_DIRTY_TEV_STAGES | PC_GX_DIRTY_TEXTURES);
    g_gx.num_tex_gens = (n > 8) ? 8 : n;
}
void GXSetTexCoordGen2(u32 dst, u32 func, u32 src, u32 mtx, GXBool normalize, u32 postmtx) {
    int is_regular;

    if (dst >= GX_MAX_TEXCOORD) {
        return;
    }
    if (!pc_gx_texgen_func_valid((int)func)) {
        return;
    }
    if (!pc_gx_texgen_src_valid((int)func, (int)src)) {
        return;
    }
    if (!pc_gx_texgen_mtx_valid((int)func, (int)mtx)) {
        return;
    }

    is_regular = pc_gx_texgen_is_regular((int)func);
    if (is_regular) {
        if (!pc_gx_texgen_post_mtx_valid((int)postmtx)) {
            return;
        }
    }

    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEXGEN);
    g_gx.tex_gen_type[dst] = func;
    g_gx.tex_gen_src[dst] = src;
    g_gx.tex_gen_mtx[dst] = mtx;
    g_gx.tex_gen_post_mtx[dst] = postmtx;
    g_gx.tex_gen_normalize[dst] = normalize ? 1 : 0;
}
void GXSetLineWidth(u8 width, GXTexOffset texOffsets) {
    g_gx.line_width = width;
    g_gx.line_tex_offset = (u32)texOffsets;
    glLineWidth((float)width / 6.0f);
}
void GXSetPointSize(u8 size, GXTexOffset texOffsets) {
    g_gx.point_size = size;
    g_gx.point_tex_offset = (u32)texOffsets;
    glPointSize((float)size / 6.0f);
}
void GXEnableTexOffsets(GXTexCoordID coord, GXBool line, GXBool point) {
    if (coord >= 8) return;
    g_gx.tex_offset_line_enable[coord] = line ? 1u : 0u;
    g_gx.tex_offset_point_enable[coord] = point ? 1u : 0u;
}
void GXGetLineWidth(u8* width, GXTexOffset* texOffsets) {
    if (width) *width = g_gx.line_width;
    if (texOffsets) *texOffsets = (GXTexOffset)g_gx.line_tex_offset;
}
void GXGetPointSize(u8* size, GXTexOffset* texOffsets) {
    if (size) *size = g_gx.point_size;
    if (texOffsets) *texOffsets = (GXTexOffset)g_gx.point_tex_offset;
}
void GXSetTexCoordCylWrap(GXTexCoordID coord, GXBool s_enable, GXBool t_enable) {
    if (coord >= GX_MAX_TEXCOORD) {
        return;
    }

    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEXGEN);
    g_gx.texcoord_cyl_wrap_s[coord] = s_enable ? 1u : 0u;
    g_gx.texcoord_cyl_wrap_t[coord] = t_enable ? 1u : 0u;
}
void GXSetTexCoordScaleManually(GXTexCoordID coord, GXBool enable, u16 ss, u16 ts) {
    if (coord >= GX_MAX_TEXCOORD) {
        return;
    }

    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_TEXGEN);

    g_gx.texcoord_manual_scale_enable[coord] = enable ? 1u : 0u;
    if (enable) {
        /*
         * SDK programs SU size registers with (size - 1). Clamp here to avoid
         * invalid 0-sized divisors in the shader path when callers pass 0.
         */
        g_gx.texcoord_manual_scale_s[coord] = (ss == 0u) ? 1u : ss;
        g_gx.texcoord_manual_scale_t[coord] = (ts == 0u) ? 1u : ts;
    }
}
void GXSetTexCoordBias(u32 coord, u8 s, u8 t) { (void)coord; (void)s; (void)t; }

/* --- Framebuffer / Copy --- */
static u32 pc_gx_pack_disp_copy_yscale(f32 yscale) {
    u32 reg;
    if (!(yscale > 0.0f)) return 0x1FFu;
    reg = (u32)(256.0f / yscale);
    reg &= 0x1FFu;
    if (reg == 0) reg = 1;
    return reg;
}

static u32 pc_gx_num_xfb_lines(u32 efb_height, u32 yscale_reg) {
    u32 count;
    u32 real_height;
    u32 scale_down;

    if (efb_height == 0) return 0;
    if (yscale_reg == 0) return efb_height;

    count = (efb_height - 1u) * 256u;
    real_height = count / yscale_reg + 1u;

    scale_down = yscale_reg;
    if (scale_down > 0x80u && scale_down < 0x100u) {
        while ((scale_down & 1u) == 0u) {
            scale_down >>= 1u;
        }
        if (scale_down != 0 && (efb_height % scale_down) == 0u) {
            ++real_height;
        }
    }

    if (real_height > 1024u) real_height = 1024u;
    return real_height;
}

static void pc_gx_bbox_expand_rect(int left, int top, int wd, int ht) {
    int right, bottom;
    if (wd <= 0 || ht <= 0) return;
    right = left + wd - 1;
    bottom = top + ht - 1;
    if (right < left || bottom < top) return;

    if (!g_gx.bbox_valid) {
        g_gx.bbox_valid = 1;
        g_gx.bbox_left = (u16)(left < 0 ? 0 : left);
        g_gx.bbox_top = (u16)(top < 0 ? 0 : top);
        g_gx.bbox_right = (u16)(right < 0 ? 0 : right);
        g_gx.bbox_bottom = (u16)(bottom < 0 ? 0 : bottom);
        return;
    }

    if (left < (int)g_gx.bbox_left) g_gx.bbox_left = (u16)(left < 0 ? 0 : left);
    if (top < (int)g_gx.bbox_top) g_gx.bbox_top = (u16)(top < 0 ? 0 : top);
    if (right > (int)g_gx.bbox_right) g_gx.bbox_right = (u16)(right < 0 ? 0 : right);
    if (bottom > (int)g_gx.bbox_bottom) g_gx.bbox_bottom = (u16)(bottom < 0 ? 0 : bottom);
}

static void pc_gx_bbox_touch_draw(void) {
    /* Bounding-box readback is approximated from current draw coverage region. */
    int left = g_gx.scissor[0];
    int top = g_gx.scissor[1];
    int wd = g_gx.scissor[2];
    int ht = g_gx.scissor[3];

    if (wd <= 0 || ht <= 0) {
        left = 0;
        top = 0;
        wd = g_pc_window_w;
        ht = g_pc_window_h;
    }

    pc_gx_bbox_expand_rect(left, top, wd, ht);
}

static inline void pc_gx_apply_copy_gamma(u8* r, u8* g, u8* b) {
    float gamma;
    float fr, fg, fb;
    if (!r || !g || !b) return;

    switch (g_gx.copy_gamma) {
    case GX_GM_1_7: gamma = 1.7f; break;
    case GX_GM_2_2: gamma = 2.2f; break;
    case GX_GM_1_0:
    default:
        return;
    }

    fr = powf((float)(*r) / 255.0f, 1.0f / gamma);
    fg = powf((float)(*g) / 255.0f, 1.0f / gamma);
    fb = powf((float)(*b) / 255.0f, 1.0f / gamma);
    *r = (u8)(fr < 0.0f ? 0 : fr > 1.0f ? 255 : (int)(fr * 255.0f + 0.5f));
    *g = (u8)(fg < 0.0f ? 0 : fg > 1.0f ? 255 : (int)(fg * 255.0f + 0.5f));
    *b = (u8)(fb < 0.0f ? 0 : fb > 1.0f ? 255 : (int)(fb * 255.0f + 0.5f));
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    g_gx.clear_color[0] = clear_clr.r / 255.0f;
    g_gx.clear_color[1] = clear_clr.g / 255.0f;
    g_gx.clear_color[2] = clear_clr.b / 255.0f;
    g_gx.clear_color[3] = clear_clr.a / 255.0f;
    g_gx.clear_depth = clear_z / (float)0x00FFFFFF;
}

static int pc_gx_calc_read_region(const int src[4], int* read_left, int* read_top, int* read_wd, int* read_ht) {
    int left = src[0];
    int top = src[1];
    int wd = src[2];
    int ht = src[3];

    if (wd <= 0 || ht <= 0) return 0;

#ifdef PC_ENHANCEMENTS
    {
        float sx = (float)g_pc_window_w / (float)PC_GC_WIDTH;
        float sy = (float)g_pc_window_h / (float)PC_GC_HEIGHT;
        left = (int)(left * sx);
        top = (int)(top * sy);
        wd = (int)(wd * sx);
        ht = (int)(ht * sy);
    }
#endif

    if (left < 0) { wd += left; left = 0; }
    if (top < 0) { ht += top; top = 0; }
    if (left + wd > g_pc_window_w) wd = g_pc_window_w - left;
    if (top + ht > g_pc_window_h) ht = g_pc_window_h - top;
    if (wd <= 0 || ht <= 0) return 0;

    *read_left = left;
    *read_top = top;
    *read_wd = wd;
    *read_ht = ht;
    return 1;
}

static void pc_gx_clear_copy_rect(const int src[4], GXBool clear_depth);

static int pc_gx_is_writable_range(const void* ptr, size_t size) {
    if (!ptr || size == 0) return 0;
#ifdef _WIN32
    const u8* p = (const u8*)ptr;
    size_t remaining = size;

    while (remaining > 0) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)p, &mbi, sizeof(mbi)) == 0) {
            return 0;
        }
        if (mbi.State != MEM_COMMIT) {
            return 0;
        }
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) {
            return 0;
        }

        u8* region_end = (u8*)mbi.BaseAddress + mbi.RegionSize;
        size_t chunk = (size_t)(region_end - p);
        if (chunk > remaining) chunk = remaining;
        p += chunk;
        remaining -= chunk;
    }
    return 1;
#else
    return 1;
#endif
}

static int pc_gx_resolve_ring_target(void* dest, size_t total_px, u16** out_base, size_t* out_start_px) {
    uintptr_t dest_addr = (uintptr_t)dest;
    uintptr_t fb1 = (uintptr_t)DemoFrameBuffer1;
    uintptr_t fb2 = (uintptr_t)DemoFrameBuffer2;
    size_t fb_bytes = total_px * sizeof(u16);
    int dest_is_low32 = 1;

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
    dest_is_low32 = ((dest_addr >> 32) == 0);
#endif

    if (DemoFrameBuffer1 && dest_addr >= fb1 && dest_addr < fb1 + fb_bytes) {
        *out_base = (u16*)DemoFrameBuffer1;
        *out_start_px = (dest_addr - fb1) / sizeof(u16);
        return 1;
    }
    if (DemoFrameBuffer2 && dest_addr >= fb2 && dest_addr < fb2 + fb_bytes) {
        *out_base = (u16*)DemoFrameBuffer2;
        *out_start_px = (dest_addr - fb2) / sizeof(u16);
        return 1;
    }

    if (dest_is_low32 && DemoCurrentBuffer) {
        uintptr_t cur_base = (uintptr_t)DemoCurrentBuffer;
        u32 delta32 = (u32)dest_addr - (u32)cur_base;
        uintptr_t candidate = cur_base + (uintptr_t)delta32;

        if (DemoCurrentBuffer == DemoFrameBuffer1 && candidate >= fb1 && candidate < fb1 + fb_bytes) {
            *out_base = (u16*)DemoFrameBuffer1;
            *out_start_px = (candidate - fb1) / sizeof(u16);
            return 1;
        }
        if (DemoCurrentBuffer == DemoFrameBuffer2 && candidate >= fb2 && candidate < fb2 + fb_bytes) {
            *out_base = (u16*)DemoFrameBuffer2;
            *out_start_px = (candidate - fb2) / sizeof(u16);
            return 1;
        }
    }

    /* frb-aa-full uses (u32) pointer math; recover when destination got truncated to 32-bit. */
    if (dest_is_low32) {
        u32 low = (u32)dest_addr;
        if (DemoFrameBuffer1) {
            uintptr_t base = (uintptr_t)DemoFrameBuffer1;
            uintptr_t candidate = (base & ~((uintptr_t)0xFFFFFFFFu)) | (uintptr_t)low;
            if (candidate >= base && candidate < base + fb_bytes) {
                *out_base = (u16*)DemoFrameBuffer1;
                *out_start_px = (candidate - base) / sizeof(u16);
                return 1;
            }
        }
        if (DemoFrameBuffer2) {
            uintptr_t base = (uintptr_t)DemoFrameBuffer2;
            uintptr_t candidate = (base & ~((uintptr_t)0xFFFFFFFFu)) | (uintptr_t)low;
            if (candidate >= base && candidate < base + fb_bytes) {
                *out_base = (u16*)DemoFrameBuffer2;
                *out_start_px = (candidate - base) / sizeof(u16);
                return 1;
            }
        }
    }

    return 0;
}

static void pc_gx_copy_disp_to_memory(void* dest, const u8* rgba, int read_wd, int read_ht, int out_wd, int out_ht) {
    int stride_px = g_gx.copy_dst[0] > 0 ? (int)VIPadFrameBufferWidth((u16)g_gx.copy_dst[0]) : out_wd;
    int fb_ht = g_gx.copy_dst[1] > 0 ? g_gx.copy_dst[1] : out_ht;
    int field_mode = g_gx.copy_frame2field;
    int field_offset = 0;
    int sampled_ht = read_ht;

    if (stride_px <= 0) stride_px = out_wd;
    if (fb_ht <= 0) fb_ht = out_ht;
    if (stride_px <= 0 || fb_ht <= 0) return;
    if (field_mode == GX_COPY_INTLC_ODD) field_offset = 1;
    if (field_mode != GX_COPY_PROGRESSIVE) {
        sampled_ht = (read_ht - field_offset + 1) / 2;
        if (sampled_ht <= 0) sampled_ht = 1;
    }

    size_t total_px = (size_t)stride_px * (size_t)fb_ht;
    u16* ring_base = NULL;
    size_t start_px = 0;
    int resolved_ring = pc_gx_resolve_ring_target(dest, total_px, &ring_base, &start_px);
    if (!resolved_ring) {
        size_t bytes_needed = (size_t)out_wd * (size_t)out_ht * sizeof(u16);
        if (!pc_gx_is_writable_range(dest, bytes_needed)) {
            return;
        }
    }

    /* frb-aa-full bottom pass:
     *   clamp = bottom
     *   src   = (0,2,w,efb-2)
     *   dest  = xfb + (efb-2) lines
     *
     * On GC this sequence is used to place the second pass into the bottom
     * half of the displayed XFB. Emulate that placement directly so the top
     * half from pass 1 is preserved.
     */
    if (ring_base &&
        g_gx.copy_clamp == GX_CLAMP_BOTTOM &&
        g_gx.copy_src[1] == 2 &&
        g_gx.copy_src[3] >= fb_ht - 2 &&
        start_px == (size_t)stride_px * (size_t)(fb_ht - 2)) {
        int rows = fb_ht / 2;
        int dst_row0 = fb_ht - rows;
        int src_rows = read_ht / 2;
        if (rows <= 0 || src_rows <= 0) return;

        for (int y = 0; y < rows; y++) {
            /* Sample the upper half of this readback (contains bottom-pass image). */
            int src_y = (read_ht - 1) - ((y * src_rows) / rows);
            int dst_row = dst_row0 + y;
            for (int x = 0; x < out_wd && x < stride_px; x++) {
                int src_x = (x * read_wd) / out_wd;
                const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
                u8 r = p[0];
                u8 g = p[1];
                u8 b = p[2];
                pc_gx_apply_copy_gamma(&r, &g, &b);
                u16 rgb565 = (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                ring_base[(size_t)dst_row * (size_t)stride_px + (size_t)x] = rgb565;
            }
        }
        return;
    }

    for (int y = 0; y < out_ht; y++) {
        int sample_y = (y * sampled_ht) / out_ht;
        int src_y = field_offset + ((field_mode == GX_COPY_PROGRESSIVE) ? sample_y : (sample_y * 2));
        if (src_y >= read_ht) src_y = read_ht - 1;
        src_y = read_ht - 1 - src_y;
        for (int x = 0; x < out_wd; x++) {
            int src_x = (x * read_wd) / out_wd;
            const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
            u8 r = p[0];
            u8 g = p[1];
            u8 b = p[2];
            pc_gx_apply_copy_gamma(&r, &g, &b);
            u16 rgb565 = (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));

            if (ring_base) {
                size_t dst_idx = start_px + (size_t)y * (size_t)stride_px + (size_t)x;
                ring_base[dst_idx % total_px] = rgb565;
            } else {
                ((u16*)dest)[(size_t)y * (size_t)out_wd + (size_t)x] = rgb565;
            }
        }
    }
}

static void pc_gx_clear_copy_src(void) {
    /* GXSetZMode(update_enable) controls whether copy clears affect Z. */
    pc_gx_clear_copy_rect(g_gx.copy_src, g_gx.z_update_enable ? GX_TRUE : GX_FALSE);
}

void GXCopyDisp(void* dest, GXBool clear) {
    pc_gx_commit_pending_and_flush();

    if (dest) {
        int read_left = 0, read_top = 0, read_wd = 0, read_ht = 0;
        if (pc_gx_calc_read_region(g_gx.copy_src, &read_left, &read_top, &read_wd, &read_ht)) {
            int gl_y = g_pc_window_h - (read_top + read_ht);
            if (gl_y >= 0) {
                size_t rgba_size = (size_t)read_wd * (size_t)read_ht * 4;
                u8* rgba = (u8*)malloc(rgba_size);
                if (rgba) {
                    /* XFB destination dimensions come from GXSetDispCopyDst(),
                     * not the EFB source rectangle. Using copy_src here causes
                     * partial/stale writes that look like frame ghosting. */
                    int out_wd = g_gx.copy_dst[0];
                    int out_ht = g_gx.copy_dst[1];
                    if (out_wd <= 0) out_wd = g_gx.copy_src[2];
                    if (out_ht <= 0) out_ht = g_gx.copy_src[3];
                    if (out_wd <= 0) out_wd = read_wd;
                    if (out_ht <= 0) out_ht = read_ht;

                    glReadBuffer(GL_BACK);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(read_left, gl_y, read_wd, read_ht, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                    pc_gx_copy_disp_to_memory(dest, rgba, read_wd, read_ht, out_wd, out_ht);
                    free(rgba);
                }
            }
        }
    }

    if (clear) {
        pc_gx_clear_copy_src();
    }
}

void GXSetDispCopyGamma(GXGamma gamma) {
    if (gamma < GX_GM_1_0 || gamma > GX_GM_2_2) gamma = GX_GM_1_0;
    g_gx.copy_gamma = (int)gamma;
}
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    g_gx.copy_src[0] = left; g_gx.copy_src[1] = top;
    g_gx.copy_src[2] = wd; g_gx.copy_src[3] = ht;
}
void GXSetDispCopyDst(u16 wd, u16 ht) { g_gx.copy_dst[0] = wd; g_gx.copy_dst[1] = ht; }
void GXSetDispCopyFrame2Field(GXCopyMode mode) {
    switch (mode) {
    case GX_COPY_PROGRESSIVE:
    case GX_COPY_INTLC_EVEN:
    case GX_COPY_INTLC_ODD:
        g_gx.copy_frame2field = (int)mode;
        break;
    default:
        g_gx.copy_frame2field = GX_COPY_PROGRESSIVE;
        break;
    }
}
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 target;
    f32 yScale;
    f32 bestScale;
    u32 iScale;
    u32 realHeight;

    if (efbHeight == 0 || xfbHeight == 0) return 1.0f;
    target = xfbHeight;
    yScale = (f32)target / (f32)efbHeight;
    iScale = pc_gx_pack_disp_copy_yscale(yScale);
    realHeight = pc_gx_num_xfb_lines((u32)efbHeight, iScale);

    while (realHeight > xfbHeight && target > 0) {
        --target;
        yScale = (f32)target / (f32)efbHeight;
        iScale = pc_gx_pack_disp_copy_yscale(yScale);
        realHeight = pc_gx_num_xfb_lines((u32)efbHeight, iScale);
    }

    bestScale = yScale;
    while (realHeight < xfbHeight) {
        bestScale = yScale;
        ++target;
        yScale = (f32)target / (f32)efbHeight;
        iScale = pc_gx_pack_disp_copy_yscale(yScale);
        realHeight = pc_gx_num_xfb_lines((u32)efbHeight, iScale);
    }

    return bestScale;
}
u32 GXSetDispCopyYScale(f32 vscale) {
    u32 iScale;
    u32 ht;
    if (!(vscale >= 1.0f)) vscale = 1.0f;
    iScale = pc_gx_pack_disp_copy_yscale(vscale);
    g_gx.copy_yscale = vscale;
    g_gx.copy_yscale_reg = (int)iScale;
    ht = (u32)(g_gx.copy_src[3] > 0 ? g_gx.copy_src[3] : 0);
    return pc_gx_num_xfb_lines(ht, iScale);
}
u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
    u32 iScale;
    if (efbHeight == 0) return 0;
    if (!(yScale >= 1.0f)) yScale = 1.0f;
    iScale = pc_gx_pack_disp_copy_yscale(yScale);
    return (u16)pc_gx_num_xfb_lines((u32)efbHeight, iScale);
}
void GXSetCopyFilter(GXBool aa, u8 sample_pattern[12][2], GXBool vf, u8 vfilter[7]) {
    if (g_pc_gx_dl.active) {
        u32 op = PCGX_DL_OP_COPY_FILTER;
        pc_gx_dl_write(&op, sizeof(op));
        return;
    }
    g_gx.copy_aa_enable = aa ? 1 : 0;
    g_gx.copy_vf_enable = vf ? 1 : 0;
    if (sample_pattern && aa) {
        memcpy(g_gx.copy_sample_pattern, sample_pattern, sizeof(g_gx.copy_sample_pattern));
    } else {
        for (int i = 0; i < 12; ++i) {
            g_gx.copy_sample_pattern[i][0] = 6;
            g_gx.copy_sample_pattern[i][1] = 6;
        }
    }
    if (vfilter) {
        memcpy(g_gx.copy_vfilter, vfilter, sizeof(g_gx.copy_vfilter));
    } else {
        g_gx.copy_vfilter[0] = 0;
        g_gx.copy_vfilter[1] = 0;
        g_gx.copy_vfilter[2] = 21;
        g_gx.copy_vfilter[3] = 22;
        g_gx.copy_vfilter[4] = 21;
        g_gx.copy_vfilter[5] = 0;
        g_gx.copy_vfilter[6] = 0;
    }
}
void GXAdjustForOverscan(GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver) {
    u16 hor2;
    u16 ver2;
    u32 verf;
    if (!rmin || !rmout) return;
    if (rmin != rmout) *rmout = *rmin;

    hor2 = (u16)(hor * 2);
    ver2 = (u16)(ver * 2);

    rmout->fbWidth = (u16)(rmin->fbWidth - hor2);
    verf = ((u32)ver2 * (u32)rmin->efbHeight) / (u32)(rmin->xfbHeight ? rmin->xfbHeight : 1);
    rmout->efbHeight = (u16)(rmin->efbHeight - verf);
    if (rmin->xFBmode == VI_XFBMODE_SF && (rmin->viTVmode & 2) != 2) {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver);
    } else {
        rmout->xfbHeight = (u16)(rmin->xfbHeight - ver2);
    }
    rmout->viWidth = (u16)(rmin->viWidth - hor2);
    rmout->viHeight = (u16)(rmin->viHeight - ver2);
    rmout->viXOrigin = (u16)(rmin->viXOrigin + hor);
    rmout->viYOrigin = (u16)(rmin->viYOrigin + ver);
}

static inline u8 pc_gx_luma_u8(const u8* p) {
    /* Integer luma approximation (0.299, 0.587, 0.114). */
    return (u8)((p[0] * 77 + p[1] * 150 + p[2] * 29 + 128) >> 8);
}

static inline u8 pc_gx_q3(u8 v) { return (u8)((v * 7 + 127) / 255); }
static inline u8 pc_gx_q4(u8 v) { return (u8)((v * 15 + 127) / 255); }
static inline u8 pc_gx_q5(u8 v) { return (u8)((v * 31 + 127) / 255); }
static inline u8 pc_gx_q6(u8 v) { return (u8)((v * 63 + 127) / 255); }

static inline u16 pc_gx_pack_rgb565(const u8* p) {
    return (u16)((pc_gx_q5(p[0]) << 11) | (pc_gx_q6(p[1]) << 5) | pc_gx_q5(p[2]));
}

static inline u16 pc_gx_pack_rgb5a3(const u8* p) {
    /*
     * GX RGB5A3:
     *   bit15=1: 1RRRRRGGGGGBBBBB (opaque)
     *   bit15=0: 0AAARRRRGGGGBBBB (with alpha)
     */
    if (p[3] >= 224) {
        return (u16)(0x8000 | (pc_gx_q5(p[0]) << 10) | (pc_gx_q5(p[1]) << 5) | pc_gx_q5(p[2]));
    }
    return (u16)((pc_gx_q3(p[3]) << 12) | (pc_gx_q4(p[0]) << 8) | (pc_gx_q4(p[1]) << 4) | pc_gx_q4(p[2]));
}

static inline void pc_gx_store_be16(u8* out, u16 v) {
    out[0] = (u8)((v >> 8) & 0xFF);
    out[1] = (u8)(v & 0xFF);
}

static void pc_gx_clear_copy_rect(const int src[4], GXBool clear_depth) {
    int read_left = 0, read_top = 0, read_wd = 0, read_ht = 0;
    if (!pc_gx_calc_read_region(src, &read_left, &read_top, &read_wd, &read_ht)) {
        return;
    }

    int gl_y = g_pc_window_h - (read_top + read_ht);
    if (gl_y < 0) return;

    GLboolean had_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint old_scissor[4] = {0};
    GLboolean old_depth_mask = GL_TRUE;
    GLboolean old_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};

    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);

    glEnable(GL_SCISSOR_TEST);
    glScissor(read_left, gl_y, read_wd, read_ht);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearDepth(g_gx.clear_depth);
    glClearColor(g_gx.clear_color[0], g_gx.clear_color[1], g_gx.clear_color[2], g_gx.clear_color[3]);
    if (clear_depth) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glDepthMask(old_depth_mask);
    glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
    if (had_scissor) {
        glScissor(old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

static void pc_gx_copy_tex_execute(void* dest, GXBool clear) {
    pc_gx_commit_pending_and_flush();

    if (!dest) return;

    /* Source rectangle (EFB readback) comes from GXSetTexCopySrc. */
    int src_wd = g_gx.tex_copy_src[2];
    int src_ht = g_gx.tex_copy_src[3];
    if (src_wd <= 0 || src_ht <= 0) return;

    /*
     * Destination texture dimensions come from GXSetTexCopyDst.
     * frb-copy with box filter (mipmap flag) sets dst = src/2.
     * Using src dims here overruns destination allocations.
     */
    int out_wd = g_gx.tex_copy_dst[0] > 0 ? g_gx.tex_copy_dst[0] : src_wd;
    int out_ht = g_gx.tex_copy_dst[1] > 0 ? g_gx.tex_copy_dst[1] : src_ht;
    if (out_wd <= 0 || out_ht <= 0) return;
    if (out_wd > 4096 || out_ht > 4096) return;

#ifdef PC_ENHANCEMENTS
    /* Scale readback coordinates from GC coords to window resolution */
    float sx = (float)g_pc_window_w / (float)PC_GC_WIDTH;
    float sy = (float)g_pc_window_h / (float)PC_GC_HEIGHT;
    int read_left = (int)(g_gx.tex_copy_src[0] * sx);
    int read_top  = (int)(g_gx.tex_copy_src[1] * sy);
    int read_wd   = (int)(src_wd * sx);
    int read_ht   = (int)(src_ht * sy);
#else
    int read_left = g_gx.tex_copy_src[0];
    int read_top  = g_gx.tex_copy_src[1];
    int read_wd   = src_wd;
    int read_ht   = src_ht;
#endif

    if (read_left < 0) { read_wd += read_left; read_left = 0; }
    if (read_top < 0)  { read_ht += read_top;  read_top = 0; }
    if (read_left + read_wd > g_pc_window_w) read_wd = g_pc_window_w - read_left;
    if (read_top + read_ht > g_pc_window_h)  read_ht = g_pc_window_h - read_top;
    if (read_wd <= 0 || read_ht <= 0) return;

    int gl_y = g_pc_window_h - (read_top + read_ht);
    if (gl_y < 0) return;
    u32 fmt = g_gx.tex_copy_fmt;

#ifndef PC_ENHANCEMENTS
    /*
     * Z24X8 copy path: read depth buffer and store 24-bit depth into an RGBA8-like
     * tiled layout so GX_TF_Z24X8 can be sampled as RGB bytes in the shader.
     */
    if (fmt == GX_TF_Z24X8) {
        size_t z_size = (size_t)read_wd * (size_t)read_ht * sizeof(float);
        float* zbuf = (float*)malloc(z_size);
        if (!zbuf) return;

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(read_left, gl_y, read_wd, read_ht, GL_DEPTH_COMPONENT, GL_FLOAT, zbuf);

        {
            u8* out = (u8*)dest;
            int bw = (out_wd + 3) / 4;
            int bh = (out_ht + 3) / 4;
            for (int by = 0; by < bh; by++) {
                for (int bx = 0; bx < bw; bx++) {
                    u8 ar[16][2];
                    u8 gb[16][2];
                    memset(ar, 0, sizeof(ar));
                    memset(gb, 0, sizeof(gb));

                    for (int i = 0; i < 16; i++) {
                        int x = i & 3;
                        int y = i >> 2;
                        int px = bx * 4 + x;
                        int py = by * 4 + y;
                        if (px < out_wd && py < out_ht) {
                            int src_x = px * read_wd / out_wd;
                            int src_y = read_ht - 1 - (py * read_ht / out_ht);
                            float z = zbuf[src_y * read_wd + src_x];
                            if (z < 0.0f) z = 0.0f;
                            if (z > 1.0f) z = 1.0f;
                            u32 z24 = (u32)(z * 16777215.0f + 0.5f);
                            ar[i][0] = 0xFF;
                            ar[i][1] = (u8)((z24 >> 16) & 0xFF);
                            gb[i][0] = (u8)((z24 >> 8) & 0xFF);
                            gb[i][1] = (u8)(z24 & 0xFF);
                        }
                    }
                    for (int i = 0; i < 16; i++) {
                        out[0] = ar[i][0];
                        out[1] = ar[i][1];
                        out += 2;
                    }
                    for (int i = 0; i < 16; i++) {
                        out[0] = gb[i][0];
                        out[1] = gb[i][1];
                        out += 2;
                    }
                }
            }
        }

        free(zbuf);
        if (clear) {
            pc_gx_clear_copy_rect(g_gx.tex_copy_src, GX_FALSE);
        }
        return;
    }
#endif

    size_t rgba_size = (size_t)read_wd * (size_t)read_ht * 4;
    u8* rgba = (u8*)malloc(rgba_size);
    if (!rgba) return;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(read_left, gl_y, read_wd, read_ht, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

#ifdef PC_ENHANCEMENTS
    /* Store as full-res GL texture; GXLoadTexObj will substitute it for the RGB565 buffer */
    {
        /* Flip: glReadPixels is bottom-up, textures are top-down */
        int row_bytes = read_wd * 4;
        for (int y = 0; y < read_ht / 2; y++) {
            u8* top_row = &rgba[y * row_bytes];
            u8* bot_row = &rgba[(read_ht - 1 - y) * row_bytes];
            for (int b = 0; b < row_bytes; b++) {
                u8 tmp = top_row[b];
                top_row[b] = bot_row[b];
                bot_row[b] = tmp;
            }
        }

        GLuint efb_tex;
        glGenTextures(1, &efb_tex);
        glBindTexture(GL_TEXTURE_2D, efb_tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, read_wd, read_ht, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        pc_gx_efb_capture_store((uintptr_t)dest, efb_tex);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
#else
    {
        u8* out = (u8*)dest;

        switch (fmt) {
            case GX_TF_RGB565:
            case GX_TF_RGB5A3:
            case GX_TF_IA8:
            case GX_CTF_RA8:
            case GX_CTF_RG8:
            case GX_CTF_GB8:
            case GX_TF_Z16:
            case GX_CTF_Z16L: {
                int bw = (out_wd + 3) / 4;
                int bh = (out_ht + 3) / 4;
                for (int by = 0; by < bh; by++) {
                    for (int bx = 0; bx < bw; bx++) {
                        for (int y = 0; y < 4; y++) {
                            for (int x = 0; x < 4; x++) {
                                int px = bx * 4 + x;
                                int py = by * 4 + y;
                                u8 b0 = 0, b1 = 0;

                                if (px < out_wd && py < out_ht) {
                                    int src_x = px * read_wd / out_wd;
                                    int src_y = read_ht - 1 - (py * read_ht / out_ht);
                                    const u8* p = &rgba[(src_y * read_wd + src_x) * 4];

                                    if (fmt == GX_TF_RGB565) {
                                        u16 v = pc_gx_pack_rgb565(p);
                                        b0 = (u8)((v >> 8) & 0xFF);
                                        b1 = (u8)(v & 0xFF);
                                    } else if (fmt == GX_TF_RGB5A3) {
                                        u16 v = pc_gx_pack_rgb5a3(p);
                                        b0 = (u8)((v >> 8) & 0xFF);
                                        b1 = (u8)(v & 0xFF);
                                    } else if (fmt == GX_TF_IA8) {
                                        b0 = p[3];
                                        b1 = pc_gx_luma_u8(p);
                                    } else if (fmt == GX_CTF_RA8) {
                                        b0 = p[3];
                                        b1 = p[0];
                                    } else if (fmt == GX_CTF_RG8) {
                                        b0 = p[1];
                                        b1 = p[0];
                                    } else if (fmt == GX_CTF_GB8) {
                                        b0 = p[2];
                                        b1 = p[1];
                                    } else {
                                        u16 z = (u16)(pc_gx_luma_u8(p) * 257u);
                                        b0 = (u8)((z >> 8) & 0xFF);
                                        b1 = (u8)(z & 0xFF);
                                    }
                                }
                                out[0] = b0;
                                out[1] = b1;
                                out += 2;
                            }
                        }
                    }
                }
                break;
            }
            case GX_TF_RGBA8:
            case GX_TF_Z24X8: {
                int bw = (out_wd + 3) / 4;
                int bh = (out_ht + 3) / 4;
                for (int by = 0; by < bh; by++) {
                    for (int bx = 0; bx < bw; bx++) {
                        u8 ar[16][2];
                        u8 gb[16][2];
                        memset(ar, 0, sizeof(ar));
                        memset(gb, 0, sizeof(gb));

                        for (int i = 0; i < 16; i++) {
                            int x = i & 3;
                            int y = i >> 2;
                            int px = bx * 4 + x;
                            int py = by * 4 + y;
                            if (px < out_wd && py < out_ht) {
                                int src_x = px * read_wd / out_wd;
                                int src_y = read_ht - 1 - (py * read_ht / out_ht);
                                const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
                                ar[i][0] = p[3];
                                ar[i][1] = p[0];
                                gb[i][0] = p[1];
                                gb[i][1] = p[2];
                            }
                        }
                        for (int i = 0; i < 16; i++) {
                            out[0] = ar[i][0];
                            out[1] = ar[i][1];
                            out += 2;
                        }
                        for (int i = 0; i < 16; i++) {
                            out[0] = gb[i][0];
                            out[1] = gb[i][1];
                            out += 2;
                        }
                    }
                }
                break;
            }
            case GX_TF_I8:
            case GX_CTF_A8:
            case GX_CTF_R8:
            case GX_CTF_G8:
            case GX_CTF_B8:
            case GX_TF_Z8:
            case GX_CTF_Z8M:
            case GX_CTF_Z8L: {
                int bw = (out_wd + 7) / 8;
                int bh = (out_ht + 3) / 4;
                for (int by = 0; by < bh; by++) {
                    for (int bx = 0; bx < bw; bx++) {
                        for (int y = 0; y < 4; y++) {
                            for (int x = 0; x < 8; x++) {
                                int px = bx * 8 + x;
                                int py = by * 4 + y;
                                u8 v = 0;
                                if (px < out_wd && py < out_ht) {
                                    int src_x = px * read_wd / out_wd;
                                    int src_y = read_ht - 1 - (py * read_ht / out_ht);
                                    const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
                                    if (fmt == GX_TF_A8 || fmt == GX_CTF_A8) v = p[3];
                                    else if (fmt == GX_CTF_R8) v = p[0];
                                    else if (fmt == GX_CTF_G8) v = p[1];
                                    else if (fmt == GX_CTF_B8) v = p[2];
                                    else v = pc_gx_luma_u8(p);
                                }
                                *out++ = v;
                            }
                        }
                    }
                }
                break;
            }
            case GX_TF_IA4:
            case GX_CTF_RA4: {
                int bw = (out_wd + 7) / 8;
                int bh = (out_ht + 3) / 4;
                for (int by = 0; by < bh; by++) {
                    for (int bx = 0; bx < bw; bx++) {
                        for (int y = 0; y < 4; y++) {
                            for (int x = 0; x < 8; x++) {
                                int px = bx * 8 + x;
                                int py = by * 4 + y;
                                u8 packed = 0;
                                if (px < out_wd && py < out_ht) {
                                    int src_x = px * read_wd / out_wd;
                                    int src_y = read_ht - 1 - (py * read_ht / out_ht);
                                    const u8* p = &rgba[(src_y * read_wd + src_x) * 4];
                                    u8 a4 = pc_gx_q4(p[3]);
                                    u8 i4 = (fmt == GX_CTF_RA4) ? pc_gx_q4(p[0]) : pc_gx_q4(pc_gx_luma_u8(p));
                                    packed = (u8)((a4 << 4) | i4);
                                }
                                *out++ = packed;
                            }
                        }
                    }
                }
                break;
            }
            case GX_TF_I4:
            case GX_CTF_R4:
            case GX_CTF_Z4: {
                int bw = (out_wd + 7) / 8;
                int bh = (out_ht + 7) / 8;
                for (int by = 0; by < bh; by++) {
                    for (int bx = 0; bx < bw; bx++) {
                        for (int y = 0; y < 8; y++) {
                            for (int x = 0; x < 8; x += 2) {
                                int px0 = bx * 8 + x;
                                int px1 = px0 + 1;
                                int py = by * 8 + y;
                                u8 hi = 0, lo = 0;
                                if (px0 < out_wd && py < out_ht) {
                                    int sx0 = px0 * read_wd / out_wd;
                                    int sy0 = read_ht - 1 - (py * read_ht / out_ht);
                                    const u8* p0 = &rgba[(sy0 * read_wd + sx0) * 4];
                                    hi = (fmt == GX_CTF_R4) ? pc_gx_q4(p0[0]) : pc_gx_q4(pc_gx_luma_u8(p0));
                                }
                                if (px1 < out_wd && py < out_ht) {
                                    int sx1 = px1 * read_wd / out_wd;
                                    int sy1 = read_ht - 1 - (py * read_ht / out_ht);
                                    const u8* p1 = &rgba[(sy1 * read_wd + sx1) * 4];
                                    lo = (fmt == GX_CTF_R4) ? pc_gx_q4(p1[0]) : pc_gx_q4(pc_gx_luma_u8(p1));
                                }
                                *out++ = (u8)((hi << 4) | lo);
                            }
                        }
                    }
                }
                break;
            }
            default: {
                size_t bytes = GXGetTexBufferSize((u16)out_wd, (u16)out_ht, fmt, (GXBool)(g_gx.tex_copy_mipmap ? 1 : 0), 0);
                if (bytes > 0) {
                    memset(dest, 0, bytes);
                }
                break;
            }
        }
    }
#endif

    free(rgba);
    if (clear) {
        /* GXSetZMode(update_enable) controls whether copy clears affect Z. */
        pc_gx_clear_copy_rect(g_gx.tex_copy_src, g_gx.z_update_enable ? GX_TRUE : GX_FALSE);
    }
}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    if (g_pc_gx_dl.active) {
        u32 pkt[5] = { PCGX_DL_OP_TEXCOPY_SRC, left, top, wd, ht };
        pc_gx_dl_write(pkt, sizeof(pkt));
        return;
    }
    g_gx.tex_copy_src[0] = left;
    g_gx.tex_copy_src[1] = top;
    g_gx.tex_copy_src[2] = wd;
    g_gx.tex_copy_src[3] = ht;
}
void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) {
    if (g_pc_gx_dl.active) {
        u32 pkt[5] = { PCGX_DL_OP_TEXCOPY_DST, wd, ht, fmt, mipmap ? 1u : 0u };
        pc_gx_dl_write(pkt, sizeof(pkt));
        return;
    }
    g_gx.tex_copy_dst[0] = wd;
    g_gx.tex_copy_dst[1] = ht;
    g_gx.tex_copy_fmt = fmt;
    g_gx.tex_copy_mipmap = mipmap ? 1 : 0;
}
void GXCopyTex(void* dest, GXBool clear) {
    if (g_pc_gx_dl.active) {
        u32 op = PCGX_DL_OP_COPY_TEX;
        u64 dest64 = (u64)(uintptr_t)dest;
        u32 clear_u32 = clear ? 1u : 0u;
        pc_gx_dl_write(&op, sizeof(op));
        pc_gx_dl_write(&dest64, sizeof(dest64));
        pc_gx_dl_write(&clear_u32, sizeof(clear_u32));
        return;
    }

    pc_gx_copy_tex_execute(dest, clear);
}
void GXSetCopyClamp(GXFBClamp clamp) { g_gx.copy_clamp = (int)clamp; }

void GXClearBoundingBox(void) {
    g_gx.bbox_valid = 0;
    g_gx.bbox_left = 0;
    g_gx.bbox_top = 0;
    g_gx.bbox_right = 0;
    g_gx.bbox_bottom = 0;
}

void GXReadBoundingBox(u16* left, u16* top, u16* right, u16* bottom) {
    if (!g_gx.bbox_valid) {
        if (left) *left = 0;
        if (top) *top = 0;
        if (right) *right = 0;
        if (bottom) *bottom = 0;
        return;
    }
    if (left) *left = g_gx.bbox_left;
    if (top) *top = g_gx.bbox_top;
    if (right) *right = g_gx.bbox_right;
    if (bottom) *bottom = g_gx.bbox_bottom;
}

/* --- FIFO --- */
struct __GXFifoObj {
    u8 pad[128];
    void* base;
    u32 size;
    void* rp;
    void* wp;
    u32 hi;
    u32 lo;
};

static GXFifoObj s_cpu_fifo;
static GXFifoObj s_gp_fifo;
static GXFifoObj* s_cpu_fifo_ptr = &s_cpu_fifo;
static GXFifoObj* s_gp_fifo_ptr = &s_gp_fifo;

/* --- GX Init / Management --- */
GXFifoObj* GXInit(void* base, u32 size) {
    pc_gx_init();

    memset(&s_cpu_fifo, 0, sizeof(s_cpu_fifo));
    memset(&s_gp_fifo, 0, sizeof(s_gp_fifo));

    s_cpu_fifo.base = base;
    s_cpu_fifo.size = size;
    s_cpu_fifo.rp = base;
    s_cpu_fifo.wp = base;
    s_cpu_fifo.hi = size;
    s_cpu_fifo.lo = size / 2;
    s_gp_fifo = s_cpu_fifo;

    s_cpu_fifo_ptr = &s_cpu_fifo;
    s_gp_fifo_ptr = &s_gp_fifo;
    g_draw_sync_cb = NULL;
    g_draw_done_cb = NULL;
    g_verify_cb = NULL;
    g_verify_level = GX_WARN_ALL;
    g_breakpt_cb = NULL;
    g_breakpt_addr = NULL;
    g_current_gx_thread = OSGetCurrentThread();
    g_draw_sync_token = 0;
    g_draw_done_pending = GX_FALSE;
    g_write_gather_redirect = NULL;
    g_write_gather_prev = NULL;

    /* Match documented SDK GXInit defaults for core lighting/culling state. */
    {
        const GXColor black = { 0x00, 0x00, 0x00, 0x00 };
        const GXColor white = { 0xFF, 0xFF, 0xFF, 0xFF };
        GXSetCullMode(GX_CULL_BACK);
        GXSetNumChans(0);
        GXSetChanCtrl(
            GX_COLOR0A0,
            GX_DISABLE,
            GX_SRC_REG,
            GX_SRC_VTX,
            GX_LIGHT_NULL,
            GX_DF_NONE,
            GX_AF_NONE
        );
        GXSetChanAmbColor(GX_COLOR0A0, black);
        GXSetChanMatColor(GX_COLOR0A0, white);
        GXSetChanCtrl(
            GX_COLOR1A1,
            GX_DISABLE,
            GX_SRC_REG,
            GX_SRC_VTX,
            GX_LIGHT_NULL,
            GX_DF_NONE,
            GX_AF_NONE
        );
        GXSetChanAmbColor(GX_COLOR1A1, black);
        GXSetChanMatColor(GX_COLOR1A1, white);
    }

    return s_cpu_fifo_ptr;
}

void GXSetMisc(u32 token, u32 val) { (void)token; (void)val; }
void GXFlush(void) { pc_gx_commit_pending_and_flush(); glFlush(); }
void GXResetWriteGatherPipe(void) {
    /* PC backend has no hardware write-gather FIFO state to rewind.
     * Keep this as a synchronization point for callers that depend on it. */
    pc_gx_commit_pending_and_flush();
    glFlush();
}
void GXAbortFrame(void) {
    g_gx.in_begin = 0;
    g_gx.vertex_pending = 0;
    g_gx.current_vertex_idx = 0;
    g_draw_done_pending = GX_FALSE;
}
void GXSetDrawSync(u16 token) {
    g_draw_sync_token = token;
    pc_gx_commit_pending_and_flush();
    glFlush();
    if (g_draw_sync_cb) {
        g_draw_sync_cb(token);
    }
}
u16 GXReadDrawSync(void) { return g_draw_sync_token; }
void GXSetDrawDone(void) {
    pc_gx_commit_pending_and_flush();
    glFlush();
    g_draw_done_pending = GX_TRUE;
}
void GXWaitDrawDone(void) {
    if (!g_draw_done_pending) {
        return;
    }
    pc_gx_commit_pending_and_flush();
    glFinish();
    g_draw_done_pending = GX_FALSE;
    if (g_draw_done_cb) {
        g_draw_done_cb();
    }
}
void GXDrawDone(void) {
    GXSetDrawDone();
    GXWaitDrawDone();
}

void pc_gx_prepare_for_present(void) {
    if (!g_pc_gx_initialized) return;
    glUseProgram(0);
    g_gx.current_shader = 0;
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GXPixModeSync(void) {
    pc_gx_commit_pending_and_flush();
    glFinish();
}
void GXTexModeSync(void) {
    pc_gx_commit_pending_and_flush();
    glFinish();
}

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    GXDrawSyncCallback prev = g_draw_sync_cb;
    g_draw_sync_cb = cb;
    return prev;
}
GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb) {
    GXDrawDoneCallback prev = g_draw_done_cb;
    g_draw_done_cb = cb;
    return prev;
}

static u32 pc_gx_fifo_count(const GXFifoObj* fifo) {
    intptr_t delta;
    if (!fifo || !fifo->base || fifo->size == 0 || !fifo->rp || !fifo->wp) return 0;
    delta = (const u8*)fifo->wp - (const u8*)fifo->rp;
    if (delta < 0) delta += (intptr_t)fifo->size;
    if ((u32)delta > fifo->size && fifo->size != 0) {
        delta %= (intptr_t)fifo->size;
    }
    return (u32)delta;
}

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    u32 hi;
    u32 lo;
    if (!fifo) return;
    fifo->base = base;
    fifo->size = size;
    fifo->rp = base;
    fifo->wp = base;

    hi = (size > 0x4000u) ? (size - 0x4000u) : size;
    hi &= ~0x1Fu;
    lo = (size >> 1) & ~0x1Fu;
    if (lo >= hi && hi >= 32u) lo = hi - 32u;
    fifo->hi = hi;
    fifo->lo = lo;
}
void GXInitFifoPtrs(GXFifoObj* fifo, void* rp, void* wp) {
    if (!fifo) return;
    if (!rp) rp = fifo->base;
    if (!wp) wp = fifo->base;
    fifo->rp = rp;
    fifo->wp = wp;
}
void GXInitFifoLimits(GXFifoObj* fifo, u32 hi, u32 lo) {
    if (!fifo) return;
    fifo->hi = hi;
    fifo->lo = lo;
}
void GXSetCPUFifo(GXFifoObj* fifo) {
    if (fifo) s_cpu_fifo_ptr = fifo;
}
void GXSetGPFifo(GXFifoObj* fifo) {
    if (fifo) s_gp_fifo_ptr = fifo;
}
void GXSaveCPUFifo(GXFifoObj* fifo) {
    if (fifo && s_cpu_fifo_ptr) {
        GXFlush();
        *fifo = *s_cpu_fifo_ptr;
    }
}
void GXSaveGPFifo(GXFifoObj* fifo) {
    if (fifo && s_gp_fifo_ptr) {
        GXFlush();
        *fifo = *s_gp_fifo_ptr;
    }
}
void GXGetGPStatus(GXBool* a, GXBool* b, GXBool* c, GXBool* d, GXBool* e) {
    GXFifoObj* fifo = s_gp_fifo_ptr;
    u32 count = pc_gx_fifo_count(fifo);
    if (a) *a = (fifo && count > fifo->hi) ? GX_TRUE : GX_FALSE;
    if (b) *b = (fifo && count < fifo->lo) ? GX_TRUE : GX_FALSE;
    if (c) *c = (g_gx.in_begin || g_gx.vertex_pending) ? GX_FALSE : GX_TRUE;
    if (d) *d = (g_gx.in_begin || g_gx.vertex_pending) ? GX_FALSE : GX_TRUE;
    if (e) *e = (fifo && g_breakpt_addr && fifo->wp == g_breakpt_addr) ? GX_TRUE : GX_FALSE;
}
void GXGetFifoStatus(GXFifoObj* f, GXBool* a, GXBool* b, u32* c, GXBool* d, GXBool* e, GXBool* g) {
    GXFifoObj* fifo = f ? f : s_cpu_fifo_ptr;
    u32 count = pc_gx_fifo_count(fifo);
    if (a) *a = (fifo && count > fifo->hi) ? GX_TRUE : GX_FALSE;
    if (b) *b = (fifo && count < fifo->lo) ? GX_TRUE : GX_FALSE;
    if (c) *c = count;
    if (d) *d = (fifo && fifo == s_cpu_fifo_ptr) ? GX_TRUE : GX_FALSE;
    if (e) *e = (fifo && fifo == s_gp_fifo_ptr) ? GX_TRUE : GX_FALSE;
    if (g) {
        if (fifo && fifo == s_cpu_fifo_ptr && fifo->rp && fifo->wp) {
            *g = ((uintptr_t)fifo->wp < (uintptr_t)fifo->rp) ? GX_TRUE : GX_FALSE;
        } else {
            *g = GX_FALSE;
        }
    }
}
void GXGetFifoPtrs(GXFifoObj* f, void** rp, void** wp) {
    GXFifoObj* fifo = f ? f : s_cpu_fifo_ptr;
    if (rp) *rp = fifo ? fifo->rp : NULL;
    if (wp) *wp = fifo ? fifo->wp : NULL;
}
void* GXGetFifoBase(GXFifoObj* f) {
    GXFifoObj* fifo = f ? f : s_cpu_fifo_ptr;
    return fifo ? fifo->base : NULL;
}
u32 GXGetFifoSize(GXFifoObj* f) {
    GXFifoObj* fifo = f ? f : s_cpu_fifo_ptr;
    return fifo ? fifo->size : 0;
}
void GXGetFifoLimits(GXFifoObj* f, u32* hi, u32* lo) {
    GXFifoObj* fifo = f ? f : s_cpu_fifo_ptr;
    if (hi) *hi = fifo ? fifo->hi : 0;
    if (lo) *lo = fifo ? fifo->lo : 0;
}
GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) {
    GXBreakPtCallback prev = g_breakpt_cb;
    g_breakpt_cb = cb;
    return prev;
}
void GXEnableBreakPt(void* bp) {
    g_breakpt_addr = bp;
    if (g_breakpt_cb && s_gp_fifo_ptr && bp && s_gp_fifo_ptr->wp == bp) {
        g_breakpt_cb();
    }
}
void GXDisableBreakPt(void) { g_breakpt_addr = NULL; }
OSThread* GXSetCurrentGXThread(void) {
    OSThread* prev = g_current_gx_thread;
    g_current_gx_thread = OSGetCurrentThread();
    return prev;
}
OSThread* GXGetCurrentGXThread(void) { return g_current_gx_thread; }
GXFifoObj* GXGetCPUFifo(void) { return s_cpu_fifo_ptr; }
GXFifoObj* GXGetGPFifo(void) { return s_gp_fifo_ptr; }
GXFifoObj* GXGetGPFIfo(void) { return GXGetGPFifo(); }
u32 GXGetOverflowCount(void) { return 0; }
u32 GXResetOverflowCount(void) { return 0; }
volatile void* GXRedirectWriteGatherPipe(void* ptr) {
    if (!ptr) return NULL;
    pc_gx_commit_pending_and_flush();
    glFlush();
    g_write_gather_prev = g_write_gather_redirect;
    g_write_gather_redirect = (volatile void*)ptr;
    return (volatile void*)ptr;
}
void GXRestoreWriteGatherPipe(void) {
    pc_gx_commit_pending_and_flush();
    glFlush();
    g_write_gather_redirect = g_write_gather_prev;
    g_write_gather_prev = NULL;
}
BOOL IsWriteGatherBufferEmpty(void) { return TRUE; }

/* --- Display List --- */
void GXBeginDisplayList(void* list, u32 size) {
    pc_gx_commit_pending_and_flush();
    /* SDK contract: no nested display lists; buffer must be 32-byte aligned
     * and sized in 32-byte units. In non-assert builds, fail gracefully. */
    if (g_pc_gx_dl.active) return;
    if (!list || size == 0 || (size & 31u) != 0u || (((uintptr_t)list) & 31u) != 0u) return;
    g_pc_gx_dl.active = 1;
    g_pc_gx_dl.buf = (u8*)list;
    g_pc_gx_dl.size = size;
    g_pc_gx_dl.off = 0;
    g_pc_gx_dl.overflow = 0;
}
u32 GXEndDisplayList(void) {
    u32 nbytes = 0;
    if (!g_pc_gx_dl.active) return 0;
    /* SDK docs specify GXEndDisplayList performs a flush. Keep list active
     * while flushing so any pending primitive is recorded to the list first. */
    GXFlush();
    if (g_pc_gx_dl.active && !g_pc_gx_dl.overflow) {
        nbytes = g_pc_gx_dl.off;
        if (g_pc_gx_dl.buf && g_pc_gx_dl.size > 0) {
            while ((nbytes & 31u) != 0u && nbytes < g_pc_gx_dl.size) {
                g_pc_gx_dl.buf[nbytes++] = GX_NOP;
            }
        }
    }
    g_pc_gx_dl.active = 0;
    g_pc_gx_dl.buf = NULL;
    g_pc_gx_dl.size = 0;
    g_pc_gx_dl.off = 0;
    g_pc_gx_dl.overflow = 0;
    return nbytes;
}

typedef struct {
    int vtx_desc[PC_GX_MAX_ATTR];
    PCGXVertexFormat vtx_fmt[PC_GX_MAX_VTXFMT];
    const void* array_base[PC_GX_MAX_ATTR];
    u8 array_stride[PC_GX_MAX_ATTR];
    u32 matindex_a;
    u32 matindex_b;
} PCGXDLDecodeState;

static u16 pc_gx_dl_read_be16(const u8* p) {
    return pc_gx_be16(p);
}

static u32 pc_gx_dl_read_be32(const u8* p) {
    return pc_gx_be32(p);
}

static void pc_gx_dl_state_init(PCGXDLDecodeState* st) {
    if (!st) return;
    memcpy(st->vtx_desc, g_gx.vtx_desc, sizeof(st->vtx_desc));
    memcpy(st->vtx_fmt, g_gx.vtx_fmt, sizeof(st->vtx_fmt));
    memcpy((void*)st->array_base, g_gx.array_base, sizeof(st->array_base));
    memcpy(st->array_stride, g_gx.array_stride, sizeof(st->array_stride));
    st->matindex_a =
        ((u32)g_gx.current_mtx * 3u) |
        (((u32)g_gx.current_tex_mtx_idx[0] * 3u) << 6) |
        (((u32)g_gx.current_tex_mtx_idx[1] * 3u) << 12) |
        (((u32)g_gx.current_tex_mtx_idx[2] * 3u) << 18) |
        (((u32)g_gx.current_tex_mtx_idx[3] * 3u) << 24);
    st->matindex_b =
        (((u32)g_gx.current_tex_mtx_idx[4] * 3u) << 0) |
        (((u32)g_gx.current_tex_mtx_idx[5] * 3u) << 6) |
        (((u32)g_gx.current_tex_mtx_idx[6] * 3u) << 12) |
        (((u32)g_gx.current_tex_mtx_idx[7] * 3u) << 18);
}

static int pc_gx_dl_read_comp_f32(const u8* ptr, u32 remaining, u8 type, u8 frac, float* out_value, u32* out_consumed) {
    float scale = 1.0f;
    if (!ptr || !out_value || !out_consumed) return 0;

    switch (type) {
        case GX_U8:
            if (remaining < 1) return 0;
            if (frac) scale = 1.0f / (float)(1u << frac);
            *out_value = (float)ptr[0] * scale;
            *out_consumed = 1;
            return 1;
        case GX_S8:
            if (remaining < 1) return 0;
            if (frac) scale = 1.0f / (float)(1u << frac);
            *out_value = (float)(s8)ptr[0] * scale;
            *out_consumed = 1;
            return 1;
        case GX_U16:
            if (remaining < 2) return 0;
            if (frac) scale = 1.0f / (float)(1u << frac);
            *out_value = (float)pc_gx_dl_read_be16(ptr) * scale;
            *out_consumed = 2;
            return 1;
        case GX_S16:
            if (remaining < 2) return 0;
            if (frac) scale = 1.0f / (float)(1u << frac);
            *out_value = (float)(s16)pc_gx_dl_read_be16(ptr) * scale;
            *out_consumed = 2;
            return 1;
        case GX_F32: {
            union {
                u32 u;
                float f;
            } cvt;
            if (remaining < 4) return 0;
            cvt.u = pc_gx_dl_read_be32(ptr);
            *out_value = cvt.f;
            *out_consumed = 4;
            return 1;
        }
        default:
            break;
    }
    return 0;
}

static int pc_gx_dl_read_color_direct(const u8* ptr, u32 remaining, u8 cnt, u8 type, u8 out_rgba[4], u32* out_consumed) {
    if (!ptr || !out_rgba || !out_consumed) return 0;

    out_rgba[0] = 255;
    out_rgba[1] = 255;
    out_rgba[2] = 255;
    out_rgba[3] = 255;

    switch (type) {
        case GX_RGB565: {
            u16 c;
            if (remaining < 2) return 0;
            c = pc_gx_dl_read_be16(ptr);
            out_rgba[0] = (u8)(((c >> 11) & 0x1F) * 255 / 31);
            out_rgba[1] = (u8)(((c >> 5) & 0x3F) * 255 / 63);
            out_rgba[2] = (u8)((c & 0x1F) * 255 / 31);
            out_rgba[3] = 255;
            *out_consumed = 2;
            return 1;
        }
        case GX_RGB8: {
            if (remaining < 3) return 0;
            out_rgba[0] = ptr[0];
            out_rgba[1] = ptr[1];
            out_rgba[2] = ptr[2];
            out_rgba[3] = 255;
            *out_consumed = 3;
            return 1;
        }
        case GX_RGBX8: {
            if (remaining < 4) return 0;
            out_rgba[0] = ptr[0];
            out_rgba[1] = ptr[1];
            out_rgba[2] = ptr[2];
            out_rgba[3] = 255;
            *out_consumed = 4;
            return 1;
        }
        case GX_RGBA4: {
            u16 c;
            if (remaining < 2) return 0;
            c = pc_gx_dl_read_be16(ptr);
            out_rgba[0] = (u8)((((c >> 12) & 0x0F) * 255) / 15);
            out_rgba[1] = (u8)((((c >> 8) & 0x0F) * 255) / 15);
            out_rgba[2] = (u8)((((c >> 4) & 0x0F) * 255) / 15);
            out_rgba[3] = (u8)(((c & 0x0F) * 255) / 15);
            *out_consumed = 2;
            return 1;
        }
        case GX_RGBA6: {
            u32 c;
            if (remaining < 3) return 0;
            c = ((u32)ptr[0] << 16) | ((u32)ptr[1] << 8) | (u32)ptr[2];
            out_rgba[0] = (u8)((((c >> 18) & 0x3F) * 255) / 63);
            out_rgba[1] = (u8)((((c >> 12) & 0x3F) * 255) / 63);
            out_rgba[2] = (u8)((((c >> 6) & 0x3F) * 255) / 63);
            out_rgba[3] = (u8)(((c & 0x3F) * 255) / 63);
            *out_consumed = 3;
            return 1;
        }
        case GX_RGBA8:
        default: {
            if (remaining < 4) return 0;
            out_rgba[0] = ptr[0];
            out_rgba[1] = ptr[1];
            out_rgba[2] = ptr[2];
            out_rgba[3] = ptr[3];
            *out_consumed = 4;
            if (cnt == GX_CLR_RGB) {
                out_rgba[3] = 255;
            }
            return 1;
        }
    }
}

static u8 pc_gx_dl_get_default_matrix_index(const PCGXDLDecodeState* st, u32 attr) {
    if (!st) return 0;

    if (attr == GX_VA_PNMTXIDX) {
        return (u8)(st->matindex_a & 0x3Fu);
    }

    if (attr >= GX_VA_TEX0MTXIDX && attr <= GX_VA_TEX3MTXIDX) {
        u32 tex = attr - GX_VA_TEX0MTXIDX;
        u32 shift = 6u * (tex + 1u);
        return (u8)((st->matindex_a >> shift) & 0x3Fu);
    }

    if (attr >= GX_VA_TEX4MTXIDX && attr <= GX_VA_TEX7MTXIDX) {
        u32 tex = attr - GX_VA_TEX4MTXIDX;
        u32 shift = 6u * tex;
        return (u8)((st->matindex_b >> shift) & 0x3Fu);
    }

    return 0;
}

static int pc_gx_dl_emit_indexed_attr(PCGXDLDecodeState* st, u8 vtxfmt, u32 attr, u32 index) {
    const PCGXVertexFormat* fmt;
    const u8* base;
    const u8* src;
    u8 cnt;
    u8 type;
    u8 frac;

    if (!st || vtxfmt >= GX_MAX_VTXFMT) return 0;
    if (attr >= PC_GX_MAX_ATTR) return 0;

    if (attr <= GX_VA_TEX7MTXIDX) {
        pc_gx_set_matrix_index_attr(attr, (u8)(index & 0x3Fu));
        return 1;
    }

    fmt = &st->vtx_fmt[vtxfmt];
    base = (const u8*)st->array_base[attr];
    if (!base) return 1;
    src = base + index * st->array_stride[attr];

    switch (attr) {
        case GX_VA_POS: {
            float x;
            float y;
            float z;
            cnt = fmt->attr_cnt[GX_VA_POS];
            type = fmt->attr_type[GX_VA_POS];
            frac = fmt->attr_frac[GX_VA_POS];
            if (cnt != GX_POS_XY && cnt != GX_POS_XYZ) cnt = GX_POS_XYZ;
            pc_gx_read_position_indexed(src, cnt, type, frac, &x, &y, &z);
            GXPosition3f32(x, y, z);
            return 1;
        }
        case GX_VA_NRM: {
            float x;
            float y;
            float z;
            type = fmt->attr_type[GX_VA_NRM];
            frac = fmt->attr_frac[GX_VA_NRM];
            pc_gx_read_normal_indexed(src, type, frac, &x, &y, &z);
            GXNormal3f32(x, y, z);
            return 1;
        }
        case GX_VA_CLR0:
        case GX_VA_CLR1: {
            u8 rgba[4];
            cnt = fmt->attr_cnt[attr];
            type = fmt->attr_type[attr];
            if (cnt != GX_CLR_RGB && cnt != GX_CLR_RGBA) cnt = GX_CLR_RGBA;
            pc_gx_read_color_indexed(src, cnt, type, rgba);
            GXColor4u8(rgba[0], rgba[1], rgba[2], rgba[3]);
            return 1;
        }
        case GX_VA_TEX0:
        case GX_VA_TEX1:
        case GX_VA_TEX2:
        case GX_VA_TEX3:
        case GX_VA_TEX4:
        case GX_VA_TEX5:
        case GX_VA_TEX6:
        case GX_VA_TEX7: {
            float s;
            float t;
            cnt = fmt->attr_cnt[attr];
            type = fmt->attr_type[attr];
            frac = fmt->attr_frac[attr];
            if (cnt != GX_TEX_S && cnt != GX_TEX_ST) cnt = GX_TEX_ST;
            pc_gx_read_texcoord_indexed(src, cnt, type, frac, &s, &t);
            pc_gx_set_texcoord_channel(attr - GX_VA_TEX0, s, t);
            return 1;
        }
        default:
            return 1;
    }
}

static int pc_gx_dl_emit_direct_attr(PCGXDLDecodeState* st, u8 vtxfmt, u32 attr, const u8* ptr, u32 remaining, u32* out_consumed) {
    const PCGXVertexFormat* fmt;
    u32 used = 0;
    u32 consumed = 0;
    float comps[9];
    u8 rgba[4];
    int comp_count = 0;
    int i;
    u8 cnt;
    u8 type;
    u8 frac;

    if (!st || !ptr || !out_consumed || vtxfmt >= GX_MAX_VTXFMT) return 0;
    if (attr >= PC_GX_MAX_ATTR) return 0;
    *out_consumed = 0;
    fmt = &st->vtx_fmt[vtxfmt];

    if (attr <= GX_VA_TEX7MTXIDX) {
        if (remaining < 1) return 0;
        pc_gx_set_matrix_index_attr(attr, (u8)(ptr[0] & 0x3Fu));
        *out_consumed = 1;
        return 1;
    }

    switch (attr) {
        case GX_VA_POS:
            cnt = fmt->attr_cnt[GX_VA_POS];
            type = fmt->attr_type[GX_VA_POS];
            frac = fmt->attr_frac[GX_VA_POS];
            comp_count = (cnt == GX_POS_XY) ? 2 : 3;
            for (i = 0; i < comp_count; ++i) {
                if (!pc_gx_dl_read_comp_f32(ptr + used, remaining - used, type, frac, &comps[i], &consumed)) return 0;
                used += consumed;
            }
            if (comp_count == 2) comps[2] = 0.0f;
            GXPosition3f32(comps[0], comps[1], comps[2]);
            *out_consumed = used;
            return 1;
        case GX_VA_NRM:
            cnt = fmt->attr_cnt[GX_VA_NRM];
            type = fmt->attr_type[GX_VA_NRM];
            frac = fmt->attr_frac[GX_VA_NRM];
            comp_count = (cnt == GX_NRM_XYZ) ? 3 : 9;
            for (i = 0; i < comp_count; ++i) {
                float tmp;
                if (!pc_gx_dl_read_comp_f32(ptr + used, remaining - used, type, frac, &tmp, &consumed)) return 0;
                comps[i] = tmp;
                used += consumed;
            }
            if (comp_count == 9) {
                GXNormal3f32(comps[0], comps[1], comps[2]);
                GXNormal3f32(comps[3], comps[4], comps[5]);
                GXNormal3f32(comps[6], comps[7], comps[8]);
            } else {
                GXNormal3f32(comps[0], comps[1], comps[2]);
            }
            *out_consumed = used;
            return 1;
        case GX_VA_CLR0:
        case GX_VA_CLR1:
            cnt = fmt->attr_cnt[attr];
            type = fmt->attr_type[attr];
            if (!pc_gx_dl_read_color_direct(ptr, remaining, cnt, type, rgba, &consumed)) return 0;
            GXColor4u8(rgba[0], rgba[1], rgba[2], rgba[3]);
            *out_consumed = consumed;
            return 1;
        case GX_VA_TEX0:
        case GX_VA_TEX1:
        case GX_VA_TEX2:
        case GX_VA_TEX3:
        case GX_VA_TEX4:
        case GX_VA_TEX5:
        case GX_VA_TEX6:
        case GX_VA_TEX7:
            cnt = fmt->attr_cnt[attr];
            type = fmt->attr_type[attr];
            frac = fmt->attr_frac[attr];
            comp_count = (cnt == GX_TEX_S) ? 1 : 2;
            if (!pc_gx_dl_read_comp_f32(ptr + used, remaining - used, type, frac, &comps[0], &consumed)) return 0;
            used += consumed;
            if (comp_count == 2) {
                if (!pc_gx_dl_read_comp_f32(ptr + used, remaining - used, type, frac, &comps[1], &consumed)) return 0;
                used += consumed;
            } else {
                comps[1] = 0.0f;
            }
            pc_gx_set_texcoord_channel(attr - GX_VA_TEX0, comps[0], comps[1]);
            *out_consumed = used;
            return 1;
        default:
            return 0;
    }
}

static void pc_gx_dl_apply_vcd_lo(PCGXDLDecodeState* st, u32 data) {
    if (!st) return;
    st->vtx_desc[GX_VA_PNMTXIDX] = (data & 0x1u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX0MTXIDX] = (data & 0x2u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX1MTXIDX] = (data & 0x4u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX2MTXIDX] = (data & 0x8u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX3MTXIDX] = (data & 0x10u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX4MTXIDX] = (data & 0x20u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX5MTXIDX] = (data & 0x40u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX6MTXIDX] = (data & 0x80u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_TEX7MTXIDX] = (data & 0x100u) ? GX_DIRECT : GX_NONE;
    st->vtx_desc[GX_VA_POS] = (int)((data >> 9) & 0x3u);
    st->vtx_desc[GX_VA_NRM] = (int)((data >> 11) & 0x3u);
    st->vtx_desc[GX_VA_CLR0] = (int)((data >> 13) & 0x3u);
    st->vtx_desc[GX_VA_CLR1] = (int)((data >> 15) & 0x3u);
}

static void pc_gx_dl_apply_vcd_hi(PCGXDLDecodeState* st, u32 data) {
    int i;
    if (!st) return;
    for (i = 0; i < 8; ++i) {
        st->vtx_desc[GX_VA_TEX0 + i] = (int)((data >> (i * 2)) & 0x3u);
    }
}

static void pc_gx_dl_apply_vat(PCGXDLDecodeState* st, u8 reg_addr, u8 vat_idx, u32 data) {
    PCGXVertexFormat* fmt;
    u8 nrm_cnt;
    u8 nrm_idx3;
    if (!st || vat_idx >= GX_MAX_VTXFMT) return;
    fmt = &st->vtx_fmt[vat_idx];

    switch (reg_addr) {
        case 0x7:
            fmt->attr_cnt[GX_VA_POS] = (u8)(data & 0x1u);
            fmt->attr_type[GX_VA_POS] = (u8)((data >> 1) & 0x7u);
            fmt->attr_frac[GX_VA_POS] = (u8)((data >> 4) & 0x1Fu);

            nrm_cnt = (u8)((data >> 9) & 0x1u);
            nrm_idx3 = (u8)((data >> 31) & 0x1u);
            fmt->attr_cnt[GX_VA_NRM] = nrm_cnt ? (nrm_idx3 ? GX_NRM_NBT3 : GX_NRM_NBT) : GX_NRM_XYZ;
            fmt->attr_type[GX_VA_NRM] = (u8)((data >> 10) & 0x7u);
            fmt->attr_frac[GX_VA_NRM] = 0;

            fmt->attr_cnt[GX_VA_CLR0] = (u8)((data >> 13) & 0x1u);
            fmt->attr_type[GX_VA_CLR0] = (u8)((data >> 14) & 0x7u);
            fmt->attr_frac[GX_VA_CLR0] = 0;

            fmt->attr_cnt[GX_VA_CLR1] = (u8)((data >> 17) & 0x1u);
            fmt->attr_type[GX_VA_CLR1] = (u8)((data >> 18) & 0x7u);
            fmt->attr_frac[GX_VA_CLR1] = 0;

            fmt->attr_cnt[GX_VA_TEX0] = (u8)((data >> 21) & 0x1u);
            fmt->attr_type[GX_VA_TEX0] = (u8)((data >> 22) & 0x7u);
            fmt->attr_frac[GX_VA_TEX0] = (u8)((data >> 25) & 0x1Fu);
            break;
        case 0x8:
            fmt->attr_cnt[GX_VA_TEX1] = (u8)((data >> 0) & 0x1u);
            fmt->attr_type[GX_VA_TEX1] = (u8)((data >> 1) & 0x7u);
            fmt->attr_frac[GX_VA_TEX1] = (u8)((data >> 4) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX2] = (u8)((data >> 9) & 0x1u);
            fmt->attr_type[GX_VA_TEX2] = (u8)((data >> 10) & 0x7u);
            fmt->attr_frac[GX_VA_TEX2] = (u8)((data >> 13) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX3] = (u8)((data >> 18) & 0x1u);
            fmt->attr_type[GX_VA_TEX3] = (u8)((data >> 19) & 0x7u);
            fmt->attr_frac[GX_VA_TEX3] = (u8)((data >> 22) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX4] = (u8)((data >> 27) & 0x1u);
            fmt->attr_type[GX_VA_TEX4] = (u8)((data >> 28) & 0x7u);
            break;
        case 0x9:
            fmt->attr_frac[GX_VA_TEX4] = (u8)((data >> 0) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX5] = (u8)((data >> 5) & 0x1u);
            fmt->attr_type[GX_VA_TEX5] = (u8)((data >> 6) & 0x7u);
            fmt->attr_frac[GX_VA_TEX5] = (u8)((data >> 9) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX6] = (u8)((data >> 14) & 0x1u);
            fmt->attr_type[GX_VA_TEX6] = (u8)((data >> 15) & 0x7u);
            fmt->attr_frac[GX_VA_TEX6] = (u8)((data >> 18) & 0x1Fu);

            fmt->attr_cnt[GX_VA_TEX7] = (u8)((data >> 23) & 0x1u);
            fmt->attr_type[GX_VA_TEX7] = (u8)((data >> 24) & 0x7u);
            fmt->attr_frac[GX_VA_TEX7] = (u8)((data >> 27) & 0x1Fu);
            break;
        default:
            break;
    }
}

static void pc_gx_dl_apply_cp_stream_reg(PCGXDLDecodeState* st, u8 reg8, u32 data) {
    u8 reg_idx;
    u8 reg_addr;
    if (!st) return;

    reg_idx = (u8)(reg8 & 0x0Fu);
    reg_addr = (u8)((reg8 >> 4) & 0x0Fu);
    switch (reg_addr) {
        case 0x3:
            st->matindex_a = data;
            break;
        case 0x4:
            st->matindex_b = data;
            break;
        case 0x5:
            pc_gx_dl_apply_vcd_lo(st, data);
            break;
        case 0x6:
            pc_gx_dl_apply_vcd_hi(st, data);
            break;
        case 0x7:
        case 0x8:
        case 0x9:
            pc_gx_dl_apply_vat(st, reg_addr, reg_idx, data);
            break;
        case 0xA:
        case 0xB: {
            u32 cp_attr = reg_idx;
            if (cp_attr <= (GX_VA_TEX7 - GX_VA_POS)) {
                u32 attr = GX_VA_POS + cp_attr;
                if (reg_addr == 0xA) {
                    st->array_base[attr] = (const void*)(uintptr_t)(u32)data;
                } else {
                    st->array_stride[attr] = (u8)(data & 0xFFu);
                }
            }
            break;
        }
        default:
            break;
    }
}

static float pc_gx_dl_u32_as_f32(u32 bits) {
    float v;
    memcpy(&v, &bits, sizeof(v));
    return v;
}

static void pc_gx_dl_apply_xf_reg_packet(u32 addr, u32 count, const u32* words) {
    if (!words || count == 0) return;

    /* Projection packet from __GXSetProjection: XF 0x1020, 6 floats + type. */
    if (addr == 0x1020u && count >= 7u) {
        float pv[7];
        pv[0] = (words[6] != 0u) ? 1.0f : 0.0f; /* GX_ORTHOGRAPHIC or GX_PERSPECTIVE */
        for (u32 i = 0; i < 6u; ++i) {
            pv[i + 1u] = pc_gx_dl_u32_as_f32(words[i]);
        }
        GXSetProjectionv(pv);
        return;
    }

    /* Position matrix packet: addr = id*4, count = 12 floats. */
    if (addr < 0x400u && count == 12u && (addr % 12u) == 0u) {
        float mtx[3][4];
        u32 id = addr / 4u;
        for (u32 i = 0; i < 12u; ++i) {
            ((float*)mtx)[i] = pc_gx_dl_u32_as_f32(words[i]);
        }
        GXLoadPosMtxImm(mtx, id);
        return;
    }

    /* Normal matrix packet: addr = id*3 + 0x400, count = 9 floats. */
    if (addr >= 0x400u && addr < 0x500u && count == 9u && ((addr - 0x400u) % 9u) == 0u) {
        float mtx3x3[3][3];
        u32 id = (addr - 0x400u) / 3u;
        for (u32 i = 0; i < 9u; ++i) {
            ((float*)mtx3x3)[i] = pc_gx_dl_u32_as_f32(words[i]);
        }
        GXLoadNrmMtxImm3x3(mtx3x3, id);
        return;
    }

    /* Texture matrix packet: regular or post-transform texture matrix ranges. */
    if ((count == 8u || count == 12u) &&
        ((addr >= 120u && addr < 0x400u) || (addr >= 0x500u && addr < 0x600u))) {
        float mtx[3][4];
        u32 id;
        memset(mtx, 0, sizeof(mtx));
        for (u32 i = 0; i < count; ++i) {
            ((float*)mtx)[i] = pc_gx_dl_u32_as_f32(words[i]);
        }
        if (addr >= 0x500u) {
            id = ((addr - 0x500u) / 4u) + GX_PTTEXMTX0;
        } else {
            id = addr / 4u;
        }
        GXLoadTexMtxImm(mtx, id, (count == 8u) ? GX_MTX2x4 : GX_MTX3x4);
        return;
    }
}

static void pc_gx_dl_apply_index_load(u8 cmd, u32 packed) {
    u32 offset = packed & 0x0FFFu;
    u32 count = ((packed >> 12) & 0x0Fu) + 1u;
    u16 mtx_indx = (u16)(packed >> 16);

    switch (cmd) {
    case GX_LOAD_INDX_A:
        /* GXLoadPosMtxIndx: offset = id * 4, count = 12. */
        if (count == 12u && (offset % 4u) == 0u) {
            u32 id = offset / 4u;
            GXLoadPosMtxIndx(mtx_indx, id);
        }
        break;
    case GX_LOAD_INDX_B:
        /* GXLoadNrmMtxIndx3x3: offset = id * 3 + 0x400, count = 9. */
        if (count == 9u && offset >= 0x400u && ((offset - 0x400u) % 3u) == 0u) {
            u32 id = (offset - 0x400u) / 3u;
            GXLoadNrmMtxIndx3x3(mtx_indx, id);
        }
        break;
    case GX_LOAD_INDX_C:
        /* GXLoadTexMtxIndx: regular or post-transform texture matrix. */
        if ((count == 8u || count == 12u) && (offset % 4u) == 0u) {
            u32 id;
            GXTexMtxType type = (count == 8u) ? GX_MTX2x4 : GX_MTX3x4;
            if (offset >= 0x500u) {
                id = GX_PTTEXMTX0 + ((offset - 0x500u) / 4u);
            } else {
                id = offset / 4u;
            }
            GXLoadTexMtxIndx(mtx_indx, id, type);
        }
        break;
    case GX_LOAD_INDX_D:
    default:
        /* Not used by current titles/tests in this backend path yet. */
        break;
    }
}

static u32 pc_gx_bp_decode_ras_chan(u32 ras_chan) {
    switch (ras_chan & 0x7u) {
        case 0: return GX_COLOR0A0;
        case 1: return GX_COLOR1A1;
        case 5: return GX_ALPHA_BUMP;
        case 6: return GX_ALPHA_BUMPN;
        case 7: return GX_COLOR_ZERO;
        default: return GX_COLOR_ZERO;
    }
}

static u32 pc_gx_bp_decode_tev_op(u32 bias, u32 op_bit, u32 scale_or_cmp) {
    if ((bias & 0x3u) == 0x3u) {
        /* Compare mode: GX_TEV_COMP_* encoding is 8 + (mode*2 + cmp) */
        return (u32)(8u + (((scale_or_cmp & 0x3u) << 1) | (op_bit & 0x1u)));
    }
    return (op_bit & 0x1u) ? GX_TEV_SUB : GX_TEV_ADD;
}

static void pc_gx_dl_apply_bp_reg(u8 reg, u32 data24) {
    u32 data = data24 & 0x00FFFFFFu;

    if (reg == 0x00) {
        u32 num_tex_gens = data & 0xFu;
        u32 num_chans = (data >> 4) & 0x7u;
        u32 num_tev_stages = ((data >> 10) & 0xFu) + 1u;
        u32 cull_mode = (data >> 14) & 0x3u;
        u32 num_ind_stages = (data >> 16) & 0x7u;

        GXSetNumTexGens((u8)num_tex_gens);
        GXSetNumChans((u8)num_chans);
        GXSetNumTevStages((u8)num_tev_stages);
        GXSetCullMode(cull_mode);
        GXSetNumIndStages((u8)num_ind_stages);
        return;
    }

    if (reg == 0x25 || reg == 0x26) {
        u32 even_stage = (u32)(reg - 0x25u) * 2u;
        u32 ss0 = (data >> 0) & 0xFu;
        u32 ts0 = (data >> 4) & 0xFu;
        u32 ss1 = (data >> 8) & 0xFu;
        u32 ts1 = (data >> 12) & 0xFu;
        GXSetIndTexCoordScale(even_stage, ss0, ts0);
        GXSetIndTexCoordScale(even_stage + 1u, ss1, ts1);
        return;
    }

    if (reg == 0x27) {
        u32 i;
        for (i = 0; i < 4u; ++i) {
            u32 shift = i * 6u;
            u32 tex_map = (data >> shift) & 0x7u;
            u32 tex_coord = (data >> (shift + 3u)) & 0x7u;
            GXSetIndTexOrder(i, tex_coord, tex_map);
        }
        return;
    }

    if (reg >= 0x28 && reg <= 0x2F) {
        u32 stage_even = (u32)(reg - 0x28u) * 2u;
        u32 tex_map_even = (data >> 0) & 0x7u;
        u32 tex_coord_even = (data >> 3) & 0x7u;
        u32 enable_even = (data >> 6) & 0x1u;
        u32 color_even = (data >> 7) & 0x7u;

        u32 tex_map_odd = (data >> 12) & 0x7u;
        u32 tex_coord_odd = (data >> 15) & 0x7u;
        u32 enable_odd = (data >> 18) & 0x1u;
        u32 color_odd = (data >> 19) & 0x7u;

        if (!enable_even) {
            tex_map_even = GX_TEXMAP_NULL;
            tex_coord_even = GX_TEXCOORD_NULL;
        }
        if (!enable_odd) {
            tex_map_odd = GX_TEXMAP_NULL;
            tex_coord_odd = GX_TEXCOORD_NULL;
        }

        GXSetTevOrder(stage_even, tex_coord_even, tex_map_even, pc_gx_bp_decode_ras_chan(color_even));
        GXSetTevOrder(stage_even + 1u, tex_coord_odd, tex_map_odd, pc_gx_bp_decode_ras_chan(color_odd));
        return;
    }

    if (reg == 0x40) {
        GXSetZMode((GXBool)((data >> 0) & 0x1u), (data >> 1) & 0x7u, (GXBool)((data >> 4) & 0x1u));
        return;
    }

    if (reg == 0x41) {
        u32 blend_enable = (data >> 0) & 0x1u;
        u32 logic_enable = (data >> 1) & 0x1u;
        u32 dither = (data >> 2) & 0x1u;
        u32 color_update = (data >> 3) & 0x1u;
        u32 alpha_update = (data >> 4) & 0x1u;
        u32 dst_factor = (data >> 5) & 0x7u;
        u32 src_factor = (data >> 8) & 0x7u;
        u32 subtract = (data >> 11) & 0x1u;
        u32 logic_op = (data >> 12) & 0xFu;
        u32 blend_mode = GX_BM_NONE;

        if (subtract) blend_mode = GX_BM_SUBTRACT;
        else if (logic_enable) blend_mode = GX_BM_LOGIC;
        else if (blend_enable) blend_mode = GX_BM_BLEND;

        GXSetBlendMode(blend_mode, src_factor, dst_factor, logic_op);
        GXSetDither((GXBool)dither);
        GXSetColorUpdate((GXBool)color_update);
        GXSetAlphaUpdate((GXBool)alpha_update);
        return;
    }

    if (reg == 0x42) {
        g_bp_cmode1_pix_subfmt = (int)((data >> 9) & 0x3u);
        GXSetDstAlpha((GXBool)((data >> 8) & 0x1u), (u8)(data & 0xFFu));
        if (((u32)g_bp_pe_pixfmt_bits & 0x7u) == 4u) {
            GXSetPixelFmt(
                pc_gx_bp_decode_pixel_fmt((u32)g_bp_pe_pixfmt_bits, (u32)g_bp_cmode1_pix_subfmt),
                (GXZFmt16)g_gx.z_fmt
            );
        }
        return;
    }

    if (reg == 0x43) {
        u32 pe_fmt_bits = data & 0x7u;
        u32 z_fmt = (data >> 3) & 0x7u;
        /* PE control register: pixel format (PE-encoded), Z format, and Z compare location. */
        g_bp_pe_pixfmt_bits = (int)pe_fmt_bits;
        GXSetPixelFmt(pc_gx_bp_decode_pixel_fmt(pe_fmt_bits, (u32)g_bp_cmode1_pix_subfmt), (GXZFmt16)z_fmt);
        GXSetZCompLoc((GXBool)((data >> 6) & 0x1u));
        return;
    }

    if (reg >= 0xC0 && reg <= 0xDF) {
        u32 stage = (reg - 0xC0u) >> 1;
        if ((reg & 1u) == 0u) {
            u32 d = (data >> 0) & 0xFu;
            u32 c = (data >> 4) & 0xFu;
            u32 b = (data >> 8) & 0xFu;
            u32 a = (data >> 12) & 0xFu;
            u32 bias = (data >> 16) & 0x3u;
            u32 op_bit = (data >> 18) & 0x1u;
            u32 clamp = (data >> 19) & 0x1u;
            u32 scale_or_cmp = (data >> 20) & 0x3u;
            u32 dest = (data >> 22) & 0x3u;
            u32 tev_op = pc_gx_bp_decode_tev_op(bias, op_bit, scale_or_cmp);
            u32 tev_bias = (bias == 3u) ? GX_TB_ZERO : bias;
            u32 tev_scale = (bias == 3u) ? GX_CS_SCALE_1 : scale_or_cmp;

            GXSetTevColorIn(stage, a, b, c, d);
            GXSetTevColorOp(stage, tev_op, tev_bias, tev_scale, (GXBool)clamp, dest);
        } else {
            u32 rswap = (data >> 0) & 0x3u;
            u32 tswap = (data >> 2) & 0x3u;
            u32 d = (data >> 4) & 0x7u;
            u32 c = (data >> 7) & 0x7u;
            u32 b = (data >> 10) & 0x7u;
            u32 a = (data >> 13) & 0x7u;
            u32 bias = (data >> 16) & 0x3u;
            u32 op_bit = (data >> 18) & 0x1u;
            u32 clamp = (data >> 19) & 0x1u;
            u32 scale_or_cmp = (data >> 20) & 0x3u;
            u32 dest = (data >> 22) & 0x3u;
            u32 tev_op = pc_gx_bp_decode_tev_op(bias, op_bit, scale_or_cmp);
            u32 tev_bias = (bias == 3u) ? GX_TB_ZERO : bias;
            u32 tev_scale = (bias == 3u) ? GX_CS_SCALE_1 : scale_or_cmp;

            GXSetTevAlphaIn(stage, a, b, c, d);
            GXSetTevAlphaOp(stage, tev_op, tev_bias, tev_scale, (GXBool)clamp, dest);
            GXSetTevSwapMode(stage, rswap, tswap);
        }
        return;
    }

    if (reg == 0xF3) {
        u32 ref0 = (data >> 0) & 0xFFu;
        u32 ref1 = (data >> 8) & 0xFFu;
        u32 comp0 = (data >> 16) & 0x7u;
        u32 comp1 = (data >> 19) & 0x7u;
        u32 logic = (data >> 22) & 0x3u;
        GXSetAlphaCompare(comp0, (u8)ref0, logic, comp1, (u8)ref1);
        return;
    }

    if (reg >= 0xF6 && reg <= 0xFD) {
        u32 stage_even = (u32)(reg - 0xF6u) * 2u;
        u32 swap_rb = (data >> 0) & 0x3u;
        u32 swap_ga = (data >> 2) & 0x3u;
        u32 kcsel_even = (data >> 4) & 0x1Fu;
        u32 kasel_even = (data >> 9) & 0x1Fu;
        u32 kcsel_odd = (data >> 14) & 0x1Fu;
        u32 kasel_odd = (data >> 19) & 0x1Fu;

        GXSetTevSwapModeTable((reg - 0xF6u), swap_rb, swap_ga, swap_rb, swap_ga);
        GXSetTevKColorSel(stage_even, kcsel_even);
        GXSetTevKAlphaSel(stage_even, kasel_even);
        GXSetTevKColorSel(stage_even + 1u, kcsel_odd);
        GXSetTevKAlphaSel(stage_even + 1u, kasel_odd);
        return;
    }
}

static u32 pc_gx_dl_execute_draw_command(const u8* ptr, u32 remaining, PCGXDLDecodeState* st) {
    u8 cmd;
    u8 prim;
    u8 vtxfmt;
    u32 nverts;
    u32 v;
    const u8* cur;
    u32 left;
    u32 attr;

    if (!ptr || !st || remaining < 3) return 0;

    cmd = ptr[0];
    prim = (u8)(cmd & GX_OPCODE_MASK);
    vtxfmt = (u8)(cmd & GX_VAT_MASK);
    if (vtxfmt >= GX_MAX_VTXFMT) return 0;

    switch (prim) {
        case GX_DRAW_QUADS:
        case 0x88: /* GX_DRAW_QUADS_2 behaves the same as GX_DRAW_QUADS */
        case GX_DRAW_TRIANGLES:
        case GX_DRAW_TRIANGLE_STRIP:
        case GX_DRAW_TRIANGLE_FAN:
        case GX_DRAW_LINES:
        case GX_DRAW_LINE_STRIP:
        case GX_DRAW_POINTS:
            break;
        default:
            return 0;
    }

    nverts = pc_gx_dl_read_be16(ptr + 1);
    cur = ptr + 3;
    left = remaining - 3;

    GXBegin((u32)prim, (u32)vtxfmt, (u16)nverts);

    for (v = 0; v < nverts; ++v) {
        if (st->vtx_desc[GX_VA_PNMTXIDX] == GX_NONE) {
            pc_gx_set_matrix_index_attr(GX_VA_PNMTXIDX, pc_gx_dl_get_default_matrix_index(st, GX_VA_PNMTXIDX));
        }
        for (attr = GX_VA_TEX0MTXIDX; attr <= GX_VA_TEX7MTXIDX; ++attr) {
            if (st->vtx_desc[attr] == GX_NONE) {
                pc_gx_set_matrix_index_attr(attr, pc_gx_dl_get_default_matrix_index(st, attr));
            }
        }
        for (attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7; ++attr) {
            u32 type = (u32)st->vtx_desc[attr];
            if (type == GX_NONE) continue;

            if (type == GX_INDEX8) {
                u32 index_bytes = 1;
                if (left < 1) {
                    GXEnd();
                    return 0;
                }
                if (attr == GX_VA_NRM && st->vtx_fmt[vtxfmt].attr_cnt[GX_VA_NRM] == GX_NRM_NBT3) {
                    index_bytes = 3;
                    if (left < index_bytes) {
                        GXEnd();
                        return 0;
                    }
                    GXNormal1x8(cur[0]);
                    GXNormal1x8(cur[1]);
                    GXNormal1x8(cur[2]);
                    cur += index_bytes;
                    left -= index_bytes;
                    continue;
                }
                if (!pc_gx_dl_emit_indexed_attr(st, vtxfmt, attr, (u32)cur[0])) {
                    GXEnd();
                    return 0;
                }
                cur += index_bytes;
                left -= index_bytes;
            } else if (type == GX_INDEX16) {
                u32 index;
                u32 index_bytes = 2;
                if (left < 2) {
                    GXEnd();
                    return 0;
                }
                if (attr == GX_VA_NRM && st->vtx_fmt[vtxfmt].attr_cnt[GX_VA_NRM] == GX_NRM_NBT3) {
                    index_bytes = 6;
                    if (left < index_bytes) {
                        GXEnd();
                        return 0;
                    }
                    GXNormal1x16(pc_gx_dl_read_be16(cur + 0));
                    GXNormal1x16(pc_gx_dl_read_be16(cur + 2));
                    GXNormal1x16(pc_gx_dl_read_be16(cur + 4));
                    cur += index_bytes;
                    left -= index_bytes;
                    continue;
                }
                index = (u32)pc_gx_dl_read_be16(cur);
                if (!pc_gx_dl_emit_indexed_attr(st, vtxfmt, attr, index)) {
                    GXEnd();
                    return 0;
                }
                cur += index_bytes;
                left -= index_bytes;
            } else if (type == GX_DIRECT) {
                u32 consumed = 0;
                if (!pc_gx_dl_emit_direct_attr(st, vtxfmt, attr, cur, left, &consumed) || consumed > left) {
                    GXEnd();
                    return 0;
                }
                cur += consumed;
                left -= consumed;
            } else {
                GXEnd();
                return 0;
            }
        }
    }

    GXEnd();
    return (u32)(cur - ptr);
}

static int pc_gx_dl_try_execute_custom(const u8* p, u32 nbytes) {
    u32 off = 0;

    while (off + sizeof(u32) <= nbytes) {
        u32 op = 0;

        /* GXEndDisplayList pads to 32-byte alignment using 0x00 (GX_NOP). */
        if (p[off] == 0) {
            u32 i;
            int all_zero = 1;
            for (i = off; i < nbytes; ++i) {
                if (p[i] != 0) {
                    all_zero = 0;
                    break;
                }
            }
            if (all_zero) return 1;
        }

        memcpy(&op, p + off, sizeof(op));
        off += sizeof(op);

        switch (op) {
            case PCGX_DL_OP_TEXCOPY_SRC: {
                u32 v[4];
                if (off + sizeof(v) > nbytes) return 0;
                memcpy(v, p + off, sizeof(v));
                off += sizeof(v);
                GXSetTexCopySrc((u16)v[0], (u16)v[1], (u16)v[2], (u16)v[3]);
                break;
            }
            case PCGX_DL_OP_TEXCOPY_DST: {
                u32 v[4];
                if (off + sizeof(v) > nbytes) return 0;
                memcpy(v, p + off, sizeof(v));
                off += sizeof(v);
                GXSetTexCopyDst((u16)v[0], (u16)v[1], v[2], (GXBool)(v[3] ? 1 : 0));
                break;
            }
            case PCGX_DL_OP_COPY_FILTER:
                break;
            case PCGX_DL_OP_COPY_TEX: {
                u64 dest64 = 0;
                u32 clear = 0;
                if (off + sizeof(dest64) + sizeof(clear) > nbytes) return 0;
                memcpy(&dest64, p + off, sizeof(dest64));
                off += sizeof(dest64);
                memcpy(&clear, p + off, sizeof(clear));
                off += sizeof(clear);
                pc_gx_copy_tex_execute((void*)(uintptr_t)dest64, (GXBool)(clear ? 1 : 0));
                break;
            }
            case PCGX_DL_OP_DRAW_BATCH: {
                u32 prim = 0;
                u32 count = 0;
                u32 bytes = 0;
                const PCGXVertex* verts;
                int old_prim;
                int old_vtxfmt;
                int old_expected;
                int old_in_begin;
                int old_pending;
                int old_idx;
                PCGXVertex old_current;

                if (off + sizeof(prim) + sizeof(count) > nbytes) return 0;
                memcpy(&prim, p + off, sizeof(prim));
                off += sizeof(prim);
                memcpy(&count, p + off, sizeof(count));
                off += sizeof(count);
                if (count == 0 || count > PC_GX_MAX_VERTS) return 0;

                bytes = count * (u32)sizeof(PCGXVertex);
                if (off + bytes > nbytes) return 0;
                verts = (const PCGXVertex*)(const void*)(p + off);
                off += bytes;

                old_prim = g_gx.current_primitive;
                old_vtxfmt = g_gx.current_vtxfmt;
                old_expected = g_gx.expected_vertex_count;
                old_in_begin = g_gx.in_begin;
                old_pending = g_gx.vertex_pending;
                old_idx = g_gx.current_vertex_idx;
                old_current = g_gx.current_vertex;

                g_gx.current_primitive = (int)prim;
                g_gx.current_vertex_idx = (int)count;
                g_gx.in_begin = 0;
                g_gx.vertex_pending = 0;
                memcpy(g_gx.vertex_buffer, verts, bytes);
                pc_gx_flush_vertices();

                g_gx.current_primitive = old_prim;
                g_gx.current_vtxfmt = old_vtxfmt;
                g_gx.expected_vertex_count = old_expected;
                g_gx.in_begin = old_in_begin;
                g_gx.vertex_pending = old_pending;
                g_gx.current_vertex_idx = old_idx;
                g_gx.current_vertex = old_current;
                break;
            }
            default:
                return 0;
        }
    }

    return off == nbytes;
}

static void pc_gx_dl_execute_raw_internal(const u8* p, u32 nbytes, PCGXDLDecodeState* st, u32 depth) {
    u32 off = 0;
    if (!p || !st || depth > 8) return;

    while (off < nbytes) {
        u32 remaining = nbytes - off;
        u8 cmd = p[off];
        u8 prim = (u8)(cmd & GX_OPCODE_MASK);

        if (prim >= GX_DRAW_QUADS && prim <= GX_DRAW_POINTS) {
            u32 consumed = pc_gx_dl_execute_draw_command(p + off, remaining, st);
            if (consumed == 0) break;
            off += consumed;
            continue;
        }

        switch (cmd) {
            case GX_NOP:
            case GX_CMD_INVL_VC:
                off += 1;
                break;
            case GX_LOAD_BP_REG:
                if (remaining < 5) return;
                {
                    u32 packed = pc_gx_dl_read_be32(p + off + 1);
                    u8 reg = (u8)((packed >> 24) & 0xFFu);
                    u32 data = packed & 0x00FFFFFFu;
                    pc_gx_dl_apply_bp_reg(reg, data);
                }
                off += 5;
                break;
            case GX_LOAD_CP_REG: {
                u8 reg8;
                u32 data;
                if (remaining < 6) return;
                reg8 = p[off + 1];
                data = pc_gx_dl_read_be32(p + off + 2);
                pc_gx_dl_apply_cp_stream_reg(st, reg8, data);
                off += 6;
                break;
            }
            case GX_LOAD_XF_REG: {
                if (remaining < 5) return;
                u32 hdr_be = pc_gx_dl_read_be32(p + off + 1);
                u32 count = ((hdr_be >> 16) & 0x0F) + 1;
                u32 addr = hdr_be & 0xFFFFu;
                u32 packet_size = 1 + 4 + count * 4;
                if (remaining < packet_size) return;
                {
                    u32 words[16];
                    u32 n = (count <= 16u) ? count : 16u;
                    for (u32 i = 0; i < n; ++i) {
                        words[i] = pc_gx_dl_read_be32(p + off + 5 + (i * 4));
                    }
                    pc_gx_dl_apply_xf_reg_packet(addr, n, words);
                }
                off += packet_size;
                break;
            }
            case GX_LOAD_INDX_A:
            case GX_LOAD_INDX_B:
            case GX_LOAD_INDX_C:
            case GX_LOAD_INDX_D:
                if (remaining < 5) return;
                {
                    u32 packed = pc_gx_dl_read_be32(p + off + 1);
                    pc_gx_dl_apply_index_load(cmd, packed);
                }
                off += 5;
                break;
            case GX_CMD_CALL_DL:
            case (GX_CMD_CALL_DL | 0x04):
                /* 1-byte opcode + 32-bit address + 32-bit size */
                if (remaining < 9) return;
                {
                    u32 dl_addr = pc_gx_dl_read_be32(p + off + 1);
                    u32 dl_size = pc_gx_dl_read_be32(p + off + 5);
                    if (dl_addr && dl_size) {
                        pc_gx_dl_execute_raw_internal((const u8*)(uintptr_t)dl_addr, dl_size, st, depth + 1);
                    }
                }
                off += 9;
                break;
            default:
                return;
        }
    }
}

static void pc_gx_dl_execute_raw(const u8* p, u32 nbytes) {
    PCGXDLDecodeState st;
    pc_gx_dl_state_init(&st);
    pc_gx_dl_execute_raw_internal(p, nbytes, &st, 0);
}

void GXCallDisplayList(const void* list, u32 nbytes) {
    /* SDK contract: cannot call a display list while recording one, and both
     * pointer and byte count must be 32-byte aligned. */
    if (g_pc_gx_dl.active) return;
    if (!list || nbytes == 0 || (nbytes & 31u) != 0u || (((uintptr_t)list) & 31u) != 0u) return;
    pc_gx_commit_pending_and_flush();

    const u8* p = (const u8*)list;
    if (!pc_gx_dl_try_execute_custom(p, nbytes)) {
        pc_gx_dl_execute_raw(p, nbytes);
    }
}
void GXCallDisplayListLE(const void* list, u32 nbytes) {
    GXCallDisplayList(list, nbytes);
}

/* --- Indirect Texture --- */
void GXSetTevIndirect(GXTevStageID stage, GXIndTexStageID ind_stage, GXIndTexFormat fmt, GXIndTexBiasSel bias_sel,
                      GXIndTexMtxID mtx_sel, GXIndTexWrap wrap_s, GXIndTexWrap wrap_t, GXBool add_prev,
                      GXBool ind_lod, GXIndTexAlphaSel alpha_sel);

static GXIndTexWrap pc_gx_tile_size_to_wrap(u16 tile_size) {
    switch (tile_size) {
    case 256: return GX_ITW_256;
    case 128: return GX_ITW_128;
    case 64: return GX_ITW_64;
    case 32: return GX_ITW_32;
    case 16: return GX_ITW_16;
    default:  return GX_ITW_OFF;
    }
}

void GXSetTevDirect(GXTevStageID stage) {
    GXSetTevIndirect(stage, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE,
                     GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
}
void GXSetNumIndStages(u8 n) {
    pc_gx_flush_if_begin_complete();
    DIRTY(PC_GX_DIRTY_INDIRECT);
    if (n > GX_MAX_INDTEXSTAGE) n = GX_MAX_INDTEXSTAGE;
    g_gx.num_ind_stages = n;
}

void GXSetIndTexMtx(GXIndTexMtxID mtx_sel, const void* offset, s8 scale) {
    pc_gx_flush_if_begin_complete();

    int id;
    switch (mtx_sel) {
        case GX_ITM_0: case GX_ITM_1: case GX_ITM_2:   id = mtx_sel - GX_ITM_0; break;
        case GX_ITM_S0: case GX_ITM_S1: case GX_ITM_S2: id = mtx_sel - GX_ITM_S0; break;
        case GX_ITM_T0: case GX_ITM_T1: case GX_ITM_T2: id = mtx_sel - GX_ITM_T0; break;
        default: id = 0; break;
    }
    if (!offset) return;

    const float* mtx = (const float*)offset;
    g_gx.ind_mtx[id][0][0] = pc_gx_quantize_ind_mtx_coeff(mtx[0]);
    g_gx.ind_mtx[id][0][1] = pc_gx_quantize_ind_mtx_coeff(mtx[1]);
    g_gx.ind_mtx[id][0][2] = pc_gx_quantize_ind_mtx_coeff(mtx[2]);
    g_gx.ind_mtx[id][1][0] = pc_gx_quantize_ind_mtx_coeff(mtx[3]);
    g_gx.ind_mtx[id][1][1] = pc_gx_quantize_ind_mtx_coeff(mtx[4]);
    g_gx.ind_mtx[id][1][2] = pc_gx_quantize_ind_mtx_coeff(mtx[5]);

    {
        /* SDK stores (scale_exp + 17) in 6 bits; mirror effective decoded exponent. */
        int scale_bits = (((int)scale) + 17) & 0x3F;
        g_gx.ind_mtx_scale[id] = scale_bits - 17;
    }

    DIRTY(PC_GX_DIRTY_INDIRECT);
}

void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map) {
    pc_gx_flush_if_begin_complete();

    if (tex_map == GX_TEXMAP_NULL) {
        tex_map = GX_TEXMAP0;
    }
    if (tex_coord == GX_TEXCOORD_NULL) {
        tex_coord = GX_TEXCOORD0;
    }

    if (tex_map >= GX_MAX_TEXMAP) return;
    if (tex_coord >= GX_MAX_TEXCOORD) return;
    if (ind_stage >= GX_MAX_INDTEXSTAGE) return;

    DIRTY(PC_GX_DIRTY_INDIRECT);
    g_gx.ind_order[ind_stage].tex_coord = tex_coord;
    g_gx.ind_order[ind_stage].tex_map = tex_map;
}

void GXSetTevIndirect(GXTevStageID stage, GXIndTexStageID ind_stage, GXIndTexFormat fmt, GXIndTexBiasSel bias_sel,
                      GXIndTexMtxID mtx_sel, GXIndTexWrap wrap_s, GXIndTexWrap wrap_t, GXBool add_prev,
                      GXBool ind_lod, GXIndTexAlphaSel alpha_sel) {
    pc_gx_flush_if_begin_complete();
    if (stage >= GX_MAX_TEVSTAGE) return;

    if (ind_stage >= GX_MAX_INDTEXSTAGE) ind_stage = GX_INDTEXSTAGE0;
    if (fmt >= GX_MAX_ITFORMAT) fmt = GX_ITF_8;
    if (bias_sel >= GX_MAX_ITBIAS) bias_sel = GX_ITB_NONE;
    if (wrap_s >= GX_MAX_ITWRAP) wrap_s = GX_ITW_OFF;
    if (wrap_t >= GX_MAX_ITWRAP) wrap_t = GX_ITW_OFF;
    if (alpha_sel >= GX_MAX_ITBALPHA) alpha_sel = GX_ITBA_OFF;
    switch (mtx_sel) {
        case GX_ITM_OFF:
        case GX_ITM_0: case GX_ITM_1: case GX_ITM_2:
        case GX_ITM_S0: case GX_ITM_S1: case GX_ITM_S2:
        case GX_ITM_T0: case GX_ITM_T1: case GX_ITM_T2:
            break;
        default:
            mtx_sel = GX_ITM_OFF;
            break;
    }

    DIRTY(PC_GX_DIRTY_INDIRECT);
    PCGXTevStage* s = &g_gx.tev_stages[stage];
    s->ind_stage  = ind_stage;
    s->ind_format = fmt;
    s->ind_bias   = bias_sel;
    s->ind_mtx    = mtx_sel;
    s->ind_wrap_s = wrap_s;
    s->ind_wrap_t = wrap_t;
    s->ind_add_prev = add_prev;
    s->ind_lod    = ind_lod;
    s->ind_alpha  = alpha_sel;
}

void GXSetTevIndWarp(
    GXTevStageID stage, GXIndTexStageID ind_stage, GXBool signed_ofs, GXBool replace, GXIndTexMtxID mtx_sel) {
    GXIndTexWrap wrap = replace ? GX_ITW_0 : GX_ITW_OFF;
    GXIndTexBiasSel bias = signed_ofs ? GX_ITB_STU : GX_ITB_NONE;
    GXSetTevIndirect(stage, ind_stage, GX_ITF_8, bias, mtx_sel, wrap, wrap, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
}

void GXSetTevIndBumpXYZ(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) {
    GXSetTevIndirect(tev_stage, ind_stage, GX_ITF_8, GX_ITB_STU, matrix_sel, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE,
                     GX_FALSE, GX_ITBA_OFF);
}

void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u16 tilesize_s, u16 tilesize_t,
                     u16 tilespacing_s, u16 tilespacing_t, GXIndTexFormat format, GXIndTexMtxID matrix_sel,
                     GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel) {
    const GXIndTexWrap wrap_s = pc_gx_tile_size_to_wrap(tilesize_s);
    const GXIndTexWrap wrap_t = pc_gx_tile_size_to_wrap(tilesize_t);
    const float mtx[2][3] = {
        { (float)tilespacing_s / 1024.0f, 0.0f, 0.0f },
        { 0.0f, (float)tilespacing_t / 1024.0f, 0.0f },
    };

    GXSetIndTexMtx(matrix_sel, mtx, 10);
    GXSetTevIndirect(tev_stage, ind_stage, format, bias_sel, matrix_sel, wrap_s, wrap_t, GX_FALSE, GX_TRUE, alpha_sel);
}

void GXSetTevIndBumpST(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) {
    GXIndTexMtxID sm;
    GXIndTexMtxID tm;
    switch (matrix_sel) {
    case GX_ITM_0:
        sm = GX_ITM_S0;
        tm = GX_ITM_T0;
        break;
    case GX_ITM_1:
        sm = GX_ITM_S1;
        tm = GX_ITM_T1;
        break;
    case GX_ITM_2:
        sm = GX_ITM_S2;
        tm = GX_ITM_T2;
        break;
    default:
        return;
    }

    GXSetTevIndirect(tev_stage, ind_stage, GX_ITF_8, GX_ITB_ST, sm, GX_ITW_0, GX_ITW_0, GX_FALSE, GX_FALSE,
                     GX_ITBA_OFF);
    GXSetTevIndirect((GXTevStageID)(tev_stage + 1), ind_stage, GX_ITF_8, GX_ITB_ST, tm, GX_ITW_0, GX_ITW_0, GX_TRUE,
                     GX_FALSE, GX_ITBA_OFF);
    GXSetTevIndirect((GXTevStageID)(tev_stage + 2), ind_stage, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF,
                     GX_ITW_OFF, GX_TRUE, GX_FALSE, GX_ITBA_OFF);
}

void GXSetTevIndRepeat(GXTevStageID tev_stage) {
    GXSetTevIndirect(tev_stage, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_0, GX_ITW_0, GX_TRUE,
                     GX_FALSE, GX_ITBA_OFF);
}

void GXSetIndTexCoordScale(GXIndTexStageID ind_stage, GXIndTexScale scale_s, GXIndTexScale scale_t) {
    pc_gx_flush_if_begin_complete();
    if (ind_stage >= GX_MAX_INDTEXSTAGE) return;
    if (scale_s >= GX_MAX_ITSCALE) scale_s = GX_ITS_256;
    if (scale_t >= GX_MAX_ITSCALE) scale_t = GX_ITS_256;

    DIRTY(PC_GX_DIRTY_INDIRECT);
    g_gx.ind_order[ind_stage].scale_s = scale_s;
    g_gx.ind_order[ind_stage].scale_t = scale_t;
}

void __GXSetIndirectMask(u32 mask) { (void)mask; }

/* --- Z Texture --- */
void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias) {
    pc_gx_flush_if_begin_complete();
    switch (op) {
        case GX_ZT_DISABLE:
        case GX_ZT_ADD:
        case GX_ZT_REPLACE:
            g_gx.ztex_op = (int)op;
            break;
        default:
            g_gx.ztex_op = GX_ZT_DISABLE;
            break;
    }
    switch (fmt) {
        case GX_TF_Z8:
        case GX_TF_Z16:
        case GX_TF_Z24X8:
            g_gx.ztex_fmt = (int)fmt;
            break;
        default:
            g_gx.ztex_fmt = GX_TF_Z24X8;
            break;
    }
    g_gx.ztex_bias = (int)(bias & 0x00FFFFFF);
    DIRTY(PC_GX_DIRTY_DEPTH);
}

/* --- Draw Utility --- */
static GXVtxDescList s_pc_gx_draw_vcd[GX_MAX_VTXDESCLIST_SZ];
static GXVtxAttrFmtList s_pc_gx_draw_vat[GX_MAX_VTXATTRFMTLIST_SZ];

static void pc_gx_draw_get_vat_state(u32 vtxfmt, const GXVtxDescList* vcd, GXVtxAttrFmtList* out) {
    u32 n = 0;
    u32 i = 0;

    if (!vcd || !out) return;

    while (vcd[i].attr != GX_VA_NULL && n < (GX_MAX_VTXATTRFMTLIST_SZ - 1)) {
        u32 attr = (u32)vcd[i].attr;
        GXCompCnt cnt = GX_POS_XYZ;
        GXCompType type = GX_F32;
        u8 frac = 0;
        GXGetVtxAttrFmt(vtxfmt, attr, &cnt, &type, &frac);
        out[n].attr = (GXAttr)attr;
        out[n].cnt = cnt;
        out[n].type = type;
        out[n].frac = frac;
        ++n;
        ++i;
    }

    out[n].attr = GX_VA_NULL;
    out[n].cnt = 0;
    out[n].type = 0;
    out[n].frac = 0;
}

static void pc_gx_draw_get_vert_state(void) {
    GXGetVtxDescv(s_pc_gx_draw_vcd);
    pc_gx_draw_get_vat_state(GX_VTXFMT3, s_pc_gx_draw_vcd, s_pc_gx_draw_vat);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
}

static void pc_gx_draw_restore_vert_state(void) {
    GXSetVtxDescv(s_pc_gx_draw_vcd);
    GXSetVtxAttrFmtv(GX_VTXFMT3, s_pc_gx_draw_vat);
}

static void pc_gx_draw_vsub(f32* p1, f32* p2, f32* out) {
    out[0] = p2[0] - p1[0];
    out[1] = p2[1] - p1[1];
    out[2] = p2[2] - p1[2];
}

static void pc_gx_draw_vcross(f32* u, f32* v, f32* out) {
    f32 n0 = u[1] * v[2] - u[2] * v[1];
    f32 n1 = u[2] * v[0] - u[0] * v[2];
    f32 n2 = u[0] * v[1] - u[1] * v[0];
    out[0] = n0;
    out[1] = n1;
    out[2] = n2;
}

static void pc_gx_draw_normalize(f32* v) {
    f32 d = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (d <= 1e-12f) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 1.0f;
        return;
    }
    v[0] /= d;
    v[1] /= d;
    v[2] /= d;
}

static void pc_gx_draw_vertex(f32* v, f32* n) {
    GXPosition3f32(v[0], v[1], v[2]);
    GXNormal3f32(n[0], n[1], n[2]);
}

static void pc_gx_draw_dump_triangle(f32* v0, f32* v1, f32* v2) {
    GXBegin(GX_TRIANGLES, GX_VTXFMT3, 3);
    pc_gx_draw_vertex(v0, v0);
    pc_gx_draw_vertex(v1, v1);
    pc_gx_draw_vertex(v2, v2);
    GXEnd();
}

static void pc_gx_draw_subdivide(u8 depth, f32* v0, f32* v1, f32* v2) {
    f32 v01[3], v12[3], v20[3];
    u32 i;

    if (depth == 0) {
        pc_gx_draw_dump_triangle(v0, v1, v2);
        return;
    }

    for (i = 0; i < 3; ++i) {
        v01[i] = v0[i] + v1[i];
        v12[i] = v1[i] + v2[i];
        v20[i] = v2[i] + v0[i];
    }

    pc_gx_draw_normalize(v01);
    pc_gx_draw_normalize(v12);
    pc_gx_draw_normalize(v20);

    pc_gx_draw_subdivide((u8)(depth - 1), v0, v01, v20);
    pc_gx_draw_subdivide((u8)(depth - 1), v1, v12, v01);
    pc_gx_draw_subdivide((u8)(depth - 1), v2, v20, v12);
    pc_gx_draw_subdivide((u8)(depth - 1), v01, v12, v20);
}

static void pc_gx_draw_subdiv_triangle(u8 depth, u8 i, f32 data[][3], u8 ndx[][3]) {
    f32* x0 = data[ndx[i][0]];
    f32* x1 = data[ndx[i][1]];
    f32* x2 = data[ndx[i][2]];
    pc_gx_draw_subdivide(depth, x0, x1, x2);
}

void GXDrawCylinder(u8 numEdges) {
    s32 i;
    f32 top = 1.0f;
    f32 bottom = -top;
    f32 x[100];
    f32 y[100];
    f32 angle;

    if (numEdges > 99) numEdges = 99;
    if (numEdges < 3) numEdges = 3;

    pc_gx_draw_get_vert_state();

    for (i = 0; i <= (s32)numEdges; ++i) {
        angle = i * 2.0f * 3.14159265358979323846f / numEdges;
        x[i] = cosf(angle);
        y[i] = sinf(angle);
    }

    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (u16)((numEdges + 1) * 2));
    for (i = 0; i <= (s32)numEdges; ++i) {
        GXPosition3f32(x[i], y[i], bottom);
        GXNormal3f32(x[i], y[i], 0.0f);
        GXPosition3f32(x[i], y[i], top);
        GXNormal3f32(x[i], y[i], 0.0f);
    }
    GXEnd();

    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, (u16)(numEdges + 2));
    GXPosition3f32(0.0f, 0.0f, top);
    GXNormal3f32(0.0f, 0.0f, 1.0f);
    for (i = 0; i <= (s32)numEdges; ++i) {
        GXPosition3f32(x[i], -y[i], top);
        GXNormal3f32(0.0f, 0.0f, 1.0f);
    }
    GXEnd();

    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, (u16)(numEdges + 2));
    GXPosition3f32(0.0f, 0.0f, bottom);
    GXNormal3f32(0.0f, 0.0f, -1.0f);
    for (i = 0; i <= (s32)numEdges; ++i) {
        GXPosition3f32(x[i], y[i], bottom);
        GXNormal3f32(0.0f, 0.0f, -1.0f);
    }
    GXEnd();

    pc_gx_draw_restore_vert_state();
}

void GXDrawTorus(f32 rc, u8 numc, u8 numt) {
    GXAttrType ttype = GX_NONE;
    s32 i, j, k;
    f32 s, t;
    f32 x, y, z;
    f32 twopi = 2.0f * 3.14159265358979323846f;
    f32 rt;

    if (rc >= 1.0f) rc = 0.999f;
    if (rc <= 0.0f) rc = 0.25f;
    if (numc < 3) numc = 3;
    if (numt < 3) numt = 3;

    rt = 1.0f - rc;
    GXGetVtxDesc(GX_VA_TEX0, &ttype);
    pc_gx_draw_get_vert_state();
    if (ttype != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    }

    for (i = 0; i < (s32)numc; ++i) {
        GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (u16)((numt + 1) * 2));
        for (j = 0; j <= (s32)numt; ++j) {
            for (k = 1; k >= 0; --k) {
                s = (f32)((i + k) % numc);
                t = (f32)(j % numt);

                x = (rt - rc * cosf(s * twopi / numc)) * cosf(t * twopi / numt);
                y = (rt - rc * cosf(s * twopi / numc)) * sinf(t * twopi / numt);
                z = rc * sinf(s * twopi / numc);
                GXPosition3f32(x, y, z);

                x = -cosf(t * twopi / numt) * cosf(s * twopi / numc);
                y = -sinf(t * twopi / numt) * cosf(s * twopi / numc);
                z = sinf(s * twopi / numc);
                GXNormal3f32(x, y, z);

                if (ttype != GX_NONE) {
                    GXTexCoord2f32((f32)(i + k) / numc, (f32)j / numt);
                }
            }
        }
        GXEnd();
    }

    pc_gx_draw_restore_vert_state();
}

void GXDrawSphere(u8 numMajor, u8 numMinor) {
    GXAttrType ttype = GX_NONE;
    f32 radius = 1.0f;
    f32 majorStep;
    f32 minorStep;
    s32 i, j;

    if (numMajor < 2) numMajor = 2;
    if (numMinor < 3) numMinor = 3;
    majorStep = (3.14159265358979323846f / numMajor);
    minorStep = (2.0f * 3.14159265358979323846f / numMinor);

    GXGetVtxDesc(GX_VA_TEX0, &ttype);
    pc_gx_draw_get_vert_state();
    if (ttype != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    }

    for (i = 0; i < (s32)numMajor; ++i) {
        f32 a = i * majorStep;
        f32 b = a + majorStep;
        f32 r0 = radius * sinf(a);
        f32 r1 = radius * sinf(b);
        f32 z0 = radius * cosf(a);
        f32 z1 = radius * cosf(b);

        GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (u16)((numMinor + 1) * 2));
        for (j = 0; j <= (s32)numMinor; ++j) {
            f32 c = j * minorStep;
            f32 x = cosf(c);
            f32 y = sinf(c);

            GXPosition3f32(x * r1, y * r1, z1);
            GXNormal3f32((x * r1) / radius, (y * r1) / radius, z1 / radius);
            if (ttype != GX_NONE) {
                GXTexCoord2f32(j / (f32)numMinor, (i + 1) / (f32)numMajor);
            }

            GXPosition3f32(x * r0, y * r0, z0);
            GXNormal3f32((x * r0) / radius, (y * r0) / radius, z0 / radius);
            if (ttype != GX_NONE) {
                GXTexCoord2f32(j / (f32)numMinor, (i + 0) / (f32)numMajor);
            }
        }
        GXEnd();
    }

    pc_gx_draw_restore_vert_state();
}

static void pc_gx_draw_cube_face(f32 nx, f32 ny, f32 nz,
                                 f32 tx, f32 ty, f32 tz,
                                 f32 bx, f32 by, f32 bz,
                                 u32 binormal, u32 texture) {
    const f32 sz = 1.0f / 1.732050808f;

    GXPosition3f32((nx + tx + bx) * sz, (ny + ty + by) * sz, (nz + tz + bz) * sz);
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(1, 1);
    }

    GXPosition3f32((nx - tx + bx) * sz, (ny - ty + by) * sz, (nz - tz + bz) * sz);
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(0, 1);
    }

    GXPosition3f32((nx - tx - bx) * sz, (ny - ty - by) * sz, (nz - tz - bz) * sz);
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(0, 0);
    }

    GXPosition3f32((nx + tx - bx) * sz, (ny + ty - by) * sz, (nz + tz - bz) * sz);
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(1, 0);
    }
}

void GXDrawCube(void) {
    GXAttrType ntype = GX_NONE;
    GXAttrType ttype = GX_NONE;

    GXGetVtxDesc(GX_VA_NBT, &ntype);
    GXGetVtxDesc(GX_VA_TEX0, &ttype);
    pc_gx_draw_get_vert_state();

    if (ntype != GX_NONE) {
        GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NBT, GX_NRM_NBT, GX_F32, 0);
    }
    if (ttype != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
    }

    GXBegin(GX_QUADS, GX_VTXFMT3, 24);
    pc_gx_draw_cube_face(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, ntype, ttype);
    pc_gx_draw_cube_face( 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 0.0f, -1.0f, ntype, ttype);
    pc_gx_draw_cube_face( 0.0f,-1.0f, 0.0f,-1.0f, 0.0f,  0.0f, 0.0f, 0.0f,  1.0f, ntype, ttype);
    pc_gx_draw_cube_face( 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,  1.0f,-1.0f, 0.0f,  0.0f, ntype, ttype);
    pc_gx_draw_cube_face( 0.0f, 0.0f,-1.0f, 0.0f,-1.0f,  0.0f, 1.0f, 0.0f,  0.0f, ntype, ttype);
    pc_gx_draw_cube_face( 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  0.0f, 0.0f,-1.0f,  0.0f, ntype, ttype);
    GXEnd();

    pc_gx_draw_restore_vert_state();
}

static f32 s_pc_gx_odata[6][3] = {
    { 1.0f,  0.0f,  0.0f },
    { -1.0f, 0.0f,  0.0f },
    { 0.0f,  1.0f,  0.0f },
    { 0.0f, -1.0f,  0.0f },
    { 0.0f,  0.0f,  1.0f },
    { 0.0f,  0.0f, -1.0f },
};
static u8 s_pc_gx_ondex[8][3] = {
    {0,4,2}, {1,2,4}, {0,3,4}, {1,4,3},
    {0,2,5}, {1,5,2}, {0,5,3}, {1,3,5},
};

void GXDrawOctahedron(void) {
    s32 i;
    pc_gx_draw_get_vert_state();
    for (i = 7; i >= 0; --i) {
        pc_gx_draw_subdiv_triangle(0, (u8)i, s_pc_gx_odata, s_pc_gx_ondex);
    }
    pc_gx_draw_restore_vert_state();
}

static f32 s_pc_gx_idata[12][3] = {
    {-0.525731112119133606f, 0.0f,  0.850650808352039932f},
    { 0.525731112119133606f, 0.0f,  0.850650808352039932f},
    {-0.525731112119133606f, 0.0f, -0.850650808352039932f},
    { 0.525731112119133606f, 0.0f, -0.850650808352039932f},
    { 0.0f,  0.850650808352039932f,  0.525731112119133606f},
    { 0.0f,  0.850650808352039932f, -0.525731112119133606f},
    { 0.0f, -0.850650808352039932f,  0.525731112119133606f},
    { 0.0f, -0.850650808352039932f, -0.525731112119133606f},
    { 0.850650808352039932f,  0.525731112119133606f, 0.0f},
    {-0.850650808352039932f,  0.525731112119133606f, 0.0f},
    { 0.850650808352039932f, -0.525731112119133606f, 0.0f},
    {-0.850650808352039932f, -0.525731112119133606f, 0.0f},
};
static u8 s_pc_gx_index[20][3] = {
    {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
    {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
    {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
    {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11},
};

void GXDrawIcosahedron(void) {
    s32 i;
    pc_gx_draw_get_vert_state();
    for (i = 19; i >= 0; --i) {
        pc_gx_draw_subdiv_triangle(0, (u8)i, s_pc_gx_idata, s_pc_gx_index);
    }
    pc_gx_draw_restore_vert_state();
}

void GXDrawSphere1(u8 depth) {
    s32 i;
    pc_gx_draw_get_vert_state();
    for (i = 19; i >= 0; --i) {
        pc_gx_draw_subdiv_triangle(depth, (u8)i, s_pc_gx_idata, s_pc_gx_index);
    }
    pc_gx_draw_restore_vert_state();
}

static u32 s_pc_gx_dodeca_polygons[12][5] = {
    {0, 12, 10, 11, 16},
    {1, 17, 8, 9, 13},
    {2, 14, 9, 8, 18},
    {3, 19, 11, 10, 15},
    {4, 14, 2, 3, 15},
    {5, 12, 0, 1, 13},
    {6, 17, 1, 0, 16},
    {7, 19, 3, 2, 18},
    {8, 17, 6, 7, 18},
    {9, 14, 4, 5, 13},
    {10, 12, 5, 4, 15},
    {11, 19, 7, 6, 16},
};

static f32 s_pc_gx_dodeca_verts[20][3] = {
    {-0.809015f, 0.0f,      0.309015f},
    {-0.809015f, 0.0f,     -0.309015f},
    { 0.809015f, 0.0f,     -0.309015f},
    { 0.809015f, 0.0f,      0.309015f},
    { 0.309015f,-0.809015f, 0.0f},
    {-0.309015f,-0.809015f, 0.0f},
    {-0.309015f, 0.809015f, 0.0f},
    { 0.309015f, 0.809015f, 0.0f},
    { 0.0f,      0.309015f,-0.809015f},
    { 0.0f,     -0.309015f,-0.809015f},
    { 0.0f,     -0.309015f, 0.809015f},
    { 0.0f,      0.309015f, 0.809015f},
    {-0.5f,     -0.5f,      0.5f},
    {-0.5f,     -0.5f,     -0.5f},
    { 0.5f,     -0.5f,     -0.5f},
    { 0.5f,     -0.5f,      0.5f},
    {-0.5f,      0.5f,      0.5f},
    {-0.5f,      0.5f,     -0.5f},
    { 0.5f,      0.5f,     -0.5f},
    { 0.5f,      0.5f,      0.5f},
};

void GXDrawDodeca(void) {
    u32 i;
    pc_gx_draw_get_vert_state();

    for (i = 0; i < 12; ++i) {
        f32 *p0, *p1, *p2;
        f32 u[3], v[3], n[3];
        p0 = &s_pc_gx_dodeca_verts[s_pc_gx_dodeca_polygons[i][0]][0];
        p1 = &s_pc_gx_dodeca_verts[s_pc_gx_dodeca_polygons[i][1]][0];
        p2 = &s_pc_gx_dodeca_verts[s_pc_gx_dodeca_polygons[i][2]][0];

        pc_gx_draw_vsub(p1, p2, u);
        pc_gx_draw_vsub(p1, p0, v);
        pc_gx_draw_vcross(u, v, n);
        pc_gx_draw_normalize(n);

        GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, 5);
        pc_gx_draw_vertex(s_pc_gx_dodeca_verts[s_pc_gx_dodeca_polygons[i][4]], n);
        pc_gx_draw_vertex(s_pc_gx_dodeca_verts[s_pc_gx_dodeca_polygons[i][3]], n);
        pc_gx_draw_vertex(p2, n);
        pc_gx_draw_vertex(p1, n);
        pc_gx_draw_vertex(p0, n);
        GXEnd();
    }

    pc_gx_draw_restore_vert_state();
}

/* --- Perf --- */
static GXPerf0 s_pc_gx_perf0_metric = GX_PERF0_NONE;
static GXPerf1 s_pc_gx_perf1_metric = GX_PERF1_NONE;
static GXVCachePerf s_pc_gx_vcache_metric = GX_VC_ALL;

static u32 s_pc_gx_gp_counter0 = 0;
static u32 s_pc_gx_gp_counter1 = 0;
static u32 s_pc_gx_vcache_check = 0;
static u32 s_pc_gx_vcache_miss = 0;
static u32 s_pc_gx_vcache_stall = 0;
static u32 s_pc_gx_mem_metric[10] = {0};
static u32 s_pc_gx_pix_metric[6] = {0};

void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1) {
    /*
     * Hardware counters are not available on PC backend.
     * Keep API-compatible state so Dolphin SDK callers compile/run.
     */
    s_pc_gx_perf0_metric = perf0;
    s_pc_gx_perf1_metric = perf1;
    s_pc_gx_gp_counter0 = 0;
    s_pc_gx_gp_counter1 = 0;
}

void GXClearGPMetric(void) {
    s_pc_gx_gp_counter0 = 0;
    s_pc_gx_gp_counter1 = 0;
}

void GXReadGPMetric(u32* cnt0, u32* cnt1) {
    if (cnt0) *cnt0 = s_pc_gx_gp_counter0;
    if (cnt1) *cnt1 = s_pc_gx_gp_counter1;
}

u32 GXReadGP0Metric(void) { return s_pc_gx_gp_counter0; }

u32 GXReadGP1Metric(void) { return s_pc_gx_gp_counter1; }

void GXReadMemMetric(
    u32* cp_req, u32* tc_req, u32* cpu_rd_req, u32* cpu_wr_req, u32* dsp_req, u32* io_req,
    u32* vi_req, u32* pe_req, u32* rf_req, u32* fi_req) {
    if (cp_req) *cp_req = s_pc_gx_mem_metric[0];
    if (tc_req) *tc_req = s_pc_gx_mem_metric[1];
    if (cpu_rd_req) *cpu_rd_req = s_pc_gx_mem_metric[2];
    if (cpu_wr_req) *cpu_wr_req = s_pc_gx_mem_metric[3];
    if (dsp_req) *dsp_req = s_pc_gx_mem_metric[4];
    if (io_req) *io_req = s_pc_gx_mem_metric[5];
    if (vi_req) *vi_req = s_pc_gx_mem_metric[6];
    if (pe_req) *pe_req = s_pc_gx_mem_metric[7];
    if (rf_req) *rf_req = s_pc_gx_mem_metric[8];
    if (fi_req) *fi_req = s_pc_gx_mem_metric[9];
}

void GXClearMemMetric(void) { memset(s_pc_gx_mem_metric, 0, sizeof(s_pc_gx_mem_metric)); }

void GXReadPixMetric(
    u32* top_pixels_in, u32* top_pixels_out, u32* bot_pixels_in, u32* bot_pixels_out,
    u32* clr_pixels_in, u32* copy_clks) {
    if (top_pixels_in) *top_pixels_in = s_pc_gx_pix_metric[0];
    if (top_pixels_out) *top_pixels_out = s_pc_gx_pix_metric[1];
    if (bot_pixels_in) *bot_pixels_in = s_pc_gx_pix_metric[2];
    if (bot_pixels_out) *bot_pixels_out = s_pc_gx_pix_metric[3];
    if (clr_pixels_in) *clr_pixels_in = s_pc_gx_pix_metric[4];
    if (copy_clks) *copy_clks = s_pc_gx_pix_metric[5];
}

void GXClearPixMetric(void) { memset(s_pc_gx_pix_metric, 0, sizeof(s_pc_gx_pix_metric)); }

void GXSetVCacheMetric(GXVCachePerf attr) {
    s_pc_gx_vcache_metric = attr;
    (void)s_pc_gx_vcache_metric;
}

void GXReadVCacheMetric(u32* check, u32* miss, u32* stall) {
    if (check) *check = s_pc_gx_vcache_check;
    if (miss) *miss = s_pc_gx_vcache_miss;
    if (stall) *stall = s_pc_gx_vcache_stall;
}

void GXClearVCacheMetric(void) {
    s_pc_gx_vcache_check = 0;
    s_pc_gx_vcache_miss = 0;
    s_pc_gx_vcache_stall = 0;
}

void GXReadXfRasMetric(u32* xf_wait_in, u32* xf_wait_out, u32* ras_busy, u32* clocks) {
    (void)s_pc_gx_perf0_metric;
    (void)s_pc_gx_perf1_metric;
    if (xf_wait_in) *xf_wait_in = 0;
    if (xf_wait_out) *xf_wait_out = 0;
    if (ras_busy) *ras_busy = 0;
    if (clocks) *clocks = 0;
}

/* --- Verify --- */
void GXSetVerifyLevel(GXVerifyLevel level) { g_verify_level = level; }
GXVerifyCallback GXSetVerifyCallback(GXVerifyCallback cb) {
    GXVerifyCallback prev = g_verify_cb;
    g_verify_cb = cb;
    return prev;
}

void GXResetStreamState(void) {
    g_gx.in_begin = 0;
    g_gx.vertex_pending = 0;
    g_gx.current_vertex_idx = 0;
    g_gx.matrix_index_write_phase = 0;
    memset(&g_gx.current_vertex, 0, sizeof(g_gx.current_vertex));
    g_gx.current_vertex.pn_mtx_idx = (float)g_gx.current_mtx;
    for (int i = 0; i < 8; ++i) {
        g_gx.current_vertex.tex_mtx_idx[i] = (float)g_gx.current_tex_mtx_idx[i];
    }
}


