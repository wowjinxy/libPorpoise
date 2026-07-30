#include <simulator/sim_gx_GlRenderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>

extern "C" void __VIHostOnDraw(void) __attribute__((weak));

namespace {

std::vector<SIM::GX::RenderVertex> ExpandQuads(
    const std::vector<SIM::GX::RenderVertex>& vertices) {
    std::vector<SIM::GX::RenderVertex> triangles;
    triangles.reserve((vertices.size() / 4) * 6);
    for (size_t i = 0; i + 3 < vertices.size(); i += 4) {
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 1]);
        triangles.push_back(vertices[i + 2]);
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 2]);
        triangles.push_back(vertices[i + 3]);
    }
    return triangles;
}

std::vector<SIM::GX::RenderVertex> ExpandQuadStrip(
    const std::vector<SIM::GX::RenderVertex>& vertices) {
    std::vector<SIM::GX::RenderVertex> triangles;
    if (vertices.size() < 4) {
        return triangles;
    }

    triangles.reserve(((vertices.size() - 2) / 2) * 6);
    for (size_t i = 0; i + 3 < vertices.size(); i += 2) {
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 1]);
        triangles.push_back(vertices[i + 3]);
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 3]);
        triangles.push_back(vertices[i + 2]);
    }
    return triangles;
}

GLenum ToGlPrimitive(GXPrimitive primitive) {
    switch (primitive) {
        case GX_POINTS:
            return GL_POINTS;
        case GX_LINES:
            return GL_LINES;
        case GX_LINESTRIP:
            return GL_LINE_STRIP;
        case GX_TRIANGLESTRIP:
            return GL_TRIANGLE_STRIP;
        case GX_TRIANGLEFAN:
            return GL_TRIANGLE_FAN;
        case GX_TRIANGLES:
        case GX_QUADS:
        case GX_QUADSTRIP:
        default:
            return GL_TRIANGLES;
    }
}

GLenum ToGlCompare(GXCompare function) {
    switch (function) {
        case GX_NEVER:
            return GL_NEVER;
        case GX_LESS:
            return GL_LESS;
        case GX_EQUAL:
            return GL_EQUAL;
        case GX_LEQUAL:
            return GL_LEQUAL;
        case GX_GREATER:
            return GL_GREATER;
        case GX_NEQUAL:
            return GL_NOTEQUAL;
        case GX_GEQUAL:
            return GL_GEQUAL;
        case GX_ALWAYS:
            return GL_ALWAYS;
        default:
            return GL_LEQUAL;
    }
}

GLenum ToGlSourceBlendFactor(GXBlendFactor factor) {
    switch (factor) {
        case GX_BL_ZERO:
            return GL_ZERO;
        case GX_BL_ONE:
            return GL_ONE;
        case GX_BL_SRCCOL:
            return GL_DST_COLOR;
        case GX_BL_INVSRCCOL:
            return GL_ONE_MINUS_DST_COLOR;
        case GX_BL_SRCALPHA:
            return GL_SRC_ALPHA;
        case GX_BL_INVSRCALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case GX_BL_DSTALPHA:
            return GL_DST_ALPHA;
        case GX_BL_INVDSTALPHA:
            return GL_ONE_MINUS_DST_ALPHA;
        default:
            return GL_ONE;
    }
}

GLenum ToGlDestinationBlendFactor(GXBlendFactor factor) {
    switch (factor) {
        case GX_BL_ZERO:
            return GL_ZERO;
        case GX_BL_ONE:
            return GL_ONE;
        case GX_BL_DSTCOL:
            return GL_SRC_COLOR;
        case GX_BL_INVDSTCOL:
            return GL_ONE_MINUS_SRC_COLOR;
        case GX_BL_SRCALPHA:
            return GL_SRC_ALPHA;
        case GX_BL_INVSRCALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case GX_BL_DSTALPHA:
            return GL_DST_ALPHA;
        case GX_BL_INVDSTALPHA:
            return GL_ONE_MINUS_DST_ALPHA;
        default:
            return GL_ZERO;
    }
}

GLenum ToGlLogicOperation(GXLogicOp operation) {
    switch (operation) {
        case GX_LO_CLEAR:
            return GL_CLEAR;
        case GX_LO_AND:
            return GL_AND;
        case GX_LO_REVAND:
            return GL_AND_REVERSE;
        case GX_LO_COPY:
            return GL_COPY;
        case GX_LO_INVAND:
            return GL_AND_INVERTED;
        case GX_LO_NOOP:
            return GL_NOOP;
        case GX_LO_XOR:
            return GL_XOR;
        case GX_LO_OR:
            return GL_OR;
        case GX_LO_NOR:
            return GL_NOR;
        case GX_LO_EQUIV:
            return GL_EQUIV;
        case GX_LO_INV:
            return GL_INVERT;
        case GX_LO_REVOR:
            return GL_OR_REVERSE;
        case GX_LO_INVCOPY:
            return GL_COPY_INVERTED;
        case GX_LO_INVOR:
            return GL_OR_INVERTED;
        case GX_LO_NAND:
            return GL_NAND;
        case GX_LO_SET:
            return GL_SET;
        default:
            return GL_COPY;
    }
}

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

SIM::GX::RenderColor ArrayToColor(const std::array<float, 4>& color) {
    return {color[0], color[1], color[2], color[3]};
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
        const SIM::GX::RenderColor materialRegister =
            ArrayToColor(channel.materialColor);
        const SIM::GX::RenderColor ambientRegister =
            ArrayToColor(channel.ambientColor);
        const float* vertexComponents = vertexColor.Data();
        const float* materialComponents = materialRegister.Data();
        const float* ambientComponents = ambientRegister.Data();
        const float material =
            control.materialSource == GX_SRC_VTX
                ? vertexComponents[component]
                : materialComponents[component];
        if (!control.lightingEnabled) {
            return ClampUnit(material);
        }

        float lightAccumulation =
            control.ambientSource == GX_SRC_VTX
                ? vertexComponents[component]
                : ambientComponents[component];

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
                const auto hardwareDirection = Normalize({
                    light.direction[0],
                    light.direction[1],
                    light.direction[2],
                });
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
                if (distanceAttenuation > 0.000001f) {
                    attenuation =
                        cosineAttenuation / distanceAttenuation;
                } else {
                    attenuation = 0.0f;
                }
            } else if (control.attenuationFunction == GX_AF_SPEC) {
                const auto halfAngle = Normalize({
                    light.direction[0],
                    light.direction[1],
                    light.direction[2],
                });
                const float cosine =
                    std::max(0.0f, Dot(viewNormal, halfAngle));
                attenuation = std::max(
                    0.0f,
                    light.distanceAttenuation[0] +
                        light.distanceAttenuation[1] * cosine +
                        light.distanceAttenuation[2] * cosine * cosine);
            }

            lightAccumulation +=
                light.color[component] * diffuse * attenuation;
        }

        return ClampUnit(material * ClampUnit(lightAccumulation));
    };

    SIM::GX::RenderColor output;
    output.r = evaluateComponent(0, channel.colorControl);
    output.g = evaluateComponent(1, channel.colorControl);
    output.b = evaluateComponent(2, channel.colorControl);
    output.a = evaluateComponent(3, channel.alphaControl);
    return output;
}

