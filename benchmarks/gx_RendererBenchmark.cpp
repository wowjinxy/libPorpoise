#define SDL_MAIN_HANDLED

#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>

#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern const char* SIM_GXVertexShader;
extern const char* SIM_GXFragmentShader;

namespace {

std::uint64_t UniformCalls;
std::uint64_t BufferUploads;
std::uint64_t DrawCalls;

PFNGLUNIFORMMATRIX4FVPROC RealUniformMatrix4fv;
PFNGLUNIFORM1IPROC RealUniform1i;
PFNGLUNIFORM1IVPROC RealUniform1iv;
PFNGLUNIFORM4IVPROC RealUniform4iv;
PFNGLUNIFORM2IVPROC RealUniform2iv;
PFNGLUNIFORM4FVPROC RealUniform4fv;
PFNGLUNIFORM1FVPROC RealUniform1fv;
PFNGLUNIFORM3FVPROC RealUniform3fv;
PFNGLUNIFORM1FPROC RealUniform1f;
PFNGLUNIFORM1UIPROC RealUniform1ui;
PFNGLBUFFERDATAPROC RealBufferData;
PFNGLDRAWARRAYSPROC RealDrawArrays;

void APIENTRY CountUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value) {
    ++UniformCalls;
    RealUniformMatrix4fv(location, count, transpose, value);
}

void APIENTRY CountUniform1i(GLint location, GLint value) {
    ++UniformCalls;
    RealUniform1i(location, value);
}

void APIENTRY CountUniform1iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    ++UniformCalls;
    RealUniform1iv(location, count, value);
}

void APIENTRY CountUniform4iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    ++UniformCalls;
    RealUniform4iv(location, count, value);
}

void APIENTRY CountUniform2iv(
    GLint location,
    GLsizei count,
    const GLint* value) {
    ++UniformCalls;
    RealUniform2iv(location, count, value);
}

void APIENTRY CountUniform4fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    ++UniformCalls;
    RealUniform4fv(location, count, value);
}

void APIENTRY CountUniform1fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    ++UniformCalls;
    RealUniform1fv(location, count, value);
}

void APIENTRY CountUniform3fv(
    GLint location,
    GLsizei count,
    const GLfloat* value) {
    ++UniformCalls;
    RealUniform3fv(location, count, value);
}

void APIENTRY CountUniform1f(GLint location, GLfloat value) {
    ++UniformCalls;
    RealUniform1f(location, value);
}

void APIENTRY CountUniform1ui(GLint location, GLuint value) {
    ++UniformCalls;
    RealUniform1ui(location, value);
}

void APIENTRY CountBufferData(
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLenum usage) {
    ++BufferUploads;
    RealBufferData(target, size, data, usage);
}

void APIENTRY CountDrawArrays(
    GLenum mode,
    GLint first,
    GLsizei count) {
    ++DrawCalls;
    RealDrawArrays(mode, first, count);
}

void InstallCounters() {
    RealUniformMatrix4fv = glad_glUniformMatrix4fv;
    RealUniform1i = glad_glUniform1i;
    RealUniform1iv = glad_glUniform1iv;
    RealUniform4iv = glad_glUniform4iv;
    RealUniform2iv = glad_glUniform2iv;
    RealUniform4fv = glad_glUniform4fv;
    RealUniform1fv = glad_glUniform1fv;
    RealUniform3fv = glad_glUniform3fv;
    RealUniform1f = glad_glUniform1f;
    RealUniform1ui = glad_glUniform1ui;
    RealBufferData = glad_glBufferData;
    RealDrawArrays = glad_glDrawArrays;

    glad_glUniformMatrix4fv = CountUniformMatrix4fv;
    glad_glUniform1i = CountUniform1i;
    glad_glUniform1iv = CountUniform1iv;
    glad_glUniform4iv = CountUniform4iv;
    glad_glUniform2iv = CountUniform2iv;
    glad_glUniform4fv = CountUniform4fv;
    glad_glUniform1fv = CountUniform1fv;
    glad_glUniform3fv = CountUniform3fv;
    glad_glUniform1f = CountUniform1f;
    glad_glUniform1ui = CountUniform1ui;
    glad_glBufferData = CountBufferData;
    glad_glDrawArrays = CountDrawArrays;
}

