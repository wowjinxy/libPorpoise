#include <simulator/sim_gx_GlRenderer.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>

namespace {

float ClampUnit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

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

float Length(const SIM::GX::RenderVector3& vector) {
    return std::sqrt(Dot(vector, vector));
}

SIM::GX::RenderVector3 Normalize(
    const SIM::GX::RenderVector3& vector) {
    const float length = Length(vector);
    if (length <= 0.000001f) {
        return {};
    }
    return {
        vector.x / length,
        vector.y / length,
        vector.z / length,
    };
}

void ApplyUnlitChannel(
    const SIM::GX::ChannelState& channel,
    SIM::GX::RenderColor& vertexColor) {
    const float* colorMaterial =
        channel.colorControl.materialSource == GX_SRC_VTX
            ? vertexColor.Data()
            : channel.materialColor.data();
    const float alphaMaterial =
        channel.alphaControl.materialSource == GX_SRC_VTX
            ? vertexColor.a
            : channel.materialColor[3];
    vertexColor = {
        ClampUnit(colorMaterial[0]),
        ClampUnit(colorMaterial[1]),
        ClampUnit(colorMaterial[2]),
        ClampUnit(alphaMaterial),
    };
}

SIM::GX::RenderColor EvaluateChannelLighting(
    const SIM::GX::GlobalState& state,
    const SIM::GX::ChannelState& channel,
    const SIM::GX::RenderColor& vertexColor,
    const SIM::GX::RenderVector3& viewPosition,
    const SIM::GX::RenderVector3& viewNormal) {
    const auto evaluateComponent = [&](
        size_t component,
        const SIM::GX::ChannelControlState& control) {
        const float* vertexComponents = vertexColor.Data();
        const float material =
            control.materialSource == GX_SRC_VTX
                ? vertexComponents[component]
                : channel.materialColor[component];
        if (!control.lightingEnabled) {
            return ClampUnit(material);
        }

        float lightAccumulation =
            control.ambientSource == GX_SRC_VTX
                ? vertexComponents[component]
                : channel.ambientColor[component];

        for (size_t lightIndex = 0; lightIndex < 8; ++lightIndex) {
            if ((control.lightMask & (1u << lightIndex)) == 0u) {
                continue;
            }
            const auto& light = state.GetLightState(lightIndex);
            if (!light.valid) {
                continue;
            }

            const SIM::GX::RenderVector3 lightDelta = {
                light.position[0] - viewPosition.x,
                light.position[1] - viewPosition.y,
                light.position[2] - viewPosition.z,
            };
            const float distance = Length(lightDelta);
            const auto vertexToLight = Normalize(lightDelta);
            const float normalDotLight = Dot(viewNormal, vertexToLight);

            float diffuse = 1.0f;
            switch (control.diffuseFunction) {
                case GX_DF_SIGN:
                    diffuse = normalDotLight;
                    break;
                case GX_DF_CLAMP:
                    diffuse = std::max(0.0f, normalDotLight);
                    break;
                case GX_DF_NONE:
                default:
                    break;
            }

            float attenuation = 1.0f;
            if (control.attenuationFunction == GX_AF_SPOT) {
                const SIM::GX::RenderVector3 hardwareDirection = {
                    light.direction[0],
                    light.direction[1],
                    light.direction[2],
                };
                const float cosine =
                    std::max(0.0f, Dot(vertexToLight, hardwareDirection));
                const float cosineAttenuation = std::max(
                    0.0f,
                    light.cosineAttenuation[0] +
                        light.cosineAttenuation[1] * cosine +
                        light.cosineAttenuation[2] * cosine * cosine);
                const float distanceAttenuation =
                    light.distanceAttenuation[0] +
                    light.distanceAttenuation[1] * distance +
                    light.distanceAttenuation[2] * distance * distance;
                attenuation = distanceAttenuation > 0.000001f
                    ? cosineAttenuation / distanceAttenuation
                    : 0.0f;
            } else if (control.attenuationFunction == GX_AF_SPEC) {
                const SIM::GX::RenderVector3 halfAngle = {
                    light.direction[0],
                    light.direction[1],
                    light.direction[2],
                };
                // GX_AF_SPEC uses the light direction as a precomputed
                // half-angle.  Back-facing lights contribute no specular
                // term.  The cosine coefficients form the numerator and the
                // distance coefficients form a second polynomial in that
                // same specular angle (not the vertex/light distance).
                const float specularAngle = normalDotLight >= 0.0f
                    ? std::max(0.0f, Dot(viewNormal, halfAngle))
                    : 0.0f;
                const float cosineAttenuation = std::max(
                    0.0f,
                    light.cosineAttenuation[0] +
                        light.cosineAttenuation[1] * specularAngle +
                        light.cosineAttenuation[2] *
                            specularAngle * specularAngle);

                SIM::GX::RenderVector3 distanceCoefficients = {
                    light.distanceAttenuation[0],
                    light.distanceAttenuation[1],
                    light.distanceAttenuation[2],
                };
                if (control.diffuseFunction != GX_DF_NONE) {
                    distanceCoefficients = Normalize(distanceCoefficients);
                }
                const float distanceAttenuation =
                    distanceCoefficients.x +
                    distanceCoefficients.y * specularAngle +
                    distanceCoefficients.z *
                        specularAngle * specularAngle;
                attenuation = distanceAttenuation > 0.000001f
                    ? cosineAttenuation / distanceAttenuation
                    : 0.0f;
            }

            lightAccumulation +=
                light.color[component] * diffuse * attenuation;
        }

        return ClampUnit(material * ClampUnit(lightAccumulation));
    };

    return {
        evaluateComponent(0, channel.colorControl),
        evaluateComponent(1, channel.colorControl),
        evaluateComponent(2, channel.colorControl),
        evaluateComponent(3, channel.alphaControl),
    };
}

}

