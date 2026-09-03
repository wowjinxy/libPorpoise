R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertex_color;
layout (location = 3) in vec2 texCoords;
layout (location = 4) in uint posNormalMtxIdx;
layout (location = 5) in uvec4 texMtxIdx0;
layout (location = 6) in uvec4 texMtxIdx1;

struct Light {
  vec4 mPosition;
  vec4 mDirection;
  vec4 mColor;
  vec4 mCosAtt;
  vec4 mDistAtt;
};

struct ColorChannel {
  vec4 mMaterialColor;
  vec4 mAmbientColor;
  uint mMaterialSource;
  uint mAmbientSource;
  uint mDiffuseFunction;
  uint mAttnFunction;
  uint mLightingEnabled;
  uint mLightMask;
  uint pad1;
  uint pad2;
};

struct TexGenConfig {
  uint mMatrixId;
  uint mType;
};

uniform mat4 u_projection;
uniform mat4 u_normalMtx[10];
uniform uint u_numTexGens;
uniform uint u_numChans;
uniform TexGenConfig u_texGens[8 /* GX_MAX_TEXCOORD */];

layout (std140) uniform lightConfigBlock {
  uniform Light u_lights[8 /*GX_MAX_LIGHTID*/];
  uniform ColorChannel u_colorChannels[4];
};

uniform uint mtxIdxA;
uniform uint pnMtxIdxEnabled;

layout (std140) uniform matrixMemoryBlock {
    /* matrix xf memory:
    0x0: start of position matrices,
    0x78: start of texture matrices (4x3)
    0x400: start of normal matrices (3x3)
    */
    uniform vec4 u_posTextureMatrixMemory[60];
    uniform vec3 u_normalMatrixMemory[30];
};

smooth out vec3 rasc;
smooth out float rasa;
smooth out vec2 gxTexCoords[8 /* GX_MAX_TEXCOORD */];

vec3 calculatedNormal = vec3(0.0);

mat4 GetPositionMatrix(uint row) {
    // A valid 3x4 matrix needs three available rows.
    if (row > 27u)
        return mat4(1.0);

    vec4 r0 = u_posTextureMatrixMemory[row];
    vec4 r1 = u_posTextureMatrixMemory[row+1u];
    vec4 r2 = u_posTextureMatrixMemory[row+2u];

    // GLSL constructor arguments are columns.
    return mat4(
        vec4(r0.x, r1.x, r2.x, 0.0),
        vec4(r0.y, r1.y, r2.y, 0.0),
        vec4(r0.z, r1.z, r2.z, 0.0),
        vec4(r0.w, r1.w, r2.w, 1.0)
    );
}

mat4 GetTextureMatrix(uint row) {
    // A valid 3x4 matrix needs three available rows.
    if (row > 57u || row == 0u)
        return mat4(1.0);

    vec4 r0 = u_posTextureMatrixMemory[row];
    vec4 r1 = u_posTextureMatrixMemory[row+1u];
    vec4 r2 = u_posTextureMatrixMemory[row+2u];

    // GLSL constructor arguments are columns.
    return mat4(
        vec4(r0.x, r1.x, r2.x, 0.0),
        vec4(r0.y, r1.y, r2.y, 0.0),
        vec4(r0.z, r1.z, r2.z, 0.0),
        vec4(r0.w, r1.w, r2.w, 1.0)
    );
}

uint GetTexMtxIdx(uint texId) {
    uint idx = 0u;
    uvec4 texIdxs = texMtxIdx0;
    if(texId > 3u) {
        texIdxs = texMtxIdx1;
        texId = texId - 3u;
    }

    switch(texId) {
        default:
        case 0u:
            idx = texIdxs.x;
            break;
        case 1u:
            idx = texIdxs.y;
            break;
        case 2u:
            idx = texIdxs.z;
            break;
        case 3u:
            idx = texIdxs.w;
            break;
    }

    return idx;
}

vec2 GenerateTexCoords(vec2 texCoordsIn, uint matrixId, uint texGenType)
{
    vec2 result = vec2(0.0, 0.0);
    switch(texGenType) {
        case 0u /* GX_TG_MTX2X4 */:
            {
                mat4 mtx = GetTextureMatrix(matrixId);
                //mtx[2] = vec4(0.0);
                //mtx[3] = vec4(0.0);
                result = (vec4(texCoordsIn, 0.0, 0.0) * mtx).xy;
            }
            break;
        case 1u /* GX_TG_MTX3X4 */:
            {
                mat4 mtx = GetTextureMatrix(matrixId);
                //mtx[3] = vec4(0.0);
                result = (vec4(texCoordsIn, 0.0, 0.0) * mtx).xy;
            }
            break;
        default:
            result = texCoordsIn;
            break;
    }

    return result;
}