void ApplyColorChannels(
    const SIM::GX::GlobalState& state,
    std::vector<SIM::GX::RenderVertex>& vertices) {
    for (auto& vertex : vertices) {
        const bool indexedMatrix =
            state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE;
        const size_t matrixIndex = indexedMatrix
            ? static_cast<size_t>(vertex.positionMatrixIndex / 3u)
            : 0u;
        const auto& modelView = indexedMatrix
            ? state.GetPositionMatrix(matrixIndex)
            : state.GetPositionMatrix();
        const auto& normalMatrix = indexedMatrix
            ? state.GetNormalMatrix(matrixIndex)
            : state.GetNormalMatrix();
        const auto viewPosition =
            TransformPoint(modelView, vertex.position);
        const auto viewNormal =
            Normalize(TransformDirection(normalMatrix, vertex.normal));
        vertex.color0 = EvaluateChannelLighting(
            state,
            state.GetChannelState(0),
            vertex.color0,
            viewPosition,
            viewNormal);
        vertex.color1 = EvaluateChannelLighting(
            state,
            state.GetChannelState(1),
            vertex.color1,
            viewPosition,
            viewNormal);
    }
}

void ApplyTexCoordGenerators(
    const SIM::GX::GlobalState& state,
    std::vector<SIM::GX::RenderVertex>& vertices) {
    for (auto& vertex : vertices) {
        const auto& modelView =
            state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE
                ? state.GetPositionMatrix(
                    static_cast<size_t>(
                        vertex.positionMatrixIndex / 3u))
                : state.GetPositionMatrix();
        const auto sourceTexCoords = vertex.texCoords;
        std::array<SIM::GX::RenderTexCoord, 8> generated = {};
        for (auto& coordinate : generated) {
            coordinate.q = 1.0f;
        }

        const auto viewPosition =
            TransformPoint(modelView, vertex.position);
        const auto viewBinormal =
            Normalize(TransformDirection(modelView, vertex.binormal));
        const auto viewTangent =
            Normalize(TransformDirection(modelView, vertex.tangent));

        for (size_t index = 0; index < generated.size(); ++index) {
            const auto& texGen = state.GetTexCoordGenState(index);
            if (texGen.function >= GX_TG_BUMP0 &&
                texGen.function <= GX_TG_BUMP7) {
                const size_t sourceIndex =
                    std::min<size_t>(
                        texGen.embossSource,
                        generated.size() - 1u);
                const size_t lightIndex =
                    std::min<size_t>(
                        texGen.embossLight,
                        7u);
                const auto& light = state.GetLightState(lightIndex);
                const auto lightDirection = Normalize({
                    light.position[0] - viewPosition.x,
                    light.position[1] - viewPosition.y,
                    light.position[2] - viewPosition.z,
                });
                generated[index] = generated[sourceIndex];
                generated[index].s += Dot(lightDirection, viewBinormal);
                generated[index].t += Dot(lightDirection, viewTangent);
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
                    sourceTexCoords[
                        static_cast<size_t>(
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
                    generated[
                        static_cast<size_t>(
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

            const auto& matrix =
                state.GetTexCoordGenMatrix(index);
            generated[index].s =
                matrix[0] * source[0] +
                matrix[1] * source[1] +
                matrix[2] * source[2] +
                matrix[3] * source[3];
            generated[index].t =
                matrix[4] * source[0] +
                matrix[5] * source[1] +
                matrix[6] * source[2] +
                matrix[7] * source[3];
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

void ApplyPositionMatrices(
    const SIM::GX::GlobalState& state,
    std::vector<SIM::GX::RenderVertex>& vertices) {
    for (auto& vertex : vertices) {
        const auto& modelView =
            state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE
                ? state.GetPositionMatrix(
                    static_cast<size_t>(
                        vertex.positionMatrixIndex / 3u))
                : state.GetPositionMatrix();
        vertex.position = TransformPoint(modelView, vertex.position);
    }
}

void ApplyRenderState(
    const SIM::GX::GlobalState& state,
    int drawableWidth,
    int drawableHeight) {
    const auto& viewport = state.GetViewportState();
    if (viewport.valid &&
        viewport.referenceWidth > 0.0f &&
        viewport.referenceHeight > 0.0f) {
        const float scaleX =
            static_cast<float>(drawableWidth) / viewport.referenceWidth;
        const float scaleY =
            static_cast<float>(drawableHeight) / viewport.referenceHeight;
        glViewport(
            static_cast<GLint>(viewport.left * scaleX),
            static_cast<GLint>(
                (viewport.referenceHeight - viewport.top - viewport.height) *
                scaleY),
            std::max(1, static_cast<GLint>(viewport.width * scaleX)),
            std::max(1, static_cast<GLint>(viewport.height * scaleY)));
    }

    const auto& scissor = state.GetScissorState();
    if (scissor.valid &&
        viewport.referenceWidth > 0.0f &&
        viewport.referenceHeight > 0.0f) {
        const float scaleX =
            static_cast<float>(drawableWidth) / viewport.referenceWidth;
        const float scaleY =
            static_cast<float>(drawableHeight) / viewport.referenceHeight;
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            static_cast<GLint>(scissor.left * scaleX),
            static_cast<GLint>(
                (viewport.referenceHeight - scissor.top - scissor.height) *
                scaleY),
            std::max(1, static_cast<GLint>(scissor.width * scaleX)),
            std::max(1, static_cast<GLint>(scissor.height * scaleY)));
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    const auto& depth = state.GetDepthState();
    if (depth.compareEnabled) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(ToGlCompare(depth.function));
        glDepthMask(depth.updateEnabled ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }

    const auto& raster = state.GetRasterState();
    // GX defines clockwise window-space polygons as front-facing.
    glFrontFace(GL_CW);
    switch (raster.cullMode) {
        case GX_CULL_FRONT:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case GX_CULL_BACK:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        case GX_CULL_ALL:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            break;
        case GX_CULL_NONE:
        default:
            glDisable(GL_CULL_FACE);
            break;
    }
    glLineWidth(raster.lineWidth);
    glPointSize(raster.pointSize);

    const auto& blend = state.GetBlendState();
    glColorMask(
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.alphaUpdateEnabled ? GL_TRUE : GL_FALSE);
    if (blend.ditherEnabled) {
        glEnable(GL_DITHER);
    } else {
        glDisable(GL_DITHER);
    }

    glDisable(GL_COLOR_LOGIC_OP);
    switch (blend.mode) {
        case GX_BM_BLEND:
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(
                ToGlSourceBlendFactor(blend.sourceFactor),
                ToGlDestinationBlendFactor(blend.destinationFactor));
            break;
        case GX_BM_SUBTRACT:
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case GX_BM_LOGIC:
            glDisable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glEnable(GL_COLOR_LOGIC_OP);
            glLogicOp(ToGlLogicOperation(blend.logicOperation));
            break;
        case GX_BM_NONE:
        default:
            glDisable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            break;
    }
}

GLenum ToGlWrap(GXTexWrapMode wrap) {
    switch (wrap) {
        case GX_REPEAT:
            return GL_REPEAT;
        case GX_MIRROR:
            return GL_MIRRORED_REPEAT;
        case GX_CLAMP:
        default:
            return GL_CLAMP_TO_EDGE;
    }
}

void DecodeI4(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 7u) / 8u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 8u; ++y) {
                for (size_t x = 0; x < 8u; x += 2u) {
                    const u8 packed = *source++;
                    const u8 intensities[2] = {
                        static_cast<u8>((packed >> 4u) * 17u),
                        static_cast<u8>((packed & 0x0fu) * 17u),
                    };
                    for (size_t pixel = 0; pixel < 2u; ++pixel) {
                        const size_t destinationX = blockX * 8u + x + pixel;
                        const size_t destinationY = blockY * 8u + y;
                        if (destinationX >= width || destinationY >= height) {
                            continue;
                        }
                        const size_t destination =
                            (destinationY * width + destinationX) * 4u;
                        const u8 intensity = intensities[pixel];
                        rgba[destination] = intensity;
                        rgba[destination + 1u] = intensity;
                        rgba[destination + 2u] = intensity;
                        rgba[destination + 3u] = intensity;
                    }
                }
            }
        }
    }
}

void DecodeI8(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 8u; ++x) {
                    const u8 intensity = *source++;
                    const size_t destinationX = blockX * 8u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }
                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] = intensity;
                    rgba[destination + 1u] = intensity;
                    rgba[destination + 2u] = intensity;
                    rgba[destination + 3u] = intensity;
                }
            }
        }
    }
}

