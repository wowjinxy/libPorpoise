#version 330 core
#define PC_GX_MAX_TEV_STAGES 16
#define PC_GX_MAX_TEXCOORDS 8
#define PC_GX_MAX_IND_STAGES 4

in vec4 v_color0;
in vec4 v_color1;
in vec2 v_texcoord0;
in vec2 v_texcoord1;
in vec2 v_texcoord2;
in vec2 v_texcoord3;
in vec2 v_texcoord4;
in vec2 v_texcoord5;
in vec2 v_texcoord6;
in vec2 v_texcoord7;
in vec3 v_normal;
in vec3 v_eye_pos;
in float v_fog_z;

uniform int u_fog_type;
uniform float u_fog_start;
uniform float u_fog_end;
uniform vec4 u_fog_color;
uniform int u_ztex_enable;
uniform int u_ztex_op;
uniform int u_ztex_fmt; /* 0=Z8, 1=Z16, 2=Z24X8 */
uniform float u_ztex_bias;
uniform int u_ztex_bias24;

uniform vec4 u_tev_prev;
uniform vec4 u_tev_reg0;
uniform vec4 u_tev_reg1;
uniform vec4 u_tev_reg2;

uniform ivec4 u_tev_color_in[PC_GX_MAX_TEV_STAGES];
uniform ivec4 u_tev_alpha_in[PC_GX_MAX_TEV_STAGES];
uniform int u_tev_color_op[PC_GX_MAX_TEV_STAGES];
uniform int u_tev_alpha_op[PC_GX_MAX_TEV_STAGES];
uniform int u_tev_color_chan[PC_GX_MAX_TEV_STAGES];
uniform int u_num_tev_stages;
uniform sampler2D u_texture[PC_GX_MAX_TEV_STAGES];
uniform int u_use_texture[PC_GX_MAX_TEV_STAGES];
uniform int u_tev_tc_src[PC_GX_MAX_TEV_STAGES];
uniform ivec2 u_tcs_cyl_wrap_enable[PC_GX_MAX_TEXCOORDS];
uniform int u_tcs_manual_enable[PC_GX_MAX_TEXCOORDS];
uniform vec2 u_tcs_manual_scale[PC_GX_MAX_TEXCOORDS];

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
uniform int u_alpha_lighting_enabled;
uniform int u_alpha_mat_src;
uniform int u_alpha_lighting_enabled1;
uniform int u_alpha_mat_src1;
uniform int u_light_mask;
uniform int u_light_mask1;
uniform vec3 u_light_pos[8];
uniform vec3 u_light_dir[8];
uniform vec4 u_light_color[8];
uniform vec3 u_light_cos_att[8];
uniform vec3 u_light_dist_att[8];

uniform vec4 u_kcolor[4];
uniform ivec3 u_tev_ksel[PC_GX_MAX_TEV_STAGES];

uniform ivec4 u_tev_bsc[PC_GX_MAX_TEV_STAGES];
uniform ivec4 u_tev_out[PC_GX_MAX_TEV_STAGES];
uniform int u_tev_clamp_mode[PC_GX_MAX_TEV_STAGES];
uniform ivec4 u_swap_table[4];
uniform ivec2 u_tev_swap[PC_GX_MAX_TEV_STAGES];

uniform int u_alpha_comp0;
uniform int u_alpha_ref0;
uniform int u_alpha_op;
uniform int u_alpha_comp1;
uniform int u_alpha_ref1;

uniform int u_num_ind_stages;
uniform sampler2D u_ind_tex[PC_GX_MAX_IND_STAGES];
uniform int u_ind_tc_src[PC_GX_MAX_IND_STAGES];
uniform vec2 u_ind_scale[PC_GX_MAX_IND_STAGES];
uniform vec3 u_ind_mtx_r0[3];
uniform vec3 u_ind_mtx_r1[3];
uniform ivec4 u_tev_ind_cfg[PC_GX_MAX_TEV_STAGES];
uniform ivec3 u_tev_ind_wrap[PC_GX_MAX_TEV_STAGES];

out vec4 fragColor;

vec2 selectTexCoord(int src,
                    vec2 tc0, vec2 tc1, vec2 tc2, vec2 tc3,
                    vec2 tc4, vec2 tc5, vec2 tc6, vec2 tc7) {
    if (src == 1) return tc1;
    if (src == 2) return tc2;
    if (src == 3) return tc3;
    if (src == 4) return tc4;
    if (src == 5) return tc5;
    if (src == 6) return tc6;
    if (src == 7) return tc7;
    return tc0;
}

