#include <simulator/sim_gx_GlRenderer.hpp>

#include <cstddef>
#include <cstring>
#include <format>
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
    //location = 4 in uint posNormalMtxIdx
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(
        4,
        1,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, posNormalMtxIdx)));
    //location = 5 in uvec4 texMtxIdx0
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(
        5,
        4,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, texMtxIdx)));
    //location = 6 in uvec4 texMtxIdx1
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(
        6,
        4,
        GL_UNSIGNED_INT,
        GL_FALSE,
        sizeof(RenderVertex),
        reinterpret_cast<void*>(offsetof(RenderVertex, texMtxIdx) + (4 * sizeof(u32))));
    
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

    // Allocate matrix memory uniform buffer
    glGenBuffers(1, &mMatrixMemoryUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, mMatrixMemoryUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, (sizeof(float) * 240) + (sizeof(float) * 90), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLint shaderProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &shaderProgram);

    mProjectionLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_projection");
    mNormalMtxLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_normalMtx");
    mNumTexGenLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_numTexGens");
    for(int i=0; i < GX_MAX_TEXCOORD; i++) {
        mTexGenMatrixLocation[i] = glGetUniformLocation(static_cast<GLuint>(shaderProgram), std::format("u_texGens[{}].mMatrixId", i).c_str());
        mTexGenTypeLocation[i] = glGetUniformLocation(static_cast<GLuint>(shaderProgram), std::format("u_texGens[{}].mType", i).c_str());
    }


    mTevTexMapLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "tevTexMaps");
    mTevStageConfigsBinding = 0;
    glUniformBlockBinding(shaderProgram, mTevStageConfigsBlock, mTevStageConfigsBinding);
    mLightConfigBlock = glGetUniformBlockIndex(shaderProgram, "lightConfigBlock");
    mLightConfigBlockBinding = 1;
    glUniformBlockBinding(shaderProgram, mLightConfigBlock, mLightConfigBlockBinding);
    mMatrixMemoryBlock = glGetUniformBlockIndex(shaderProgram, "matrixMemoryBlock");
    mMatrixMemoryBlockBinding = 2;
    glUniformBlockBinding(shaderProgram, mMatrixMemoryBlock, mMatrixMemoryBlockBinding);
    mInitialTevColorsLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "initialTevColors");
    mNumTevStagesLocation =
        glGetUniformLocation(static_cast<GLuint>(shaderProgram), "numTevStages");
    mNumChansLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "u_numChans");
    mMtxIdxALocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "mtxIdxA");
    mPnMtxIdxEnabledLocation = glGetUniformLocation(static_cast<GLuint>(shaderProgram), "pnMtxIdxEnabled");
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

    // TODO: support all normal matrices
    for(int i=0; i<1;i++) {
        glUniformMatrix4fv(
            mNormalMtxLocation,
            1,
            GL_TRUE,
            gxState.GetNormalMatrix(i).data()
        );
    }    

    glUniform1ui(mNumTexGenLocation, gxState.GetNumTexGens());
    for(int i=0; i < GX_MAX_TEXCOORD; i++) {
        glUniform1ui(mTexGenMatrixLocation[i], gxState.GetTexGenArray()[i].mMatrixId);
        glUniform1ui(mTexGenTypeLocation[i], gxState.GetTexGenArray()[i].mType);
    }

    
    glUniform1iv(mTevTexMapLocation, GX_MAX_TEVSTAGE, (const GLint*)gxState.GetTevTexMapArray());

    if(gxState.GetTevDirty()) {
        glBindBuffer(GL_UNIFORM_BUFFER, mTevStageUniformBuffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TevStageConfig) * GX_MAX_TEVSTAGE, gxState.GetTevStageConfigArray());
        glBindBufferBase(GL_UNIFORM_BUFFER, mTevStageConfigsBinding, mTevStageUniformBuffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        gxState.SetTevDirty(false);
    }

    //upload matrix memory position + texture
    glBindBuffer(GL_UNIFORM_BUFFER, mMatrixMemoryUniformBuffer);
    const float * matrixMem = gxState.GetXfMemoryPointer();
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 240 * sizeof(float), gxState.GetXfMemoryPointer());
    // upload matrix memory normal mtx
    glBufferSubData(GL_UNIFORM_BUFFER, 240 * sizeof(float), 90 * sizeof(float), gxState.GetXfMemoryPointer() + 0x400);
    glBindBufferBase(GL_UNIFORM_BUFFER, mMatrixMemoryBlockBinding, mMatrixMemoryUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // pass lights data. TODO: Add dirty state checker
    glBindBuffer(GL_UNIFORM_BUFFER, mLightsUniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Light) * 8, gxState.GetLightsArray());
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Light) * 8, sizeof(ColorChannel) * 4, gxState.GetColorChannelArray());
    glBindBufferBase(GL_UNIFORM_BUFFER, mLightConfigBlockBinding, mLightsUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUniform1ui(mMtxIdxALocation, gxState.GetCurrentPositionMtxIdx());
    glUniform1ui(mPnMtxIdxEnabledLocation, gxState.GetVertexDescriptor(GX_VA_PNMTXIDX) != GX_NONE);

    glUniform1ui(mNumChansLocation, gxState.GetNumChannels());

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
