#include <dolphin/gx/GXData.h>
#include <dolphin/hw_regs.h>
#include <dolphin/pad.h>
#include <revolution/gx.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_State.hpp>

#include <array>
#include <cmath>

extern "C" u32 VIGetCurrentLine(void) {
    return 0;
}

extern "C" u32 VIGetTvFormat(void) {
    return VI_NTSC;
}

extern "C" void __GXHostRecordPrimitive(
    u32 primitive, u32 vertexCount, u32 textureStages);

namespace {

static_assert(sizeof(GXTexObj) == 0x20);
static_assert(sizeof(GXTexObjPriv) == 0x20);
static_assert(sizeof(GXTlutObj) == 0x0c);

bool NearlyEqual(float left, float right, float tolerance = 0.0001f) {
    return std::fabs(left - right) <= tolerance;
}

bool TestIndependentTextureLodSetters() {
    GXTexObj texture = {};

    GXInitTexObjMaxLOD(&texture, 12.0f);
    GXInitTexObjMinLOD(&texture, -1.0f);
    GXInitTexObjLODBias(&texture, -1.5f);
    GXInitTexObjBiasClamp(&texture, GX_TRUE);
    GXInitTexObjEdgeLOD(&texture, GX_TRUE);
    GXInitTexObjMaxAniso(&texture, GX_ANISO_4);

    return
        NearlyEqual(GXGetTexObjMaxLOD(&texture), 10.0f) &&
        NearlyEqual(GXGetTexObjMinLOD(&texture), 0.0f) &&
        NearlyEqual(GXGetTexObjLODBias(&texture), -1.5f) &&
        GXGetTexObjBiasClamp(&texture) == GX_TRUE &&
        GXGetTexObjEdgeLOD(&texture) == GX_TRUE &&
        GXGetTexObjMaxAniso(&texture) == GX_ANISO_4;
}

bool TestHostTexturePointerPreserved() {
    alignas(32) std::array<u8, 32> image = {};
    GXTexObj texture = {};
    int userData = 42;

    GXInitTexObj(
        &texture,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXInitTexObjUserData(&texture, &userData);

    void* decodedImage = nullptr;
    u16 width = 0;
    u16 height = 0;
    GXTexFmt format = GX_TF_I8;
    GXTexWrapMode wrapS = GX_REPEAT;
    GXTexWrapMode wrapT = GX_REPEAT;
    GXBool mipmap = GX_TRUE;
    GXGetTexObjAll(
        &texture,
        &decodedImage,
        &width,
        &height,
        &format,
        &wrapS,
        &wrapT,
        &mipmap);

    return
        GXGetTexObjData(&texture) == image.data() &&
        GXGetTexObjUserData(&texture) == &userData &&
        decodedImage == image.data() &&
        width == 8 &&
        height == 8 &&
        format == GX_TF_I4 &&
        wrapS == GX_CLAMP &&
        wrapT == GX_CLAMP &&
        mipmap == GX_FALSE;
}

bool TestTextureObjectAbiBounds() {
    struct GuardedTexture {
        GXTexObj texture;
        std::array<u8, 16> guard;
    } guardedTexture = {};
    struct GuardedTlut {
        GXTlutObj tlut;
        std::array<u8, 16> guard;
    } guardedTlut = {};
    alignas(32) std::array<u8, 32> image = {};
    std::array<u16, 16> palette = {};

    guardedTexture.guard.fill(0xa5);
    guardedTlut.guard.fill(0x5a);

    GXInitTexObj(
        &guardedTexture.texture,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_CLAMP,
        GX_FALSE);
    GXInitTexObjLOD(
        &guardedTexture.texture,
        GX_NEAR,
        GX_LINEAR,
        0.0f,
        0.0f,
        0.0f,
        GX_FALSE,
        GX_FALSE,
        GX_ANISO_1);
    GXInitTlutObj(
        &guardedTlut.tlut,
        palette.data(),
        GX_TL_IA8,
        static_cast<u16>(palette.size()));

    for (u8 value : guardedTexture.guard) {
        if (value != 0xa5) {
            return false;
        }
    }
    for (u8 value : guardedTlut.guard) {
        if (value != 0x5a) {
            return false;
        }
    }
    return true;
}

bool TestFifoQueries() {
    GXFifoObj fifo = {};
    auto* state = reinterpret_cast<__GXFifoObj*>(&fifo);
    state->count = 0x1234;
    state->wrap = GX_TRUE;

    return GXGetFifoCount(&fifo) == 0x1234 &&
           GXGetFifoWrap(&fifo) == GX_TRUE;
}

u16 DrawSyncCallbackToken = 0;
u32 DrawSyncCallbackCount = 0;

void RecordDrawSyncToken(u16 token) {
    DrawSyncCallbackToken = token;
    DrawSyncCallbackCount++;
}

bool TestHostDrawSyncCompletion() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    DrawSyncCallbackToken = 0;
    DrawSyncCallbackCount = 0;
    const GXDrawSyncCallback previousCallback =
        GXSetDrawSyncCallback(RecordDrawSyncToken);

    GXSetDrawSync(0xb00b);
    const bool firstTokenCompleted =
        GXReadDrawSync() == 0xb00b &&
        DrawSyncCallbackToken == 0xb00b &&
        DrawSyncCallbackCount == 1;

    GXSetDrawSync(0xbeef);
    const bool secondTokenCompleted =
        GXReadDrawSync() == 0xbeef &&
        DrawSyncCallbackToken == 0xbeef &&
        DrawSyncCallbackCount == 2;

    GXSetDrawSyncCallback(previousCallback);
    return firstTokenCompleted && secondTokenCompleted;
}

bool TestHostPadKeyboardFallback() {
    std::array<PADStatus, PAD_MAX_CONTROLLERS> pads = {};
    if (!PADInit()) {
        return false;
    }
    PADRead(pads.data());
    return pads[0].err == PAD_ERR_NONE;
}

bool TestZScaleOffsetCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    GXSetZScaleOffset(0.5f, -0.25f);
    GXSetViewport(10.0f, 20.0f, 640.0f, 480.0f, 0.25f, 0.75f);