vec2 applyTexCoordCylWrap(int coord, vec2 tc) {
    vec2 out_tc = tc;
    if (coord < 0 || coord >= PC_GX_MAX_TEXCOORDS) {
        return out_tc;
    }

    /*
     * Approximate GX cylindrical-wrap seam behavior by remapping to a
     * nearest-seam interval around 0 before texture lookup.
     */
    if (u_tcs_cyl_wrap_enable[coord].x != 0) {
        out_tc.x = out_tc.x - floor(out_tc.x + 0.5);
    }
    if (u_tcs_cyl_wrap_enable[coord].y != 0) {
        out_tc.y = out_tc.y - floor(out_tc.y + 0.5);
    }
    return out_tc;
}

vec2 applyTexCoordManualScale(int coord, vec2 tc) {
    if (coord < 0 || coord >= PC_GX_MAX_TEXCOORDS) {
        return tc;
    }
    if (u_tcs_manual_enable[coord] == 0) {
        return tc;
    }

    vec2 scale = max(u_tcs_manual_scale[coord], vec2(1.0));
    return tc / scale;
}

vec3 getKonstC(int sel) {
    if (sel <= 7) {
        float v = float(8 - sel) / 8.0;
        return vec3(v);
    }
    if (sel <= 11) return vec3(0.0);
    if (sel == 12) return u_kcolor[0].rgb;
    if (sel == 13) return u_kcolor[1].rgb;
    if (sel == 14) return u_kcolor[2].rgb;
    if (sel == 15) return u_kcolor[3].rgb;
    int ki = (sel - 16) & 3;
    int ch = (sel - 16) >> 2;
    float c = (ch == 0) ? u_kcolor[ki].r :
              (ch == 1) ? u_kcolor[ki].g :
              (ch == 2) ? u_kcolor[ki].b : u_kcolor[ki].a;
    return vec3(c);
}

float getKonstA(int sel) {
    if (sel <= 7) return float(8 - sel) / 8.0;
    if (sel <= 15) return 0.0;
    int ki = (sel - 16) & 3;
    int ch = (sel - 16) >> 2;
    return (ch == 0) ? u_kcolor[ki].r :
           (ch == 1) ? u_kcolor[ki].g :
           (ch == 2) ? u_kcolor[ki].b : u_kcolor[ki].a;
}

vec3 getTevC(int id, vec4 prev, vec4 tex, vec4 ras,
             vec4 r0, vec4 r1, vec4 r2, vec3 konst) {
    if (id == 0)  return prev.rgb;
    if (id == 1)  return vec3(prev.a);
    if (id == 2)  return r0.rgb;
    if (id == 3)  return vec3(r0.a);
    if (id == 4)  return r1.rgb;
    if (id == 5)  return vec3(r1.a);
    if (id == 6)  return r2.rgb;
    if (id == 7)  return vec3(r2.a);
    if (id == 8)  return tex.rgb;
    if (id == 9)  return vec3(tex.a);
    if (id == 10) return ras.rgb;
    if (id == 11) return vec3(ras.a);
    if (id == 12) return vec3(1.0);
    if (id == 13) return vec3(0.5);
    if (id == 14) return konst;
    return vec3(0.0);
}

float getTevA(int id, float prev, float tex, float ras,
              float r0, float r1, float r2, float konst) {
    if (id == 0) return prev;
    if (id == 1) return r0;
    if (id == 2) return r1;
    if (id == 3) return r2;
    if (id == 4) return tex;
    if (id == 5) return ras;
    if (id == 6) return konst;
    return 0.0;
}

vec4 applySwap(vec4 v, ivec4 sw) {
    return vec4(v[sw.x], v[sw.y], v[sw.z], v[sw.w]);
}

