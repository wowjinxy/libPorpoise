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



void LoadTevColorArg(uint stageNum, uint argNum) {
  uint argType = tevStageConfigs[stageNum].mColorArgs[argNum];

  switch(argType) {
    case 0u: /* GX_CC_CPREV */
      tevRegs[argNum] = tevResult;
      break;
    case 1u: /* GX_CC_APREV */
      break;
    case 2u: /* GX_CC_C0 */
      break;
    case 3u: /* GX_CC_C1 */
      break;
    case 4u: /* GX_CC_C2 */
      break;
    case 5u: /* GX_CC_A0 */
      break;
    case 6u: /* GX_CC_A1 */
      break;
    case 7u: /* GX_CC_A2 */
      break;
    case 8u: /* GX_CC_TEXC */
      tevRegs[argNum].rgb = TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).rgb;
      break;
    case 9u: /* GX_CC_TEXA */
      tevRegs[argNum].rgb = TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).rgb;
      break;
    case 10u: /* GX_CC_RASC */
      // TODO: this isnt always color0
      tevRegs[argNum].rgb = color0.rgb;
      break;
    case 11u: /* GX_CC_RASA */
      // TODO: this isnt always color0
      tevRegs[argNum].rgb = color0.rgb;
      break;
    case 12u: /* GX_CC_ONE */
      tevRegs[argNum].rgb = vec3(1.0);
      break;
    case 13u: /* GX_CC_HALF */
      tevRegs[argNum].rgb = vec3(0.5);
      break;
    case 14u: /* GX_CC_KONST */
      break;
    case 15u: /* GX_CC_ZERO */
      tevRegs[argNum].rgb = vec3(0.0);
      break;

    default:
      // Unsupported
      tevRegs[argNum].rgb = vec3(1.0, 0.0, 1.0);
      break;
  }
}

void LoadTevAlphaArg(uint stageNum, uint argNum) {
  uint argType = tevStageConfigs[stageNum].mColorArgs[argNum];

  //TODO: remove
  tevRegs[argNum].a = 1.0;

  switch(argType) {
    case 0u: /* GX_CA_APREV */
      tevRegs[argNum].a = tevResult.a;
      break;
    case 1u: /* GX_CA_A0 */
      break;
    case 2u: /* GX_CA_A1 */
      break;
    case 3u: /* GX_CA_A2 */
      break;
    case 4u: /* GX_CC_TEXA */
      tevRegs[argNum].a = TexMapStage(stageNum, gxTexCoords[tevStageConfigs[stageNum].mTexCoordId]).a;
      break;
    case 5u: /* GX_CA_RASA */
      // TODO: this isnt always color0
      tevRegs[argNum].a = color0.a;
      break;
    case 6u: /* GX_CA_KONST */
      tevRegs[argNum].a = 1.0; /* TODO: Not always 1.0 */
      break;
    case 7u: /* GX_CA_ZERO */
      tevRegs[argNum].a = 0.0;
      break;

    default:
      // Unsupported
      tevRegs[argNum].a = 1.0;
      break;
  }
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
  for(uint i=0u; i < numTevStages; i++) {
    // Load the values into tev regs
    LoadTevColorArg(i, 0u);
    LoadTevAlphaArg(i, 0u);
    LoadTevColorArg(i, 1u);
    LoadTevAlphaArg(i, 1u);
    LoadTevColorArg(i, 2u);
    LoadTevAlphaArg(i, 2u);
    LoadTevColorArg(i, 3u);
    LoadTevAlphaArg(i, 3u);

    // Run the tev operation
    tevResult.rgb = RunTevColorOperation(tevStageConfigs[i].mColorOperation);
    tevResult.a = RunTevAlphaOperation(tevStageConfigs[i].mAlphaOperation);
  }

  color = tevResult;
}
)""
