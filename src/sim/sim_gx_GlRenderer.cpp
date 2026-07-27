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
        const auto& modelView =
            state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE
                ? state.GetPositionMatrix(
                    static_cast<size_t>(
                        vertex.positionMatrixIndex / 3u))
                : state.GetPositionMatrix();
        const auto viewPosition =
            TransformPoint(modelView, vertex.position);
        const auto viewNormal =
            Normalize(TransformDirection(modelView, vertex.normal));
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
                    0.0f,
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

}

namespace SIM::GX {

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

    Initialize();

    const auto& gxState = GetGlobalState();
    std::vector<RenderVertex> shadedVertices(vertices);
    ApplyColorChannels(gxState, shadedVertices);
    ApplyTexCoordGenerators(gxState, shadedVertices);
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
            texture.format == GX_TF_RGB565 ||
            texture.format == GX_TF_RGB5A3 ||
            texture.format == GX_TF_RGBA8;
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
            if (texture.format == GX_TF_RGBA8) {
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

    std::vector<u8> framebuffer(
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

    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glReadBuffer(static_cast<GLenum>(previousReadBuffer));

    if (destinationFormat == GX_CTF_A8 ||
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
                        u8 alpha = 0;
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
                            alpha = framebuffer[sourceOffset + 3u];
                        }
                        *encoded++ = alpha;
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
