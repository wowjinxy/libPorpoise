R""(
#version 330 core
uniform uint numTevStages;
uniform sampler2D tevTexMaps[16];

smooth in vec4 color0;
smooth in vec2 gxTexCoords[8 /* GX_MAX_TEXCOORD */];
out vec4 color;

// This is the same as TevStageConfig in sim_gx_State.hpp, just with enums replaced by uints
struct TevStageConfig {
  uint mMode;
  uint mColorOperation;
  uint mAlphaOperation;
  uint pad1;
  uvec4 mColorArgs;
  uvec4 mAlphaArgs;
  uint mOutReg;
  uint mClampMode;
  uint mBias;
  uint mScale;
  uint mTexCoordId;
};

layout (std140) uniform tevConfigBlock {
  uniform TevStageConfig tevStageConfigs[16 /* GX_MAXTEVSTAGES */];
};

uniform vec4 initialTevColors[4];


vec4 tevRegs[4];
vec4 tevResult = vec4(0.0, 0.0, 0.0, 1.0);

vec4 TexMapStage0(vec2 stageTexCoords) {
  return texture(tevTexMaps[0], stageTexCoords);
}

vec4 TexMapStage1(vec2 stageTexCoords) {
  return texture(tevTexMaps[1], stageTexCoords);
}

vec4 TexMapStage2(vec2 stageTexCoords) {
  return texture(tevTexMaps[2], stageTexCoords);
}

vec4 TexMapStage3(vec2 stageTexCoords) {
  return texture(tevTexMaps[3], stageTexCoords);
}

vec4 TexMapStage4(vec2 stageTexCoords) {
  return texture(tevTexMaps[4], stageTexCoords);
}

vec4 TexMapStage5(vec2 stageTexCoords) {
  return texture(tevTexMaps[5], stageTexCoords);
}

vec4 TexMapStage6(vec2 stageTexCoords) {
  return texture(tevTexMaps[6], stageTexCoords);
}

vec4 TexMapStage7(vec2 stageTexCoords) {
  return texture(tevTexMaps[7], stageTexCoords);
}

vec4 TexMapStage8(vec2 stageTexCoords) {
  return texture(tevTexMaps[8], stageTexCoords);
}

vec4 TexMapStage9(vec2 stageTexCoords) {
  return texture(tevTexMaps[9], stageTexCoords);
}

vec4 TexMapStage10(vec2 stageTexCoords) {
  return texture(tevTexMaps[10], stageTexCoords);
}

vec4 TexMapStage11(vec2 stageTexCoords) {
  return texture(tevTexMaps[11], stageTexCoords);
}

vec4 TexMapStage12(vec2 stageTexCoords) {
  return texture(tevTexMaps[12], stageTexCoords);
}

vec4 TexMapStage13(vec2 stageTexCoords) {
  return texture(tevTexMaps[13], stageTexCoords);
}

vec4 TexMapStage14(vec2 stageTexCoords) {
  return texture(tevTexMaps[14], stageTexCoords);
}

vec4 TexMapStage15(vec2 stageTexCoords) {
  return texture(tevTexMaps[15], stageTexCoords);
}

vec4 TexMapStage(uint stage, vec2 stageTexCoords) {
  vec4 texMapResult = vec4(0.0);
  switch(stage) {
    default:
    case 0u:
      texMapResult = TexMapStage0(stageTexCoords);
      break;
    case 1u:
      texMapResult = TexMapStage1(stageTexCoords);
      break;
    case 2u:
      texMapResult = TexMapStage2(stageTexCoords);
      break;
    case 3u:
      texMapResult = TexMapStage3(stageTexCoords);
      break;
    case 4u:
      texMapResult = TexMapStage4(stageTexCoords);
      break;
    case 5u:
      texMapResult = TexMapStage5(stageTexCoords);
      break;
    case 6u:
      texMapResult = TexMapStage6(stageTexCoords);
      break;
    case 7u:
      texMapResult = TexMapStage7(stageTexCoords);
      break;
    case 8u:
      texMapResult = TexMapStage8(stageTexCoords);
      break;
    case 9u:
      texMapResult = TexMapStage9(stageTexCoords);
      break;
    case 10u:
      texMapResult = TexMapStage10(stageTexCoords);
      break;
    case 11u:
      texMapResult = TexMapStage11(stageTexCoords);
      break;
    case 12u:
      texMapResult = TexMapStage12(stageTexCoords);
      break;
    case 13u:
      texMapResult = TexMapStage13(stageTexCoords);
      break;
    case 14u:
      texMapResult = TexMapStage14(stageTexCoords);
      break;
    case 15u:
      texMapResult = TexMapStage15(stageTexCoords);
      break;
  }

  return texMapResult;
}



vec3 GetTevColorArg(uint stageNum, uint argNum) {
  uint argType = tevStageConfigs[stageNum].mColorArgs[argNum];
  vec3 result = vec3(0.0);

  switch(argType) {
    case 0u: /* GX_CC_CPREV */
      result = tevResult.rgb;
      break;
    case 1u: /* GX_CC_APREV */
      result = vec3(tevResult.a);
      break;
    case 2u: /* GX_CC_C0 */
      result = tevRegs[1].rgb;
      break;
    case 3u: /* GX_CC_C1 */
      result = tevRegs[2].rgb;
      break;
    case 4u: /* GX_CC_C2 */
      result = tevRegs[3].rgb;
      break;
    case 5u: /* GX_CC_A0 */
      result = vec3(tevRegs[1].a);
      break;
    case 6u: /* GX_CC_A1 */
      result = vec3(tevRegs[2].a);
      break;
    case 7u: /* GX_CC_A2 */
      result = vec3(tevRegs[3].a);
      break;
    case 8u: /* GX_CC_TEXC */
      result = TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).rgb;
      break;
    case 9u: /* GX_CC_TEXA */
      result = vec3(TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).a);
      break;
    case 10u: /* GX_CC_RASC */
      result = color0.rgb;
      break;
    case 11u: /* GX_CC_RASA */
      result = vec3(color0.a);
      break;
    case 12u: /* GX_CC_ONE */
      result = vec3(1.0);
      break;
    case 13u: /* GX_CC_HALF */
      result = vec3(0.5);
      break;
    case 14u: /* GX_CC_KONST */
      break;
    case 15u: /* GX_CC_ZERO */
      result = vec3(0.0);
      break;

    default:
      // Unsupported
      result = vec3(1.0, 0.0, 1.0);
      break;
  }

  return result;
}