vec4 tevStage(ivec4 cin, int cop, ivec4 ain, int aop,
              vec4 prev, vec4 tex, vec4 ras,
              vec4 r0, vec4 r1, vec4 r2,
              vec3 konstC, float konstA) {
    vec3 ca = getTevC(cin.x, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cb = getTevC(cin.y, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cc = getTevC(cin.z, prev, tex, ras, r0, r1, r2, konstC);
    vec3 cd = getTevC(cin.w, prev, tex, ras, r0, r1, r2, konstC);
    vec3 blend = mix(ca, cb, cc);
    vec3 cResult = (cop == 1) ? (cd - blend) : (cd + blend);

    float aa = getTevA(ain.x, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ab = getTevA(ain.y, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ac = getTevA(ain.z, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float ad = getTevA(ain.w, prev.a, tex.a, ras.a, r0.a, r1.a, r2.a, konstA);
    float aBlend = mix(aa, ab, ac);
    float aResult = (aop == 1) ? (ad - aBlend) : (ad + aBlend);

    return vec4(cResult, aResult);
}

vec4 applyBSC(vec4 v, ivec4 bsc) {
    if (bsc.x == 1) v.rgb += 0.5;
    else if (bsc.x == 2) v.rgb -= 0.5;
    if (bsc.y == 1) v.rgb *= 2.0;
    else if (bsc.y == 2) v.rgb *= 4.0;
    else if (bsc.y == 3) v.rgb *= 0.5;
    if (bsc.z == 1) v.a += 0.5;
    else if (bsc.z == 2) v.a -= 0.5;
    if (bsc.w == 1) v.a *= 2.0;
    else if (bsc.w == 2) v.a *= 4.0;
    else if (bsc.w == 3) v.a *= 0.5;
    return v;
}

vec3 applyTevClampModeRGB(vec3 v, int mode, int clamp_en) {
    if (mode == 0) {
        if (clamp_en != 0) return clamp(v, 0.0, 1.0);
        return clamp(v, vec3(-1024.0 / 255.0), vec3(1023.0 / 255.0));
    }

    vec3 pass;
    if (mode == 2) {
        pass = step(vec3(0.0), v);
    } else if (mode == 3) {
        float eps = 0.5 / 255.0;
        pass = vec3((abs(v.x) <= eps) ? 1.0 : 0.0,
                    (abs(v.y) <= eps) ? 1.0 : 0.0,
                    (abs(v.z) <= eps) ? 1.0 : 0.0);
    } else {
        pass = vec3((v.x <= 0.0) ? 1.0 : 0.0,
                    (v.y <= 0.0) ? 1.0 : 0.0,
                    (v.z <= 0.0) ? 1.0 : 0.0);
    }

    if (clamp_en != 0) pass = vec3(1.0) - pass;
    return pass;
}

float applyTevClampModeA(float v, int mode, int clamp_en) {
    if (mode == 0) {
        if (clamp_en != 0) return clamp(v, 0.0, 1.0);
        return clamp(v, -1024.0 / 255.0, 1023.0 / 255.0);
    }

    float pass;
    if (mode == 2) {
        pass = (v >= 0.0) ? 1.0 : 0.0;
    } else if (mode == 3) {
        pass = (abs(v) <= (0.5 / 255.0)) ? 1.0 : 0.0;
    } else {
        pass = (v <= 0.0) ? 1.0 : 0.0;
    }

    if (clamp_en != 0) pass = 1.0 - pass;
    return pass;
}

void writeToReg(vec4 val, ivec4 out_cfg, int clamp_mode,
                inout vec4 prev, inout vec4 r0, inout vec4 r1, inout vec4 r2) {
    vec3 rgb = applyTevClampModeRGB(val.rgb, clamp_mode, out_cfg.x);
    float a  = applyTevClampModeA(val.a, clamp_mode, out_cfg.y);
    if (out_cfg.z == 0) prev.rgb = rgb;
    else if (out_cfg.z == 1) r0.rgb = rgb;
    else if (out_cfg.z == 2) r1.rgb = rgb;
    else r2.rgb = rgb;
    if (out_cfg.w == 0) prev.a = a;
    else if (out_cfg.w == 1) r0.a = a;
    else if (out_cfg.w == 2) r1.a = a;
    else r2.a = a;
}

vec4 sampleStageTexture(int stage, vec2 tc) {
    if (stage < 0 || stage >= PC_GX_MAX_TEV_STAGES) return vec4(1.0);
    if (u_use_texture[stage] == 0) return vec4(1.0);
    return texture(u_texture[stage], tc);
}

vec4 sampleStageTextureGrad(int stage, vec2 tc, vec2 lod_tc) {
    if (stage < 0 || stage >= PC_GX_MAX_TEV_STAGES) return vec4(1.0);
    if (u_use_texture[stage] == 0) return vec4(1.0);
    return textureGrad(u_texture[stage], tc, dFdx(lod_tc), dFdy(lod_tc));
}

vec3 sampleIndTex(int ind_stage, int ind_fmt,
                  vec2 tc0, vec2 tc1, vec2 tc2, vec2 tc3,
                  vec2 tc4, vec2 tc5, vec2 tc6, vec2 tc7) {
    if (ind_stage < 0 || ind_stage >= PC_GX_MAX_IND_STAGES) {
        return vec3(0.0);
    }
    vec2 tc = selectTexCoord(u_ind_tc_src[ind_stage], tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7);
    tc *= u_ind_scale[ind_stage];
    vec3 c = texture(u_ind_tex[ind_stage], tc).rgb;

    /* GX_ITF_*: quantize indirect sample precision before matrix processing. */
    vec3 q = floor(clamp(c * 255.0 + 0.5, 0.0, 255.0));
    float maxv = 255.0;
    if (ind_fmt == 1) {         /* GX_ITF_5 */
        q = floor(q / 8.0);
        maxv = 31.0;
    } else if (ind_fmt == 2) {  /* GX_ITF_4 */
        q = floor(q / 16.0);
        maxv = 15.0;
    } else if (ind_fmt == 3) {  /* GX_ITF_3 */
        q = floor(q / 32.0);
        maxv = 7.0;
    }
    return q / maxv;
}

vec2 applyIndirect(ivec4 ind_cfg, ivec3 ind_wrap, vec2 coord,
                   vec2 prev_offset,
                   vec2 tc0, vec2 tc1, vec2 tc2, vec2 tc3,
                   vec2 tc4, vec2 tc5, vec2 tc6, vec2 tc7) {
    int ind_mtx_id = ind_cfg.y;
    if (ind_mtx_id == 0) return coord;
    int ind_fmt = (ind_cfg.w >> 2) & 3;

    vec3 indSample = sampleIndTex(ind_cfg.x, ind_fmt, tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7);

    int bias = ind_cfg.z;
    vec3 biased = indSample;
    float biasAmt = (ind_fmt == 0) ? -(128.0 / 255.0)
                   : (ind_fmt == 1) ?  (1.0 / 31.0)
                   : (ind_fmt == 2) ?  (1.0 / 15.0)
                                    :  (1.0 / 7.0);
    if ((bias & 1) != 0) biased.x += biasAmt;
    if ((bias & 2) != 0) biased.y += biasAmt;
    if ((bias & 4) != 0) biased.z += biasAmt;

    vec2 offset = vec2(0.0);
    int idx;
    if (ind_mtx_id >= 1 && ind_mtx_id <= 3) {
        idx = ind_mtx_id - 1;
        offset.x = dot(u_ind_mtx_r0[idx], biased);
        offset.y = dot(u_ind_mtx_r1[idx], biased);
    } else if (ind_mtx_id >= 5 && ind_mtx_id <= 7) {
        idx = ind_mtx_id - 5;
        offset.x = dot(u_ind_mtx_r0[idx], biased);
        offset.y = 0.0;
    } else if (ind_mtx_id >= 9 && ind_mtx_id <= 11) {
        idx = ind_mtx_id - 9;
        offset.x = 0.0;
        offset.y = dot(u_ind_mtx_r1[idx], biased);
    }

    if ((ind_wrap.z & 0x1) != 0) offset += prev_offset;

    vec2 wrappedCoord = coord;
    if (ind_wrap.x == 6) wrappedCoord.x = 0.0;
    else if (ind_wrap.x >= 1 && ind_wrap.x <= 5) {
        float wrapSize = float(1 << (9 - ind_wrap.x));
        float w = coord.x * 256.0;
        wrappedCoord.x = (w - wrapSize * floor(w / wrapSize)) / 256.0;
    }
    if (ind_wrap.y == 6) wrappedCoord.y = 0.0;
    else if (ind_wrap.y >= 1 && ind_wrap.y <= 5) {
        float wrapSize = float(1 << (9 - ind_wrap.y));
        float w = coord.y * 256.0;
        wrappedCoord.y = (w - wrapSize * floor(w / wrapSize)) / 256.0;
    }

    return wrappedCoord + offset;
}

bool alphaTest(int comp, float val, float ref) {
    const float EPS = 0.5 / 255.0;
    if (comp == 0) return false;
    if (comp == 1) return val < ref;
    if (comp == 2) return abs(val - ref) < EPS;
    if (comp == 3) return val <= ref;
    if (comp == 4) return val > ref;
    if (comp == 5) return abs(val - ref) >= EPS;
    if (comp == 6) return val >= ref;
    return true;
}

vec3 evalChanRGB(vec3 matC, vec3 ambC, int lightingEnabled, int lightMask, int diffFn, int attnFn, vec3 N) {
    if (lightingEnabled == 0) {
        return matC;
    }

    vec3 lightAccum = ambC;
    for (int i = 0; i < 8; i++) {
        if ((lightMask & (1 << i)) != 0) {
            vec3 Lvec = u_light_pos[i] - v_eye_pos;
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

vec4 rasterColorForChan(int chanSel, vec4 ras0, vec4 ras1) {
    if (chanSel == 1 || chanSel == 3 || chanSel == 5 || chanSel == 8) {
        return ras1;
    }
    if (chanSel == 6 || chanSel == 255) {
        return vec4(1.0);
    }
    return ras0;
}

void main() {
    vec2 tc0 = applyTexCoordManualScale(0, applyTexCoordCylWrap(0, v_texcoord0));
    vec2 tc1 = applyTexCoordManualScale(1, applyTexCoordCylWrap(1, v_texcoord1));
    vec2 tc2 = applyTexCoordManualScale(2, applyTexCoordCylWrap(2, v_texcoord2));
    vec2 tc3 = applyTexCoordManualScale(3, applyTexCoordCylWrap(3, v_texcoord3));
    vec2 tc4 = applyTexCoordManualScale(4, applyTexCoordCylWrap(4, v_texcoord4));
    vec2 tc5 = applyTexCoordManualScale(5, applyTexCoordCylWrap(5, v_texcoord5));
    vec2 tc6 = applyTexCoordManualScale(6, applyTexCoordCylWrap(6, v_texcoord6));
    vec2 tc7 = applyTexCoordManualScale(7, applyTexCoordCylWrap(7, v_texcoord7));

    vec2 stage_tc[PC_GX_MAX_TEV_STAGES];
    vec4 stage_tex[PC_GX_MAX_TEV_STAGES];
    vec2 ind_prev_offset = vec2(0.0);

    for (int s = 0; s < PC_GX_MAX_TEV_STAGES; ++s) {
        vec2 base = selectTexCoord(u_tev_tc_src[s], tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7);
        vec2 st = base;
        bool use_ind = (s < u_num_tev_stages && u_num_ind_stages > 0 &&
                        u_tev_ind_cfg[s].y != 0 &&
                        u_tev_ind_cfg[s].x >= 0 &&
                        u_tev_ind_cfg[s].x < u_num_ind_stages);
        if (use_ind) {
            st = applyIndirect(u_tev_ind_cfg[s], u_tev_ind_wrap[s], st, ind_prev_offset,
                               tc0, tc1, tc2, tc3, tc4, tc5, tc6, tc7);
            ind_prev_offset = st - base;
        }
        stage_tc[s] = st;
        if (use_ind && ((u_tev_ind_wrap[s].z & 0x2) != 0)) {
            /* GX utc_lod=true: compute LOD from unmodified coordinates. */
            stage_tex[s] = sampleStageTextureGrad(s, st, base);
        } else {
            stage_tex[s] = sampleStageTexture(s, st);
        }
    }

    vec4 rasColor0;
    vec4 rasColor1;
    if (u_num_chans == 0) {
        rasColor0 = vec4(1.0);
        rasColor1 = vec4(1.0);
    } else {
        vec3 N = normalize(v_normal);

        vec3 matC = (u_chan_mat_src != 0) ? v_color0.rgb : u_mat_color.rgb;
        vec3 ambC = (u_chan_amb_src != 0) ? v_color0.rgb : u_amb_color.rgb;
        rasColor0.rgb = evalChanRGB(matC, ambC, u_lighting_enabled, u_light_mask, u_chan_diff_fn, u_chan_attn_fn, N);

        float matA = (u_alpha_mat_src != 0) ? v_color0.a : u_mat_color.a;
        if (u_alpha_lighting_enabled != 0) {
            rasColor0.a = matA * u_amb_color.a;
        } else {
            rasColor0.a = matA;
        }

        vec3 matC1 = (u_chan_mat_src1 != 0) ? v_color1.rgb : u_mat_color1.rgb;
        vec3 ambC1 = (u_chan_amb_src1 != 0) ? v_color1.rgb : u_amb_color1.rgb;
        rasColor1.rgb = evalChanRGB(matC1, ambC1, u_lighting_enabled1, u_light_mask1, u_chan_diff_fn1, u_chan_attn_fn1, N);

        float matA1 = (u_alpha_mat_src1 != 0) ? v_color1.a : u_mat_color1.a;
        if (u_alpha_lighting_enabled1 != 0) {
            rasColor1.a = matA1 * u_amb_color1.a;
        } else {
            rasColor1.a = matA1;
        }
    }

    vec4 prev = u_tev_prev;
    vec4 r0 = u_tev_reg0;
    vec4 r1 = u_tev_reg1;
    vec4 r2 = u_tev_reg2;

    for (int s = 0; s < PC_GX_MAX_TEV_STAGES; ++s) {
        if (s >= u_num_tev_stages) break;

        vec4 sTex = applySwap(stage_tex[s], u_swap_table[u_tev_swap[s].y]);
        vec4 sRas = applySwap(rasterColorForChan(u_tev_color_chan[s], rasColor0, rasColor1),
                              u_swap_table[u_tev_swap[s].x]);
        vec3 kc = getKonstC(u_tev_ksel[s].x);
        float ka = getKonstA(u_tev_ksel[s].y);

        vec4 outc = tevStage(u_tev_color_in[s], u_tev_color_op[s],
                             u_tev_alpha_in[s], u_tev_alpha_op[s],
                             prev, sTex, sRas, r0, r1, r2, kc, ka);
        outc = applyBSC(outc, u_tev_bsc[s]);
        writeToReg(outc, u_tev_out[s], u_tev_clamp_mode[s], prev, r0, r1, r2);
    }

    if (u_alpha_comp0 != 7 || u_alpha_comp1 != 7) {
        float ref0 = float(u_alpha_ref0) / 255.0;
        float ref1 = float(u_alpha_ref1) / 255.0;
        bool pass0 = alphaTest(u_alpha_comp0, prev.a, ref0);
        bool pass1 = alphaTest(u_alpha_comp1, prev.a, ref1);
        bool pass;
        if (u_alpha_op == 0)      pass = pass0 && pass1;
        else if (u_alpha_op == 1) pass = pass0 || pass1;
        else if (u_alpha_op == 2) pass = pass0 != pass1;
        else                      pass = pass0 == pass1;
        if (!pass) discard;
    }

    int zStage = clamp(u_num_tev_stages - 1, 0, PC_GX_MAX_TEV_STAGES - 1);
    if (u_ztex_enable != 0 && u_use_texture[zStage] != 0) {
        const float z24Max = 16777215.0;
        vec4 ztexColor = stage_tex[zStage];
        float ztex24 = 0.0;
        if (u_ztex_fmt == 0) {
            /* Z8: keep texel in low 8 bits, upper bits zero-filled. */
            ztex24 = floor(ztexColor.r * 255.0 + 0.5);
        } else if (u_ztex_fmt == 1) {
            /* Z16: high byte in A, low byte in I/R, right-justified into u24. */
            float zHi = floor(ztexColor.a * 255.0 + 0.5);
            float zLo = floor(ztexColor.r * 255.0 + 0.5);
            ztex24 = zHi * 256.0 + zLo;
        } else {
            /* Z24X8: use RGB as the 24-bit depth value. */
            vec3 zrgb = floor(ztexColor.rgb * 255.0 + 0.5);
            ztex24 = dot(zrgb, vec3(65536.0, 256.0, 1.0));
        }

        float ref24 = floor(clamp(gl_FragCoord.z, 0.0, 1.0) * z24Max + 0.5);
        float zout24 = ref24;
        if (u_ztex_op == 1) {
            zout24 = ref24 + ztex24;
        } else if (u_ztex_op == 2) {
            zout24 = ztex24;
        }

        float bias24 = float(u_ztex_bias24 & 0x00FFFFFF);
        zout24 = clamp(zout24 + bias24, 0.0, z24Max);
        gl_FragDepth = zout24 / z24Max;
    }

    fragColor = prev;

    if (u_fog_type != 0) {
        float fog_denom = max(u_fog_end - u_fog_start, 1e-6);
        float fog_factor = clamp((v_fog_z - u_fog_start) / fog_denom, 0.0, 1.0);
        fragColor.rgb = mix(fragColor.rgb, u_fog_color.rgb, fog_factor);
    }
}
