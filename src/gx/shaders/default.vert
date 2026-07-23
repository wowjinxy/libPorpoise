#version 330 core
#define PC_GX_MAX_TEXGENS 8

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color0;
layout(location = 12) in vec4 a_color1;
layout(location = 3) in vec2 a_texcoord0;
layout(location = 5) in vec2 a_texcoord1;
layout(location = 6) in vec2 a_texcoord2;
layout(location = 7) in vec2 a_texcoord3;
layout(location = 8) in vec2 a_texcoord4;
layout(location = 9) in vec2 a_texcoord5;
layout(location = 10) in vec2 a_texcoord6;
layout(location = 11) in vec2 a_texcoord7;
layout(location = 4) in float a_pn_mtx_idx;
layout(location = 13) in vec4 a_tex_mtx_idx0123;
layout(location = 14) in vec4 a_tex_mtx_idx4567;
layout(location = 15) in vec3 a_binormal;

uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat3 u_normal_mtx;
uniform mat4 u_modelview_arr[10];
uniform mat3 u_normal_mtx_arr[10];
uniform int u_use_matrix_array;
uniform vec4 u_texmtx_row0[PC_GX_MAX_TEXGENS];
uniform vec4 u_texmtx_row1[PC_GX_MAX_TEXGENS];
uniform vec4 u_texmtx_row2[PC_GX_MAX_TEXGENS];
uniform int u_texmtx_enable[PC_GX_MAX_TEXGENS];
uniform int u_texmtxidx_enable[PC_GX_MAX_TEXGENS];
uniform vec4 u_texmtx_all_row0[10];
uniform vec4 u_texmtx_all_row1[10];
uniform vec4 u_texmtx_all_row2[10];
uniform vec4 u_postmtx_row0[PC_GX_MAX_TEXGENS];
uniform vec4 u_postmtx_row1[PC_GX_MAX_TEXGENS];
uniform vec4 u_postmtx_row2[PC_GX_MAX_TEXGENS];
uniform int u_postmtx_enable[PC_GX_MAX_TEXGENS];
uniform int u_texgen_src[PC_GX_MAX_TEXGENS];
uniform int u_texgen_type[PC_GX_MAX_TEXGENS];
uniform int u_texgen_normalize[PC_GX_MAX_TEXGENS];
uniform int u_texgen_qt_notcalc[PC_GX_MAX_TEXGENS];
uniform int u_lighting_enabled;
uniform vec4 u_mat_color;
uniform vec4 u_amb_color;
uniform int u_chan_mat_src;
uniform int u_chan_amb_src;
uniform int u_chan_diff_fn;
uniform int u_chan_attn_fn;
uniform int u_lighting_enabled1;
uniform vec4 u_mat_color1;
uniform vec4 u_amb_color1;
uniform int u_chan_mat_src1;
uniform int u_chan_amb_src1;
uniform int u_chan_diff_fn1;
uniform int u_chan_attn_fn1;
uniform int u_num_chans;
uniform int u_light_mask;
uniform int u_light_mask1;
uniform vec3 u_light_pos[8];
uniform vec3 u_light_dir[8];
uniform vec4 u_light_color[8];
uniform vec3 u_light_cos_att[8];
uniform vec3 u_light_dist_att[8];

out vec4 v_color0;
out vec4 v_color1;
out vec2 v_texcoord0;
out vec2 v_texcoord1;
out vec2 v_texcoord2;
out vec2 v_texcoord3;
out vec2 v_texcoord4;
out vec2 v_texcoord5;
out vec2 v_texcoord6;
out vec2 v_texcoord7;
out vec3 v_normal;
out vec3 v_eye_pos;
out float v_fog_z;