float GetTevAlphaArg(uint stageNum, uint argNum) {
  uint argType = tevStageConfigs[stageNum].mAlphaArgs[argNum];
  float result = 1.0;

  switch(argType) {
    case 0u: /* GX_CA_APREV */
      result = tevResult.a;
      break;
    case 1u: /* GX_CA_A0 */
      result = tevRegs[1].a;
      break;
    case 2u: /* GX_CA_A1 */
      result = tevRegs[2].a;
      break;
    case 3u: /* GX_CA_A2 */
      result = tevRegs[3].a;
      break;
    case 4u: /* GX_CC_TEXA */
      result = TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).a;
      break;
    case 5u: /* GX_CA_RASA */
      result = color0.a;
      break;
    case 6u: /* GX_CA_KONST */
      result = 1.0; /* TODO: Not always 1.0 */
      break;
    case 7u: /* GX_CA_ZERO */
      result = 0.0;
      break;

    default:
      // Unsupported
      result = 1.0;
      break;
  }

  return result;
}

vec3 RunTevColorOperation(uint op) {
  vec3 result = vec3(0.0);
  switch(op) {
    case 0u: /* GX_TEV_ADD */
      result = (tevRegs[0]*(vec4(1.0) - tevRegs[2]) + tevRegs[1] * tevRegs[2] + tevRegs[3]).rgb;
      break;
    
    case 1u: /* GX_TEV_SUB */
      result = (-tevRegs[0] * (vec4(1.0) - tevRegs[2]) - tevRegs[1] * tevRegs[2] + tevRegs[3]).rgb;
      break;
    
    case 8u: /* GX_TEV_COMP_R8_GT */
      if(tevRegs[0].r > tevRegs[1].r) {
        result = tevRegs[2].rgb;
      } else {
        result = tevRegs[3].rgb;
      }
      break;

    case 9u: /* GX_TEV_COMP_R8_EQ */
      if(tevRegs[0].r == tevRegs[1].r) {
        result = tevRegs[2].rgb;
      } else {
        result = tevRegs[3].rgb;
      }
      break;
    
    case 15u: /* GX_TEV_COMP_RGB8_EQ */
      if(tevRegs[0].r == tevRegs[1].r) {
        result.r = tevRegs[3].r + tevRegs[2].r;
      } else {
        result.r = tevRegs[3].r;
      }

      if(tevRegs[0].g == tevRegs[1].g) {
        result.g = tevRegs[3].g + tevRegs[2].g;
      } else {
        result.g = tevRegs[3].g;
      }

      if(tevRegs[0].b == tevRegs[1].b) {
        result.b = tevRegs[3].b + tevRegs[2].b;
      } else {
        result.b = tevRegs[3].b;
      }
      break;
      

    default: /* unsupported operation */
      result = vec3(1.0, 0.0, 1.0);
      break;
  }

  // TODO Apply Scale/bias

  return result;
}

