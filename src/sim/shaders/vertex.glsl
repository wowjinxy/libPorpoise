R""(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertex_color;
layout (location = 3) in vec2 texCoords;

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
uniform mat4 u_modelview;
uniform mat4 u_textureMtx[10];
uniform uint u_numTexGens;
uniform uint u_numChans;
uniform TexGenConfig u_texGens[8 /* GX_MAX_TEXCOORD */];

layout (std140) uniform lightConfigBlock {
  uniform Light u_lights[8 /*GX_MAX_LIGHTID*/];
  uniform ColorChannel u_colorChannels[4];
};

smooth out vec3 rasc;
smooth out float rasa;
smooth out vec2 gxTexCoords[8 /* GX_MAX_TEXCOORD */];

vec2 GenerateTexCoords(vec2 texCoordsIn, uint matrixId, uint texGenType)
{
    vec2 result = vec2(0.0, 0.0);
    switch(texGenType) {
        case 0u /* GX_TG_MTX2X4 */:
            {
                mat4 mtx = u_textureMtx[matrixId];
                //mtx[2] = vec4(0.0);
                //mtx[3] = vec4(0.0);
                result = (vec4(texCoordsIn, 0.0, 0.0) * mtx).xy;
            }
            break;
        case 1u /* GX_TG_MTX3X4 */:
            {
                mat4 mtx = u_textureMtx[matrixId];
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

vec4 ProcessChannel(uint chan) {
    uint colorChannelNum = 2u * chan;
    uint alphaChannelNum = 2u * chan + 1u;

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

                //We need top calculate Attenuation, DiffuseAtten, and LightColor (given in the light struct)

                //TODO: this is just a placeholder for now
                lightSumColor += vec3(0.1);
            }
        }

        lightFuncColor = ambientColor + lightSumColor;
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

                //We need top calculate Attenuation, DiffuseAtten, and LightColor (given in the light struct)

                //TODO: this is just a placeholder for now
                lightSumAlpha += 0.1;
            }
        }

        lightFuncAlpha = ambientAlpha + lightSumAlpha;
    }

    channelColor.a = materialAlpha * lightFuncAlpha;


    return channelColor;
}

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
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