void DecodeIA4(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 8u; ++x) {
                    const u8 packed = *source++;
                    const u8 intensity =
                        static_cast<u8>((packed & 0x0fu) * 17u);
                    const u8 alpha =
                        static_cast<u8>((packed >> 4u) * 17u);
                    const size_t destinationX = blockX * 8u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }
                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] = intensity;
                    rgba[destination + 1u] = intensity;
                    rgba[destination + 2u] = intensity;
                    rgba[destination + 3u] = alpha;
                }
            }
        }
    }
}

void DecodeIA8(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const u8 alpha = *source++;
                    const u8 intensity = *source++;
                    const size_t destinationX = blockX * 4u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }
                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] = intensity;
                    rgba[destination + 1u] = intensity;
                    rgba[destination + 2u] = intensity;
                    rgba[destination + 3u] = alpha;
                }
            }
        }
    }
}

void DecodeRGBA8(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            const u8* alphaRed = source;
            const u8* greenBlue = source + 32u;
            source += 64u;

            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const size_t sourcePixel = y * 4u + x;
                    const size_t destinationX = blockX * 4u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }
                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] =
                        alphaRed[sourcePixel * 2u + 1u];
                    rgba[destination + 1u] =
                        greenBlue[sourcePixel * 2u];
                    rgba[destination + 2u] =
                        greenBlue[sourcePixel * 2u + 1u];
                    rgba[destination + 3u] =
                        alphaRed[sourcePixel * 2u];
                }
            }
        }
    }
}

void DecodeCMPR(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 7u) / 8u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t subBlock = 0; subBlock < 4u; ++subBlock) {
                const size_t subX = (subBlock & 1u) * 4u;
                const size_t subY = (subBlock >> 1u) * 4u;
                const u16 endpoints[2] = {
                    static_cast<u16>(
                        (static_cast<u16>(source[0]) << 8u) |
                        static_cast<u16>(source[1])),
                    static_cast<u16>(
                        (static_cast<u16>(source[2]) << 8u) |
                        static_cast<u16>(source[3])),
                };
                source += 4u;

                std::array<std::array<u8, 4>, 4> palette = {};
                for (size_t endpoint = 0; endpoint < 2u; ++endpoint) {
                    const u16 packed = endpoints[endpoint];
                    palette[endpoint][0] = static_cast<u8>(
                        ((packed >> 11u) & 0x1fu) * 255u / 31u);
                    palette[endpoint][1] = static_cast<u8>(
                        ((packed >> 5u) & 0x3fu) * 255u / 63u);
                    palette[endpoint][2] = static_cast<u8>(
                        (packed & 0x1fu) * 255u / 31u);
                    palette[endpoint][3] = 255u;
                }

                if (endpoints[0] > endpoints[1]) {
                    for (size_t component = 0; component < 3u; ++component) {
                        palette[2][component] = static_cast<u8>(
                            (2u * palette[0][component] +
                             palette[1][component]) /
                            3u);
                        palette[3][component] = static_cast<u8>(
                            (palette[0][component] +
                             2u * palette[1][component]) /
                            3u);
                    }
                    palette[2][3] = 255u;
                    palette[3][3] = 255u;
                } else {
                    for (size_t component = 0; component < 3u; ++component) {
                        palette[2][component] = static_cast<u8>(
                            (palette[0][component] +
                             palette[1][component]) /
                            2u);
                    }
                    palette[2][3] = 255u;
                }

                for (size_t y = 0; y < 4u; ++y) {
                    const u8 selectors = *source++;
                    for (size_t x = 0; x < 4u; ++x) {
                        const size_t paletteIndex =
                            (selectors >> (6u - x * 2u)) & 0x03u;
                        const size_t destinationX =
                            blockX * 8u + subX + x;
                        const size_t destinationY =
                            blockY * 8u + subY + y;
                        if (destinationX >= width ||
                            destinationY >= height) {
                            continue;
                        }
                        const size_t destination =
                            (destinationY * width + destinationX) * 4u;
                        std::copy(
                            palette[paletteIndex].begin(),
                            palette[paletteIndex].end(),
                            rgba.begin() +
                                static_cast<std::ptrdiff_t>(destination));
                    }
                }
            }
        }
    }
}

void DecodeRGB565(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const u16 packed =
                        static_cast<u16>(
                            (static_cast<u16>(source[0]) << 8u) |
                            static_cast<u16>(source[1]));
                    source += 2u;

                    const size_t destinationX = blockX * 4u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }
                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] = static_cast<u8>(
                        ((packed >> 11u) & 0x1fu) * 255u / 31u);
                    rgba[destination + 1u] = static_cast<u8>(
                        ((packed >> 5u) & 0x3fu) * 255u / 63u);
                    rgba[destination + 2u] = static_cast<u8>(
                        (packed & 0x1fu) * 255u / 31u);
                    rgba[destination + 3u] = 255u;
                }
            }
        }
    }
}

void DecodeRGB5A3(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height) {
    const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const u16 packed =
                        static_cast<u16>(
                            (static_cast<u16>(source[0]) << 8u) |
                            static_cast<u16>(source[1]));
                    source += 2u;

                    const size_t destinationX = blockX * 4u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width || destinationY >= height) {
                        continue;
                    }

                    u8 red;
                    u8 green;
                    u8 blue;
                    u8 alpha;
                    if ((packed & 0x8000u) != 0u) {
                        red = static_cast<u8>(
                            ((packed >> 10u) & 0x1fu) * 255u / 31u);
                        green = static_cast<u8>(
                            ((packed >> 5u) & 0x1fu) * 255u / 31u);
                        blue = static_cast<u8>(
                            (packed & 0x1fu) * 255u / 31u);
                        alpha = 255u;
                    } else {
                        alpha = static_cast<u8>(
                            ((packed >> 12u) & 0x07u) * 255u / 7u);
                        red = static_cast<u8>(
                            ((packed >> 8u) & 0x0fu) * 17u);
                        green = static_cast<u8>(
                            ((packed >> 4u) & 0x0fu) * 17u);
                        blue = static_cast<u8>(
                            (packed & 0x0fu) * 17u);
                    }

                    const size_t destination =
                        (destinationY * width + destinationX) * 4u;
                    rgba[destination] = red;
                    rgba[destination + 1u] = green;
                    rgba[destination + 2u] = blue;
                    rgba[destination + 3u] = alpha;
                }
            }
        }
    }
}