    const auto& state = SIM::GX::GetGlobalState();
    if (!state.HasViewportTransform()) {
        return false;
    }

    const auto& viewport = state.GetViewportTransform();
    const auto& logicalViewport = state.GetViewportState();
    const float expectedScaleZ =
        (gx->vpFarz - gx->vpNearz) * gx->zScale;
    const float expectedOffsetZ =
        gx->vpFarz * gx->zScale + gx->zOffset;

    return
        NearlyEqual(viewport[0], 320.0f) &&
        NearlyEqual(viewport[1], -240.0f) &&
        NearlyEqual(viewport[2], expectedScaleZ, 1.0f) &&
        NearlyEqual(viewport[3], 672.0f) &&
        NearlyEqual(viewport[4], 602.0f) &&
        NearlyEqual(viewport[5], expectedOffsetZ, 1.0f) &&
        logicalViewport.valid &&
        NearlyEqual(logicalViewport.left, 10.0f) &&
        NearlyEqual(logicalViewport.top, 20.0f) &&
        NearlyEqual(logicalViewport.width, 640.0f) &&
        NearlyEqual(logicalViewport.height, 480.0f);
}

bool TestPositionTextureCoordinateGeneration() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    Mtx textureMatrix = {
        {1.0f, 2.0f, 3.0f, 4.0f},
        {5.0f, 6.0f, 7.0f, 8.0f},
        {9.0f, 10.0f, 11.0f, 12.0f},
    };
    GXLoadTexMtxImm(textureMatrix, GX_TEXMTX3, GX_MTX3x4);
    GXSetTexCoordGen(
        GX_TEXCOORD0,
        GX_TG_MTX3x4,
        GX_TG_POS,
        GX_TEXMTX3);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& texGen = state.GetTexCoordGenState(0);
    const auto& decodedMatrix = state.GetTexCoordGenMatrix(0);
    return
        texGen.source == GX_TG_POS &&
        texGen.matrixId == GX_TEXMTX3 &&
        NearlyEqual(decodedMatrix[0], 1.0f) &&
        NearlyEqual(decodedMatrix[3], 4.0f) &&
        NearlyEqual(decodedMatrix[4], 5.0f) &&
        NearlyEqual(decodedMatrix[7], 8.0f) &&
        NearlyEqual(decodedMatrix[8], 9.0f) &&
        NearlyEqual(decodedMatrix[11], 12.0f);
}

