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
    const auto& scissor = state.GetScissorState();
    if ((dirty & RenderStateViewport) != 0u) {
        if (viewport.valid &&
            viewport.referenceWidth > 0.0f &&
            viewport.referenceHeight > 0.0f) {
            const float scaleX =
                static_cast<float>(drawableWidth) / viewport.referenceWidth;
            const float scaleY =
                static_cast<float>(drawableHeight) / viewport.referenceHeight;
            // GX_SCISSOROFFSET is shared by the setup unit's viewport and
            // scissor transforms.  GlobalState exposes both coordinates
            // after removing their hardware bias, so the effective origin is
            // the programmed origin minus the decoded offset.
            const float viewportLeft =
                viewport.left - static_cast<float>(scissor.offsetX);
            const float viewportTop =
                viewport.top - static_cast<float>(scissor.offsetY);
            glViewport(
                static_cast<GLint>(viewportLeft * scaleX),
                static_cast<GLint>(
                    (viewport.referenceHeight - viewportTop - viewport.height) *
                    scaleY),
                std::max(1, static_cast<GLint>(viewport.width * scaleX)),
                std::max(1, static_cast<GLint>(viewport.height * scaleY)));
        } else {
            glViewport(0, 0, drawableWidth, drawableHeight);
        }
    }

    if ((dirty & RenderStateScissor) != 0u) {
        if (scissor.valid &&
            viewport.referenceWidth > 0.0f &&
            viewport.referenceHeight > 0.0f) {
            const auto drawableScissor = ComputeDrawableScissor(
                viewport, scissor, drawableWidth, drawableHeight);
            glEnable(GL_SCISSOR_TEST);
            glScissor(
                drawableScissor.x,
                drawableScissor.y,
                drawableScissor.width,
                drawableScissor.height);
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

struct GpuCopyTexture {
    const void* destination = nullptr;
    u16 width = 0;
    u16 height = 0;
    GXTexFmt format = GX_TF_I4;
    GLuint texture = 0;
    GLuint framebuffer = 0;
    GLuint depthTexture = 0;
    GLuint depthFramebuffer = 0;
    u64 revision = 0;
    u64 validatedInvalidationRevision = 0;
    bool active = false;
    std::vector<u8> destinationSnapshot;
};

std::vector<GpuCopyTexture> gGpuCopyTextures;
u64 gGpuCopyTextureRevision = 0;
GLuint gDepthCopyProgram = 0;
GLuint gDepthCopyVertexArray = 0;
GLint gDepthCopySamplerLocation = -1;
GLint gDepthCopyCoefficientsLocation = -1;
bool gDepthCopyProgramAttempted = false;
struct TextureSamplerCacheState {
    GLuint sampler = 0u;
    GLint wrapS = GL_CLAMP_TO_EDGE;
    GLint wrapT = GL_CLAMP_TO_EDGE;
    GLint minFilter = GL_NEAREST;
    GLint magFilter = GL_NEAREST;
    GLfloat minLod = 0.0f;
    GLfloat maxLod = 0.0f;
    GLfloat lodBias = 0.0f;
    bool valid = false;
};

std::array<
    TextureSamplerCacheState,
    SIM::GX::Detail::ShaderUniformValues::MaxTevStages> gTextureSamplers = {};

class ScopedCopyGlState {
 public:
    ScopedCopyGlState() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &mReadFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &mDrawFramebuffer);
        glGetIntegerv(GL_READ_BUFFER, &mReadBuffer);
        glGetIntegerv(GL_DRAW_BUFFER, &mDrawBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &mActiveTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &mTexture0);
        glGetIntegerv(GL_SAMPLER_BINDING, &mSampler0);
        glActiveTexture(static_cast<GLenum>(mActiveTexture));
        glGetIntegerv(GL_CURRENT_PROGRAM, &mProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &mVertexArray);
        glGetIntegerv(GL_VIEWPORT, mViewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, mColorMask);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask);
        mScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        mDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
        mBlendEnabled = glIsEnabled(GL_BLEND);
        mCullEnabled = glIsEnabled(GL_CULL_FACE);
        mLogicEnabled = glIsEnabled(GL_COLOR_LOGIC_OP);
        mDitherEnabled = glIsEnabled(GL_DITHER);
        mFramebufferSrgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    }

    ~ScopedCopyGlState() {
        glBindFramebuffer(
            GL_READ_FRAMEBUFFER,
            static_cast<GLuint>(mReadFramebuffer));
        glReadBuffer(static_cast<GLenum>(mReadBuffer));
        glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER,
            static_cast<GLuint>(mDrawFramebuffer));
        glDrawBuffer(static_cast<GLenum>(mDrawBuffer));
        glViewport(
            mViewport[0], mViewport[1], mViewport[2], mViewport[3]);
        glColorMask(
            mColorMask[0], mColorMask[1], mColorMask[2], mColorMask[3]);
        glDepthMask(mDepthMask);
        RestoreEnabled(GL_SCISSOR_TEST, mScissorEnabled);
        RestoreEnabled(GL_DEPTH_TEST, mDepthEnabled);
        RestoreEnabled(GL_BLEND, mBlendEnabled);
        RestoreEnabled(GL_CULL_FACE, mCullEnabled);
        RestoreEnabled(GL_COLOR_LOGIC_OP, mLogicEnabled);
        RestoreEnabled(GL_DITHER, mDitherEnabled);
        RestoreEnabled(GL_FRAMEBUFFER_SRGB, mFramebufferSrgbEnabled);
        glUseProgram(static_cast<GLuint>(mProgram));
        glBindVertexArray(static_cast<GLuint>(mVertexArray));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(mTexture0));
        glBindSampler(0, static_cast<GLuint>(mSampler0));
        glActiveTexture(static_cast<GLenum>(mActiveTexture));
    }

 private:
    static void RestoreEnabled(GLenum capability, GLboolean enabled) {
        if (enabled) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }

    GLint mReadFramebuffer = 0;
    GLint mDrawFramebuffer = 0;
    GLint mReadBuffer = GL_BACK;
    GLint mDrawBuffer = GL_BACK;
    GLint mActiveTexture = GL_TEXTURE0;
    GLint mTexture0 = 0;
    GLint mSampler0 = 0;
    GLint mProgram = 0;
    GLint mVertexArray = 0;
    GLint mViewport[4] = {};
    GLboolean mColorMask[4] = {};
    GLboolean mDepthMask = GL_TRUE;
    GLboolean mScissorEnabled = GL_FALSE;
    GLboolean mDepthEnabled = GL_FALSE;
    GLboolean mBlendEnabled = GL_FALSE;
    GLboolean mCullEnabled = GL_FALSE;
    GLboolean mLogicEnabled = GL_FALSE;
    GLboolean mDitherEnabled = GL_FALSE;
    GLboolean mFramebufferSrgbEnabled = GL_FALSE;
};

