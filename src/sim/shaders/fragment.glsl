R""(
#version 330 core
uniform sampler2D myTexture;
smooth in vec4 color0;
smooth in vec2 fragTexCoords;
out vec4 color;

// This is the same as TevStageConfig in sim_gx_State.hpp, just with enums replaced by uints
struct TevStageConfig {
  uint mMode;
  uint mOperation;
  uint mArgs[4];
  uint mOutReg;
  uint mClampMode;
  uint mBias;
  uint mScale;
  sampler2D mTexMapId;
  uint mTexCoordId;
};

uniform uint numTevStages;
uniform TevStageConfig tevStageConfigs[16 /* GX_MAXTEVSTAGES */];

vec4 tevRegs[4];
vec4 tevResult = vec4(0.0);

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
      tevRegs[argNum] = texture(tevStageConfigs[stageNum].mTexMapId, fragTexCoords);
      break;
    case 9u: /* GX_CC_TEXA */
      tevRegs[argNum] = texture(tevStageConfigs[stageNum].mTexMapId, fragTexCoords);
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
      tevResult = tevRegs[0]*(vec4(1.0) - tevRegs[2]) + tevRegs[1] * tevRegs[2] + tevRegs[3];
      break;
    
    case 1u: /* GX_TEV_SUB */
      tevResult = -tevRegs[0] * (vec4(1.0) - tevRegs[2]) - tevRegs[1] * tevRegs[2] + tevRegs[3];
      break;
    
    case 8u: /* GX_TEV_COMP_R8_GT */
      if(tevRegs[0].r > tevRegs[1].r) {
        tevResult = tevRegs[2];
      } else {
        tevResult = tevRegs[3];
      }
      break;

    case 9u: /* GX_TEV_COMP_R8_EQ */
      if(tevRegs[0].r == tevRegs[1].r) {
        tevResult = tevRegs[2];
      } else {
        tevResult = tevRegs[3];
      }
      break;

    default: /* unsupported operation */
      tevResult = vec4(1.0); /* TODO */
      break;
  }

  return vec4(0.0, 0.0, 0.0, 0.0);
}

void main()
{
    for(uint i=0u; i<numTevStages; i++) {
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