namespace SIM::GX {

void ApplyColorChannels(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices) {
    const size_t channelCount =
        std::min<size_t>(state.GetNumColorChannels(), 2u);
    if (channelCount == 0u || vertices.empty()) {
        return;
    }

    std::array<const ChannelState*, 2> channels = {};
    std::array<bool, 2> channelUsesLighting = {};
    bool anyChannelUsesLighting = false;
    for (size_t channelIndex = 0; channelIndex < channelCount;
         ++channelIndex) {
        channels[channelIndex] = &state.GetChannelState(channelIndex);
        channelUsesLighting[channelIndex] =
            channels[channelIndex]->colorControl.lightingEnabled ||
            channels[channelIndex]->alphaControl.lightingEnabled;
        anyChannelUsesLighting =
            anyChannelUsesLighting || channelUsesLighting[channelIndex];
    }

    // Unlit GX channels are only a material-source selection. They do not
    // consume positions, normals, matrices, or lights, so keep that common
    // path free of all model-view work in unoptimized host builds.
    if (!anyChannelUsesLighting) {
        for (auto& vertex : vertices) {
            for (size_t channelIndex = 0; channelIndex < channelCount;
                 ++channelIndex) {
                auto& color = channelIndex == 0u
                    ? vertex.color0
                    : vertex.color1;
                ApplyUnlitChannel(*channels[channelIndex], color);
            }
        }
        return;
    }

    const bool indexedMatrix =
        state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE;
    for (auto& vertex : vertices) {
        RenderVector3 viewPosition = {};
        RenderVector3 viewNormal = {};
        const size_t matrixIndex = indexedMatrix
            ? static_cast<size_t>(vertex.positionMatrixIndex / 3u)
            : 0u;
        const auto& modelView = indexedMatrix
            ? state.GetPositionMatrix(matrixIndex)
            : state.GetPositionMatrix();
        const auto& normalMatrix = indexedMatrix
            ? state.GetNormalMatrix(matrixIndex)
            : state.GetNormalMatrix();
        viewPosition = TransformPoint(modelView, vertex.position);
        viewNormal = Normalize(
            TransformDirection(normalMatrix, vertex.normal));

        for (size_t channelIndex = 0; channelIndex < channelCount;
             ++channelIndex) {
            auto& color = channelIndex == 0u
                ? vertex.color0
                : vertex.color1;
            if (channelUsesLighting[channelIndex]) {
                color = EvaluateChannelLighting(
                    state,
                    *channels[channelIndex],
                    color,
                    viewPosition,
                    viewNormal);
            } else {
                ApplyUnlitChannel(*channels[channelIndex], color);
            }
        }
    }
}

}
