#include <simulator/sim_gx_GlRenderer.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

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
    const auto& modelView = state.GetPositionMatrix();
    for (auto& vertex : vertices) {
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
                        rgba[destination + 3u] = 255u;
                    }
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
            2,
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
    const GLint textureMatrixLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_texmatrix0");
    const GLint useTextureLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_use_texture0");
    const GLint tevColorModeLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_tev_color_mode");
    const GLint textureLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_texture0");
    if (projectionLocation >= 0) {
        glUniformMatrix4fv(
            projectionLocation,
            1,
            GL_TRUE,
            gxState.GetProjectionMatrix().data());
    }
    if (modelViewLocation >= 0) {
        glUniformMatrix4fv(
            modelViewLocation,
            1,
            GL_TRUE,
            gxState.GetPositionMatrix().data());
    }
    if (textureMatrixLocation >= 0) {
        glUniformMatrix4fv(
            textureMatrixLocation,
            1,
            GL_TRUE,
            gxState.GetTextureMatrix(0).data());
    }

    bool useTexture = false;
    const auto& firstStage = gxState.GetTevStageState(0);
    if (gxState.GetNumTevStages() > 0u &&
        firstStage.textureEnabled &&
        firstStage.textureMap < mTextures.size()) {
        const size_t textureIndex = firstStage.textureMap;
        const auto& texture = gxState.GetTextureState(textureIndex);
        if (texture.data != nullptr &&
            texture.width > 0 &&
            texture.height > 0 &&
            (texture.format == GX_TF_I4 ||
             texture.format == GX_TF_RGBA8)) {
            if (mTextures[textureIndex] == 0u) {
                glGenTextures(1, &mTextures[textureIndex]);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mTextures[textureIndex]);
            if (mTextureRevisions[textureIndex] != texture.revision) {
                std::vector<u8> rgba;
                if (texture.format == GX_TF_RGBA8) {
                    DecodeRGBA8(
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
                    texture.minFilter == GX_LINEAR ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(
                    GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    texture.magFilter == GX_LINEAR ? GL_LINEAR : GL_NEAREST);
                mTextureRevisions[textureIndex] = texture.revision;
            }
            useTexture = true;
        }
    }
    if (useTextureLocation >= 0) {
        glUniform1i(useTextureLocation, useTexture ? 1 : 0);
    }
    if (tevColorModeLocation >= 0) {
        glUniform1i(
            tevColorModeLocation,
            static_cast<GLint>(firstStage.colorMode));
    }
    if (textureLocation >= 0) {
        glUniform1i(textureLocation, 0);
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