float CalcDiffuse(uint channelNum, vec3 lightDistance) {
    float diffuseAttenuation = 1.0;
    if(u_colorChannels[channelNum].mDiffuseFunction == 1u) {
        /* GX_DF_SIGN */
        diffuseAttenuation = dot(calculatedNormal, lightDistance);
    } else if(u_colorChannels[channelNum].mDiffuseFunction == 2u) {
        /* GX_DF_CLAMP */
        diffuseAttenuation = max(dot(calculatedNormal, lightDistance), 0.0);
    }

    return diffuseAttenuation;
}

vec4 ProcessChannel(uint chan) {
    uint colorChannelNum = chan;
    uint alphaChannelNum = chan + 2u;

    vec4 channelColor = vec4(0.0);

    // Color channel
    vec3 materialColor = vec3(0.0);
    if(u_colorChannels[colorChannelNum].mMaterialSource == 0u) {
        /* GX_SRC_REG */
        materialColor = u_colorChannels[colorChannelNum].mMaterialColor.rgb;
    } else {
        /* GX_SRC_VTX */
        materialColor = vertex_color.rgb;
    }

    // Do the Light Func
    vec3 lightFuncColor = vec3(1.0);
    if(u_colorChannels[colorChannelNum].mLightingEnabled > 0u) {
        // Illumination: Ambient + the sum of all enabled lights (Attenuation * DiffuseAttenuation * lightColor)
        vec3 ambientColor = vec3(0.0);
        if(u_colorChannels[colorChannelNum].mAmbientSource == 0u) {
            /* GX_SRC_REG */
            ambientColor = u_colorChannels[colorChannelNum].mAmbientColor.rgb;
        } else {
            /* GX_SRC_VTX */
            ambientColor = vertex_color.rgb;
        }

        // Now we actually need to sum up the lights
        vec3 lightSumColor = vec3(0.0);
        for(uint lightNum = 0u; lightNum < 8u; lightNum++) {
            if((u_colorChannels[colorChannelNum].mLightMask & (1u << lightNum)) > 0u) {
                // This light is enabled
                vec3 lightDistance = u_lights[lightNum].mPosition.xyz - position;
                float dist2 = dot(lightDistance, lightDistance);
                float dist = sqrt(dist2);
                lightDistance = lightDistance / dist;

                //We need top calculate Attenuation, DiffuseAtten, and LightColor (given in the light struct)
                float attenuation = 1.0;

                if(u_colorChannels[colorChannelNum].mAttnFunction == 0u) {
                    /* GX_AF_SPEC */
                    if(dot(lightDistance, calculatedNormal) >= 0) {
                        attenuation = max(0.0, dot(calculatedNormal, u_lights[lightNum].mDirection.xyz));
                    } else {
                        attenuation = 0.0;
                    }
                    float cos_attn = dot(u_lights[lightNum].mCosAtt.xyz, vec3(1.0, attenuation, attenuation * attenuation));
                    float dist_attn = 1.0;

                    // do the dist attenuation function
                    if(u_colorChannels[colorChannelNum].mDiffuseFunction != 0u /* GX_DF_NONE */) {
                        dist_attn = max(0.0, dot(normalize(u_lights[lightNum].mDistAtt.xyz), vec3(1.0, attenuation, attenuation * attenuation)));
                    } else {
                        dist_attn = max(0.0, dot(u_lights[lightNum].mDistAtt.xyz, vec3(1.0, attenuation, attenuation * attenuation)));
                    }

                    attenuation = max(0.0, cos_attn / dist_attn);
                } else if(u_colorChannels[colorChannelNum].mAttnFunction == 1u) {
                    /* GX_AF_SPOT */
                    float cosine = max(0.0, dot(lightDistance, u_lights[lightNum].mDirection.xyz));
                    float cos_attn = dot(u_lights[lightNum].mCosAtt.xyz, vec3(1.0, cosine, cosine * cosine));
                    float dist_attn = dot(u_lights[lightNum].mDistAtt.xyz, vec3(1.0, dist, dist2));
                    attenuation = max(0.0, cos_attn / dist_attn);
                }



                float diffuse = CalcDiffuse(colorChannelNum, lightDistance);



                lightSumColor.r += attenuation * diffuse * u_lights[lightNum].mColor.r;
                lightSumColor.g += attenuation * diffuse * u_lights[lightNum].mColor.g;
                lightSumColor.b += attenuation * diffuse * u_lights[lightNum].mColor.b;
            }
        }

        lightFuncColor = clamp(ambientColor + lightSumColor, vec3(0.0), vec3(1.0));
    }

    channelColor.rgb = materialColor * lightFuncColor;



    // Now do it all again for the alpha channel
    float materialAlpha = 0.0;
    if(u_colorChannels[alphaChannelNum].mMaterialSource == 0u) {
        /* GX_SRC_REG */
        materialAlpha = u_colorChannels[alphaChannelNum].mMaterialColor.a;
    } else {
        /* GX_SRC_VTX */
        materialAlpha = vertex_color.a;
    }

    // Do the Light Func
    float lightFuncAlpha = 1.0;
    if(u_colorChannels[alphaChannelNum].mLightingEnabled > 0u) {
        // Illumination: Ambient + the sum of all enabled lights (Attenuation * DiffuseAttenuation * lightColor)
        float ambientAlpha = 0.0;
        if(u_colorChannels[alphaChannelNum].mAmbientSource == 0u) {
            /* GX_SRC_REG */
            ambientAlpha = u_colorChannels[alphaChannelNum].mAmbientColor.a;
        } else {
            /* GX_SRC_VTX */
            ambientAlpha = vertex_color.a;
        }

        // Now we actually need to sum up the lights
        float lightSumAlpha = 0.0;
        for(uint lightNum = 0u; lightNum < 8u; lightNum++) {
            if((u_colorChannels[alphaChannelNum].mLightMask & (1u << lightNum)) > 0u) {
                // This light is enabled
                vec3 lightDistance = u_lights[lightNum].mPosition.xyz - position;
                float dist2 = dot(lightDistance, lightDistance);
                float dist = sqrt(dist2);
                lightDistance = lightDistance / dist;

                //We need top calculate Attenuation, DiffuseAtten, and LightColor (given in the light struct)
                float attenuation = 1.0;

                if(u_colorChannels[alphaChannelNum].mAttnFunction == 0u) {
                    /* GX_AF_SPEC */
                    if(dot(lightDistance, calculatedNormal) >= 0) {
                        attenuation = max(0.0, dot(calculatedNormal, u_lights[lightNum].mDirection.xyz));
                    } else {
                        attenuation = 0.0;
                    }
                    float cos_attn = dot(u_lights[lightNum].mCosAtt.xyz, vec3(1.0, attenuation, attenuation * attenuation));
                    float dist_attn = 1.0;

                    // do the dist attenuation function
                    if(u_colorChannels[alphaChannelNum].mDiffuseFunction != 0u /* GX_DF_NONE */) {
                        dist_attn = max(0.0, dot(normalize(u_lights[lightNum].mDistAtt.xyz), vec3(1.0, attenuation, attenuation * attenuation)));
                    } else {
                        dist_attn = max(0.0, dot(u_lights[lightNum].mDistAtt.xyz, vec3(1.0, attenuation, attenuation * attenuation)));
                    }

                    attenuation = max(0.0, cos_attn / dist_attn);
                } else if(u_colorChannels[alphaChannelNum].mAttnFunction == 1u) {
                    /* GX_AF_SPOT */
                    float cosine = max(0.0, dot(lightDistance, u_lights[lightNum].mDirection.xyz));
                    float cos_attn = dot(u_lights[lightNum].mCosAtt.xyz, vec3(1.0, cosine, cosine * cosine));
                    float dist_attn = dot(u_lights[lightNum].mDistAtt.xyz, vec3(1.0, dist, dist2));
                    attenuation = max(0.0, cos_attn / dist_attn);
                }

                float diffuse = CalcDiffuse(alphaChannelNum, lightDistance);

                lightSumAlpha += attenuation * diffuse * u_lights[lightNum].mColor.a;
            }
        }

        lightFuncAlpha = clamp(ambientAlpha + lightSumAlpha, 0.0, 1.0);
    }

    channelColor.a = materialAlpha * lightFuncAlpha;


    return channelColor;
}