bool TestTextureMatrix2x4ImplicitQ() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    Mtx textureMatrix = {
        {0.5f, 0.0f, 0.0f, 0.25f},
        {0.0f, 0.5f, 0.0f, 0.50f},
        {9.0f, 10.0f, 11.0f, 12.0f},
    };
    GXLoadTexMtxImm(textureMatrix, GX_TEXMTX0, GX_MTX2x4);
    GXSetTexCoordGen(
        GX_TEXCOORD0,
        GX_TG_MTX2x4,
        GX_TG_TEX0,
        GX_TEXMTX0);

    const auto& matrix =
        SIM::GX::GetGlobalState().GetTexCoordGenMatrix(0);
    return
        NearlyEqual(matrix[0], 0.5f) &&
        NearlyEqual(matrix[3], 0.25f) &&
        NearlyEqual(matrix[5], 0.5f) &&
        NearlyEqual(matrix[7], 0.50f) &&
        NearlyEqual(matrix[8], 0.0f) &&
        NearlyEqual(matrix[9], 0.0f) &&
        NearlyEqual(matrix[10], 0.0f) &&
        NearlyEqual(matrix[11], 1.0f);
}

bool TestHostPerformanceMetrics() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();
    __cpReg = __CPRegs;
    __peReg = __PERegs;
    __memReg = __MEMRegs;

    GXClearMemMetric();
    GXSetGPMetric(GX_PERF0_CLOCKS, GX_PERF1_VERTICES);
    GXClearGPMetric();
    __GXHostRecordPrimitive(GX_TRIANGLES, 6, 1);

    u32 clocks = 0;
    u32 vertices = 0;
    GXReadGPMetric(&clocks, &vertices);

    u32 topIn = 0;
    u32 topOut = 0;
    u32 bottomIn = 0;
    u32 bottomOut = 0;
    u32 colorIn = 0;
    u32 copyClocks = 0;
    GXReadPixMetric(
        &topIn,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);

    u32 cpRequests = 0;
    u32 textureRequests = 0;
    u32 unused[8] = {};
    GXReadMemMetric(
        &cpRequests,
        &textureRequests,
        &unused[0],
        &unused[1],
        &unused[2],
        &unused[3],
        &unused[4],
        &unused[5],
        &unused[6],
        &unused[7]);

    GXClearPixMetric();
    u32 beforeQueuedReset = 0;
    GXReadPixMetric(
        &beforeQueuedReset,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);
    __GXHostRecordPrimitive(GX_TRIANGLES, 3, 1);
    u32 afterQueuedReset = 0;
    GXReadPixMetric(
        &afterQueuedReset,
        &topOut,
        &bottomIn,
        &bottomOut,
        &colorIn,
        &copyClocks);

    return
        clocks > 0 &&
        vertices == 6 &&
        topIn > 0 &&
        topOut == afterQueuedReset &&
        textureRequests > 0 &&
        cpRequests > 0 &&
        beforeQueuedReset == topIn &&
        afterQueuedReset > 0 &&
        afterQueuedReset < beforeQueuedReset;
}

bool TestResetWritePipeCompatibility() {
    GXSetResetWritePipe(GX_TRUE);
    GXSetResetWritePipe(GX_FALSE);
    return true;
}

bool TestXfChannelAndLightState() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const GXColor ambient = {16, 32, 48, 64};
    const GXColor material = {192, 128, 64, 255};
    const GXColor lightColor = {255, 224, 192, 160};
    GXLightObj light = {};

    GXSetChanAmbColor(GX_COLOR0A0, ambient);
    GXSetChanMatColor(GX_COLOR0A0, material);
    GXSetChanCtrl(
        GX_COLOR0A0,
        GX_ENABLE,
        GX_SRC_REG,
        GX_SRC_REG,
        GX_LIGHT0 | GX_LIGHT5,
        GX_DF_CLAMP,
        GX_AF_NONE);
    GXInitLightColor(&light, lightColor);
    GXInitLightSpot(&light, 0.0f, GX_SP_OFF);
    GXInitLightDistAttn(&light, 0.0f, 0.0f, GX_DA_OFF);
    GXInitLightPos(&light, 1.0f, 2.0f, 3.0f);
    GXLoadLightObjImm(&light, GX_LIGHT0);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& channel = state.GetChannelState(0);
    const auto& loadedLight = state.GetLightState(0);
    return
        NearlyEqual(channel.ambientColor[0], 16.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[1], 32.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[2], 48.0f / 255.0f) &&
        NearlyEqual(channel.ambientColor[3], 64.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[0], 192.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[1], 128.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[2], 64.0f / 255.0f) &&
        NearlyEqual(channel.materialColor[3], 1.0f) &&
        channel.colorControl.lightingEnabled &&
        channel.colorControl.materialSource == GX_SRC_REG &&
        channel.colorControl.ambientSource == GX_SRC_REG &&
        channel.colorControl.lightMask == (GX_LIGHT0 | GX_LIGHT5) &&
        channel.colorControl.diffuseFunction == GX_DF_CLAMP &&
        channel.colorControl.attenuationFunction == GX_AF_NONE &&
        loadedLight.valid &&
        NearlyEqual(loadedLight.color[0], 1.0f) &&
        NearlyEqual(loadedLight.color[1], 224.0f / 255.0f) &&
        NearlyEqual(loadedLight.color[2], 192.0f / 255.0f) &&
        NearlyEqual(loadedLight.color[3], 160.0f / 255.0f) &&
        NearlyEqual(loadedLight.cosineAttenuation[0], 1.0f) &&
        NearlyEqual(loadedLight.distanceAttenuation[0], 1.0f) &&
        NearlyEqual(loadedLight.position[0], 1.0f) &&
        NearlyEqual(loadedLight.position[1], 2.0f) &&
        NearlyEqual(loadedLight.position[2], 3.0f);
}

