#include <simulator/sim_gx_GlRenderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_host_Allocator.hpp>
#include <simulator/sim_gx_State.hpp>

extern "C" void __VIHostOnDraw(void) __attribute__((weak));

namespace {

void ExpandQuads(
    const std::vector<SIM::GX::RenderVertex>& vertices,
    std::vector<SIM::GX::RenderVertex>& triangles) {
    triangles.clear();
    triangles.reserve((vertices.size() / 4) * 6);
    for (size_t i = 0; i + 3 < vertices.size(); i += 4) {
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 1]);
        triangles.push_back(vertices[i + 2]);
        triangles.push_back(vertices[i]);
        triangles.push_back(vertices[i + 2]);
        triangles.push_back(vertices[i + 3]);
    }
}

void ExpandQuadStrip(
    const std::vector<SIM::GX::RenderVertex>& vertices,
    std::vector<SIM::GX::RenderVertex>& triangles) {
    triangles.clear();
    if (vertices.size() < 4) {
        return;
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

void DrainGlErrors() {
    // A lost context can report an error indefinitely. Bound cleanup so a
    // failed optional fast path always reaches the mutable-buffer fallback.
    constexpr size_t maxErrors = 32u;
    for (size_t error = 0u;
         error < maxErrors && glGetError() != GL_NO_ERROR;
         ++error) {
    }
}

void ConfigureRenderVertexAttributes() {
    using SIM::GX::RenderTexCoord;
    using SIM::GX::RenderVertex;

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

    for (size_t index = 0; index < 8u; ++index) {
        glEnableVertexAttribArray(static_cast<GLuint>(6u + index));
        glVertexAttribPointer(
            static_cast<GLuint>(6u + index),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(RenderVertex),
            reinterpret_cast<void*>(
                offsetof(RenderVertex, texCoords) +
                index * sizeof(RenderTexCoord)));
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

void ApplyTexCoordGenerators(
    const SIM::GX::GlobalState& state,
    std::vector<SIM::GX::RenderVertex>& vertices) {
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
        // Inactive generator slots retain their decoded source coordinates;
        // RenderTexCoord's default q=1 also remains intact for absent inputs.
        auto generated = sourceTexCoords;

        SIM::GX::RenderVector3 viewPosition = {};
        SIM::GX::RenderVector3 viewBinormal = {};
        SIM::GX::RenderVector3 viewTangent = {};
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
    const bool indexedMatrix =
        state.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE;
    if (!indexedMatrix) {
        const auto& modelView = state.GetPositionMatrix();
        for (auto& vertex : vertices) {
            vertex.position = TransformPoint(modelView, vertex.position);
        }
        return;
    }

    for (auto& vertex : vertices) {
        const auto& modelView = state.GetPositionMatrix(
            static_cast<size_t>(vertex.positionMatrixIndex / 3u));
        vertex.position = TransformPoint(modelView, vertex.position);
    }
}

void ApplyRenderState(
    const SIM::GX::GlobalState& state,
    int drawableWidth,
    int drawableHeight,
    u32 dirty) {
    using namespace SIM::GX::Detail;
    const auto& viewport = state.GetViewportState();
    if ((dirty & RenderStateViewport) != 0u) {
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
        } else {
            glViewport(0, 0, drawableWidth, drawableHeight);
        }
    }

    const auto& scissor = state.GetScissorState();
    if ((dirty & RenderStateScissor) != 0u) {
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
    }

    const auto& depth = state.GetDepthState();
    if ((dirty & RenderStateDepth) != 0u) {
        if (depth.compareEnabled) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(ToGlCompare(depth.function));
            glDepthMask(depth.updateEnabled ? GL_TRUE : GL_FALSE);
        } else {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        }
    }

    const auto& raster = state.GetRasterState();
    if ((dirty & RenderStateRaster) != 0u) {
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
    }

    const auto& blend = state.GetBlendState();
    if ((dirty & RenderStateBlend) != 0u) {
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

GLenum ToGlMinFilter(GXTexFilter filter) {
    const auto selection =
        SIM::GX::Detail::SelectTextureFilter(filter);
    if (selection.mipmapFilter ==
        SIM::GX::Detail::TextureMipmapFilter::Nearest) {
        return selection.linearTexels
            ? GL_LINEAR_MIPMAP_NEAREST
            : GL_NEAREST_MIPMAP_NEAREST;
    }
    if (selection.mipmapFilter ==
        SIM::GX::Detail::TextureMipmapFilter::Linear) {
        return selection.linearTexels
            ? GL_LINEAR_MIPMAP_LINEAR
            : GL_NEAREST_MIPMAP_LINEAR;
    }
    return selection.linearTexels ? GL_LINEAR : GL_NEAREST;
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
    if (tlut.CanonicalData() == nullptr) {
        return palette;
    }

    const u8* source =
        static_cast<const u8*>(tlut.CanonicalData());
    for (size_t index = 0; index < tlut.entries; ++index) {
        const SIM::GX::DecodedTlutColor color =
            SIM::GX::DecodeTlutEntry(tlut.format, source);
        source += 2u;
        const size_t destination = index * 4u;

        palette[destination] = color.red;
        palette[destination + 1u] = color.green;
        palette[destination + 2u] = color.blue;
        palette[destination + 3u] = color.alpha;
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

namespace Detail {

namespace {

bool SameViewport(
    const ViewportState& left,
    const ViewportState& right) {
    return
        left.left == right.left &&
        left.top == right.top &&
        left.width == right.width &&
        left.height == right.height &&
        left.referenceWidth == right.referenceWidth &&
        left.referenceHeight == right.referenceHeight &&
        left.valid == right.valid;
}

bool SameScissor(
    const ScissorState& left,
    const ScissorState& right) {
    return
        left.left == right.left &&
        left.top == right.top &&
        left.width == right.width &&
        left.height == right.height &&
        left.valid == right.valid;
}

bool SameDepth(
    const DepthState& left,
    const DepthState& right) {
    return
        left.compareEnabled == right.compareEnabled &&
        left.function == right.function &&
        left.updateEnabled == right.updateEnabled;
}

bool SameRaster(
    const RasterState& left,
    const RasterState& right) {
    return
        left.cullMode == right.cullMode &&
        left.lineWidth == right.lineWidth &&
        left.pointSize == right.pointSize;
}

bool SameBlend(
    const BlendState& left,
    const BlendState& right) {
    return
        left.mode == right.mode &&
        left.sourceFactor == right.sourceFactor &&
        left.destinationFactor == right.destinationFactor &&
        left.logicOperation == right.logicOperation &&
        left.colorUpdateEnabled == right.colorUpdateEnabled &&
        left.alphaUpdateEnabled == right.alphaUpdateEnabled &&
        left.ditherEnabled == right.ditherEnabled;
}

}

u32 RenderStateCache::Update(
    const ViewportState& viewport,
    const ScissorState& scissor,
    const DepthState& depth,
    const RasterState& raster,
    const BlendState& blend,
    int drawableWidth,
    int drawableHeight) {
    u32 dirty = 0u;
    if (!mValid) {
        dirty = RenderStateAll;
    } else {
        if (mDrawableWidth != drawableWidth ||
            mDrawableHeight != drawableHeight ||
            !SameViewport(mViewport, viewport)) {
            dirty |= RenderStateViewport | RenderStateScissor;
        } else if (!SameScissor(mScissor, scissor)) {
            dirty |= RenderStateScissor;
        }
        if (!SameDepth(mDepth, depth)) {
            dirty |= RenderStateDepth;
        }
        if (!SameRaster(mRaster, raster)) {
            dirty |= RenderStateRaster;
        }
        if (!SameBlend(mBlend, blend)) {
            dirty |= RenderStateBlend;
        }
    }

    mViewport = viewport;
    mScissor = scissor;
    mDepth = depth;
    mRaster = raster;
    mBlend = blend;
    mDrawableWidth = drawableWidth;
    mDrawableHeight = drawableHeight;
    mValid = true;
    return dirty;
}

TextureFilterSelection SelectTextureFilter(GXTexFilter filter) {
    switch (filter) {
        case GX_LINEAR:
            return {true, TextureMipmapFilter::None};
        case GX_NEAR_MIP_NEAR:
            return {false, TextureMipmapFilter::Nearest};
        case GX_LIN_MIP_NEAR:
            return {true, TextureMipmapFilter::Nearest};
        case GX_NEAR_MIP_LIN:
            return {false, TextureMipmapFilter::Linear};
        case GX_LIN_MIP_LIN:
            return {true, TextureMipmapFilter::Linear};
        case GX_NEAR:
        default:
            return {false, TextureMipmapFilter::None};
    }
}

bool DecodeCanonicalTextureMipLevelToRgba(
    const TextureState& texture,
    const u8* canonicalBytes,
    size_t canonicalByteSize,
    size_t level,
    const std::vector<u8>& palette,
    std::vector<u8>& rgba) {
    TextureMipLevelLayout layout;
    if (canonicalBytes == nullptr ||
        !GetTextureMipLevelLayout(texture, level, layout) ||
        layout.offset > canonicalByteSize ||
        layout.byteSize > canonicalByteSize - layout.offset) {
        rgba.clear();
        return false;
    }

    const u8* source = canonicalBytes + layout.offset;
    if (texture.format == GX_TF_RGBA8 ||
        texture.format == GX_TF_Z24X8) {
        DecodeRGBA8(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_RGB565) {
        DecodeRGB565(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_RGB5A3) {
        DecodeRGB5A3(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_I8) {
        DecodeI8(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_IA4) {
        DecodeIA4(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_IA8) {
        DecodeIA8(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_C4) {
        DecodeC4(
            source, rgba, layout.width, layout.height, palette);
    } else if (texture.format == GX_TF_C8) {
        DecodeC8(
            source, rgba, layout.width, layout.height, palette);
    } else if (texture.format == GX_TF_C14X2) {
        DecodeC14X2(
            source, rgba, layout.width, layout.height, palette);
    } else if (texture.format == GX_TF_Z8) {
        DecodeI8(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_Z16) {
        DecodeIA8(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_CMPR) {
        DecodeCMPR(source, rgba, layout.width, layout.height);
    } else if (texture.format == GX_TF_I4) {
        DecodeI4(source, rgba, layout.width, layout.height);
    } else {
        rgba.clear();
        return false;
    }
    return true;
}

bool DecodeTextureToRgba(
    const TextureState& texture,
    const std::vector<u8>& palette,
    std::vector<u8>& rgba) {
    std::vector<u8> canonicalBytes;
    if (!CopyCanonicalTextureBytes(texture, canonicalBytes)) {
        rgba.clear();
        return false;
    }
    return DecodeCanonicalTextureMipLevelToRgba(
        texture,
        canonicalBytes.data(),
        canonicalBytes.size(),
        0u,
        palette,
        rgba);
}

}

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

    const bool persistentMappingAvailable =
        GLAD_GL_ARB_buffer_storage != 0 &&
        glad_glBufferStorage != nullptr &&
        glad_glMapBufferRange != nullptr &&
        glad_glFenceSync != nullptr &&
        glad_glClientWaitSync != nullptr &&
        glad_glDeleteSync != nullptr;
    if (persistentMappingAvailable) {
        constexpr size_t totalVertexCapacity =
            VertexStreamPageCapacity * VertexStreamPageCount;
        constexpr size_t totalByteCapacity =
            totalVertexCapacity * sizeof(RenderVertex);
        constexpr GLbitfield mapFlags =
            GL_MAP_WRITE_BIT |
            GL_MAP_PERSISTENT_BIT |
            GL_MAP_COHERENT_BIT;
        DrainGlErrors();
        glBufferStorage(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(totalByteCapacity),
            nullptr,
            mapFlags);
        const GLenum storageError = glGetError();
        if (storageError == GL_NO_ERROR) {
            mMappedVertexBytes = static_cast<u8*>(glMapBufferRange(
                GL_ARRAY_BUFFER,
                0,
                static_cast<GLsizeiptr>(totalByteCapacity),
                mapFlags));
        }
        const GLenum mappingError =
            storageError == GL_NO_ERROR ? glGetError() : storageError;
        mPersistentVertexStream =
            mMappedVertexBytes != nullptr &&
            mappingError == GL_NO_ERROR;
        if (!mPersistentVertexStream) {
            if (mMappedVertexBytes != nullptr) {
                glUnmapBuffer(GL_ARRAY_BUFFER);
                mMappedVertexBytes = nullptr;
            }
            DrainGlErrors();
            // Immutable storage cannot fall back to glBufferData. Replace the
            // object with a normal mutable GL 3.3 buffer if setup failed.
            glDeleteBuffers(1, &mVertexBuffer);
            glGenBuffers(1, &mVertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
        }
    }

    ConfigureRenderVertexAttributes();
}

void GlRenderer::AdvancePersistentVertexPage(size_t pageIndex) {
    if (mVertexStreamPageHasDraws) {
        GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0u);
        if (fence == nullptr) {
            glFinish();
        } else {
            mVertexStreamFences[mActiveVertexStreamPage] = fence;
        }
    }

    GLsync pending =
        static_cast<GLsync>(mVertexStreamFences[pageIndex]);
    if (pending != nullptr) {
        GLenum waitResult = GL_TIMEOUT_EXPIRED;
        while (waitResult == GL_TIMEOUT_EXPIRED) {
            waitResult = glClientWaitSync(
                pending,
                GL_SYNC_FLUSH_COMMANDS_BIT,
                1000000u);
        }
        if (waitResult == GL_WAIT_FAILED) {
            glFinish();
        }
        glDeleteSync(pending);
        mVertexStreamFences[pageIndex] = nullptr;
    }

    mActiveVertexStreamPage = pageIndex;
    mVertexStreamPageHasDraws = false;
}

void GlRenderer::DrawPersistentVertices(
    const std::vector<RenderVertex>& vertices,
    GXPrimitive primitive) {
    const GLenum glPrimitive = ToGlPrimitive(primitive);
    if (vertices.size() > VertexStreamPageCapacity &&
        glPrimitive != GL_TRIANGLES) {
        // Native FIFO primitive counts fit in u16. Keep the public renderer
        // entry point lossless for synthetic/host callers that exceed that
        // invariant by using a separate mutable overflow buffer.
        DrawOverflowVertices(vertices, primitive);
        return;
    }

    size_t sourceOffset = 0u;
    while (sourceOffset < vertices.size()) {
        const size_t remaining = vertices.size() - sourceOffset;
        size_t chunkSize =
            std::min(remaining, VertexStreamPageCapacity);
        if (remaining > VertexStreamPageCapacity &&
            glPrimitive == GL_TRIANGLES) {
            chunkSize -= chunkSize % 3u;
        }
        // Native GX primitive counts fit in one page. Only triangle lists
        // (including expanded quads) are split, at triangle boundaries.
        if (chunkSize == 0u) {
            DrawOverflowVertices(vertices, primitive);
            return;
        }

        Detail::VertexStreamAllocation allocation;
        if (!mVertexStreamRing.Allocate(chunkSize, allocation)) {
            DrawOverflowVertices(vertices, primitive);
            return;
        }
        if (allocation.pageChanged) {
            AdvancePersistentVertexPage(allocation.pageIndex);
        }

        std::memcpy(
            mMappedVertexBytes +
                allocation.firstVertex * sizeof(RenderVertex),
            vertices.data() + sourceOffset,
            chunkSize * sizeof(RenderVertex));
        glDrawArrays(
            glPrimitive,
            static_cast<GLint>(allocation.firstVertex),
            static_cast<GLsizei>(chunkSize));
        mVertexStreamPageHasDraws = true;
        sourceOffset += chunkSize;
    }
}

void GlRenderer::DrawOverflowVertices(
    const std::vector<RenderVertex>& vertices,
    GXPrimitive primitive) {
    if (mOverflowVertexArray == 0u) {
        glGenVertexArrays(1, &mOverflowVertexArray);
        glGenBuffers(1, &mOverflowVertexBuffer);
        glBindVertexArray(mOverflowVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, mOverflowVertexBuffer);
        ConfigureRenderVertexAttributes();
    } else {
        glBindVertexArray(mOverflowVertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, mOverflowVertexBuffer);
    }

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(RenderVertex)),
        vertices.data(),
        GL_STREAM_DRAW);
    glDrawArrays(
        ToGlPrimitive(primitive),
        0,
        static_cast<GLsizei>(vertices.size()));
}

void GlRenderer::SetDrawableSize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    mDrawableWidth = width;
    mDrawableHeight = height;
    // The owner also applies a raw full-window glViewport during context
    // acquisition, so force the semantic GX viewport/scissor back afterward
    // even when the drawable dimensions did not change.
    mRenderStateCache.Invalidate();
}

void GlRenderer::SetShaderProgram(unsigned int program) {
    if (mShaderProgram == program) {
        return;
    }
    mShaderProgram = program;
    InvalidateShaderProgramCache();
    mRenderStateCache.Invalidate();
}

void GlRenderer::InvalidateShaderProgramCache() {
    mUniformLocations.Invalidate();
    mUniformValues.Invalidate();
    mUniformStateRevisionValid = false;
}

void GlRenderer::InvalidateRenderStateCache() {
    mRenderStateCache.Invalidate();
}

void GlRenderer::Draw(std::vector<RenderVertex>& vertices, GXPrimitive primitive) {
    if (vertices.empty()) {
        return;
    }

    if (__VIHostOnDraw != nullptr) {
        __VIHostOnDraw();
    }
    Initialize();

    const auto& gxState = GetGlobalState();
    ApplyColorChannels(gxState, vertices);
    ApplyTextureCoordinateGeneration(gxState, vertices);
    ApplyPositionMatrices(gxState, vertices);

    const std::vector<RenderVertex>* drawVertices = &vertices;
    if (primitive == GX_QUADS) {
        ExpandQuads(vertices, mExpandedVertices);
        drawVertices = &mExpandedVertices;
    } else if (primitive == GX_QUADSTRIP) {
        ExpandQuadStrip(vertices, mExpandedVertices);
        drawVertices = &mExpandedVertices;
    }

    if (drawVertices->empty()) {
        return;
    }

    if (mShaderProgram == 0u) {
        return;
    }

    const u32 renderStateDirty = mRenderStateCache.Update(
        gxState.GetViewportState(),
        gxState.GetScissorState(),
        gxState.GetDepthState(),
        gxState.GetRasterState(),
        gxState.GetBlendState(),
        mDrawableWidth,
        mDrawableHeight);
    if (renderStateDirty != 0u) {
        ApplyRenderState(
            gxState,
            mDrawableWidth,
            mDrawableHeight,
            renderStateDirty);
    }
    const u64 uniformStateRevision = gxState.GetUniformStateRevision();
    const bool updateUniformValues =
        !mUniformStateRevisionValid ||
        mUniformStateRevision != uniformStateRevision ||
        mUniformDrawableWidth != mDrawableWidth ||
        mUniformDrawableHeight != mDrawableHeight;
    using Detail::ShaderUniform;
    auto& uniformValues = mUniformScratch;
    if (updateUniformValues) {
        uniformValues.projection = gxState.GetProjectionMatrix();
        uniformValues.modelView = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        uniformValues.useTextures.fill(0);
    }
    constexpr size_t maxTevStages =
        Detail::ShaderUniformValues::MaxTevStages;
    auto& useTextures = uniformValues.useTextures;
    auto& textureUnits = uniformValues.stageTextures;
    auto& textureCoordinates = uniformValues.stageTexCoords;
    auto& rasterChannels = uniformValues.stageRasterChannels;
    auto& colorInputs = uniformValues.tevColorInputs;
    auto& alphaInputs = uniformValues.tevAlphaInputs;
    auto& colorOperations = uniformValues.tevColorOperations;
    auto& alphaOperations = uniformValues.tevAlphaOperations;
    auto& outputRegisters = uniformValues.tevOutputRegisters;
    auto& swapSelectors = uniformValues.tevSwapSelectors;
    auto& swapTables = uniformValues.tevSwapTables;
    auto& registers = uniformValues.tevRegisters;
    auto& konstColors = uniformValues.tevKonstColors;
    auto& konstAlphas = uniformValues.tevKonstAlphas;

    const size_t numTevStages =
        std::min(gxState.GetNumTevStages(), maxTevStages);
    if (updateUniformValues) {
        uniformValues.numTevStages = static_cast<int>(numTevStages);
    }
    const size_t stageLoopCount =
        updateUniformValues ? maxTevStages : numTevStages;
    for (size_t stageIndex = 0;
         stageIndex < stageLoopCount;
         ++stageIndex) {
        const auto& stage =
            gxState.GetTevStageState(stageIndex);
        if (updateUniformValues) {
            textureUnits[stageIndex] = static_cast<int>(stageIndex);
            textureCoordinates[stageIndex] =
                static_cast<int>(stage.textureCoordinate);
            rasterChannels[stageIndex] =
                static_cast<int>(stage.rasterChannel);
            for (size_t input = 0; input < 4u; ++input) {
                colorInputs[stageIndex * 4u + input] =
                    static_cast<int>(stage.colorInputs[input]);
                alphaInputs[stageIndex * 4u + input] =
                    static_cast<int>(stage.alphaInputs[input]);
            }
            colorOperations[stageIndex * 4u] =
                static_cast<int>(stage.colorOperation);
            colorOperations[stageIndex * 4u + 1u] =
                static_cast<int>(stage.colorBias);
            colorOperations[stageIndex * 4u + 2u] =
                static_cast<int>(stage.colorScale);
            colorOperations[stageIndex * 4u + 3u] =
                stage.colorClamp ? 1 : 0;
            alphaOperations[stageIndex * 4u] =
                static_cast<int>(stage.alphaOperation);
            alphaOperations[stageIndex * 4u + 1u] =
                static_cast<int>(stage.alphaBias);
            alphaOperations[stageIndex * 4u + 2u] =
                static_cast<int>(stage.alphaScale);
            alphaOperations[stageIndex * 4u + 3u] =
                stage.alphaClamp ? 1 : 0;
            outputRegisters[stageIndex * 2u] =
                static_cast<int>(stage.colorOutput);
            outputRegisters[stageIndex * 2u + 1u] =
                static_cast<int>(stage.alphaOutput);
            swapSelectors[stageIndex * 2u] =
                static_cast<int>(stage.rasterSwapTable);
            swapSelectors[stageIndex * 2u + 1u] =
                static_cast<int>(stage.textureSwapTable);
            const auto konstColor =
                gxState.GetTevKonstColor(stageIndex);
            std::copy(
                konstColor.begin(),
                konstColor.end(),
                konstColors.begin() +
                    static_cast<std::ptrdiff_t>(stageIndex * 4u));
            konstAlphas[stageIndex] =
                gxState.GetTevKonstAlpha(stageIndex);
        }

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
            GetTextureSourceByteSize(texture) == 0u ||
            !supported) {
            continue;
        }

        const bool textureObjectCreated =
            mTextures[textureIndex] == 0u;
        if (textureObjectCreated) {
            glGenTextures(1, &mTextures[textureIndex]);
        }
        glActiveTexture(
            static_cast<GLenum>(GL_TEXTURE0 + stageIndex));
        glBindTexture(
            GL_TEXTURE_2D,
            mTextures[textureIndex]);
        const bool textureStateChanged =
            mTextureRevisions[textureIndex] != texture.revision;
        const u64 invalidationRevision =
            gxState.GetTextureInvalidationRevision();
        const bool usesTlut =
            texture.format == GX_TF_C4 ||
            texture.format == GX_TF_C8 ||
            texture.format == GX_TF_C14X2;
        const TlutState* tlut =
            usesTlut
                ? &gxState.GetTlutState(texture.tlutName)
                : nullptr;
        const u64 tlutRevision =
            tlut != nullptr ? tlut->revision : 0u;
        const bool validateTexture =
            Detail::ShouldValidateTexture(
                textureObjectCreated,
                usesTlut,
                mTextureRevisions[textureIndex],
                texture.revision,
                mTextureInvalidationRevisions[textureIndex],
                invalidationRevision,
                mTextureTlutRevisions[textureIndex],
                tlutRevision);
        const bool uploadTexture =
            validateTexture &&
            !mTextureSnapshots[textureIndex].Matches(texture, tlut);
        if (uploadTexture) {
            std::vector<u8> canonicalBytes;
            std::vector<u8> palette;
            if (usesTlut) {
                palette = DecodeTlut(*tlut);
            }
            if (!CopyCanonicalTextureBytes(texture, canonicalBytes)) {
                continue;
            }
            const size_t levelCount =
                GetTextureMipLevelCount(texture);
            bool decodedAllLevels = levelCount != 0u;
            for (size_t level = 0u;
                 level < levelCount;
                 ++level) {
                TextureMipLevelLayout layout;
                std::vector<u8> rgba;
                if (!GetTextureMipLevelLayout(texture, level, layout) ||
                    !Detail::DecodeCanonicalTextureMipLevelToRgba(
                        texture,
                        canonicalBytes.data(),
                        canonicalBytes.size(),
                        level,
                        palette,
                        rgba)) {
                    decodedAllLevels = false;
                    break;
                }
                glTexImage2D(
                    GL_TEXTURE_2D,
                    static_cast<GLint>(level),
                    GL_RGBA8,
                    layout.width,
                    layout.height,
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    rgba.data());
            }
            if (!decodedAllLevels) {
                continue;
            }
            mTextureSnapshots[textureIndex].Capture(texture, tlut);
        }
        if (textureObjectCreated || textureStateChanged) {
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
                ToGlMinFilter(texture.minFilter));
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAG_FILTER,
                texture.magFilter == GX_LINEAR
                    ? GL_LINEAR
                    : GL_NEAREST);
            const size_t levelCount =
                GetTextureMipLevelCount(texture);
            const GLint maximumLevel =
                levelCount == 0u
                    ? 0
                    : static_cast<GLint>(levelCount - 1u);
            const GLfloat maximumLod =
                static_cast<GLfloat>(maximumLevel);
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_BASE_LEVEL,
                0);
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAX_LEVEL,
                maximumLevel);
            glTexParameterf(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_LOD,
                std::clamp(
                    texture.minLod,
                    0.0f,
                    maximumLod));
            glTexParameterf(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAX_LOD,
                std::clamp(
                    texture.maxLod,
                    0.0f,
                    maximumLod));
            glTexParameterf(
                GL_TEXTURE_2D,
                GL_TEXTURE_LOD_BIAS,
                texture.lodBias);
        }
        if (validateTexture) {
            mTextureRevisions[textureIndex] =
                texture.revision;
            mTextureInvalidationRevisions[textureIndex] =
                invalidationRevision;
            mTextureTlutRevisions[textureIndex] =
                tlutRevision;
        }
        if (updateUniformValues) {
            useTextures[stageIndex] = 1;
        }
    }

    if (updateUniformValues) {
    const auto& uniformLocations = mUniformLocations.Resolve(
        static_cast<GLuint>(mShaderProgram),
        [](unsigned int program, const char* name) {
            return glGetUniformLocation(static_cast<GLuint>(program), name);
        });
    const GLint projectionLocation =
        uniformLocations[ShaderUniform::Projection];
    const GLint modelViewLocation =
        uniformLocations[ShaderUniform::ModelView];
    const GLint numTevStagesLocation =
        uniformLocations[ShaderUniform::NumTevStages];
    const GLint useTexturesLocation =
        uniformLocations[ShaderUniform::UseTextures];
    const GLint stageTexturesLocation =
        uniformLocations[ShaderUniform::StageTextures];
    const GLint stageTexCoordsLocation =
        uniformLocations[ShaderUniform::StageTexCoords];
    const GLint stageRasterChannelsLocation =
        uniformLocations[ShaderUniform::StageRasterChannels];
    const GLint tevColorInputsLocation =
        uniformLocations[ShaderUniform::TevColorInputs];
    const GLint tevAlphaInputsLocation =
        uniformLocations[ShaderUniform::TevAlphaInputs];
    const GLint tevColorOperationsLocation =
        uniformLocations[ShaderUniform::TevColorOperations];
    const GLint tevAlphaOperationsLocation =
        uniformLocations[ShaderUniform::TevAlphaOperations];
    const GLint tevOutputRegistersLocation =
        uniformLocations[ShaderUniform::TevOutputRegisters];
    const GLint tevSwapSelectorsLocation =
        uniformLocations[ShaderUniform::TevSwapSelectors];
    const GLint tevSwapTablesLocation =
        uniformLocations[ShaderUniform::TevSwapTables];
    const GLint tevRegistersLocation =
        uniformLocations[ShaderUniform::TevRegisters];
    const GLint tevKonstColorsLocation =
        uniformLocations[ShaderUniform::TevKonstColors];
    const GLint tevKonstAlphasLocation =
        uniformLocations[ShaderUniform::TevKonstAlphas];
    const GLint alphaComparison0Location =
        uniformLocations[ShaderUniform::AlphaComparison0];
    const GLint alphaReference0Location =
        uniformLocations[ShaderUniform::AlphaReference0];
    const GLint alphaOperationLocation =
        uniformLocations[ShaderUniform::AlphaOperation];
    const GLint alphaComparison1Location =
        uniformLocations[ShaderUniform::AlphaComparison1];
    const GLint alphaReference1Location =
        uniformLocations[ShaderUniform::AlphaReference1];
    const GLint fogTypeLocation =
        uniformLocations[ShaderUniform::FogType];
    const GLint fogOrthographicLocation =
        uniformLocations[ShaderUniform::FogOrthographic];
    const GLint fogALocation =
        uniformLocations[ShaderUniform::FogA];
    const GLint fogBLocation =
        uniformLocations[ShaderUniform::FogB];
    const GLint fogCLocation =
        uniformLocations[ShaderUniform::FogC];
    const GLint fogColorLocation =
        uniformLocations[ShaderUniform::FogColor];
    const GLint fogRangeEnabledLocation =
        uniformLocations[ShaderUniform::FogRangeEnabled];
    const GLint fogRangeCenterLocation =
        uniformLocations[ShaderUniform::FogRangeCenter];
    const GLint fogRangeTableLocation =
        uniformLocations[ShaderUniform::FogRangeTable];
    const GLint fogXScaleLocation =
        uniformLocations[ShaderUniform::FogXScale];
    const GLint zTextureOperationLocation =
        uniformLocations[ShaderUniform::ZTextureOperation];
    const GLint zTextureFormatLocation =
        uniformLocations[ShaderUniform::ZTextureFormat];
    const GLint zTextureBiasLocation =
        uniformLocations[ShaderUniform::ZTextureBias];
    for (size_t tableIndex = 0; tableIndex < 4u; ++tableIndex) {
        const auto& table = gxState.GetTevSwapTable(tableIndex);
        for (size_t component = 0; component < 4u; ++component) {
            swapTables[tableIndex * 4u + component] =
                static_cast<int>(table[component]);
        }
    }
    for (size_t registerIndex = 0;
         registerIndex < 4u;
         ++registerIndex) {
        const auto& source = gxState.GetTevColor(registerIndex);
        std::copy(
            source.begin(),
            source.end(),
            registers.begin() +
                static_cast<std::ptrdiff_t>(registerIndex * 4u));
    }

    const auto& alphaCompare = gxState.GetAlphaCompareState();
    uniformValues.alphaComparison0 =
        static_cast<int>(alphaCompare.comparison0);
    uniformValues.alphaReference0 =
        static_cast<int>(alphaCompare.reference0);
    uniformValues.alphaOperation =
        static_cast<int>(alphaCompare.operation);
    uniformValues.alphaComparison1 =
        static_cast<int>(alphaCompare.comparison1);
    uniformValues.alphaReference1 =
        static_cast<int>(alphaCompare.reference1);

    const auto& fog = gxState.GetFogState();
    uniformValues.fogType = static_cast<int>(fog.type);
    uniformValues.fogOrthographic = fog.orthographic ? 1 : 0;
    uniformValues.fogA =
        std::ldexp(fog.parameterA, fog.parameterBShift);
    uniformValues.fogB =
        static_cast<float>(fog.parameterBMagnitude) /
        8388638.0f *
        std::ldexp(1.0f, static_cast<int>(fog.parameterBShift) - 1);
    uniformValues.fogC = fog.parameterC;
    uniformValues.fogColor = fog.color;
    uniformValues.fogRangeEnabled =
        fog.rangeAdjustmentEnabled ? 1 : 0;
    uniformValues.fogRangeCenter =
        static_cast<float>(fog.rangeAdjustmentCenter);
    for (size_t index = 0;
         index < uniformValues.fogRangeTable.size();
         ++index) {
        uniformValues.fogRangeTable[index] =
            static_cast<float>(fog.rangeAdjustmentTable[index]) / 256.0f;
    }
    const auto& viewport = gxState.GetViewportState();
    uniformValues.fogXScale =
        mDrawableWidth > 0 && viewport.referenceWidth > 0.0f
            ? viewport.referenceWidth /
                static_cast<float>(mDrawableWidth)
            : 1.0f;

    const auto& zTexture = gxState.GetZTextureState();
    uniformValues.zTextureOperation =
        static_cast<int>(zTexture.operation);
    uniformValues.zTextureFormat = 2;
    if (zTexture.format == GX_TF_Z8) {
        uniformValues.zTextureFormat = 0;
    } else if (zTexture.format == GX_TF_Z16) {
        uniformValues.zTextureFormat = 1;
    }
    uniformValues.zTextureBias = zTexture.bias;

    const u64 uniformDirty = mUniformValues.Update(uniformValues);
    const auto isUniformDirty = [uniformDirty](ShaderUniform uniform) {
        return Detail::IsShaderUniformDirty(uniformDirty, uniform);
    };

    if (projectionLocation >= 0 &&
        isUniformDirty(ShaderUniform::Projection)) {
        glUniformMatrix4fv(
            projectionLocation,
            1,
            GL_TRUE,
            uniformValues.projection.data());
    }
    if (modelViewLocation >= 0 &&
        isUniformDirty(ShaderUniform::ModelView)) {
        glUniformMatrix4fv(
            modelViewLocation,
            1,
            GL_TRUE,
            uniformValues.modelView.data());
    }
    if (numTevStagesLocation >= 0 &&
        isUniformDirty(ShaderUniform::NumTevStages)) {
        glUniform1i(numTevStagesLocation, uniformValues.numTevStages);
    }
    if (useTexturesLocation >= 0 &&
        isUniformDirty(ShaderUniform::UseTextures)) {
        glUniform1iv(
            useTexturesLocation,
            static_cast<GLsizei>(useTextures.size()),
            useTextures.data());
    }
    if (stageTexturesLocation >= 0 &&
        isUniformDirty(ShaderUniform::StageTextures)) {
        glUniform1iv(
            stageTexturesLocation,
            static_cast<GLsizei>(textureUnits.size()),
            textureUnits.data());
    }
    if (stageTexCoordsLocation >= 0 &&
        isUniformDirty(ShaderUniform::StageTexCoords)) {
        glUniform1iv(
            stageTexCoordsLocation,
            static_cast<GLsizei>(textureCoordinates.size()),
            textureCoordinates.data());
    }
    if (stageRasterChannelsLocation >= 0 &&
        isUniformDirty(ShaderUniform::StageRasterChannels)) {
        glUniform1iv(
            stageRasterChannelsLocation,
            static_cast<GLsizei>(rasterChannels.size()),
            rasterChannels.data());
    }
    if (tevColorInputsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevColorInputs)) {
        glUniform4iv(
            tevColorInputsLocation,
            static_cast<GLsizei>(maxTevStages),
            colorInputs.data());
    }
    if (tevAlphaInputsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevAlphaInputs)) {
        glUniform4iv(
            tevAlphaInputsLocation,
            static_cast<GLsizei>(maxTevStages),
            alphaInputs.data());
    }
    if (tevColorOperationsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevColorOperations)) {
        glUniform4iv(
            tevColorOperationsLocation,
            static_cast<GLsizei>(maxTevStages),
            colorOperations.data());
    }
    if (tevAlphaOperationsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevAlphaOperations)) {
        glUniform4iv(
            tevAlphaOperationsLocation,
            static_cast<GLsizei>(maxTevStages),
            alphaOperations.data());
    }
    if (tevOutputRegistersLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevOutputRegisters)) {
        glUniform2iv(
            tevOutputRegistersLocation,
            static_cast<GLsizei>(maxTevStages),
            outputRegisters.data());
    }
    if (tevSwapSelectorsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevSwapSelectors)) {
        glUniform2iv(
            tevSwapSelectorsLocation,
            static_cast<GLsizei>(maxTevStages),
            swapSelectors.data());
    }
    if (tevSwapTablesLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevSwapTables)) {
        glUniform4iv(tevSwapTablesLocation, 4, swapTables.data());
    }
    if (tevRegistersLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevRegisters)) {
        glUniform4fv(tevRegistersLocation, 4, registers.data());
    }
    if (tevKonstColorsLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevKonstColors)) {
        glUniform4fv(
            tevKonstColorsLocation,
            static_cast<GLsizei>(maxTevStages),
            konstColors.data());
    }
    if (tevKonstAlphasLocation >= 0 &&
        isUniformDirty(ShaderUniform::TevKonstAlphas)) {
        glUniform1fv(
            tevKonstAlphasLocation,
            static_cast<GLsizei>(maxTevStages),
            konstAlphas.data());
    }
    if (alphaComparison0Location >= 0 &&
        isUniformDirty(ShaderUniform::AlphaComparison0)) {
        glUniform1i(
            alphaComparison0Location,
            uniformValues.alphaComparison0);
    }
    if (alphaReference0Location >= 0 &&
        isUniformDirty(ShaderUniform::AlphaReference0)) {
        glUniform1i(
            alphaReference0Location,
            uniformValues.alphaReference0);
    }
    if (alphaOperationLocation >= 0 &&
        isUniformDirty(ShaderUniform::AlphaOperation)) {
        glUniform1i(
            alphaOperationLocation,
            uniformValues.alphaOperation);
    }
    if (alphaComparison1Location >= 0 &&
        isUniformDirty(ShaderUniform::AlphaComparison1)) {
        glUniform1i(
            alphaComparison1Location,
            uniformValues.alphaComparison1);
    }
    if (alphaReference1Location >= 0 &&
        isUniformDirty(ShaderUniform::AlphaReference1)) {
        glUniform1i(
            alphaReference1Location,
            uniformValues.alphaReference1);
    }
    if (fogTypeLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogType)) {
        glUniform1i(fogTypeLocation, uniformValues.fogType);
    }
    if (fogOrthographicLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogOrthographic)) {
        glUniform1i(
            fogOrthographicLocation,
            uniformValues.fogOrthographic);
    }
    if (fogALocation >= 0 && isUniformDirty(ShaderUniform::FogA)) {
        glUniform1f(fogALocation, uniformValues.fogA);
    }
    if (fogBLocation >= 0 && isUniformDirty(ShaderUniform::FogB)) {
        glUniform1f(fogBLocation, uniformValues.fogB);
    }
    if (fogCLocation >= 0 && isUniformDirty(ShaderUniform::FogC)) {
        glUniform1f(fogCLocation, uniformValues.fogC);
    }
    if (fogColorLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogColor)) {
        glUniform3fv(fogColorLocation, 1, uniformValues.fogColor.data());
    }
    if (fogRangeEnabledLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogRangeEnabled)) {
        glUniform1i(
            fogRangeEnabledLocation,
            uniformValues.fogRangeEnabled);
    }
    if (fogRangeCenterLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogRangeCenter)) {
        glUniform1f(
            fogRangeCenterLocation,
            uniformValues.fogRangeCenter);
    }
    if (fogRangeTableLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogRangeTable)) {
        glUniform1fv(
            fogRangeTableLocation,
            static_cast<GLsizei>(uniformValues.fogRangeTable.size()),
            uniformValues.fogRangeTable.data());
    }
    if (fogXScaleLocation >= 0 &&
        isUniformDirty(ShaderUniform::FogXScale)) {
        glUniform1f(fogXScaleLocation, uniformValues.fogXScale);
    }
    if (zTextureOperationLocation >= 0 &&
        isUniformDirty(ShaderUniform::ZTextureOperation)) {
        glUniform1i(
            zTextureOperationLocation,
            uniformValues.zTextureOperation);
    }
    if (zTextureFormatLocation >= 0 &&
        isUniformDirty(ShaderUniform::ZTextureFormat)) {
        glUniform1i(
            zTextureFormatLocation,
            uniformValues.zTextureFormat);
    }
    if (zTextureBiasLocation >= 0 &&
        isUniformDirty(ShaderUniform::ZTextureBias)) {
        glUniform1ui(zTextureBiasLocation, uniformValues.zTextureBias);
    }
    mUniformStateRevision = uniformStateRevision;
    mUniformDrawableWidth = mDrawableWidth;
    mUniformDrawableHeight = mDrawableHeight;
    mUniformStateRevisionValid = true;
    }

    glBindVertexArray(mVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    if (mPersistentVertexStream) {
        DrawPersistentVertices(*drawVertices, primitive);
    } else {
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                drawVertices->size() * sizeof(RenderVertex)),
            drawVertices->data(),
            GL_STREAM_DRAW);
        glDrawArrays(
            ToGlPrimitive(primitive),
            0,
            static_cast<GLsizei>(drawVertices->size()));
    }
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
    SIM::HostAllocationScope hostAllocations;
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