float RunTevAlphaOperation(uint op) {
  float result = 1.0;

  // run operation
  switch(op) {
    case 0u: /* GX_TEV_ADD */
      result = (tevRegs[0]*(vec4(1.0) - tevRegs[2]) + tevRegs[1] * tevRegs[2] + tevRegs[3]).a;
      break;
    
    case 1u: /* GX_TEV_SUB */
      result = (-tevRegs[0] * (vec4(1.0) - tevRegs[2]) - tevRegs[1] * tevRegs[2] + tevRegs[3]).a;
      break;
    
    case 8u: /* GX_TEV_COMP_R8_GT */
      if(tevRegs[0].r > tevRegs[1].r) {
        result = tevRegs[2].a;
      } else {
        result = tevRegs[3].a;
      }
      break;

    case 9u: /* GX_TEV_COMP_R8_EQ */
      if(tevRegs[0].r == tevRegs[1].r) {
        result = tevRegs[2].a;
      } else {
        result = tevRegs[3].a;
      }
      break;

    default: /* unsupported operation */
      result = 1.0;
      break;
  }

  // TODO apply scale/bias

  return result;
}

void main()
{
  tevRegs = initialTevColors;
  for(uint i=0u; i < numTevStages; i++) {
    vec4 tevArgs[4];

    // Load the values into tev regs
    tevArgs[0].rgb = GetTevColorArg(i, 0u);
    tevArgs[0].a = GetTevAlphaArg(i, 0u);
    tevArgs[1].rgb = GetTevColorArg(i, 1u);
    tevArgs[1].a = GetTevAlphaArg(i, 1u);
    tevArgs[2].rgb = GetTevColorArg(i, 2u);
    tevArgs[2].a = GetTevAlphaArg(i, 2u);
    tevArgs[3].rgb = GetTevColorArg(i, 3u);
    tevArgs[3].a = GetTevAlphaArg(i, 3u);

    tevRegs = tevArgs;

    // Run the tev operation
    tevResult.rgb = RunTevColorOperation(tevStageConfigs[i].mColorOperation);
    tevResult.a = RunTevAlphaOperation(tevStageConfigs[i].mAlphaOperation);
  }

  color = tevResult;
  if(color.a < 0.01) {
    discard;
  }
}
)""
