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

struct TexGenConfig {
  uint mMatrixId;
  uint mType;
};

uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat4 u_textureMtx[10];
uniform uint u_numTexGens;
uniform TexGenConfig u_texGens[8 /* GX_MAX_TEXCOORD */];
uniform Light u_lights[8 /*GX_MAX_LIGHTID*/];

smooth out vec4 color0;
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

void main()
{
    gl_Position = u_projection * u_modelview * vec4(position, 1.0);
    color0 = vertex_color;

    for(uint i=0u; i<u_numTexGens; i++) {
        gxTexCoords[i] = GenerateTexCoords(texCoords, u_texGens[i].mMatrixId, u_texGens[i].mType);
    }
}
)""