vec4 texgenInput(int src, vec3 pos, vec3 nrm, vec3 binrm, vec3 tangent,
                 vec3 tex[PC_GX_MAX_TEXGENS], vec3 gen[PC_GX_MAX_TEXGENS],
                 vec4 color0, vec4 color1) {
    if (src == 0) return vec4(pos, 1.0);             /* GX_TG_POS */
    if (src == 1) return vec4(nrm, 1.0);             /* GX_TG_NRM */
    if (src == 2) return vec4(binrm, 1.0);           /* GX_TG_BINRM */
    if (src == 3) return vec4(tangent, 1.0);         /* GX_TG_TANGENT */
    if (src >= 4 && src <= 11) return vec4(tex[src - 4], 1.0);        /* GX_TG_TEX0..7 */
    if (src >= 12 && src <= 18) return vec4(gen[src - 12], 1.0);      /* GX_TG_TEXCOORD0..6 */
    if (src == 19) return vec4(color0.rg, 0.0, 1.0);                  /* GX_TG_COLOR0 */
    if (src == 20) return vec4(color1.rg, 0.0, 1.0);                  /* GX_TG_COLOR1 */
    return vec4(tex[0], 1.0);
}

vec3 evalChanRGBVertex(vec3 matC, vec3 ambC, int lightingEnabled, int lightMask, int diffFn, int attnFn, vec3 N, vec3 eyePos) {
    if (lightingEnabled == 0) {
        return matC;
    }

    vec3 lightAccum = ambC;
    for (int i = 0; i < 8; i++) {
        if ((lightMask & (1 << i)) != 0) {
            vec3 Lvec = u_light_pos[i] - eyePos;
            float L2 = dot(Lvec, Lvec);
            float invDist = (L2 > 1e-8) ? inversesqrt(L2) : 0.0;
            float dist = (L2 > 1e-8) ? (L2 * invDist) : 0.0;
            vec3 L = (L2 > 1e-8) ? (Lvec * invDist) : vec3(0.0, 0.0, 1.0);

            float ndotl = dot(N, L);
            float diff;
            if (diffFn == 0) {
                diff = 1.0;
            } else if (diffFn == 1) {
                diff = ndotl;
            } else {
                diff = max(ndotl, 0.0);
            }

            float attn = 1.0;
            if (attnFn != 2) {
                float cosTheta;
                float den;
                vec3 D = u_light_dir[i];
                float D2 = dot(D, D);
                D = (D2 > 1e-8) ? (D * inversesqrt(D2)) : vec3(0.0, 0.0, -1.0);

                if (attnFn == 0) {
                    float ndotl_clamped = max(ndotl, 0.0);
                    if (ndotl_clamped <= 0.0) {
                        attn = 0.0;
                    } else {
                        cosTheta = max(dot(N, D), 0.0);
                        den = u_light_dist_att[i].x +
                              u_light_dist_att[i].y * ndotl_clamped +
                              u_light_dist_att[i].z * ndotl_clamped * ndotl_clamped;

                        float ang = u_light_cos_att[i].x +
                                    u_light_cos_att[i].y * cosTheta +
                                    u_light_cos_att[i].z * cosTheta * cosTheta;
                        if (ang < 0.0) ang = 0.0;
                        attn = (den > 1e-8) ? (ang / den) : 0.0;
                    }
                } else {
                    cosTheta = dot(L, D);
                    den = u_light_dist_att[i].x +
                          u_light_dist_att[i].y * dist +
                          u_light_dist_att[i].z * dist * dist;

                    float ang = u_light_cos_att[i].x +
                                u_light_cos_att[i].y * cosTheta +
                                u_light_cos_att[i].z * cosTheta * cosTheta;
                    if (ang < 0.0) ang = 0.0;
                    attn = (den > 1e-8) ? (ang / den) : 0.0;
                }
            }

            lightAccum += (diff * attn) * u_light_color[i].rgb;
        }
    }

    return clamp(matC * lightAccum, 0.0, 1.0);
}

int texMtxSlotForTexGen(int idx) {
    float v = (idx < 4) ? a_tex_mtx_idx0123[idx] : a_tex_mtx_idx4567[idx - 4];
    return clamp(int(v + 0.5), 0, 9);
}