void SendBpRegister(u32 value) {
    SIM_GX_CommandProcessor_SendU8(0x61);
    SIM_GX_CommandProcessor_SendU32(value);
}

bool TestBpRenderStateCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    const u32 blendValue =
        0x41000000u |
        (1u << 0) |
        (1u << 2) |
        (1u << 3) |
        (1u << 4) |
        (static_cast<u32>(GX_BL_INVSRCALPHA) << 5) |
        (static_cast<u32>(GX_BL_SRCALPHA) << 8) |
        (static_cast<u32>(GX_LO_XOR) << 12);
    SendBpRegister(blendValue);
    SendBpRegister(
        0x40000000u |
        1u |
        (static_cast<u32>(GX_GREATER) << 1) |
        (1u << 4));
    // GX writes API back-face culling as hardware mode 1.
    SendBpRegister(0x00000000u | (1u << 14));
    SendBpRegister(0x22000000u | 12u | (18u << 8));
    SendBpRegister(
        0xc0000000u |
        (static_cast<u32>(GX_CC_ZERO) << 12) |
        (static_cast<u32>(GX_CC_TEXC) << 8) |
        (static_cast<u32>(GX_CC_RASC) << 4) |
        static_cast<u32>(GX_CC_ZERO));
    SendBpRegister(
        0xf3000000u |
        60u |
        (40u << 8) |
        (static_cast<u32>(GX_LESS) << 16) |
        (static_cast<u32>(GX_GREATER) << 19) |
        (static_cast<u32>(GX_AOP_XOR) << 22));

    const auto& state = SIM::GX::GetGlobalState();
    const auto& blend = state.GetBlendState();
    const auto& depth = state.GetDepthState();
    const auto& alphaCompare = state.GetAlphaCompareState();
    const auto& raster = state.GetRasterState();
    const auto& firstStage = state.GetTevStageState(0);
    return
        blend.mode == GX_BM_BLEND &&
        blend.sourceFactor == GX_BL_SRCALPHA &&
        blend.destinationFactor == GX_BL_INVSRCALPHA &&
        blend.logicOperation == GX_LO_XOR &&
        blend.colorUpdateEnabled &&
        blend.alphaUpdateEnabled &&
        blend.ditherEnabled &&
        depth.compareEnabled &&
        depth.function == GX_GREATER &&
        depth.updateEnabled &&
        alphaCompare.comparison0 == GX_LESS &&
        alphaCompare.reference0 == 60 &&
        alphaCompare.operation == GX_AOP_XOR &&
        alphaCompare.comparison1 == GX_GREATER &&
        alphaCompare.reference1 == 40 &&
        raster.cullMode == GX_CULL_BACK &&
        NearlyEqual(raster.lineWidth, 2.0f) &&
        NearlyEqual(raster.pointSize, 3.0f) &&
        firstStage.colorMode == SIM::GX::TevColorMode::Modulate;
}