std::vector<u8> DecodeTlut(const SIM::GX::TlutState& tlut) {
    std::vector<u8> palette(
        static_cast<size_t>(tlut.entries) * 4u,
        0);
    if (tlut.data == nullptr) {
        return palette;
    }

    const u8* source = static_cast<const u8*>(tlut.data);
    for (size_t index = 0; index < tlut.entries; ++index) {
        const u16 packed =
            static_cast<u16>(
                (static_cast<u16>(source[0]) << 8u) |
                static_cast<u16>(source[1]));
        source += 2u;
        const size_t destination = index * 4u;

        if (tlut.format == GX_TL_RGB565) {
            palette[destination] = static_cast<u8>(
                ((packed >> 11u) & 0x1fu) * 255u / 31u);
            palette[destination + 1u] = static_cast<u8>(
                ((packed >> 5u) & 0x3fu) * 255u / 63u);
            palette[destination + 2u] = static_cast<u8>(
                (packed & 0x1fu) * 255u / 31u);
            palette[destination + 3u] = 255u;
        } else if (tlut.format == GX_TL_RGB5A3) {
            if ((packed & 0x8000u) != 0u) {
                palette[destination] = static_cast<u8>(
                    ((packed >> 10u) & 0x1fu) * 255u / 31u);
                palette[destination + 1u] = static_cast<u8>(
                    ((packed >> 5u) & 0x1fu) * 255u / 31u);
                palette[destination + 2u] = static_cast<u8>(
                    (packed & 0x1fu) * 255u / 31u);
                palette[destination + 3u] = 255u;
            } else {
                palette[destination] = static_cast<u8>(
                    ((packed >> 8u) & 0x0fu) * 17u);
                palette[destination + 1u] = static_cast<u8>(
                    ((packed >> 4u) & 0x0fu) * 17u);
                palette[destination + 2u] = static_cast<u8>(
                    (packed & 0x0fu) * 17u);
                palette[destination + 3u] = static_cast<u8>(
                    ((packed >> 12u) & 0x07u) * 255u / 7u);
            }
        } else {
            const u8 intensity = static_cast<u8>(packed >> 8u);
            palette[destination] = intensity;
            palette[destination + 1u] = intensity;
            palette[destination + 2u] = intensity;
            palette[destination + 3u] =
                static_cast<u8>(packed & 0xffu);
        }
    }
    return palette;
}

void WritePalettePixel(
    std::vector<u8>& rgba,
    size_t destination,
    const std::vector<u8>& palette,
    size_t paletteIndex) {
    const size_t source = paletteIndex * 4u;
    if (source + 3u >= palette.size()) {
        rgba[destination + 3u] = 255u;
        return;
    }
    std::copy(
        palette.begin() + static_cast<std::ptrdiff_t>(source),
        palette.begin() + static_cast<std::ptrdiff_t>(source + 4u),
        rgba.begin() + static_cast<std::ptrdiff_t>(destination));
}

void DecodeC4(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height,
    const std::vector<u8>& palette) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 7u) / 8u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 8u; ++y) {
                for (size_t x = 0; x < 8u; x += 2u) {
                    const u8 packed = *source++;
                    for (size_t pixel = 0; pixel < 2u; ++pixel) {
                        const size_t destinationX =
                            blockX * 8u + x + pixel;
                        const size_t destinationY =
                            blockY * 8u + y;
                        if (destinationX >= width ||
                            destinationY >= height) {
                            continue;
                        }
                        const size_t paletteIndex =
                            pixel == 0u
                                ? packed >> 4u
                                : packed & 0x0fu;
                        WritePalettePixel(
                            rgba,
                            (destinationY * width + destinationX) * 4u,
                            palette,
                            paletteIndex);
                    }
                }
            }
        }
    }
}

void DecodeC8(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height,
    const std::vector<u8>& palette) {
    const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 8u; ++x) {
                    const size_t paletteIndex = *source++;
                    const size_t destinationX = blockX * 8u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width ||
                        destinationY >= height) {
                        continue;
                    }
                    WritePalettePixel(
                        rgba,
                        (destinationY * width + destinationX) * 4u,
                        palette,
                        paletteIndex);
                }
            }
        }
    }
}

void DecodeC14X2(
    const u8* source,
    std::vector<u8>& rgba,
    u16 width,
    u16 height,
    const std::vector<u8>& palette) {
    const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
    rgba.assign(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
        0);

    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const size_t paletteIndex =
                        static_cast<size_t>(
                            ((static_cast<u16>(source[0]) << 8u) |
                             static_cast<u16>(source[1])) &
                            0x3fffu);
                    source += 2u;
                    const size_t destinationX = blockX * 4u + x;
                    const size_t destinationY = blockY * 4u + y;
                    if (destinationX >= width ||
                        destinationY >= height) {
                        continue;
                    }
                    WritePalettePixel(
                        rgba,
                        (destinationY * width + destinationX) * 4u,
                        palette,
                        paletteIndex);
                }
            }
        }
    }
}

}

namespace SIM::GX {

u8 ConvertRgbToCopyIntensity(u8 red, u8 green, u8 blue) {
    const int y =
        (257 * static_cast<int>(red) +
         504 * static_cast<int>(green) +
          98 * static_cast<int>(blue) +
         16500) /
        1000;
    return static_cast<u8>(std::clamp(y, 16, 235));
}

u8 ConvertColorToTextureCopyByte(
    u32 destinationFormat,
    u8 red,
    u8 green,
    u8 blue,
    u8 alpha) {
    switch (destinationFormat) {
        case GX_CTF_A8:
            return alpha;
        case GX_CTF_R8:
            return red;
        case GX_CTF_G8:
            return green;
        case GX_CTF_B8:
            return blue;
        case GX_TF_I8:
        default:
            return ConvertRgbToCopyIntensity(red, green, blue);
    }
}

void EncodeRgb565TextureCopy(
    const u8* rgba,
    u16 width,
    u16 height,
    u8* encoded) {
    if (rgba == nullptr || encoded == nullptr || width == 0 || height == 0) {
        return;
    }

    const size_t blockColumns =
        (static_cast<size_t>(width) + 3u) / 4u;
    const size_t blockRows =
        (static_cast<size_t>(height) + 3u) / 4u;
    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < 4u; ++x) {
                    const size_t sourceX = blockX * 4u + x;
                    const size_t sourceY = blockY * 4u + y;
                    u16 packed = 0;
                    if (sourceX < width && sourceY < height) {
                        const size_t source =
                            (sourceY * width + sourceX) * 4u;
                        packed =
                            static_cast<u16>(
                                (static_cast<u16>(rgba[source] >> 3u) << 11u) |
                                (static_cast<u16>(rgba[source + 1u] >> 2u) << 5u) |
                                static_cast<u16>(rgba[source + 2u] >> 3u));
                    }
                    *encoded++ = static_cast<u8>(packed >> 8u);
                    *encoded++ = static_cast<u8>(packed & 0xffu);
                }
            }
        }
    }
}

void EncodeDepthTextureCopy(
    const float* depth,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded) {
    if (depth == nullptr || encoded == nullptr || width == 0 || height == 0) {
        return;
    }

    const size_t blockWidth =
        destinationFormat == GX_TF_Z8 ? 8u : 4u;
    const size_t blockColumns =
        (static_cast<size_t>(width) + blockWidth - 1u) / blockWidth;
    const size_t blockRows =
        (static_cast<size_t>(height) + 3u) / 4u;
    for (size_t blockY = 0; blockY < blockRows; ++blockY) {
        for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
            for (size_t y = 0; y < 4u; ++y) {
                for (size_t x = 0; x < blockWidth; ++x) {
                    const size_t sourceX = blockX * blockWidth + x;
                    const size_t sourceY = blockY * 4u + y;
                    u32 depth24 = 0;
                    if (sourceX < width && sourceY < height) {
                        // GX projection produces NDC depth in -1..0. OpenGL
                        // maps that into window depth 0..0.5, so expand it
                        // back to the GX EFB's full 0..1 depth range.
                        const float normalized = std::clamp(
                            depth[sourceY * width + sourceX] * 2.0f,
                            0.0f,
                            1.0f);
                        depth24 = static_cast<u32>(
                            std::lround(normalized * 16777215.0f));
                    }

                    if (destinationFormat == GX_TF_Z8) {
                        *encoded++ = static_cast<u8>(depth24 >> 16u);
                    } else {
                        const u16 depth16 =
                            static_cast<u16>(depth24 >> 8u);
                        // Z16 copies are sampled as IA8 by GX. The copy
                        // engine places the low depth byte in alpha and the
                        // high byte in intensity, opposite a native IA8 word.
                        *encoded++ = static_cast<u8>(depth16 & 0xffu);
                        *encoded++ = static_cast<u8>(depth16 >> 8u);
                    }
                }
            }
        }
    }
}