bool IsPassThroughCopyFilter(const SIM::GX::CopyFilterState& filter) {
    const auto coefficients = filter.EffectiveCoefficients();
    return coefficients[0] == 0u &&
        coefficients[1] == 64u &&
        coefficients[2] == 0u;
}

bool IsGpuColorCopyFormat(u32 format) {
    return format == GX_TF_RGB565 || format == GX_TF_RGBA8;
}

size_t GpuCopyDestinationByteSize(
    const void* destination,
    u16 width,
    u16 height,
    u32 format) {
    SIM::GX::TextureState texture;
    texture.data = destination;
    texture.width = width;
    texture.height = height;
    texture.format = static_cast<GXTexFmt>(format);
    texture.mipmap = false;
    return SIM::GX::GetTextureSourceByteSize(texture);
}

bool SnapshotGpuCopyDestination(
    const void* destination,
    u16 width,
    u16 height,
    u32 format,
    std::vector<u8>& snapshot) {
    const size_t byteSize = GpuCopyDestinationByteSize(
        destination, width, height, format);
    if (destination == nullptr || byteSize == 0u) {
        snapshot.clear();
        return false;
    }
    snapshot.resize(byteSize);
    std::memcpy(snapshot.data(), destination, byteSize);
    return true;
}

void ConfigureTextureSampler(
    size_t textureUnit,
    const SIM::GX::TextureState& texture,
    size_t mipLevelCount) {
    if (textureUnit >= gTextureSamplers.size()) {
        return;
    }
    auto& state = gTextureSamplers[textureUnit];
    if (state.sampler == 0u) {
        glGenSamplers(1, &state.sampler);
    }
    const GLfloat maximumLod =
        mipLevelCount == 0u
            ? 0.0f
            : static_cast<GLfloat>(mipLevelCount - 1u);
    const GLint wrapS = ToGlWrap(texture.wrapS);
    const GLint wrapT = ToGlWrap(texture.wrapT);
    const GLint minFilter = ToGlMinFilter(texture.minFilter);
    const GLint magFilter =
        texture.magFilter == GX_LINEAR ? GL_LINEAR : GL_NEAREST;
    const GLfloat minLod =
        std::clamp(texture.minLod, 0.0f, maximumLod);
    const GLfloat maxLod =
        std::clamp(texture.maxLod, 0.0f, maximumLod);

    const auto updateInteger = [&](
        GLenum parameter, GLint value, GLint& cached) {
        if (!state.valid || cached != value) {
            glSamplerParameteri(state.sampler, parameter, value);
            cached = value;
        }
    };
    const auto updateFloat = [&](
        GLenum parameter, GLfloat value, GLfloat& cached) {
        if (!state.valid || cached != value) {
            glSamplerParameterf(state.sampler, parameter, value);
            cached = value;
        }
    };
    updateInteger(GL_TEXTURE_WRAP_S, wrapS, state.wrapS);
    updateInteger(GL_TEXTURE_WRAP_T, wrapT, state.wrapT);
    updateInteger(GL_TEXTURE_MIN_FILTER, minFilter, state.minFilter);
    updateInteger(GL_TEXTURE_MAG_FILTER, magFilter, state.magFilter);
    updateFloat(GL_TEXTURE_MIN_LOD, minLod, state.minLod);
    updateFloat(GL_TEXTURE_MAX_LOD, maxLod, state.maxLod);
    updateFloat(GL_TEXTURE_LOD_BIAS, texture.lodBias, state.lodBias);
    state.valid = true;
    glBindSampler(static_cast<GLuint>(textureUnit), state.sampler);
}

GpuCopyTexture* FindGpuCopyTextureKey(
    const void* destination,
    u16 width,
    u16 height,
    u32 format) {
    for (auto& copy : gGpuCopyTextures) {
        if (copy.destination == destination &&
            copy.width == width &&
            copy.height == height &&
            copy.format == static_cast<GXTexFmt>(format)) {
            return &copy;
        }
    }
    return nullptr;
}

GpuCopyTexture* FindLatestGpuCopyTexture(const void* destination) {
    GpuCopyTexture* latest = nullptr;
    for (auto& copy : gGpuCopyTextures) {
        if (copy.destination == destination &&
            (latest == nullptr || copy.revision > latest->revision)) {
            latest = &copy;
        }
    }
    return latest;
}

void InvalidateGpuCopyDestination(const void* destination) {
    if (auto* copy = FindLatestGpuCopyTexture(destination)) {
        copy->active = false;
    }
}

GpuCopyTexture* ResolveGpuCopyTexture(
    const SIM::GX::TextureState& texture,
    u64 invalidationRevision) {
    if (texture.data == nullptr || texture.mipmap ||
        texture.sourceEncoding !=
            SIM::GX::TextureState::SourceEncoding::CanonicalBigEndian) {
        return nullptr;
    }

    auto* copy = FindLatestGpuCopyTexture(texture.data);
    if (copy == nullptr || !copy->active || copy->texture == 0u ||
        copy->width != texture.width || copy->height != texture.height ||
        copy->format != texture.format) {
        return nullptr;
    }

    if (copy->validatedInvalidationRevision != invalidationRevision) {
        const size_t byteSize = SIM::GX::GetTextureSourceByteSize(texture);
        if (byteSize == 0u ||
            copy->destinationSnapshot.size() != byteSize ||
            std::memcmp(
                texture.data,
                copy->destinationSnapshot.data(),
                byteSize) != 0) {
            // GXInvalidateTexAll makes CPU writes visible. If the destination
            // bytes changed since GXCopyTex, the RAM texture supersedes the
            // GPU alias and must use the canonical decode path.
            copy->active = false;
            return nullptr;
        }
        copy->validatedInvalidationRevision = invalidationRevision;
    }
    return copy;
}

bool CheckFramebufferComplete() {
    return glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) ==
        GL_FRAMEBUFFER_COMPLETE;
}

bool CreateGpuColorCopyTexture(GpuCopyTexture& copy) {
    glGenTextures(1, &copy.texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, copy.texture);
    const GLint internalFormat =
        copy.format == GX_TF_RGB565 ? GL_RGB565 : GL_RGBA8;
    const GLenum uploadFormat =
        copy.format == GX_TF_RGB565 ? GL_RGB : GL_RGBA;
    const GLenum uploadType =
        copy.format == GX_TF_RGB565
            ? GL_UNSIGNED_SHORT_5_6_5
            : GL_UNSIGNED_BYTE;
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        copy.width,
        copy.height,
        0,
        uploadFormat,
        uploadType,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &copy.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.framebuffer);
    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        copy.texture,
        0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return CheckFramebufferComplete();
}

GLuint CompileCopyShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }
    glDeleteShader(shader);
    return 0u;
}

bool InitializeDepthCopyProgram() {
    if (gDepthCopyProgram != 0u) {
        return true;
    }
    if (gDepthCopyProgramAttempted) {
        return false;
    }
    gDepthCopyProgramAttempted = true;

    static constexpr const char* vertexSource = R"(
#version 330 core
void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0));
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
)";
    static constexpr const char* fragmentSource = R"(
