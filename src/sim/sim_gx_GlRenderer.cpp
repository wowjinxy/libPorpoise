#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstddef>
#include <cstring>
#include <vector>

#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

namespace {

const SIM::GX::RenderVertex * ExpandQuads(
    const SIM::GX::RenderVertex * vertices, size_t numVertices) {
    SIM::GX::RenderVertex * triangles = new SIM::GX::RenderVertex[(numVertices / 4) * 6];
    size_t trianglesIdx = 0;
    for (size_t i = 0; i + 3 < numVertices; i += 4) {
        triangles[trianglesIdx++] = (vertices[i]);
        triangles[trianglesIdx++] = (vertices[i + 1]);
        triangles[trianglesIdx++] = (vertices[i + 2]);
        triangles[trianglesIdx++] = (vertices[i]);
        triangles[trianglesIdx++] = (vertices[i + 2]);
        triangles[trianglesIdx++] = (vertices[i + 3]);
    }
    return triangles;
}

const SIM::GX::RenderVertex * ExpandQuadStrip(
    const SIM::GX::RenderVertex * vertices, size_t numVertices) {
    if (numVertices < 4) {
        SIM::GX::RenderVertex * triangles = new SIM::GX::RenderVertex[numVertices];
        std::memcpy(triangles, vertices, sizeof(SIM::GX::RenderVertex) * numVertices);
        return triangles;
    }

    SIM::GX::RenderVertex * triangles = new SIM::GX::RenderVertex[((numVertices - 2) / 2) * 6];
    size_t trianglesIdx = 0;
    for (size_t i = 0; i + 3 < numVertices; i += 2) {
        triangles[trianglesIdx++] = (vertices[i]);
        triangles[trianglesIdx++] = (vertices[i + 1]);
        triangles[trianglesIdx++] = (vertices[i + 2]);
        triangles[trianglesIdx++] = (vertices[i]);
        triangles[trianglesIdx++] = (vertices[i + 2]);
        triangles[trianglesIdx++] = (vertices[i + 3]);
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

}

namespace SIM::GX {

void GlRenderer::Initialize() {
    if (mVertexArray != 0) {
        return;
    }

    glGenVertexArrays(1, &mVertexArray);
    glGenBuffers(1, &mVertexBuffer);
    glBindVertexArray(mVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);

    //location = 0 in vec3 position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, position)));
    //location = 1 in vec4 vertex_color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, color0)));
    //location = 2 in vec2 texCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, texCoords)));
    
    glGenBuffers(1, &mTevStageUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, mTevStageUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(TevStageConfig) * GX_MAX_TEVSTAGE, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void GlRenderer::Draw(const RenderVertex * vertices, size_t numVertices, GXPrimitive primitive) {
    #ifdef TRACY_ENABLE
    ZoneScoped;
    #endif
    if (numVertices == 0) {
        return;
    }

    Initialize();

    const RenderVertex * expandedVertices;
    const RenderVertex* drawVertices = vertices;
    size_t numDrawVertices = numVertices;
    if (primitive == GX_QUADS) {
        expandedVertices = ExpandQuads(vertices, numVertices);
        drawVertices = expandedVertices;
        numDrawVertices = (numVertices / 4) * 6;
    } else if (primitive == GX_QUADSTRIP) {
        expandedVertices = ExpandQuadStrip(vertices, numVertices);
        drawVertices = expandedVertices;
        numDrawVertices = ((numVertices - 2) / 2) * 6;
    }

    if (numDrawVertices == 0) {
        return;
    }

    GLint shaderProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shaderProgram);
    if (shaderProgram == 0) {
        return;
    }

    auto& gxState = GetGlobalState();
    const GLint projectionLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_projection");
    const GLint modelViewLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_modelview");
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

    const GLint numTevStagesLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "numTevStages");
    glUniform1i(numTevStagesLocation, gxState.GetNumTevStages());

    glBindBuffer(GL_UNIFORM_BUFFER, mTevStageUniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TevStageConfig) * GX_MAX_TEVSTAGE, gxState.GetTevStageConfigArray());
    glBindBufferBase(GL_UNIFORM_BUFFER, glGetUniformLocation(static_cast<GLuint>(shaderProgram), "tevStageConfigs"), mTevStageUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindVertexArray(mVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(numDrawVertices * sizeof(RenderVertex)),
        drawVertices,
        GL_STREAM_DRAW);
    glDrawArrays(
        ToGlPrimitive(primitive),
        0,
        static_cast<GLsizei>(numDrawVertices));

    if (primitive == GX_QUADS || primitive == GX_QUADSTRIP) {
        delete drawVertices;
    }
}

GlRenderer& GetGlRenderer() {
    static GlRenderer renderer;
    return renderer;
}

}
