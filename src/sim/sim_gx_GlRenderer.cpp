#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstddef>
#include <cstring>
#include <vector>

#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim_gx_TextureManager.hpp>
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
    //location = 1 in vec3 normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, normal)));
    //location = 2 in vec4 vertex_color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, color0)));
    //location = 3 in vec2 texCoords
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, texCoords)));
    
    // Allocate tev stage uniform buffer
    glGenBuffers(1, &mTevStageUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, mTevStageUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(TevStageConfig) * GX_MAX_TEVSTAGE, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    
    // Allocate lights uniform buffer
    glGenBuffers(1, &mLightsUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, mLightsUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, (sizeof(Light) * 8) + (sizeof(ColorChannel) * 4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLint shaderProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shaderProgram);

    mProjectionLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_projection");
    mModelViewLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_modelview");
    mTextureMtxLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_textureMtx");
    mNumTexGenLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_numTexGens");
    mTexGenLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_texGens");
    mTevTexMapLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "tevTexMaps");
    mTevStageConfigsBinding = 0;
    glUniformBlockBinding(shaderProgram, mTevStageConfigsBlock, mTevStageConfigsBinding);
    mLightConfigBlock = glGetUniformBlockIndex(shaderProgram, "lightConfigBlock");
    mLightConfigBlockBinding = 1;
    glUniformBlockBinding(shaderProgram, mLightConfigBlock, mLightConfigBlockBinding);
    mInitialTevColorsLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "initialTevColors");
    mNumTevStagesLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "numTevStages");
}

void GlRenderer::Draw(const RenderVertex * vertices, size_t numVertices, GXPrimitive primitive) {
    #ifdef TRACY_ENABLE
    ZoneScoped;
    #endif
    if (numVertices == 0) {
        return;
    }

    Initialize();
    auto& gxState = GetGlobalState();

    if(gxState.GetIsTextureDirty()) {
        TextureManager::GetInstance().ProcessTextures();
        gxState.SetTextureDirty(false);
    }

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

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(
        mProjectionLocation,
        1,
        GL_TRUE,
        gxState.GetProjectionMatrix().data());
    glUniformMatrix4fv(
        mModelViewLocation,
        1,
        GL_TRUE,
        gxState.GetPositionMatrix().data());
    
    // TODO: support all texture matrices
    for(int i=0; i<1;i++) {
        glUniformMatrix4fv(
            mTextureMtxLocation,
            1,
            GL_TRUE,
            gxState.GetTextureMatrix(i).data()
        );
    }

    glUniform1ui(mNumTexGenLocation, gxState.GetNumTexGens());
    glUniform1uiv(mTexGenLocation, sizeof(TexGenConfig) * GX_MAX_TEXCOORD, (const GLuint*)gxState.GetTexGenArray());
    
    glUniform1iv(mTevTexMapLocation, GX_MAX_TEVSTAGE, (const GLint*)gxState.GetTevTexMapArray());

    if(gxState.GetTevDirty()) {
        glBindBuffer(GL_UNIFORM_BUFFER, mTevStageUniformBuffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TevStageConfig) * GX_MAX_TEVSTAGE, gxState.GetTevStageConfigArray());
        glBindBufferBase(GL_UNIFORM_BUFFER, mTevStageConfigsBinding, mTevStageUniformBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        gxState.SetTevDirty(false);
    }

    // pass lights data. TODO: Add dirty state checker
    glBindBuffer(GL_UNIFORM_BUFFER, mLightsUniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Light) * 8, gxState.GetLightsArray());
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Light) * 8, sizeof(ColorChannel) * 4, gxState.GetColorChannelArray());
    glBindBufferBase(GL_UNIFORM_BUFFER, mLightConfigBlockBinding, mLightsUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUniform4fv(mInitialTevColorsLocation, 4, gxState.GetInitialTevColorsArray());

    glUniform1ui(mNumTevStagesLocation, gxState.GetNumTevStages());

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