#version 330 core
uniform sampler2D u_depth;
uniform uvec3 u_coefficients;
layout (location = 0) out float out_intensity;

uint gxDepthHighByte(ivec2 coordinate)
{
    ivec2 maximum = textureSize(u_depth, 0) - ivec2(1);
    float windowDepth = texelFetch(
        u_depth, clamp(coordinate, ivec2(0), maximum), 0).r;
    uint depth = min(
        uint(clamp(windowDepth * 2.0, 0.0, 1.0) * 16777216.0),
        0x00ffffffu);
    return depth >> 16u;
}

void main()
{
    ivec2 coordinate = ivec2(gl_FragCoord.xy);
    uint value =
        gxDepthHighByte(coordinate + ivec2(0, -1)) * u_coefficients.x +
        gxDepthHighByte(coordinate) * u_coefficients.y +
        gxDepthHighByte(coordinate + ivec2(0, 1)) * u_coefficients.z;
    value >>= 6u;
    if (u_coefficients.x + u_coefficients.y + u_coefficients.z >= 128u)
        value &= 0x1ffu;
    out_intensity = float(min(value, 255u)) / 255.0;
}
)";

    const GLuint vertexShader = CompileCopyShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader =
        CompileCopyShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0u || fragmentShader == 0u) {
        if (vertexShader != 0u) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0u) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        return false;
    }

    gDepthCopyProgram = program;
    gDepthCopySamplerLocation =
        glGetUniformLocation(gDepthCopyProgram, "u_depth");
    gDepthCopyCoefficientsLocation =
        glGetUniformLocation(gDepthCopyProgram, "u_coefficients");
    glGenVertexArrays(1, &gDepthCopyVertexArray);
    return gDepthCopyVertexArray != 0u;
}

bool CreateGpuDepthCopyTexture(GpuCopyTexture& copy) {
    if (!InitializeDepthCopyProgram()) {
        return false;
    }

    glGenTextures(1, &copy.texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, copy.texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8,
        copy.width,
        copy.height,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    glGenFramebuffers(1, &copy.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.framebuffer);
    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        copy.texture,
        0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (!CheckFramebufferComplete()) {
        return false;
    }

    glGenTextures(1, &copy.depthTexture);
    glBindTexture(GL_TEXTURE_2D, copy.depthTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        copy.width,
        copy.height,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glGenFramebuffers(1, &copy.depthFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.depthFramebuffer);
    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        copy.depthTexture,
        0);
    glDrawBuffer(GL_NONE);
    return CheckFramebufferComplete();
}

void DeleteGpuCopyTextureObjects(GpuCopyTexture& copy) {
    if (copy.framebuffer != 0u) {
        glDeleteFramebuffers(1, &copy.framebuffer);
    }
    if (copy.depthFramebuffer != 0u) {
        glDeleteFramebuffers(1, &copy.depthFramebuffer);
    }
    if (copy.texture != 0u) {
        glDeleteTextures(1, &copy.texture);
    }
    if (copy.depthTexture != 0u) {
        glDeleteTextures(1, &copy.depthTexture);
    }
    copy.framebuffer = 0u;
    copy.depthFramebuffer = 0u;
    copy.texture = 0u;
    copy.depthTexture = 0u;
}

GpuCopyTexture* GetOrCreateGpuCopyTexture(
    const void* destination,
    u16 width,
    u16 height,
    u32 format) {
    if (auto* existing =
            FindGpuCopyTextureKey(destination, width, height, format)) {
        return existing;
    }

    GpuCopyTexture copy;
    copy.destination = destination;
    copy.width = width;
    copy.height = height;
    copy.format = static_cast<GXTexFmt>(format);
    const bool created =
        format == GX_TF_Z8
            ? CreateGpuDepthCopyTexture(copy)
            : CreateGpuColorCopyTexture(copy);
    if (!created) {
        DeleteGpuCopyTextureObjects(copy);
        return nullptr;
    }
    gGpuCopyTextures.push_back(std::move(copy));
    return &gGpuCopyTextures.back();
}

struct GpuCopySourceRect {
    GLint left = 0;
    GLint top = 0;
    GLint right = 0;
    GLint bottom = 0;
};

GpuCopySourceRect ComputeGpuCopySourceRect(
    u16 sourceLeft,
    u16 sourceTop,
    u16 sourceWidth,
    u16 sourceHeight,
    float scaleX,
    float scaleY,
    int drawableWidth,
    int drawableHeight) {
    const GLint left = std::clamp(
        static_cast<GLint>(std::floor(sourceLeft * scaleX)),
        0,
        drawableWidth);
    const GLint right = std::clamp(
        static_cast<GLint>(
            std::ceil((sourceLeft + sourceWidth) * scaleX)),
        left,
        drawableWidth);
    const GLint topDownTop = std::clamp(
        static_cast<GLint>(std::floor(sourceTop * scaleY)),
        0,
        drawableHeight);
    const GLint topDownBottom = std::clamp(
        static_cast<GLint>(
            std::ceil((sourceTop + sourceHeight) * scaleY)),
        topDownTop,
        drawableHeight);
    // Reversing the OpenGL source Y endpoints writes GX's top row into
    // texture row zero, matching the canonical decoder's orientation.
    return {
        left,
        drawableHeight - topDownTop,
        right,
        drawableHeight - topDownBottom,
    };
}

bool BlitGpuColorCopy(
    GpuCopyTexture& copy,
    const GpuCopySourceRect& source,
    bool linearFilter) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBlitFramebuffer(
        source.left,
        source.top,
        source.right,
        source.bottom,
        0,
        0,
        copy.width,
        copy.height,
        GL_COLOR_BUFFER_BIT,
        linearFilter ? GL_LINEAR : GL_NEAREST);
    return glGetError() == GL_NO_ERROR;
}

bool BlitGpuDepthCopy(
    GpuCopyTexture& copy,
    const GpuCopySourceRect& source,
    const SIM::GX::CopyFilterState& filter) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.depthFramebuffer);
    glDrawBuffer(GL_NONE);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(
        source.left,
        source.top,
        source.right,
        source.bottom,
        0,
        0,
        copy.width,
        copy.height,
        GL_DEPTH_BUFFER_BIT,
        GL_NEAREST);
    if (glGetError() != GL_NO_ERROR) {
        return false;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, copy.width, copy.height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_DITHER);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(gDepthCopyProgram);
    glBindVertexArray(gDepthCopyVertexArray);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, copy.depthTexture);
    if (gDepthCopySamplerLocation >= 0) {
        glUniform1i(gDepthCopySamplerLocation, 0);
    }
    if (gDepthCopyCoefficientsLocation >= 0) {
        const auto coefficients = filter.EffectiveCoefficients();
        glUniform3ui(
            gDepthCopyCoefficientsLocation,
            coefficients[0],
            coefficients[1],
            coefficients[2]);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return glGetError() == GL_NO_ERROR;
}

