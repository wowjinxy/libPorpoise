#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstddef>
#include <vector>

#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

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

    static_assert(sizeof(RenderVertex) == sizeof(float) * 7);
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
}

void GlRenderer::Draw(const std::vector<RenderVertex>& vertices, GXPrimitive primitive) {
    #ifdef TRACY_ENABLE
    ZoneScoped;
    #endif
    if (vertices.empty()) {
        return;
    }

    Initialize();

    std::vector<RenderVertex> expandedVertices;
    const std::vector<RenderVertex>* drawVertices = &vertices;
    if (primitive == GX_QUADS) {
        expandedVertices = ExpandQuads(vertices);
        drawVertices = &expandedVertices;
    } else if (primitive == GX_QUADSTRIP) {
        expandedVertices = ExpandQuadStrip(vertices);
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

    const auto& gxState = GetGlobalState();
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