GLuint CompileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<size_t>(std::max(logLength, 1)));
    glGetShaderInfoLog(shader, logLength, nullptr, log.data());
    std::fprintf(stderr, "shader compilation failed: %s\n", log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint CreateProgram() {
    const GLuint vertex = CompileShader(GL_VERTEX_SHADER, SIM_GXVertexShader);
    const GLuint fragment =
        CompileShader(GL_FRAGMENT_SHADER, SIM_GXFragmentShader);
    if (vertex == 0 || fragment == 0) {
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(static_cast<size_t>(std::max(logLength, 1)));
    glGetProgramInfoLog(program, logLength, nullptr, log.data());
    std::fprintf(stderr, "shader link failed: %s\n", log.data());
    glDeleteProgram(program);
    return 0;
}

size_t ParseCount(const char* text, size_t fallback) {
    if (text == nullptr || *text == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0' || value == 0) {
        return fallback;
    }
    return static_cast<size_t>(value);
}

}

int main(int argc, char** argv) {
    const size_t drawCount =
        ParseCount(argc > 1 ? argv[1] : nullptr, 2000u);
    size_t vertexCount =
        ParseCount(argc > 2 ? argv[2] : nullptr, 192u);
    vertexCount = std::max<size_t>(3u, vertexCount - vertexCount % 3u);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    SDL_Window* window = SDL_CreateWindow(
        "libPorpoise renderer benchmark",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        64,
        64,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (window == nullptr) {
        std::fprintf(stderr, "window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr || !gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        std::fprintf(stderr, "OpenGL initialization failed: %s\n", SDL_GetError());
        if (context != nullptr) {
            SDL_GL_DeleteContext(context);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }
    SDL_GL_SetSwapInterval(0);
    InstallCounters();

    const GLuint program = CreateProgram();
    if (program == 0) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 4;
    }
    glUseProgram(program);

    SIM::GX::InitGlobalState();
    auto& renderer = SIM::GX::GetGlRenderer();
    renderer.SetDrawableSize(64, 64);
    renderer.SetShaderProgram(program);

    std::vector<SIM::GX::RenderVertex> vertices(vertexCount);
    for (size_t index = 0; index < vertices.size(); ++index) {
        const float phase = static_cast<float>(index % 3u);
        vertices[index].position = {
            phase == 0.0f ? -0.5f : 0.5f,
            phase == 2.0f ? 0.5f : -0.5f,
            0.0f,
        };
        vertices[index].normal = {0.0f, 0.0f, 1.0f};
        vertices[index].color0 = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    for (size_t warmup = 0; warmup < 64u; ++warmup) {
        renderer.Draw(vertices, GX_TRIANGLES);
    }
    glFinish();

    UniformCalls = 0;
    BufferUploads = 0;
    DrawCalls = 0;

    const auto start = std::chrono::steady_clock::now();
    for (size_t draw = 0; draw < drawCount; ++draw) {
        renderer.Draw(vertices, GX_TRIANGLES);
    }
    glFinish();
    const auto end = std::chrono::steady_clock::now();

    const double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    std::printf(
        "draws=%zu vertices_per_draw=%zu elapsed_ms=%.3f us_per_draw=%.3f "
        "uniform_calls=%llu buffer_uploads=%llu draw_calls=%llu\n",
        drawCount,
        vertexCount,
        elapsedMs,
        elapsedMs * 1000.0 / static_cast<double>(drawCount),
        static_cast<unsigned long long>(UniformCalls),
        static_cast<unsigned long long>(BufferUploads),
        static_cast<unsigned long long>(DrawCalls));

    glDeleteProgram(program);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