GpuCopyTexture* TryGpuCopyTexture(
    void* destination,
    u16 sourceLeft,
    u16 sourceTop,
    u16 sourceWidth,
    u16 sourceHeight,
    u16 destinationWidth,
    u16 destinationHeight,
    u32 destinationFormat,
    float scaleX,
    float scaleY,
    int drawableWidth,
    int drawableHeight,
    const SIM::GX::CopyFilterState& filter) {
    const bool depthCopy = destinationFormat == GX_TF_Z8;
    if ((!depthCopy && !IsGpuColorCopyFormat(destinationFormat)) ||
        (!depthCopy && !IsPassThroughCopyFilter(filter))) {
        return nullptr;
    }

    ScopedCopyGlState restoreState;
    DrainGlErrors();
    GLint readFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    if (readFramebuffer == 0) {
        glReadBuffer(GL_BACK);
    }
    auto* copy = GetOrCreateGpuCopyTexture(
        destination,
        destinationWidth,
        destinationHeight,
        destinationFormat);
    if (copy == nullptr) {
        return nullptr;
    }
    if (!SnapshotGpuCopyDestination(
            destination,
            destinationWidth,
            destinationHeight,
            destinationFormat,
            copy->destinationSnapshot)) {
        return nullptr;
    }

    const auto source = ComputeGpuCopySourceRect(
        sourceLeft,
        sourceTop,
        sourceWidth,
        sourceHeight,
        scaleX,
        scaleY,
        drawableWidth,
        drawableHeight);
    if (source.left >= source.right || source.bottom >= source.top) {
        return nullptr;
    }

    const float scaledWidth = sourceWidth * scaleX;
    const float scaledHeight = sourceHeight * scaleY;
    const bool linearFilter = filter.halfScale ||
        std::fabs(scaledWidth - destinationWidth) > 0.0001f ||
        std::fabs(scaledHeight - destinationHeight) > 0.0001f;
    const bool copied = depthCopy
        ? BlitGpuDepthCopy(*copy, source, filter)
        : BlitGpuColorCopy(*copy, source, linearFilter);
    if (!copied) {
        return nullptr;
    }

    copy->revision = ++gGpuCopyTextureRevision;
    copy->active = true;
    return copy;
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
                        // Flipper's CMPR interpolation uses the S3TC 5:3
                        // fixed-point blend, not the desktop DXT1 2:1 blend.
                        palette[2][component] = static_cast<u8>(
                            (5u * palette[0][component] +
                             3u * palette[1][component]) >>
                            3u);
                        palette[3][component] = static_cast<u8>(
                            (3u * palette[0][component] +
                             5u * palette[1][component]) >>
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
                        // GX preserves the midpoint RGB for selector three
                        // and only clears alpha. Some TEV paths observe RGB
                        // even when the sampled alpha is zero.
                        palette[3][component] = palette[2][component];
                    }
                    palette[2][3] = 255u;
                    palette[3][3] = 0u;
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
        left.offsetX == right.offsetX &&
        left.offsetY == right.offsetY &&
        left.valid == right.valid;
}

bool SameScissorOffset(
    const ScissorState& left,
    const ScissorState& right) {
    return
        left.offsetX == right.offsetX &&
        left.offsetY == right.offsetY;
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

DrawableScissorRect ComputeDrawableScissor(
    const ViewportState& viewport,
    const ScissorState& scissor,
    int drawableWidth,
    int drawableHeight) {
    if (!scissor.valid ||
        viewport.referenceWidth <= 0.0f ||
        viewport.referenceHeight <= 0.0f ||
        drawableWidth <= 0 ||
        drawableHeight <= 0) {
        return {};
    }

    const double scaleX =
        static_cast<double>(drawableWidth) / viewport.referenceWidth;
    const double scaleY =
        static_cast<double>(drawableHeight) / viewport.referenceHeight;
    const double logicalLeft =
        static_cast<double>(scissor.left) - scissor.offsetX;
    const double logicalTop =
        static_cast<double>(scissor.top) - scissor.offsetY;
    const double logicalRight = logicalLeft + scissor.width;
    const double logicalBottom = logicalTop + scissor.height;

    const int left = std::clamp(
        static_cast<int>(std::floor(logicalLeft * scaleX)),
        0,
        drawableWidth);
    const int right = std::clamp(
        static_cast<int>(std::ceil(logicalRight * scaleX)),
        0,
        drawableWidth);
    const int top = std::clamp(
        static_cast<int>(std::floor(logicalTop * scaleY)),
        0,
        drawableHeight);
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(logicalBottom * scaleY)),
        0,
        drawableHeight);

    return {
        left,
        drawableHeight - bottom,
        std::max(0, right - left),
        std::max(0, bottom - top),
    };
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
            if (!SameScissorOffset(mScissor, scissor)) {
                // The scissor offset also participates in the viewport
                // origin; ordinary scissor rectangle changes do not.
                dirty |= RenderStateViewport;
            }
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

bool ResampleOpenGlFramebufferRgba(
    const u8* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    std::vector<u8>& rgba) {
    if (framebuffer == nullptr ||
        framebufferWidth == 0u || framebufferHeight == 0u ||
        sourceWidth <= 0.0f || sourceHeight <= 0.0f ||
        destinationWidth == 0u || destinationHeight == 0u) {
        rgba.clear();
        return false;
    }

    rgba.resize(destinationWidth * destinationHeight * 4u);
    const auto pixel = [=](int topDownX, int topDownY, size_t component) {
        const size_t x = static_cast<size_t>(std::clamp(
            topDownX, 0, static_cast<int>(framebufferWidth) - 1));
        const size_t y = static_cast<size_t>(std::clamp(
            topDownY, 0, static_cast<int>(framebufferHeight) - 1));
        const size_t openGlY = framebufferHeight - 1u - y;
        return framebuffer[
            (openGlY * framebufferWidth + x) * 4u + component];
    };

    for (size_t destinationY = 0u;
         destinationY < destinationHeight;
         ++destinationY) {
        const float sampleY =
            sourceTop +
            (static_cast<float>(destinationY) + 0.5f) *
                sourceHeight / static_cast<float>(destinationHeight) -
            0.5f;
        const int y0 = static_cast<int>(std::floor(sampleY));
        const int y1 = y0 + 1;
        const float yFraction = sampleY - static_cast<float>(y0);
        for (size_t destinationX = 0u;
             destinationX < destinationWidth;
             ++destinationX) {
            const float sampleX =
                sourceLeft +
                (static_cast<float>(destinationX) + 0.5f) *
                    sourceWidth / static_cast<float>(destinationWidth) -
                0.5f;
            const int x0 = static_cast<int>(std::floor(sampleX));
            const int x1 = x0 + 1;
            const float xFraction = sampleX - static_cast<float>(x0);
            const size_t destinationOffset =
                (destinationY * destinationWidth + destinationX) * 4u;
            for (size_t component = 0u; component < 4u; ++component) {
                const float top =
                    static_cast<float>(pixel(x0, y0, component)) *
                        (1.0f - xFraction) +
                    static_cast<float>(pixel(x1, y0, component)) *
                        xFraction;
                const float bottom =
                    static_cast<float>(pixel(x0, y1, component)) *
                        (1.0f - xFraction) +
                    static_cast<float>(pixel(x1, y1, component)) *
                        xFraction;
                rgba[destinationOffset + component] = static_cast<u8>(
                    std::clamp(
                        std::lround(
                            top * (1.0f - yFraction) +
                            bottom * yFraction),
                        0l,
                        255l));
            }
        }
    }
    return true;
}