void ApplyTextureCoordinateGeneration(
    const GlobalState& state,
    std::vector<RenderVertex>& vertices) {
    ApplyTexCoordGenerators(state, vertices);
}

void GlRenderer::Initialize() {
    if (mVertexArray != 0) {
        return;
    }

    glGenVertexArrays(1, &mVertexArray);
    glGenBuffers(1, &mVertexBuffer);
    GLint drawableViewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, drawableViewport);
    mDrawableWidth = std::max(1, drawableViewport[2]);
    mDrawableHeight = std::max(1, drawableViewport[3]);
    glBindVertexArray(mVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);

    static_assert(std::is_standard_layout_v<RenderVertex>);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, color0)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, normal)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, binormal)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, tangent)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(
        5,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, color1)));

    for (size_t index = 0; index < 8; ++index) {
        glEnableVertexAttribArray(static_cast<GLuint>(6 + index));
        glVertexAttribPointer(
            static_cast<GLuint>(6 + index),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(RenderVertex),
            reinterpret_cast<void*>(
                offsetof(RenderVertex, texCoords) +
                index * sizeof(RenderTexCoord)));
    }
}

void GlRenderer::Draw(const std::vector<RenderVertex>& vertices, GXPrimitive primitive) {
    if (vertices.empty()) {
        return;
    }

    if (__VIHostOnDraw != nullptr) {
        __VIHostOnDraw();
    }
    Initialize();

    const auto& gxState = GetGlobalState();
    std::vector<RenderVertex> shadedVertices(vertices);
    ApplyColorChannels(gxState, shadedVertices);
    ApplyTextureCoordinateGeneration(gxState, shadedVertices);
    ApplyPositionMatrices(gxState, shadedVertices);

    std::vector<RenderVertex> expandedVertices;
    const std::vector<RenderVertex>* drawVertices = &shadedVertices;
    if (primitive == GX_QUADS) {
        expandedVertices = ExpandQuads(shadedVertices);
        drawVertices = &expandedVertices;
    } else if (primitive == GX_QUADSTRIP) {
        expandedVertices = ExpandQuadStrip(shadedVertices);
        drawVertices = &expandedVertices;
    }

    if (drawVertices->empty()) {
        return;
    }

    GLint shaderProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shaderProgram);
    if (shaderProgram == 0) {
        return;
    }

    ApplyRenderState(gxState, mDrawableWidth, mDrawableHeight);
    const GLint projectionLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_projection");
    const GLint modelViewLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_modelview");
    const GLint numTevStagesLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_num_tev_stages");
    const GLint useTexturesLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_use_texture[0]");
    const GLint stageTexturesLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_stage_texture[0]");
    const GLint stageTexCoordsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_stage_texcoord[0]");
    const GLint stageRasterChannelsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_stage_raster_channel[0]");
    const GLint tevColorInputsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_color_inputs[0]");
    const GLint tevAlphaInputsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_alpha_inputs[0]");
    const GLint tevColorOperationsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_color_operation[0]");
    const GLint tevAlphaOperationsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_alpha_operation[0]");
    const GLint tevOutputRegistersLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_output_registers[0]");
    const GLint tevSwapSelectorsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_swap_selectors[0]");
    const GLint tevSwapTablesLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_swap_tables[0]");
    const GLint tevRegistersLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_registers[0]");
    const GLint tevKonstColorsLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_konst_color[0]");
    const GLint tevKonstAlphasLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_tev_konst_alpha[0]");
    const GLint alphaComparison0Location =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_alpha_comparison0");
    const GLint alphaReference0Location =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_alpha_reference0");
    const GLint alphaOperationLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_alpha_operation");
    const GLint alphaComparison1Location =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_alpha_comparison1");
    const GLint alphaReference1Location =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_alpha_reference1");
    const GLint fogTypeLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_type");
    const GLint fogOrthographicLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_orthographic");
    const GLint fogALocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_a");
    const GLint fogBLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_b");
    const GLint fogCLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_c");
    const GLint fogColorLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_color");
    const GLint fogRangeEnabledLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_range_adjustment_enabled");
    const GLint fogRangeCenterLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_range_adjustment_center");
    const GLint fogRangeTableLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_range_adjustment[0]");
    const GLint fogXScaleLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_fog_x_scale");
    const GLint zTextureOperationLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_ztexture_operation");
    const GLint zTextureFormatLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_ztexture_format");
    const GLint zTextureBiasLocation =
        glGetUniformLocation(
            static_cast<GLuint>(shaderProgram),
            "u_ztexture_bias");
    if (projectionLocation >= 0) {
        glUniformMatrix4fv(
            projectionLocation,
            1,
            GL_TRUE,
            gxState.GetProjectionMatrix().data());
    }
    if (modelViewLocation >= 0) {
        constexpr std::array<float, 16> identityModelView = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        glUniformMatrix4fv(
            modelViewLocation,
            1,
            GL_TRUE,
            identityModelView.data());
    }
    constexpr size_t maxTevStages = 16;
    std::array<GLint, maxTevStages> useTextures = {};
    std::array<GLint, maxTevStages> textureUnits = {};
    std::array<GLint, maxTevStages> textureCoordinates = {};
    std::array<GLint, maxTevStages> rasterChannels = {};
    std::array<GLint, maxTevStages * 4u> colorInputs = {};
    std::array<GLint, maxTevStages * 4u> alphaInputs = {};
    std::array<GLint, maxTevStages * 4u> colorOperations = {};
    std::array<GLint, maxTevStages * 4u> alphaOperations = {};
    std::array<GLint, maxTevStages * 2u> outputRegisters = {};
    std::array<GLint, maxTevStages * 2u> swapSelectors = {};
    std::array<GLint, 4u * 4u> swapTables = {};
    std::array<float, maxTevStages * 4u> konstColors = {};
    std::array<float, maxTevStages> konstAlphas = {};

    const size_t numTevStages =
        std::min(gxState.GetNumTevStages(), maxTevStages);
    for (size_t stageIndex = 0;
         stageIndex < maxTevStages;
         ++stageIndex) {
        const auto& stage =
            gxState.GetTevStageState(stageIndex);
        textureUnits[stageIndex] =
            static_cast<GLint>(stageIndex);
        textureCoordinates[stageIndex] =
            static_cast<GLint>(stage.textureCoordinate);
        rasterChannels[stageIndex] =
            static_cast<GLint>(stage.rasterChannel);
        for (size_t input = 0; input < 4u; ++input) {
            colorInputs[stageIndex * 4u + input] =
                static_cast<GLint>(stage.colorInputs[input]);
            alphaInputs[stageIndex * 4u + input] =
                static_cast<GLint>(stage.alphaInputs[input]);
        }
        colorOperations[stageIndex * 4u] =
            static_cast<GLint>(stage.colorOperation);
        colorOperations[stageIndex * 4u + 1u] =
            static_cast<GLint>(stage.colorBias);
        colorOperations[stageIndex * 4u + 2u] =
            static_cast<GLint>(stage.colorScale);
        colorOperations[stageIndex * 4u + 3u] =
            stage.colorClamp ? 1 : 0;
        alphaOperations[stageIndex * 4u] =
            static_cast<GLint>(stage.alphaOperation);
        alphaOperations[stageIndex * 4u + 1u] =
            static_cast<GLint>(stage.alphaBias);
        alphaOperations[stageIndex * 4u + 2u] =
            static_cast<GLint>(stage.alphaScale);
        alphaOperations[stageIndex * 4u + 3u] =
            stage.alphaClamp ? 1 : 0;
        outputRegisters[stageIndex * 2u] =
            static_cast<GLint>(stage.colorOutput);
        outputRegisters[stageIndex * 2u + 1u] =
            static_cast<GLint>(stage.alphaOutput);
        swapSelectors[stageIndex * 2u] =
            static_cast<GLint>(stage.rasterSwapTable);
        swapSelectors[stageIndex * 2u + 1u] =
            static_cast<GLint>(stage.textureSwapTable);
        const auto konstColor =
            gxState.GetTevKonstColor(stageIndex);
        std::copy(
            konstColor.begin(),
            konstColor.end(),
            konstColors.begin() +
                static_cast<std::ptrdiff_t>(stageIndex * 4u));
        konstAlphas[stageIndex] =
            gxState.GetTevKonstAlpha(stageIndex);

        if (stageIndex >= numTevStages ||
            !stage.textureEnabled ||
            stage.textureMap >= mTextures.size()) {
            continue;
        }

        const size_t textureIndex = stage.textureMap;
        const auto& texture =
            gxState.GetTextureState(textureIndex);
        const bool supported =
            texture.format == GX_TF_I4 ||
            texture.format == GX_TF_I8 ||
            texture.format == GX_TF_IA4 ||
            texture.format == GX_TF_IA8 ||
            texture.format == GX_TF_RGB565 ||
            texture.format == GX_TF_RGB5A3 ||
            texture.format == GX_TF_RGBA8 ||
            texture.format == GX_TF_CMPR ||
            texture.format == GX_TF_C4 ||
            texture.format == GX_TF_C8 ||
            texture.format == GX_TF_C14X2 ||
            texture.format == GX_TF_Z8 ||
            texture.format == GX_TF_Z16 ||
            texture.format == GX_TF_Z24X8;
        if (texture.data == nullptr ||
            texture.width == 0 ||
            texture.height == 0 ||
            !supported) {
            continue;
        }

        if (mTextures[textureIndex] == 0u) {
            glGenTextures(1, &mTextures[textureIndex]);
        }
        glActiveTexture(
            static_cast<GLenum>(GL_TEXTURE0 + stageIndex));
        glBindTexture(
            GL_TEXTURE_2D,
            mTextures[textureIndex]);
        if (mTextureRevisions[textureIndex] != texture.revision) {
            std::vector<u8> rgba;
            std::vector<u8> palette;
            if (texture.format == GX_TF_C4 ||
                texture.format == GX_TF_C8 ||
                texture.format == GX_TF_C14X2) {
                palette = DecodeTlut(
                    gxState.GetTlutState(texture.tlutName));
            }
            if (texture.format == GX_TF_RGBA8 ||
                texture.format == GX_TF_Z24X8) {
                DecodeRGBA8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_RGB565) {
                DecodeRGB565(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_RGB5A3) {
                DecodeRGB5A3(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_I8) {
                DecodeI8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_IA4) {
                DecodeIA4(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_IA8) {
                DecodeIA8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_C4) {
                DecodeC4(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height,
                    palette);
            } else if (texture.format == GX_TF_C8) {
                DecodeC8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height,
                    palette);
            } else if (texture.format == GX_TF_C14X2) {
                DecodeC14X2(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height,
                    palette);
            } else if (texture.format == GX_TF_Z8) {
                DecodeI8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_Z16) {
                DecodeIA8(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else if (texture.format == GX_TF_CMPR) {
                DecodeCMPR(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            } else {
                DecodeI4(
                    static_cast<const u8*>(texture.data),
                    rgba,
                    texture.width,
                    texture.height);
            }
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                texture.width,
                texture.height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                rgba.data());
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_S,
                ToGlWrap(texture.wrapS));
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_WRAP_T,
                ToGlWrap(texture.wrapT));
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                texture.minFilter == GX_LINEAR
                    ? GL_LINEAR
                    : GL_NEAREST);
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                texture.magFilter == GX_LINEAR
                    ? GL_LINEAR
                    : GL_NEAREST);
            mTextureRevisions[textureIndex] =
                texture.revision;
        }
        useTextures[stageIndex] = 1;
    }

    if (numTevStagesLocation >= 0) {
        glUniform1i(
            numTevStagesLocation,
            static_cast<GLint>(numTevStages));
    }
    if (useTexturesLocation >= 0) {
        glUniform1iv(
            useTexturesLocation,
            static_cast<GLsizei>(useTextures.size()),
            useTextures.data());
    }
    if (stageTexturesLocation >= 0) {
        glUniform1iv(
            stageTexturesLocation,
            static_cast<GLsizei>(textureUnits.size()),
            textureUnits.data());
    }
    if (stageTexCoordsLocation >= 0) {
        glUniform1iv(
            stageTexCoordsLocation,
            static_cast<GLsizei>(textureCoordinates.size()),
            textureCoordinates.data());
    }
    if (stageRasterChannelsLocation >= 0) {
        glUniform1iv(
            stageRasterChannelsLocation,
            static_cast<GLsizei>(rasterChannels.size()),
            rasterChannels.data());
    }
    if (tevColorInputsLocation >= 0) {
        glUniform4iv(
            tevColorInputsLocation,
            static_cast<GLsizei>(maxTevStages),
            colorInputs.data());
    }
    if (tevAlphaInputsLocation >= 0) {
        glUniform4iv(
            tevAlphaInputsLocation,
            static_cast<GLsizei>(maxTevStages),
            alphaInputs.data());
    }
    if (tevColorOperationsLocation >= 0) {
        glUniform4iv(
            tevColorOperationsLocation,
            static_cast<GLsizei>(maxTevStages),
            colorOperations.data());
    }
    if (tevAlphaOperationsLocation >= 0) {
        glUniform4iv(
            tevAlphaOperationsLocation,
            static_cast<GLsizei>(maxTevStages),
            alphaOperations.data());
    }
    if (tevOutputRegistersLocation >= 0) {
        glUniform2iv(
            tevOutputRegistersLocation,
            static_cast<GLsizei>(maxTevStages),
            outputRegisters.data());
    }
    if (tevSwapSelectorsLocation >= 0) {
        glUniform2iv(
            tevSwapSelectorsLocation,
            static_cast<GLsizei>(maxTevStages),
            swapSelectors.data());
    }
    if (tevSwapTablesLocation >= 0) {
        for (size_t tableIndex = 0;
             tableIndex < 4u;
             ++tableIndex) {
            const auto& table =
                gxState.GetTevSwapTable(tableIndex);
            for (size_t component = 0;
                 component < 4u;
                 ++component) {
                swapTables[tableIndex * 4u + component] =
                    static_cast<GLint>(table[component]);
            }
        }
        glUniform4iv(
            tevSwapTablesLocation,
            4,
            swapTables.data());
    }
    if (tevRegistersLocation >= 0) {
        std::array<float, 16> registers = {};
        for (size_t registerIndex = 0;
             registerIndex < 4u;
             ++registerIndex) {
            const auto& source =
                gxState.GetTevColor(registerIndex);
            std::copy(
                source.begin(),
                source.end(),
                registers.begin() +
                    static_cast<std::ptrdiff_t>(
                        registerIndex * 4u));
        }
        glUniform4fv(
            tevRegistersLocation,
            4,
            registers.data());
    }
    if (tevKonstColorsLocation >= 0) {
        glUniform4fv(
            tevKonstColorsLocation,
            static_cast<GLsizei>(maxTevStages),
            konstColors.data());
    }
    if (tevKonstAlphasLocation >= 0) {
        glUniform1fv(
            tevKonstAlphasLocation,
            static_cast<GLsizei>(maxTevStages),
            konstAlphas.data());
    }
    const auto& alphaCompare = gxState.GetAlphaCompareState();
    if (alphaComparison0Location >= 0) {
        glUniform1i(
            alphaComparison0Location,
            static_cast<GLint>(alphaCompare.comparison0));
    }
    if (alphaReference0Location >= 0) {
        glUniform1i(
            alphaReference0Location,
            static_cast<GLint>(alphaCompare.reference0));
    }
    if (alphaOperationLocation >= 0) {
        glUniform1i(
            alphaOperationLocation,
            static_cast<GLint>(alphaCompare.operation));
    }
    if (alphaComparison1Location >= 0) {
        glUniform1i(
            alphaComparison1Location,
            static_cast<GLint>(alphaCompare.comparison1));
    }
    if (alphaReference1Location >= 0) {
        glUniform1i(
            alphaReference1Location,
            static_cast<GLint>(alphaCompare.reference1));
    }
    const auto& fog = gxState.GetFogState();
    const float fogA =
        std::ldexp(fog.parameterA, fog.parameterBShift);
    const float fogB =
        static_cast<float>(fog.parameterBMagnitude) /
        8388638.0f *
        std::ldexp(1.0f, static_cast<int>(fog.parameterBShift) - 1);
    if (fogTypeLocation >= 0) {
        glUniform1i(fogTypeLocation, static_cast<GLint>(fog.type));
    }
    if (fogOrthographicLocation >= 0) {
        glUniform1i(fogOrthographicLocation, fog.orthographic ? 1 : 0);
    }
    if (fogALocation >= 0) {
        glUniform1f(fogALocation, fogA);
    }
    if (fogBLocation >= 0) {
        glUniform1f(fogBLocation, fogB);
    }
    if (fogCLocation >= 0) {
        glUniform1f(fogCLocation, fog.parameterC);
    }
    if (fogColorLocation >= 0) {
        glUniform3fv(fogColorLocation, 1, fog.color.data());
    }
    if (fogRangeEnabledLocation >= 0) {
        glUniform1i(
            fogRangeEnabledLocation,
            fog.rangeAdjustmentEnabled ? 1 : 0);
    }
    if (fogRangeCenterLocation >= 0) {
        glUniform1f(
            fogRangeCenterLocation,
            static_cast<float>(fog.rangeAdjustmentCenter));
    }
    if (fogRangeTableLocation >= 0) {
        std::array<float, 10> rangeAdjustments = {};
        for (size_t index = 0;
             index < rangeAdjustments.size();
             ++index) {
            rangeAdjustments[index] =
                static_cast<float>(fog.rangeAdjustmentTable[index]) /
                256.0f;
        }
        glUniform1fv(
            fogRangeTableLocation,
            static_cast<GLsizei>(rangeAdjustments.size()),
            rangeAdjustments.data());
    }
    if (fogXScaleLocation >= 0) {
        const auto& viewport = gxState.GetViewportState();
        const float xScale =
            mDrawableWidth > 0 && viewport.referenceWidth > 0.0f
                ? viewport.referenceWidth /
                    static_cast<float>(mDrawableWidth)
                : 1.0f;
        glUniform1f(fogXScaleLocation, xScale);
    }
    const auto& zTexture = gxState.GetZTextureState();
    if (zTextureOperationLocation >= 0) {
        glUniform1i(
            zTextureOperationLocation,
            static_cast<GLint>(zTexture.operation));
    }
    if (zTextureFormatLocation >= 0) {
        GLint format = 2;
        if (zTexture.format == GX_TF_Z8) {
            format = 0;
        } else if (zTexture.format == GX_TF_Z16) {
            format = 1;
        }
        glUniform1i(zTextureFormatLocation, format);
    }
    if (zTextureBiasLocation >= 0) {
        glUniform1ui(zTextureBiasLocation, zTexture.bias);
    }

    glBindVertexArray(mVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(drawVertices->size() * sizeof(RenderVertex)),
        drawVertices->data(),
        GL_STREAM_DRAW);
    glDrawArrays(
        ToGlPrimitive(primitive),
        0,
        static_cast<GLsizei>(drawVertices->size()));
}

