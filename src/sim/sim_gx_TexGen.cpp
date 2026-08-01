#include <simulator/sim_gx_GlRenderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>

namespace {

SIM::GX::RenderVector3 TransformPoint(
    const std::array<float, 16>& matrix,
    const SIM::GX::RenderVector3& point) {
    return {
        matrix[0] * point.x + matrix[1] * point.y +
            matrix[2] * point.z + matrix[3],
        matrix[4] * point.x + matrix[5] * point.y +
            matrix[6] * point.z + matrix[7],
        matrix[8] * point.x + matrix[9] * point.y +
            matrix[10] * point.z + matrix[11],
    };
}

SIM::GX::RenderVector3 TransformDirection(
    const std::array<float, 16>& matrix,
    const SIM::GX::RenderVector3& direction) {
    return {
        matrix[0] * direction.x + matrix[1] * direction.y +
            matrix[2] * direction.z,
        matrix[4] * direction.x + matrix[5] * direction.y +
            matrix[6] * direction.z,
        matrix[8] * direction.x + matrix[9] * direction.y +
            matrix[10] * direction.z,
    };
}

float Dot(
    const SIM::GX::RenderVector3& left,
    const SIM::GX::RenderVector3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

SIM::GX::RenderVector3 Normalize(
    const SIM::GX::RenderVector3& vector) {
    const float length = std::sqrt(Dot(vector, vector));
    if (length <= 0.000001f) {
        return {};
    }
    return {
        vector.x / length,
        vector.y / length,
        vector.z / length,
    };
}

}

namespace SIM::GX {

void ApplyTextureCoordinateGeneration(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices) {
    const size_t generatorCount =
        std::min<size_t>(state.GetNumTexGens(), 8u);
    if (generatorCount == 0u) {
        return;
    }

    bool needsBumpVectors = false;
    for (size_t index = 0; index < generatorCount; ++index) {
        const auto function =
            state.GetTexCoordGenState(index).function;
        if (function >= GX_TG_BUMP0 && function <= GX_TG_BUMP7) {
            needsBumpVectors = true;
            break;
        }
    }

    for (auto& vertex : vertices) {
        const auto sourceTexCoords = vertex.texCoords;
        auto generated = sourceTexCoords;

        RenderVector3 viewPosition = {};
        RenderVector3 viewBinormal = {};
        RenderVector3 viewTangent = {};
        if (needsBumpVectors) {
            const auto& modelView =
                state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE
                    ? state.GetPositionMatrix(
                        static_cast<size_t>(
                            vertex.positionMatrixIndex / 3u))
                    : state.GetPositionMatrix();
            viewPosition = TransformPoint(modelView, vertex.position);
            viewBinormal = Normalize(
                TransformDirection(modelView, vertex.binormal));
            viewTangent = Normalize(
                TransformDirection(modelView, vertex.tangent));
        }

        for (size_t index = 0; index < generatorCount; ++index) {
            const auto& texGen = state.GetTexCoordGenState(index);
            if (texGen.function >= GX_TG_BUMP0 &&
                texGen.function <= GX_TG_BUMP7) {
                const size_t sourceIndex =
                    std::min<size_t>(
                        texGen.embossSource,
                        generated.size() - 1u);
                const size_t lightIndex =
                    std::min<size_t>(texGen.embossLight, 7u);
                const auto& light = state.GetLightState(lightIndex);
                const auto lightDirection = Normalize({
                    light.position[0] - viewPosition.x,
                    light.position[1] - viewPosition.y,
                    light.position[2] - viewPosition.z,
                });
                generated[index] = generated[sourceIndex];
                generated[index].s +=
                    Dot(lightDirection, viewBinormal);
                generated[index].t +=
                    Dot(lightDirection, viewTangent);
                continue;
            }

            if (texGen.function == GX_TG_SRTG) {
                const auto& sourceColor =
                    texGen.source == GX_TG_COLOR1
                        ? vertex.color1
                        : vertex.color0;
                generated[index] = {
                    sourceColor.r,
                    sourceColor.g,
                    1.0f,
                };
                continue;
            }

            std::array<float, 4> source = {
                0.0f,
                0.0f,
                0.0f,
                1.0f,
            };
            if (texGen.source == GX_TG_POS) {
                source = {
                    vertex.position.x,
                    vertex.position.y,
                    vertex.position.z,
                    1.0f,
                };
            } else if (texGen.source == GX_TG_NRM) {
                source = {
                    vertex.normal.x,
                    vertex.normal.y,
                    vertex.normal.z,
                    1.0f,
                };
            } else if (texGen.source == GX_TG_BINRM) {
                source = {
                    vertex.binormal.x,
                    vertex.binormal.y,
                    vertex.binormal.z,
                    1.0f,
                };
            } else if (texGen.source == GX_TG_TANGENT) {
                source = {
                    vertex.tangent.x,
                    vertex.tangent.y,
                    vertex.tangent.z,
                    1.0f,
                };
            } else if (
                texGen.source >= GX_TG_TEX0 &&
                texGen.source <= GX_TG_TEX7) {
                const auto& coordinate =
                    sourceTexCoords[static_cast<size_t>(
                        texGen.source - GX_TG_TEX0)];
                source = {
                    coordinate.s,
                    coordinate.t,
                    coordinate.q,
                    1.0f,
                };
            } else if (
                texGen.source >= GX_TG_TEXCOORD0 &&
                texGen.source <= GX_TG_TEXCOORD6) {
                const auto& coordinate =
                    generated[static_cast<size_t>(
                        texGen.source - GX_TG_TEXCOORD0)];
                source = {
                    coordinate.s,
                    coordinate.t,
                    coordinate.q,
                    1.0f,
                };
            } else if (
                texGen.source == GX_TG_COLOR0 ||
                texGen.source == GX_TG_COLOR1) {
                const auto& sourceColor =
                    texGen.source == GX_TG_COLOR1
                        ? vertex.color1
                        : vertex.color0;
                source = {
                    sourceColor.r,
                    sourceColor.g,
                    0.0f,
                    1.0f,
                };
            }

            const auto& matrix = state.GetTexCoordGenMatrix(index);
            generated[index].s =
                matrix[0] * source[0] + matrix[1] * source[1] +
                matrix[2] * source[2] + matrix[3] * source[3];
            generated[index].t =
                matrix[4] * source[0] + matrix[5] * source[1] +
                matrix[6] * source[2] + matrix[7] * source[3];
            generated[index].q =
                texGen.function == GX_TG_MTX3x4
                    ? matrix[8] * source[0] +
                        matrix[9] * source[1] +
                        matrix[10] * source[2] +
                        matrix[11] * source[3]
                    : 1.0f;

            if (texGen.normalize) {
                const float length = std::sqrt(
                    generated[index].s * generated[index].s +
                    generated[index].t * generated[index].t +
                    generated[index].q * generated[index].q);
                if (length > 0.00000001f) {
                    generated[index].s /= length;
                    generated[index].t /= length;
                    generated[index].q /= length;
                }
            }

            const auto& postMatrix =
                state.GetTexCoordGenPostMatrix(index);
            const std::array<float, 4> postSource = {
                generated[index].s,
                generated[index].t,
                generated[index].q,
                1.0f,
            };
            generated[index].s =
                postMatrix[0] * postSource[0] +
                postMatrix[1] * postSource[1] +
                postMatrix[2] * postSource[2] +
                postMatrix[3] * postSource[3];
            generated[index].t =
                postMatrix[4] * postSource[0] +
                postMatrix[5] * postSource[1] +
                postMatrix[6] * postSource[2] +
                postMatrix[7] * postSource[3];
            generated[index].q =
                postMatrix[8] * postSource[0] +
                postMatrix[9] * postSource[1] +
                postMatrix[10] * postSource[2] +
                postMatrix[11] * postSource[3];
        }

        vertex.texCoords = generated;
    }
}

}