namespace {

u8 ApplyEfbCopyFilter(
    u8 previous,
    u8 current,
    u8 next,
    const std::array<u32, 3>& coefficients) {
    u32 value =
        static_cast<u32>(previous) * coefficients[0] +
        static_cast<u32>(current) * coefficients[1] +
        static_cast<u32>(next) * coefficients[2];
    value >>= 6u;
    if (coefficients[0] + coefficients[1] + coefficients[2] >= 128u) {
        // Flipper's nine-bit accumulator wraps before its final saturation.
        value &= 0x1ffu;
    }
    return static_cast<u8>(std::min<u32>(value, 255));
}

std::array<u8, 4> DepthCopySample(float windowDepth) {
    // libPorpoise's GX projection occupies OpenGL window depth 0..0.5.
    const float normalized = std::clamp(windowDepth * 2.0f, 0.0f, 1.0f);
    const u32 depth = std::min<u32>(
        static_cast<u32>(normalized * 16777216.0f),
        0x00ffffff);
    return {
        static_cast<u8>(depth >> 16u),
        static_cast<u8>(depth >> 8u),
        static_cast<u8>(depth),
        255u,
    };
}

}

bool FilterTextureCopyRgba(
    const u8* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    const CopyFilterState& filter,
    std::vector<u8>& rgba) {
    if (framebuffer == nullptr || framebufferWidth == 0u ||
        framebufferHeight == 0u || sourceWidth <= 0.0f ||
        sourceHeight <= 0.0f || destinationWidth == 0u ||
        destinationHeight == 0u) {
        rgba.clear();
        return false;
    }

    const auto sourcePixel = [=](int x, int y, size_t component) {
        const size_t clampedX = static_cast<size_t>(std::clamp(
            x, 0, static_cast<int>(framebufferWidth) - 1));
        const size_t topDownY = static_cast<size_t>(std::clamp(
            y, 0, static_cast<int>(framebufferHeight) - 1));
        const size_t openGlY = framebufferHeight - 1u - topDownY;
        return framebuffer[
            (openGlY * framebufferWidth + clampedX) * 4u + component];
    };
    const auto linearSample = [&](float x, float y, size_t component) {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float fx = x - static_cast<float>(x0);
        const float fy = y - static_cast<float>(y0);
        const float top =
            static_cast<float>(sourcePixel(x0, y0, component)) * (1.0f - fx) +
            static_cast<float>(sourcePixel(x0 + 1, y0, component)) * fx;
        const float bottom =
            static_cast<float>(sourcePixel(x0, y0 + 1, component)) * (1.0f - fx) +
            static_cast<float>(sourcePixel(x0 + 1, y0 + 1, component)) * fx;
        // Dolphin's copy shader converts the normalized sample to uint, so
        // fractional byte values truncate instead of rounding to nearest.
        return static_cast<u8>(std::clamp(
            top * (1.0f - fy) + bottom * fy, 0.0f, 255.0f));
    };

    const float stepX = sourceWidth / static_cast<float>(destinationWidth);
    const float stepY = sourceHeight / static_cast<float>(destinationHeight);
    const bool linearFilter =
        filter.halfScale ||
        std::fabs(stepX - 1.0f) > 0.0001f ||
        std::fabs(stepY - 1.0f) > 0.0001f;
    const auto copySample = [&](float x, float y, size_t component) {
        if (linearFilter) {
            return linearSample(x, y, component);
        }
        return sourcePixel(
            static_cast<int>(std::floor(x + 0.5f)),
            static_cast<int>(std::floor(y + 0.5f)),
            component);
    };
    const auto coefficients = filter.EffectiveCoefficients();
    rgba.resize(destinationWidth * destinationHeight * 4u);
    for (size_t y = 0; y < destinationHeight; ++y) {
        const float sourceY =
            sourceTop + (static_cast<float>(y) + 0.5f) * stepY - 0.5f;
        for (size_t x = 0; x < destinationWidth; ++x) {
            const float sourceX =
                sourceLeft + (static_cast<float>(x) + 0.5f) * stepX - 0.5f;
            const size_t offset = (y * destinationWidth + x) * 4u;
            for (size_t component = 0; component < 3u; ++component) {
                rgba[offset + component] = ApplyEfbCopyFilter(
                    copySample(sourceX, sourceY - stepY, component),
                    copySample(sourceX, sourceY, component),
                    copySample(sourceX, sourceY + stepY, component),
                    coefficients);
            }
            // Copy filtering does not filter alpha, though half scaling's
            // box sample still applies to the current pixel.
            rgba[offset + 3u] = copySample(sourceX, sourceY, 3u);
        }
    }
    return true;
}

bool FilterTextureCopyDepth(
    const float* framebuffer,
    size_t framebufferWidth,
    size_t framebufferHeight,
    float sourceLeft,
    float sourceTop,
    float sourceWidth,
    float sourceHeight,
    size_t destinationWidth,
    size_t destinationHeight,
    const CopyFilterState& filter,
    std::vector<u8>& depthBytes) {
    if (framebuffer == nullptr || framebufferWidth == 0u ||
        framebufferHeight == 0u || sourceWidth <= 0.0f ||
        sourceHeight <= 0.0f || destinationWidth == 0u ||
        destinationHeight == 0u) {
        depthBytes.clear();
        return false;
    }

    const auto nearestSample = [&](float x, float y) {
        // Dolphin deliberately uses a point sampler for depth EFB copies,
        // including half-scale copies. At the 2:1 boundary this selects the
        // second texel, matching GXCopyTex's depth behavior.
        const int sampleX = std::clamp(
            static_cast<int>(std::floor(x + 0.5f)),
            0,
            static_cast<int>(framebufferWidth) - 1);
        const int topDownY = std::clamp(
            static_cast<int>(std::floor(y + 0.5f)),
            0,
            static_cast<int>(framebufferHeight) - 1);
        const size_t openGlY =
            framebufferHeight - 1u - static_cast<size_t>(topDownY);
        return DepthCopySample(framebuffer[
            openGlY * framebufferWidth + static_cast<size_t>(sampleX)]);
    };

    const float stepX = sourceWidth / static_cast<float>(destinationWidth);
    const float stepY = sourceHeight / static_cast<float>(destinationHeight);
    const auto coefficients = filter.EffectiveCoefficients();
    depthBytes.resize(destinationWidth * destinationHeight * 4u);
    for (size_t y = 0; y < destinationHeight; ++y) {
        const float sourceY =
            sourceTop + (static_cast<float>(y) + 0.5f) * stepY - 0.5f;
        for (size_t x = 0; x < destinationWidth; ++x) {
            const float sourceX =
                sourceLeft + (static_cast<float>(x) + 0.5f) * stepX - 0.5f;
            const auto previous = nearestSample(sourceX, sourceY - stepY);
            const auto current = nearestSample(sourceX, sourceY);
            const auto next = nearestSample(sourceX, sourceY + stepY);
            const size_t offset = (y * destinationWidth + x) * 4u;
            for (size_t component = 0; component < 3u; ++component) {
                depthBytes[offset + component] = ApplyEfbCopyFilter(
                    previous[component],
                    current[component],
                    next[component],
                    coefficients);
            }
            depthBytes[offset + 3u] = 255u;
        }
    }
    return true;
}

}