vec3 applyTexGen(int idx, vec3 pos, vec3 eyePos, vec3 eyeNrm, vec3 eyeBinrm, vec3 eyeTangent,
                 vec3 tex[PC_GX_MAX_TEXGENS], vec3 gen[PC_GX_MAX_TEXGENS],
                 vec4 rasColor0, vec4 rasColor1) {
    int func = u_texgen_type[idx];
    bool regular = (func == 0 || func == 1); /* GX_TG_MTX3x4 or GX_TG_MTX2x4 */
    vec4 tc = texgenInput(u_texgen_src[idx], pos, eyeNrm, eyeBinrm, eyeTangent, tex, gen, rasColor0, rasColor1);

    /* GX_TG_SRTG uses rasterized channel color to produce s/t. */
    if (func == 10) {
        vec2 st = (u_texgen_src[idx] == 20) ? rasColor1.rg : rasColor0.rg;
        /*
         * GX_TG_SRTG pulls from 8-bit raster color channels; quantize to
         * hardware-like 0..255 steps before converting back to 0..1.
         */
        st = floor(clamp(st, 0.0, 1.0) * 255.0 + 0.5) / 255.0;
        return vec3(st, 1.0);
    }

    /*
     * GX_TG_BUMP0..7 emboss mapping:
     * ldir = normalize(lightPos - eyePos)
     * d1 = dot(ldir, binormal)
     * d2 = dot(ldir, tangent)
     * dst = src + (d1,d2), preserve source q
     */
    if (func >= 2 && func <= 9) {
        int lightId = clamp(func - 2, 0, 7);
        vec3 Lvec = u_light_pos[lightId] - eyePos;
        float L2 = dot(Lvec, Lvec);
        vec3 ldir = (L2 > 1e-8) ? (Lvec * inversesqrt(L2)) : vec3(0.0, 0.0, 1.0);
        float d1 = dot(ldir, eyeBinrm);
        float d2 = dot(ldir, eyeTangent);
        /* Bump texgen projects to ST; keep q at 1.0 on the generated result. */
        return vec3(tc.x + d1, tc.y + d2, 1.0);
    }

    vec3 outStq = vec3(tc.xy, (func == 0) ? tc.z : 1.0);
    if (u_texmtxidx_enable[idx] != 0) {
        int mslot = texMtxSlotForTexGen(idx);
        outStq.x = dot(u_texmtx_all_row0[mslot], tc);
        outStq.y = dot(u_texmtx_all_row1[mslot], tc);
        outStq.z = (func == 0) ? dot(u_texmtx_all_row2[mslot], tc) : 1.0;
    } else if (u_texmtx_enable[idx] != 0) {
        outStq.x = dot(u_texmtx_row0[idx], tc);
        outStq.y = dot(u_texmtx_row1[idx], tc);
        outStq.z = (func == 0) ? dot(u_texmtx_row2[idx], tc) : 1.0;
    }

    if (u_texgen_normalize[idx] != 0 && regular) {
        float lenTc = length(outStq);
        if (lenTc > 1e-8) {
            outStq /= lenTc;
        }
    }

    if (regular && u_postmtx_enable[idx] != 0) {
        vec4 p = vec4(outStq, 1.0);
        float ps = dot(u_postmtx_row0[idx], p);
        float pt = dot(u_postmtx_row1[idx], p);
        if (func == 1 && u_texgen_qt_notcalc[idx] != 0) {
            /*
             * HW2 special case (docs): for MTX2x4 with position+single-texcoord
             * fast path, Qt_n is assumed 1.0 and Qt'_n is not calculated.
             */
            outStq = vec3(ps, pt, 1.0);
        } else {
            outStq = vec3(ps, pt, dot(u_postmtx_row2[idx], p));
        }
    }

    return outStq;
}