GlRenderer& GetGlRenderer() {
    static GlRenderer renderer;
    return renderer;
}

}

extern "C" __attribute__((weak)) void __GXHostApplyCopyClear(void) {
    auto& gxState = SIM::GX::GetGlobalState();
    if (!gxState.ConsumeCopyClearRequest()) {
        return;
    }

    const auto& clearColor = gxState.GetCopyClearColor();
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean colorWriteMask[4] = {};
    GLboolean depthWriteMask = GL_FALSE;
    glGetBooleanv(GL_COLOR_WRITEMASK, colorWriteMask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteMask);

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(
        clearColor[0],
        clearColor[1],
        clearColor[2],
        clearColor[3]);
    glClearDepth(gxState.GetCopyClearDepth());
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glColorMask(
        colorWriteMask[0],
        colorWriteMask[1],
        colorWriteMask[2],
        colorWriteMask[3]);
    glDepthMask(depthWriteMask);
    if (scissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }
}

extern "C" void __GXHostCopyTex(
    void* destination,
    u16 sourceLeft,
    u16 sourceTop,
    u16 sourceWidth,
    u16 sourceHeight,
    u16 destinationWidth,
    u16 destinationHeight,
    u32 destinationFormat,
    GXBool clear) {
    if (destination == nullptr ||
        sourceWidth == 0 ||
        sourceHeight == 0 ||
        destinationWidth == 0 ||
        destinationHeight == 0) {
        return;
    }

    SDL_Window* currentWindow = SDL_GL_GetCurrentWindow();
    if (currentWindow == nullptr) {
        return;
    }
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(
        currentWindow,
        &drawableWidth,
        &drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0) {
        return;
    }

    auto& gxState = SIM::GX::GetGlobalState();
    const auto& viewport = gxState.GetViewportState();
    const float referenceWidth =
        viewport.referenceWidth > 0.0f
            ? viewport.referenceWidth
            : 640.0f;
    const float referenceHeight =
        viewport.referenceHeight > 0.0f
            ? viewport.referenceHeight
            : 480.0f;
    const float scaleX =
        static_cast<float>(drawableWidth) / referenceWidth;
    const float scaleY =
        static_cast<float>(drawableHeight) / referenceHeight;

    GLint previousReadBuffer = GL_BACK;
    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const bool depthCopy =
        destinationFormat == GX_TF_Z8 ||
        destinationFormat == GX_TF_Z16;
    std::vector<u8> framebuffer;
    std::vector<float> depthBuffer;
    if (depthCopy) {
        depthBuffer.resize(
            static_cast<size_t>(drawableWidth) *
            static_cast<size_t>(drawableHeight));
        glReadPixels(
            0,
            0,
            drawableWidth,
            drawableHeight,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            depthBuffer.data());
    } else {
        framebuffer.resize(
            static_cast<size_t>(drawableWidth) *
            static_cast<size_t>(drawableHeight) *
            4u);
        glReadPixels(
            0,
            0,
            drawableWidth,
            drawableHeight,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            framebuffer.data());
    }

    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));

    if (depthCopy) {
        std::vector<float> copiedDepth(
            static_cast<size_t>(destinationWidth) *
                static_cast<size_t>(destinationHeight),
            0.0f);
        for (size_t destinationY = 0;
             destinationY < destinationHeight;
             ++destinationY) {
            for (size_t destinationX = 0;
                 destinationX < destinationWidth;
                 ++destinationX) {
                const float sourceX =
                    static_cast<float>(sourceLeft) +
                    (static_cast<float>(destinationX) + 0.5f) *
                        static_cast<float>(sourceWidth) /
                        static_cast<float>(destinationWidth);
                const float sourceY =
                    static_cast<float>(sourceTop) +
                    (static_cast<float>(destinationY) + 0.5f) *
                        static_cast<float>(sourceHeight) /
                        static_cast<float>(destinationHeight);
                const int framebufferX = std::clamp(
                    static_cast<int>(sourceX * scaleX),
                    0,
                    drawableWidth - 1);
                const int framebufferY = std::clamp(
                    drawableHeight - 1 -
                        static_cast<int>(sourceY * scaleY),
                    0,
                    drawableHeight - 1);
                copiedDepth[
                    destinationY *
                        static_cast<size_t>(destinationWidth) +
                    destinationX] =
                    depthBuffer[
                        static_cast<size_t>(framebufferY) *
                            static_cast<size_t>(drawableWidth) +
                        static_cast<size_t>(framebufferX)];
            }
        }
        SIM::GX::EncodeDepthTextureCopy(
            copiedDepth.data(),
            destinationWidth,
            destinationHeight,
            destinationFormat,
            static_cast<u8*>(destination));
    } else if (destinationFormat == GX_TF_RGB565) {
        std::vector<u8> copiedPixels(
            static_cast<size_t>(destinationWidth) *
                static_cast<size_t>(destinationHeight) *
                4u,
            0);
        for (size_t destinationY = 0;
             destinationY < destinationHeight;
             ++destinationY) {
            for (size_t destinationX = 0;
                 destinationX < destinationWidth;
                 ++destinationX) {
                const float sourceX =
                    static_cast<float>(sourceLeft) +
                    (static_cast<float>(destinationX) + 0.5f) *
                        static_cast<float>(sourceWidth) /
                        static_cast<float>(destinationWidth);
                const float sourceY =
                    static_cast<float>(sourceTop) +
                    (static_cast<float>(destinationY) + 0.5f) *
                        static_cast<float>(sourceHeight) /
                        static_cast<float>(destinationHeight);
                const int framebufferX = std::clamp(
                    static_cast<int>(sourceX * scaleX),
                    0,
                    drawableWidth - 1);
                const int framebufferY = std::clamp(
                    drawableHeight - 1 -
                        static_cast<int>(sourceY * scaleY),
                    0,
                    drawableHeight - 1);
                const size_t sourceOffset =
                    (static_cast<size_t>(framebufferY) *
                         static_cast<size_t>(drawableWidth) +
                     static_cast<size_t>(framebufferX)) *
                    4u;
                const size_t destinationOffset =
                    (destinationY *
                         static_cast<size_t>(destinationWidth) +
                     destinationX) *
                    4u;
                std::copy_n(
                    framebuffer.data() + sourceOffset,
                    4u,
                    copiedPixels.data() + destinationOffset);
            }
        }
        SIM::GX::EncodeRgb565TextureCopy(
            copiedPixels.data(),
            destinationWidth,
            destinationHeight,
            static_cast<u8*>(destination));
    } else if (destinationFormat == GX_CTF_A8 ||
               destinationFormat == GX_CTF_R8 ||
               destinationFormat == GX_CTF_G8 ||
               destinationFormat == GX_CTF_B8 ||
               destinationFormat == GX_TF_I8) {
        u8* encoded = static_cast<u8*>(destination);
        const size_t blockColumns =
            (static_cast<size_t>(destinationWidth) + 7u) / 8u;
        const size_t blockRows =
            (static_cast<size_t>(destinationHeight) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 8u; ++x) {
                        const size_t destinationX = blockX * 8u + x;
                        const size_t destinationY = blockY * 4u + y;
                        u8 value = 0;
                        if (destinationX < destinationWidth &&
                            destinationY < destinationHeight) {
                            const float sourceX =
                                static_cast<float>(sourceLeft) +
                                (static_cast<float>(destinationX) + 0.5f) *
                                    static_cast<float>(sourceWidth) /
                                    static_cast<float>(destinationWidth);
                            const float sourceY =
                                static_cast<float>(sourceTop) +
                                (static_cast<float>(destinationY) + 0.5f) *
                                    static_cast<float>(sourceHeight) /
                                    static_cast<float>(destinationHeight);
                            const int framebufferX = std::clamp(
                                static_cast<int>(sourceX * scaleX),
                                0,
                                drawableWidth - 1);
                            const int framebufferY = std::clamp(
                                drawableHeight - 1 -
                                    static_cast<int>(sourceY * scaleY),
                                0,
                                drawableHeight - 1);
                            const size_t sourceOffset =
                                (static_cast<size_t>(framebufferY) *
                                     static_cast<size_t>(drawableWidth) +
                                 static_cast<size_t>(framebufferX)) *
                                4u;
                            value = SIM::GX::ConvertColorToTextureCopyByte(
                                destinationFormat,
                                framebuffer[sourceOffset],
                                framebuffer[sourceOffset + 1u],
                                framebuffer[sourceOffset + 2u],
                                framebuffer[sourceOffset + 3u]);
                        }
                        *encoded++ = value;
                    }
                }
            }
        }
    }

    if (!clear) {
        return;
    }

    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissor[4] = {};
    GLboolean previousColorMask[4] = {};
    GLboolean previousDepthMask = GL_FALSE;
    GLfloat previousClearColor[4] = {};
    GLfloat previousClearDepth = 1.0f;
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &previousClearDepth);

    const int clearLeft =
        std::clamp(
            static_cast<int>(
                std::floor(static_cast<float>(sourceLeft) * scaleX)),
            0,
            drawableWidth);
    const int clearRight =
        std::clamp(
            static_cast<int>(
                std::ceil(
                    static_cast<float>(sourceLeft + sourceWidth) *
                    scaleX)),
            clearLeft,
            drawableWidth);
    const int clearTop =
        std::clamp(
            static_cast<int>(
                std::floor(static_cast<float>(sourceTop) * scaleY)),
            0,
            drawableHeight);
    const int clearBottom =
        std::clamp(
            static_cast<int>(
                std::ceil(
                    static_cast<float>(sourceTop + sourceHeight) *
                    scaleY)),
            clearTop,
            drawableHeight);
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        clearLeft,
        drawableHeight - clearBottom,
        clearRight - clearLeft,
        clearBottom - clearTop);

    const auto& blend = gxState.GetBlendState();
    const auto& depth = gxState.GetDepthState();
    glColorMask(
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.colorUpdateEnabled ? GL_TRUE : GL_FALSE,
        blend.alphaUpdateEnabled ? GL_TRUE : GL_FALSE);
    glDepthMask(depth.updateEnabled ? GL_TRUE : GL_FALSE);

    GLbitfield clearBits = 0;
    if (blend.colorUpdateEnabled || blend.alphaUpdateEnabled) {
        const auto& clearColor = gxState.GetCopyClearColor();
        glClearColor(
            clearColor[0],
            clearColor[1],
            clearColor[2],
            clearColor[3]);
        clearBits |= GL_COLOR_BUFFER_BIT;
    }
    if (depth.updateEnabled) {
        glClearDepth(gxState.GetCopyClearDepth());
        clearBits |= GL_DEPTH_BUFFER_BIT;
    }
    if (clearBits != 0) {
        glClear(clearBits);
    }

    glColorMask(
        previousColorMask[0],
        previousColorMask[1],
        previousColorMask[2],
        previousColorMask[3]);
    glDepthMask(previousDepthMask);
    glClearColor(
        previousClearColor[0],
        previousClearColor[1],
        previousClearColor[2],
        previousClearColor[3]);
    glClearDepth(previousClearDepth);
    glScissor(
        previousScissor[0],
        previousScissor[1],
        previousScissor[2],
        previousScissor[3]);
    if (!scissorEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }
}