u8 ConvertRgbToCopyIntensity(u8 red, u8 green, u8 blue) {
    // GX's intensity conversion uses the same limited-range Y coefficients
    // as its XFB conversion, with half-up rounding after division by 256.
    const u32 sum =
        66u * static_cast<u32>(red) +
        129u * static_cast<u32>(green) +
        25u * static_cast<u32>(blue) +
        16u * 256u;
    return static_cast<u8>((sum >> 8u) + ((sum >> 7u) & 1u));
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

bool EncodeDepthTextureCopyBytes(
    const u8* depthBytes,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded) {
    if (depthBytes == nullptr || encoded == nullptr ||
        width == 0u || height == 0u) {
        return false;
    }

    if (destinationFormat == GX_TF_Z24X8) {
        // Z24X8 uses the same two-plane 4x4 tile layout as RGBA8: X/Z-high
        // in the first cache line and Z-mid/Z-low in the second. Supplying
        // {high, mid, low, 0xff} to the RGBA8 packer produces that layout.
        return EncodeColorTextureCopy(
            depthBytes, width, height, GX_TF_RGBA8, encoded);
    }
    if (destinationFormat != GX_TF_Z8 &&
        destinationFormat != GX_TF_Z16) {
        return false;
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
                    std::array<u8, 3> depth = {};
                    if (sourceX < width && sourceY < height) {
                        const u8* source = depthBytes +
                            (sourceY * static_cast<size_t>(width) + sourceX) * 4u;
                        depth = {source[0], source[1], source[2]};
                    }

                    if (destinationFormat == GX_TF_Z8) {
                        *encoded++ = depth[0];
                    } else {
                        // Z16 copies are sampled as IA8 by GX. The copy
                        // engine places the low depth byte in alpha and the
                        // high byte in intensity, opposite a native IA8 word.
                        *encoded++ = depth[1];
                        *encoded++ = depth[0];
                    }
                }
            }
        }
    }
    return true;
}