void main() {
    mat4 mv = u_modelview;
    mat3 nm = u_normal_mtx;
    if (u_use_matrix_array != 0) {
        int midx = clamp(int(a_pn_mtx_idx + 0.5), 0, 9);
        mv = u_modelview_arr[midx];
        nm = u_normal_mtx_arr[midx];
    }

    vec4 eyePos = mv * vec4(a_position, 1.0);
    gl_Position = u_projection * eyePos;
    /*
     * GX projection matrices produce clip-space Z meant for the GX viewport
     * transform path. Remap to OpenGL clip-space depth so near/far consume the
     * full GL depth range instead of the lower half, which improves Z precision
     * and fixes depth ordering artifacts.
     */
    gl_Position.z = (2.0 * gl_Position.z) + gl_Position.w;
    v_eye_pos = eyePos.xyz;
    v_fog_z = -eyePos.z;
    v_color0 = a_color0;
    v_color1 = a_color1;
    v_normal = normalize(nm * a_normal);
    vec3 eyeNrm = v_normal;
    vec3 eyeBinrm = normalize(nm * a_binormal);
    vec3 eyeTangent = normalize(cross(eyeBinrm, eyeNrm));

    vec3 srcTex[PC_GX_MAX_TEXGENS];
    /*
     * GX regular texgens with GX_TG_TEXn inputs use AB01 form in XF:
     * (A,B,C,1) with C forced to 0.0 for incoming texcoords.
     */
    srcTex[0] = vec3(a_texcoord0, 0.0);
    srcTex[1] = vec3(a_texcoord1, 0.0);
    srcTex[2] = vec3(a_texcoord2, 0.0);
    srcTex[3] = vec3(a_texcoord3, 0.0);
    srcTex[4] = vec3(a_texcoord4, 0.0);
    srcTex[5] = vec3(a_texcoord5, 0.0);
    srcTex[6] = vec3(a_texcoord6, 0.0);
    srcTex[7] = vec3(a_texcoord7, 0.0);

    vec3 gen[PC_GX_MAX_TEXGENS];
    for (int i = 0; i < PC_GX_MAX_TEXGENS; ++i) {
        gen[i] = vec3(0.0, 0.0, 1.0);
    }

    vec4 rasColor0 = vec4(1.0);
    vec4 rasColor1 = vec4(1.0);
    if (u_num_chans != 0) {
        vec3 matC = (u_chan_mat_src != 0) ? a_color0.rgb : u_mat_color.rgb;
        vec3 ambC = (u_chan_amb_src != 0) ? a_color0.rgb : u_amb_color.rgb;
        rasColor0.rgb = evalChanRGBVertex(matC, ambC, u_lighting_enabled, u_light_mask, u_chan_diff_fn, u_chan_attn_fn, eyeNrm, eyePos.xyz);
        rasColor0.a = a_color0.a;

        vec3 matC1 = (u_chan_mat_src1 != 0) ? a_color1.rgb : u_mat_color1.rgb;
        vec3 ambC1 = (u_chan_amb_src1 != 0) ? a_color1.rgb : u_amb_color1.rgb;
        rasColor1.rgb = evalChanRGBVertex(matC1, ambC1, u_lighting_enabled1, u_light_mask1, u_chan_diff_fn1, u_chan_attn_fn1, eyeNrm, eyePos.xyz);
        rasColor1.a = a_color1.a;
    }

    for (int i = 0; i < PC_GX_MAX_TEXGENS; ++i) {
        gen[i] = applyTexGen(i, a_position, eyePos.xyz, eyeNrm, eyeBinrm, eyeTangent, srcTex, gen, rasColor0, rasColor1);
    }

    v_texcoord0 = gen[0].xy;
    v_texcoord1 = gen[1].xy;
    v_texcoord2 = gen[2].xy;
    v_texcoord3 = gen[3].xy;
    v_texcoord4 = gen[4].xy;
    v_texcoord5 = gen[5].xy;
    v_texcoord6 = gen[6].xy;
    v_texcoord7 = gen[7].xy;
}