bool TestBpCopyClearCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    SendBpRegister(0x4f000000u | 0x20u | (0x80u << 8));
    SendBpRegister(0x50000000u | 0x60u | (0x40u << 8));
    SendBpRegister(0x51000000u | 0x007fffffu);
    SendBpRegister(0x52000000u | (1u << 11) | (1u << 14));

    auto& state = SIM::GX::GetGlobalState();
    const auto& color = state.GetCopyClearColor();
    return
        NearlyEqual(color[0], 0x20 / 255.0f) &&
        NearlyEqual(color[1], 0x40 / 255.0f) &&
        NearlyEqual(color[2], 0x60 / 255.0f) &&
        NearlyEqual(color[3], 0x80 / 255.0f) &&
        NearlyEqual(state.GetCopyClearDepth(), 0x7fffff / 16777215.0f) &&
        state.ConsumeCopyClearRequest() &&
        !state.ConsumeCopyClearRequest();
}

bool TestDisplayCopyRebasesViewportReference() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    // GX initialization briefly uses 480 lines before this NTSC-style demo
    // switches to a 448-line EFB and scales it into the display XFB.
    GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
    GXSetViewport(0.0f, 0.0f, 640.0f, 448.0f, 0.0f, 1.0f);
    GXSetScissor(0, 0, 640, 448);

    SendBpRegister(0x49000000u);
    SendBpRegister(
        0x4a000000u |
        (640u - 1u) |
        ((448u - 1u) << 10u));
    SendBpRegister(0x52000000u | (1u << 14));

    const auto& viewport = SIM::GX::GetGlobalState().GetViewportState();
    return
        NearlyEqual(viewport.referenceWidth, 640.0f) &&
        NearlyEqual(viewport.referenceHeight, 448.0f);
}

bool TestBpTextureAndScissorCommands() {
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    SendBpRegister(
        0x28000000u |
        (3u << 0) |
        (2u << 3) |
        (1u << 6));

    const u32 encodedTop = 20u + 342u;
    const u32 encodedLeft = 10u + 342u;
    const u32 encodedBottom = encodedTop + 200u - 1u;
    const u32 encodedRight = encodedLeft + 300u - 1u;
    SendBpRegister(
        0x20000000u |
        encodedTop |
        (encodedLeft << 12));
    SendBpRegister(
        0x21000000u |
        encodedBottom |
        (encodedRight << 12));

    alignas(32) std::array<u8, 32> image = {};
    SIM_GX_CommandProcessor_LoadTexture(
        3,
        image.data(),
        8,
        8,
        GX_TF_I4,
        GX_CLAMP,
        GX_REPEAT,
        GX_NEAR,
        GX_LINEAR);

    const auto& state = SIM::GX::GetGlobalState();
    const auto& stage = state.GetTevStageState(0);
    const auto& scissor = state.GetScissorState();
    const auto& texture = state.GetTextureState(3);
    return
        stage.textureEnabled &&
        stage.textureMap == 3 &&
        stage.textureCoordinate == 2 &&
        scissor.valid &&
        scissor.left == 10 &&
        scissor.top == 20 &&
        scissor.width == 300 &&
        scissor.height == 200 &&
        texture.data == image.data() &&
        texture.width == 8 &&
        texture.height == 8 &&
        texture.format == GX_TF_I4 &&
        texture.wrapS == GX_CLAMP &&
        texture.wrapT == GX_REPEAT &&
        texture.minFilter == GX_NEAR &&
        texture.magFilter == GX_LINEAR;
}

}

int main() {
    if (!TestIndependentTextureLodSetters()) {
        return 1;
    }
    if (!TestHostTexturePointerPreserved()) {
        return 2;
    }
    if (!TestFifoQueries()) {
        return 3;
    }
    if (!TestHostDrawSyncCompletion()) {
        return 4;
    }
    if (!TestHostPadKeyboardFallback()) {
        return 5;
    }
    if (!TestZScaleOffsetCommands()) {
        return 6;
    }
    if (!TestResetWritePipeCompatibility()) {
        return 7;
    }
    if (!TestXfChannelAndLightState()) {
        return 8;
    }
    if (!TestBpRenderStateCommands()) {
        return 9;
    }
    if (!TestBpCopyClearCommands()) {
        return 10;
    }
    if (!TestDisplayCopyRebasesViewportReference()) {
        return 11;
    }
    if (!TestBpTextureAndScissorCommands()) {
        return 12;
    }
    if (!TestTextureObjectAbiBounds()) {
        return 13;
    }
    if (!TestPositionTextureCoordinateGeneration()) {
        return 14;
    }
    if (!TestTextureMatrix2x4ImplicitQ()) {
        return 15;
    }
    if (!TestHostPerformanceMetrics()) {
        return 16;
    }
    return 0;
}