void EncodeDepthTextureCopy(
    const float* depth,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded) {
    if (depth == nullptr || encoded == nullptr || width == 0u || height == 0u) {
        return;
    }

    std::vector<u8> depthBytes(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (size_t pixel = 0;
         pixel < static_cast<size_t>(width) * static_cast<size_t>(height);
         ++pixel) {
        const float normalized = std::clamp(depth[pixel] * 2.0f, 0.0f, 1.0f);
        const u32 depth24 = std::min<u32>(
            static_cast<u32>(normalized * 16777216.0f),
            0x00ffffff);
        depthBytes[pixel * 4u] = static_cast<u8>(depth24 >> 16u);
        depthBytes[pixel * 4u + 1u] = static_cast<u8>(depth24 >> 8u);
        depthBytes[pixel * 4u + 2u] = static_cast<u8>(depth24);
        depthBytes[pixel * 4u + 3u] = 255u;
    }
    (void)EncodeDepthTextureCopyBytes(
        depthBytes.data(), width, height, destinationFormat, encoded);
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

void GlRenderer::TrimTextureCache(size_t incomingDecodedBytes) {
    const auto overBudget = [this, incomingDecodedBytes]() {
        const bool entryPressure =
            mTextureCache.size() >= MaximumTextureCacheEntries;
        const bool bytePressure =
            incomingDecodedBytes > MaximumTextureCacheBytes ||
            mTextureCacheDecodedBytes >
                MaximumTextureCacheBytes - incomingDecodedBytes;
        return entryPressure || bytePressure;
    };

    while (!mTextureCache.empty() && overBudget()) {
        auto victim = mTextureCache.end();
        for (auto candidate = mTextureCache.begin();
             candidate != mTextureCache.end();
             ++candidate) {
            // A texture already bound by an earlier TEV stage in this draw
            // must remain alive until the draw is submitted.
            if (candidate->second.lastUsedSerial == mTextureUseSerial) {
                continue;
            }
            if (victim == mTextureCache.end() ||
                candidate->second.lastUsedSerial <
                    victim->second.lastUsedSerial) {
                victim = candidate;
            }
        }
        if (victim == mTextureCache.end()) {
            break;
        }

        if (victim->second.texture != 0u) {
            const GLuint texture =
                static_cast<GLuint>(victim->second.texture);
            glDeleteTextures(1, &texture);
        }
        mTextureCacheDecodedBytes -=
            victim->second.decodedByteSize;
        mTextureCache.erase(victim);
    }
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

bool EncodeColorTextureCopy(
    const u8* rgba,
    u16 width,
    u16 height,
    u32 destinationFormat,
    u8* encoded) {
    if (rgba == nullptr || encoded == nullptr || width == 0u || height == 0u) {
        return false;
    }

    const auto sourcePixel = [=](size_t x, size_t y) -> const u8* {
        return rgba + (y * static_cast<size_t>(width) + x) * 4u;
    };
    if (destinationFormat == GX_TF_I4) {
        const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
        const size_t blockRows = (static_cast<size_t>(height) + 7u) / 8u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 8u; ++y) {
                    for (size_t x = 0; x < 8u; x += 2u) {
                        u8 packed = 0u;
                        for (size_t pixel = 0; pixel < 2u; ++pixel) {
                            const size_t sourceX = blockX * 8u + x + pixel;
                            const size_t sourceY = blockY * 8u + y;
                            if (sourceX < width && sourceY < height) {
                                const u8* color = sourcePixel(sourceX, sourceY);
                                const u8 intensity = ConvertRgbToCopyIntensity(
                                    color[0], color[1], color[2]);
                                packed |= static_cast<u8>(
                                    (intensity >> 4u) << (pixel == 0u ? 4u : 0u));
                            }
                        }
                        *encoded++ = packed;
                    }
                }
            }
        }
        return true;
    }

    if (destinationFormat == GX_TF_I8 || destinationFormat == GX_CTF_A8 ||
        destinationFormat == GX_CTF_R8 || destinationFormat == GX_CTF_G8 ||
        destinationFormat == GX_CTF_B8) {
        const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
        const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 8u; ++x) {
                        const size_t sourceX = blockX * 8u + x;
                        const size_t sourceY = blockY * 4u + y;
                        u8 value = 0u;
                        if (sourceX < width && sourceY < height) {
                            const u8* color = sourcePixel(sourceX, sourceY);
                            value = ConvertColorToTextureCopyByte(
                                destinationFormat,
                                color[0],
                                color[1],
                                color[2],
                                color[3]);
                        }
                        *encoded++ = value;
                    }
                }
            }
        }
        return true;
    }

    if (destinationFormat == GX_TF_IA4) {
        const size_t blockColumns = (static_cast<size_t>(width) + 7u) / 8u;
        const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 8u; ++x) {
                        const size_t sourceX = blockX * 8u + x;
                        const size_t sourceY = blockY * 4u + y;
                        u8 packed = 0u;
                        if (sourceX < width && sourceY < height) {
                            const u8* color = sourcePixel(sourceX, sourceY);
                            packed = static_cast<u8>(
                                (color[3] & 0xf0u) |
                                (ConvertRgbToCopyIntensity(
                                     color[0], color[1], color[2]) >> 4u));
                        }
                        *encoded++ = packed;
                    }
                }
            }
        }
        return true;
    }

    if (destinationFormat == GX_TF_IA8) {
        const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
        const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 4u; ++x) {
                        const size_t sourceX = blockX * 4u + x;
                        const size_t sourceY = blockY * 4u + y;
                        u8 alpha = 0u;
                        u8 intensity = 0u;
                        if (sourceX < width && sourceY < height) {
                            const u8* color = sourcePixel(sourceX, sourceY);
                            alpha = color[3];
                            intensity = ConvertRgbToCopyIntensity(
                                color[0], color[1], color[2]);
                        }
                        *encoded++ = alpha;
                        *encoded++ = intensity;
                    }
                }
            }
        }
        return true;
    }

    if (destinationFormat == GX_TF_RGB565) {
        EncodeRgb565TextureCopy(rgba, width, height, encoded);
        return true;
    }

    if (destinationFormat == GX_TF_RGB5A3) {
        const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
        const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 4u; ++x) {
                        const size_t sourceX = blockX * 4u + x;
                        const size_t sourceY = blockY * 4u + y;
                        u16 packed = 0u;
                        if (sourceX < width && sourceY < height) {
                            const u8* color = sourcePixel(sourceX, sourceY);
                            if (color[3] > 224u) {
                                packed = static_cast<u16>(
                                    0x8000u |
                                    (static_cast<u16>(color[0] >> 3u) << 10u) |
                                    (static_cast<u16>(color[1] >> 3u) << 5u) |
                                    static_cast<u16>(color[2] >> 3u));
                            } else {
                                packed = static_cast<u16>(
                                    (static_cast<u16>(color[3] >> 5u) << 12u) |
                                    (static_cast<u16>(color[0] >> 4u) << 8u) |
                                    (static_cast<u16>(color[1] >> 4u) << 4u) |
                                    static_cast<u16>(color[2] >> 4u));
                            }
                        }
                        *encoded++ = static_cast<u8>(packed >> 8u);
                        *encoded++ = static_cast<u8>(packed);
                    }
                }
            }
        }
        return true;
    }

    if (destinationFormat == GX_TF_RGBA8) {
        const size_t blockColumns = (static_cast<size_t>(width) + 3u) / 4u;
        const size_t blockRows = (static_cast<size_t>(height) + 3u) / 4u;
        for (size_t blockY = 0; blockY < blockRows; ++blockY) {
            for (size_t blockX = 0; blockX < blockColumns; ++blockX) {
                u8* alphaRed = encoded;
                u8* greenBlue = encoded + 32u;
                encoded += 64u;
                for (size_t y = 0; y < 4u; ++y) {
                    for (size_t x = 0; x < 4u; ++x) {
                        const size_t sourceX = blockX * 4u + x;
                        const size_t sourceY = blockY * 4u + y;
                        const size_t blockPixel = y * 4u + x;
                        if (sourceX < width && sourceY < height) {
                            const u8* color = sourcePixel(sourceX, sourceY);
                            alphaRed[blockPixel * 2u] = color[3];
                            alphaRed[blockPixel * 2u + 1u] = color[0];
                            greenBlue[blockPixel * 2u] = color[1];
                            greenBlue[blockPixel * 2u + 1u] = color[2];
                        } else {
                            alphaRed[blockPixel * 2u] = 0u;
                            alphaRed[blockPixel * 2u + 1u] = 0u;
                            greenBlue[blockPixel * 2u] = 0u;
                            greenBlue[blockPixel * 2u + 1u] = 0u;
                        }
                    }
                }
            }
        }
        return true;
    }
    return false;
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
    ++mTextureUseSerial;

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
        uniformValues.stageTexCoordScales.fill(1.0f);
    }
    constexpr size_t maxTevStages =
        Detail::ShaderUniformValues::MaxTevStages;
    auto& useTextures = uniformValues.useTextures;
    auto& textureUnits = uniformValues.stageTextures;
    auto& textureCoordinates = uniformValues.stageTexCoords;
    auto& textureCoordinateScales =
        uniformValues.stageTexCoordScales;
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
            // A TEV stage with texturing disabled reads white.  Hardware
            // instead returns black when the stage is enabled but no texture
            // coordinate generators exist; preserve that distinction for the
            // fragment shader with a negative sentinel.
            useTextures[stageIndex] =
                stage.textureEnabled && gxState.GetNumTexGens() == 0u
                    ? -1
                    : 0;
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
            gxState.GetNumTexGens() == 0u ||
            stage.textureMap >= TextureMapCount) {
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

        if (updateUniformValues) {
            // Out-of-range TEV texcoord selectors fall back to coordinate
            // zero on hardware.  Use the matching setup-unit scale in that
            // case rather than indexing beyond its eight coordinate slots.
            const size_t coordinateIndex =
                stage.textureCoordinate < gxState.GetNumTexGens()
                    ? static_cast<size_t>(stage.textureCoordinate)
                    : 0u;
            const auto& coordinateScale =
                gxState.GetTexCoordScaleState(coordinateIndex);
            textureCoordinateScales[stageIndex * 2u] =
                static_cast<float>(coordinateScale.scaleS + 1u) /
                static_cast<float>(texture.width);
            textureCoordinateScales[stageIndex * 2u + 1u] =
                static_cast<float>(coordinateScale.scaleT + 1u) /
                static_cast<float>(texture.height);
        }

        const u64 invalidationRevision =
            gxState.GetTextureInvalidationRevision();
        glActiveTexture(
            static_cast<GLenum>(GL_TEXTURE0 + stageIndex));
        if (auto* gpuCopy =
                ResolveGpuCopyTexture(texture, invalidationRevision)) {
            glBindTexture(GL_TEXTURE_2D, gpuCopy->texture);
            // The same capture can be loaded through multiple GXTexObjs.
            // Per-unit sampler objects preserve each stage's independent
            // wrap/filter state while sharing one GPU-resident image.
            ConfigureTextureSampler(stageIndex, texture, 1u);
            if (updateUniformValues) {
                useTextures[stageIndex] = 1;
            }
            continue;
        }

        const bool usesTlut =
            texture.format == GX_TF_C4 ||
            texture.format == GX_TF_C8 ||
            texture.format == GX_TF_C14X2;
        const TlutState* tlut =
            usesTlut
                ? &gxState.GetTlutState(texture.tlutName)
                : nullptr;
        const auto sourceKey =
            Detail::MakeTextureSourceKey(texture, tlut);
        const size_t levelCount = sourceKey.mipLevelCount;
        const size_t decodedByteSize =
            Detail::GetDecodedTextureByteSize(texture);
        if (levelCount == 0u || decodedByteSize == 0u) {
            continue;
        }

        auto cached = mTextureCache.find(sourceKey);
        const bool textureObjectCreated =
            cached == mTextureCache.end();
        if (textureObjectCreated) {
            TrimTextureCache(decodedByteSize);
            Detail::TextureCacheEntry entry;
            GLuint textureObject = 0u;
            glGenTextures(1, &textureObject);
            if (textureObject == 0u) {
                continue;
            }
            entry.texture = textureObject;
            entry.decodedByteSize = decodedByteSize;
            entry.lastUsedSerial = mTextureUseSerial;
            cached = mTextureCache.emplace(
                sourceKey, std::move(entry)).first;
            mTextureCacheDecodedBytes += decodedByteSize;
        }

        auto& cachedTexture = cached->second;
        cachedTexture.lastUsedSerial = mTextureUseSerial;
        glBindTexture(
            GL_TEXTURE_2D,
            static_cast<GLuint>(cachedTexture.texture));
        if (textureObjectCreated) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MAX_LEVEL,
                static_cast<GLint>(levelCount - 1u));
        }
        ConfigureTextureSampler(stageIndex, texture, levelCount);

        const bool validateTexture =
            textureObjectCreated ||
            cachedTexture.validatedInvalidationRevision !=
                invalidationRevision;
        const bool uploadTexture =
            validateTexture &&
            !cachedTexture.snapshot.Matches(texture, tlut);
        bool textureReady = true;
        if (uploadTexture) {
            std::vector<u8> canonicalBytes;
            std::vector<u8> palette;
            if (usesTlut) {
                palette = DecodeTlut(*tlut);
            }
            if (!CopyCanonicalTextureBytes(texture, canonicalBytes)) {
                textureReady = false;
            }
            for (size_t level = 0u;
                 textureReady && level < levelCount;
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
                    textureReady = false;
                    continue;
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
            if (textureReady) {
                textureReady = cachedTexture.snapshot.CaptureCanonical(
                    texture, std::move(canonicalBytes), tlut);
            }
        }

        if (!textureReady) {
            if (textureObjectCreated) {
                const GLuint textureObject =
                    static_cast<GLuint>(cachedTexture.texture);
                glDeleteTextures(1, &textureObject);
                mTextureCacheDecodedBytes -=
                    cachedTexture.decodedByteSize;
                mTextureCache.erase(cached);
            }
            continue;
        }
        if (validateTexture) {
            cachedTexture.validatedInvalidationRevision =
                invalidationRevision;
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
    const GLint stageTexCoordScalesLocation =
        uniformLocations[ShaderUniform::StageTexCoordScales];
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
    if (stageTexCoordScalesLocation >= 0 &&
        isUniformDirty(ShaderUniform::StageTexCoordScales)) {
        glUniform2fv(
            stageTexCoordScalesLocation,
            static_cast<GLsizei>(maxTevStages),
            textureCoordinateScales.data());
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

    const bool depthCopy =
        destinationFormat == GX_TF_Z8 ||
        destinationFormat == GX_TF_Z16 ||
        destinationFormat == GX_TF_Z24X8;
    const auto& copyFilter = gxState.GetCopyFilterState();
    GpuCopyTexture* gpuCopy = TryGpuCopyTexture(
        destination,
        sourceLeft,
        sourceTop,
        sourceWidth,
        sourceHeight,
        destinationWidth,
        destinationHeight,
        destinationFormat,
        scaleX,
        scaleY,
        drawableWidth,
        drawableHeight,
        copyFilter);
    bool copyCompleted = gpuCopy != nullptr;

    if (!copyCompleted) {
        GLint previousReadBuffer = GL_BACK;
        GLint previousPackAlignment = 4;
        glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

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

        const float scaledSourceLeft =
            static_cast<float>(sourceLeft) * scaleX;
        const float scaledSourceTop =
            static_cast<float>(sourceTop) * scaleY;
        const float scaledSourceWidth =
            static_cast<float>(sourceWidth) * scaleX;
        const float scaledSourceHeight =
            static_cast<float>(sourceHeight) * scaleY;
        if (depthCopy) {
            std::vector<u8> copiedDepthBytes;
            if (SIM::GX::Detail::FilterTextureCopyDepth(
                    depthBuffer.data(),
                    static_cast<size_t>(drawableWidth),
                    static_cast<size_t>(drawableHeight),
                    scaledSourceLeft,
                    scaledSourceTop,
                    scaledSourceWidth,
                    scaledSourceHeight,
                    destinationWidth,
                    destinationHeight,
                    copyFilter,
                    copiedDepthBytes)) {
                copyCompleted = SIM::GX::EncodeDepthTextureCopyBytes(
                    copiedDepthBytes.data(),
                    destinationWidth,
                    destinationHeight,
                    destinationFormat,
                    static_cast<u8*>(destination));
            }
        } else {
            std::vector<u8> copiedPixels;
            if (SIM::GX::Detail::FilterTextureCopyRgba(
                    framebuffer.data(),
                    static_cast<size_t>(drawableWidth),
                    static_cast<size_t>(drawableHeight),
                    scaledSourceLeft,
                    scaledSourceTop,
                    scaledSourceWidth,
                    scaledSourceHeight,
                    destinationWidth,
                    destinationHeight,
                    copyFilter,
                    copiedPixels)) {
                copyCompleted = SIM::GX::EncodeColorTextureCopy(
                    copiedPixels.data(),
                    destinationWidth,
                    destinationHeight,
                    destinationFormat,
                    static_cast<u8*>(destination));
            }
        }
        if (copyCompleted) {
            InvalidateGpuCopyDestination(destination);
        }
    }

    if (copyCompleted) {
        SIM::GX::NotifyTextureCopyDestinationWrite(gxState);
        if (gpuCopy != nullptr) {
            gpuCopy->validatedInvalidationRevision =
                gxState.GetTextureInvalidationRevision();
        }
    }

    if (!clear) {
        return;
    }

    // SetBpRegister records the copy-trigger clear for VI's deferred display
    // path. This function performs the texture-copy clear synchronously, so
    // consume that request before later drawing can be erased at presentation.
    (void)gxState.ConsumeCopyClearRequest();

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
