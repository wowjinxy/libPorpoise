#include <simulator/sim_gx_GlRenderer.hpp>

#include <array>
#include <cstddef>
#include <cstring>

namespace SIM::GX::Detail {

namespace {

bool SameUniformValue(const float& left, const float& right) {
    static_assert(sizeof(float) == sizeof(u32));
    u32 leftBits = 0u;
    u32 rightBits = 0u;
    std::memcpy(&leftBits, &left, sizeof(leftBits));
    std::memcpy(&rightBits, &right, sizeof(rightBits));
    return leftBits == rightBits;
}

template <size_t Size>
bool SameUniformValue(
    const std::array<float, Size>& left,
    const std::array<float, Size>& right) {
    for (size_t index = 0; index < Size; ++index) {
        if (!SameUniformValue(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

template <typename Value>
bool SameUniformValue(const Value& left, const Value& right) {
    return left == right;
}

}

u64 ShaderUniformValueCache::Update(const ShaderUniformValues& values) {
    u64 dirty = 0u;
    const auto update = [this, &dirty](
                            ShaderUniform uniform,
                            auto& cached,
                            const auto& current) {
        if (!mValid || !SameUniformValue(cached, current)) {
            dirty |= ShaderUniformMask(uniform);
            cached = current;
        }
    };

    update(ShaderUniform::Projection, mValues.projection, values.projection);
    update(ShaderUniform::ModelView, mValues.modelView, values.modelView);
    update(
        ShaderUniform::NumTevStages,
        mValues.numTevStages,
        values.numTevStages);
    update(
        ShaderUniform::UseTextures,
        mValues.useTextures,
        values.useTextures);
    update(
        ShaderUniform::StageTextures,
        mValues.stageTextures,
        values.stageTextures);
    update(
        ShaderUniform::StageTexCoords,
        mValues.stageTexCoords,
        values.stageTexCoords);
    update(
        ShaderUniform::StageTexCoordScales,
        mValues.stageTexCoordScales,
        values.stageTexCoordScales);
    update(
        ShaderUniform::StageRasterChannels,
        mValues.stageRasterChannels,
        values.stageRasterChannels);
    update(
        ShaderUniform::TevColorInputs,
        mValues.tevColorInputs,
        values.tevColorInputs);
    update(
        ShaderUniform::TevAlphaInputs,
        mValues.tevAlphaInputs,
        values.tevAlphaInputs);
    update(
        ShaderUniform::TevColorOperations,
        mValues.tevColorOperations,
        values.tevColorOperations);
    update(
        ShaderUniform::TevAlphaOperations,
        mValues.tevAlphaOperations,
        values.tevAlphaOperations);
    update(
        ShaderUniform::TevOutputRegisters,
        mValues.tevOutputRegisters,
        values.tevOutputRegisters);
    update(
        ShaderUniform::TevSwapSelectors,
        mValues.tevSwapSelectors,
        values.tevSwapSelectors);
    update(
        ShaderUniform::TevSwapTables,
        mValues.tevSwapTables,
        values.tevSwapTables);
    update(
        ShaderUniform::TevRegisters,
        mValues.tevRegisters,
        values.tevRegisters);
    update(
        ShaderUniform::TevKonstColors,
        mValues.tevKonstColors,
        values.tevKonstColors);
    update(
        ShaderUniform::TevKonstAlphas,
        mValues.tevKonstAlphas,
        values.tevKonstAlphas);
    update(
        ShaderUniform::AlphaComparison0,
        mValues.alphaComparison0,
        values.alphaComparison0);
    update(
        ShaderUniform::AlphaReference0,
        mValues.alphaReference0,
        values.alphaReference0);
    update(
        ShaderUniform::AlphaOperation,
        mValues.alphaOperation,
        values.alphaOperation);
    update(
        ShaderUniform::AlphaComparison1,
        mValues.alphaComparison1,
        values.alphaComparison1);
    update(
        ShaderUniform::AlphaReference1,
        mValues.alphaReference1,
        values.alphaReference1);
    update(ShaderUniform::FogType, mValues.fogType, values.fogType);
    update(
        ShaderUniform::FogOrthographic,
        mValues.fogOrthographic,
        values.fogOrthographic);
    update(ShaderUniform::FogA, mValues.fogA, values.fogA);
    update(ShaderUniform::FogB, mValues.fogB, values.fogB);
    update(ShaderUniform::FogC, mValues.fogC, values.fogC);
    update(ShaderUniform::FogColor, mValues.fogColor, values.fogColor);
    update(
        ShaderUniform::FogRangeEnabled,
        mValues.fogRangeEnabled,
        values.fogRangeEnabled);
    update(
        ShaderUniform::FogRangeCenter,
        mValues.fogRangeCenter,
        values.fogRangeCenter);
    update(
        ShaderUniform::FogRangeTable,
        mValues.fogRangeTable,
        values.fogRangeTable);
    update(
        ShaderUniform::FogXScale,
        mValues.fogXScale,
        values.fogXScale);
    update(
        ShaderUniform::ZTextureOperation,
        mValues.zTextureOperation,
        values.zTextureOperation);
    update(
        ShaderUniform::ZTextureFormat,
        mValues.zTextureFormat,
        values.zTextureFormat);
    update(
        ShaderUniform::ZTextureBias,
        mValues.zTextureBias,
        values.zTextureBias);

    mValid = true;
    return dirty;
}

}