void main()
{
    uint modelViewRow = 0u;
    if(pnMtxIdxEnabled > 0u) {
        modelViewRow = posNormalMtxIdx;
    } else {
        modelViewRow = mtxIdxA * 3u;
    }
    mat4 modelView = GetPositionMatrix(modelViewRow);
    gl_Position = u_projection * modelView * vec4(position, 1.0);

    calculatedNormal = (vec4(normal.x, normal.y, normal.z, 0.0) * u_normalMtx[posNormalMtxIdx]).xyz;

    if(dot(calculatedNormal, calculatedNormal) > 1e-10) {
        calculatedNormal = normalize(calculatedNormal);
    }

    rasc = vec3(0.0);
    rasa = 0.0;

    vec4 channelColors[2];
    channelColors[0] = vec4(0.0);
    channelColors[1] = vec4(0.0);

    for(uint i=0u; i < u_numChans; i++) {
        channelColors[i] = ProcessChannel(i);
    }

    rasc = channelColors[0].rgb + channelColors[1].rgb;
    rasa = channelColors[0].a + channelColors[1].a;

    for(uint i=0u; i<u_numTexGens; i++) {
        gxTexCoords[i] = GenerateTexCoords(texCoords, u_texGens[i].mMatrixId, u_texGens[i].mType);
    }
}
)""
