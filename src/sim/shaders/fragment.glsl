R""(
#version 330 core
uniform uint numTevStages;
uniform sampler2D tevTexMaps[16];

smooth in vec4 color0;
smooth in vec2 fragTexCoords;
out vec4 color;

// This is the same as TevStageConfig in sim_gx_State.hpp, just with enums replaced by uints
struct TevStageConfig {
  uint mMode;
  uint mOperation;
  uint pad1;
  uint pad2;
  uvec4 mArgs;
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
vec4 tevResult = vec4(0.0);

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



void LoadTevArg(uint stageNum, uint argNum) {
  uint argType = tevStageConfigs[stageNum].mArgs[argNum];

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
      tevRegs[argNum] = TexMapStage(stageNum, fragTexCoords);
      break;
    case 9u: /* GX_CC_TEXA */
      tevRegs[argNum] = TexMapStage(stageNum, fragTexCoords);
      break;
    case 10u: /* GX_CC_RASC */
      // TODO: this isnt always color0
      tevRegs[argNum] = color0;
      break;
    case 11u: /* GX_CC_RASA */
      // TODO: this isnt always color0
      tevRegs[argNum] = color0;
      break;
    case 12u: /* GX_CC_ONE */
      tevRegs[argNum] = vec4(1.0);
      break;
    case 13u: /* GX_CC_HALF */
      tevRegs[argNum] = vec4(0.5);
      break;
    case 14u: /* GX_CC_KONST */
      break;
    case 15u: /* GX_CC_ZERO */
      tevRegs[argNum] = vec4(0.0);
      break;

    default:
      tevRegs[argNum] = vec4(1.0, 0.0, 1.0, 1.0);
  }
}

vec4 RunTevOperation(uint op) {
  vec4 result = vec4(0.0);
  switch(op) {
    case 0u: /* GX_TEV_ADD */
      result = tevRegs[0]*(vec4(1.0) - tevRegs[2]) + tevRegs[1] * tevRegs[2] + tevRegs[3];
      break;
    
    case 1u: /* GX_TEV_SUB */
      result = -tevRegs[0] * (vec4(1.0) - tevRegs[2]) - tevRegs[1] * tevRegs[2] + tevRegs[3];
      break;
    
    case 8u: /* GX_TEV_COMP_R8_GT */
      if(tevRegs[0].r > tevRegs[1].r) {
        result = tevRegs[2];
      } else {
        result = tevRegs[3];
      }
      break;

    case 9u: /* GX_TEV_COMP_R8_EQ */
      if(tevRegs[0].r == tevRegs[1].r) {
        result = tevRegs[2];
      } else {
        result = tevRegs[3];
      }
      break;

    default: /* unsupported operation */
      break;
  }

  return result;
}

void main()
{
  for(uint i=0u; i < numTevStages; i++) {
    // Load the values into tev regs
    LoadTevArg(i, 0u);
    LoadTevArg(i, 1u);
    LoadTevArg(i, 2u);
    LoadTevArg(i, 3u);

    // Run the tev operation
    tevResult = RunTevOperation(tevStageConfigs[i].mOperation);

    // Apply scale / bias to result

  }

  color = tevResult;
}
)""
