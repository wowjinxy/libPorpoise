#define SDL_MAIN_HANDLED

#include <simulator/glad/glad.h>
#include <simulator/sim_gx_Geometry.hpp>
#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern const char* SIM_GXVertexShader;
extern const char* SIM_GXFragmentShader;

namespace {

std::uint64_t UniformCalls;
std::uint64_t BufferUploads;
std::uint64_t BufferStorageCalls;
std::uint64_t BufferMapCalls;
std::uint64_t FenceCalls;
std::uint64_t FenceWaitCalls;
std::uint64_t DrawCalls;
std::uint64_t NonZeroFirstDraws;
bool ForceMapFailure;

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
PFNGLBUFFERSTORAGEPROC RealBufferStorage;
PFNGLMAPBUFFERRANGEPROC RealMapBufferRange;
PFNGLFENCESYNCPROC RealFenceSync;
PFNGLCLIENTWAITSYNCPROC RealClientWaitSync;
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

void APIENTRY CountBufferStorage(
    GLenum target,
    GLsizeiptr size,
    const void* data,
    GLbitfield flags) {
    ++BufferStorageCalls;
    RealBufferStorage(target, size, data, flags);
}

void* APIENTRY CountMapBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr length,
    GLbitfield access) {
    ++BufferMapCalls;
    if (ForceMapFailure) {
        return nullptr;
    }
    return RealMapBufferRange(target, offset, length, access);
}

GLsync APIENTRY CountFenceSync(GLenum condition, GLbitfield flags) {
    ++FenceCalls;
    return RealFenceSync(condition, flags);
}

GLenum APIENTRY CountClientWaitSync(
    GLsync sync,
    GLbitfield flags,
    GLuint64 timeout) {
    ++FenceWaitCalls;
    return RealClientWaitSync(sync, flags, timeout);
}

void APIENTRY CountDrawArrays(
    GLenum mode,
    GLint first,
    GLsizei count) {
    ++DrawCalls;
    if (first != 0) {
        ++NonZeroFirstDraws;
    }
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
    RealBufferStorage = glad_glBufferStorage;
    RealMapBufferRange = glad_glMapBufferRange;
    RealFenceSync = glad_glFenceSync;
    RealClientWaitSync = glad_glClientWaitSync;
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
    if (glad_glBufferStorage != nullptr) {
        glad_glBufferStorage = CountBufferStorage;
    }
    if (glad_glMapBufferRange != nullptr) {
        glad_glMapBufferRange = CountMapBufferRange;
    }
    if (glad_glFenceSync != nullptr) {
        glad_glFenceSync = CountFenceSync;
    }
    if (glad_glClientWaitSync != nullptr) {
        glad_glClientWaitSync = CountClientWaitSync;
    }
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

bool FramebufferContainsNonBlackPixel() {
    std::array<std::uint8_t, 64u * 64u * 4u> pixels = {};
    glReadPixels(
        0,
        0,
        64,
        64,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    for (size_t offset = 0u;
         offset < pixels.size();
         offset += 4u) {
        if (pixels[offset] != 0u ||
            pixels[offset + 1u] != 0u ||
            pixels[offset + 2u] != 0u) {
            return true;
        }
    }
    return false;
}

bool ValidatePersistentRing(SIM::GX::GlRenderer& renderer) {
    constexpr size_t pageCapacity = 65535u;
    constexpr size_t markerCount = 4u;
    std::vector<SIM::GX::RenderVertex> vertices(
        pageCapacity * markerCount);
    for (auto& vertex : vertices) {
        vertex.position = {2.0f, 2.0f, 0.0f};
        vertex.normal = {0.0f, 0.0f, 1.0f};
        vertex.color0 = {1.0f, 1.0f, 1.0f, 1.0f};
    }

    constexpr std::array<std::array<float, 2>, markerCount> centers = {{
        {{-0.55f, -0.55f}},
        {{ 0.55f, -0.55f}},
        {{-0.55f,  0.55f}},
        {{ 0.55f,  0.55f}},
    }};
    for (size_t marker = 0u; marker < markerCount; ++marker) {
        const size_t first = marker * pageCapacity;
        const float x = centers[marker][0];
        const float y = centers[marker][1];
        vertices[first].position = {x - 0.22f, y - 0.18f, 0.0f};
        vertices[first + 1u].position = {x + 0.22f, y - 0.18f, 0.0f};
        vertices[first + 2u].position = {x, y + 0.22f, 0.0f};
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.Draw(vertices, GX_TRIANGLES);
    glFinish();

    std::array<std::uint8_t, 64u * 64u * 4u> pixels = {};
    glReadPixels(
        0,
        0,
        64,
        64,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels.data());
    for (size_t quadrant = 0u; quadrant < markerCount; ++quadrant) {
        const size_t firstX = (quadrant & 1u) != 0u ? 32u : 0u;
        const size_t firstY = quadrant >= 2u ? 32u : 0u;
        bool markerRendered = false;
        for (size_t y = firstY; y < firstY + 32u; ++y) {
            for (size_t x = firstX; x < firstX + 32u; ++x) {
                const size_t offset = (y * 64u + x) * 4u;
                markerRendered =
                    markerRendered ||
                    pixels[offset] != 0u ||
                    pixels[offset + 1u] != 0u ||
                    pixels[offset + 2u] != 0u;
            }
        }
        if (!markerRendered) {
            return false;
        }
    }
    return true;
}

}

int main(int argc, char** argv) {
    const size_t drawCount =
        ParseCount(argc > 1 ? argv[1] : nullptr, 2000u);
    size_t vertexCount =
        ParseCount(argc > 2 ? argv[2] : nullptr, 192u);
    vertexCount = std::max<size_t>(3u, vertexCount - vertexCount % 3u);
    const bool forceLegacy =
        argc > 3 && std::strcmp(argv[3], "legacy") == 0;
    ForceMapFailure =
        argc > 3 && std::strcmp(argv[3], "map-fail") == 0;
    const char* requestedMode =
        forceLegacy ? "legacy" : ForceMapFailure ? "map-fail" : "auto";

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
    const bool persistentCapabilityAvailable =
        GLAD_GL_ARB_buffer_storage != 0 &&
        glad_glBufferStorage != nullptr &&
        glad_glMapBufferRange != nullptr &&
        glad_glFenceSync != nullptr &&
        glad_glClientWaitSync != nullptr &&
        glad_glDeleteSync != nullptr;
    if (!forceLegacy && !persistentCapabilityAvailable) {
        std::fprintf(
            stderr,
            "SKIP: %s mode requires persistent buffer-storage support\n",
            requestedMode);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 77;
    }
    if (forceLegacy) {
        GLAD_GL_ARB_buffer_storage = 0;
    }
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
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    // Timed rendering must produce its own output; warmup pixels cannot make
    // a broken upload path appear valid.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFinish();

    UniformCalls = 0;
    BufferUploads = 0;
    FenceCalls = 0;
    FenceWaitCalls = 0;
    DrawCalls = 0;
    NonZeroFirstDraws = 0;

    const auto start = std::chrono::steady_clock::now();
    for (size_t draw = 0; draw < drawCount; ++draw) {
        renderer.Draw(vertices, GX_TRIANGLES);
    }
    glFinish();
    const auto end = std::chrono::steady_clock::now();
    const bool timedOutputValidated = FramebufferContainsNonBlackPixel();

    const std::uint64_t timedUniformCalls = UniformCalls;
    const std::uint64_t timedBufferUploads = BufferUploads;
    const std::uint64_t initializedBufferStorageCalls = BufferStorageCalls;
    const std::uint64_t initializedBufferMapCalls = BufferMapCalls;
    const std::uint64_t timedFenceCalls = FenceCalls;
    const std::uint64_t timedFenceWaitCalls = FenceWaitCalls;
    const std::uint64_t timedDrawCalls = DrawCalls;
    const std::uint64_t timedNonZeroFirstDraws = NonZeroFirstDraws;
    bool modeCountersValidated =
        timedUniformCalls == 0u &&
        timedDrawCalls == drawCount;
    if (forceLegacy) {
        modeCountersValidated =
            modeCountersValidated &&
            initializedBufferStorageCalls == 0u &&
            initializedBufferMapCalls == 0u &&
            timedBufferUploads == drawCount &&
            timedFenceCalls == 0u &&
            timedFenceWaitCalls == 0u &&
            timedNonZeroFirstDraws == 0u;
    } else if (ForceMapFailure) {
        modeCountersValidated =
            modeCountersValidated &&
            initializedBufferStorageCalls == 1u &&
            initializedBufferMapCalls == 1u &&
            timedBufferUploads == drawCount &&
            timedFenceCalls == 0u &&
            timedFenceWaitCalls == 0u &&
            timedNonZeroFirstDraws == 0u;
    } else {
        modeCountersValidated =
            modeCountersValidated &&
            initializedBufferStorageCalls == 1u &&
            initializedBufferMapCalls == 1u &&
            timedBufferUploads == 0u &&
            timedFenceCalls != 0u &&
            timedFenceWaitCalls != 0u &&
            timedNonZeroFirstDraws != 0u;
    }
    const bool ringValidated = ValidatePersistentRing(renderer);

    const double elapsedMs =
        std::chrono::duration<double, std::milli>(end - start).count();
    std::printf(
        "requested_mode=%s draws=%zu vertices_per_draw=%zu elapsed_ms=%.3f "
        "us_per_draw=%.3f "
        "uniform_calls=%llu buffer_uploads=%llu buffer_storage=%llu "
        "buffer_maps=%llu fences=%llu fence_waits=%llu draw_calls=%llu "
        "nonzero_first_draws=%llu\n",
        requestedMode,
        drawCount,
        vertexCount,
        elapsedMs,
        elapsedMs * 1000.0 / static_cast<double>(drawCount),
        static_cast<unsigned long long>(timedUniformCalls),
        static_cast<unsigned long long>(timedBufferUploads),
        static_cast<unsigned long long>(initializedBufferStorageCalls),
        static_cast<unsigned long long>(initializedBufferMapCalls),
        static_cast<unsigned long long>(timedFenceCalls),
        static_cast<unsigned long long>(timedFenceWaitCalls),
        static_cast<unsigned long long>(timedDrawCalls),
        static_cast<unsigned long long>(timedNonZeroFirstDraws));

    glDeleteProgram(program);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (!timedOutputValidated) {
        std::fprintf(
            stderr,
            "renderer benchmark timed draws produced no color output\n");
        return 5;
    }
    if (!ringValidated) {
        std::fprintf(
            stderr,
            "renderer benchmark failed the four-page ring validation\n");
        return 6;
    }
    if (!modeCountersValidated) {
        std::fprintf(
            stderr,
            "renderer benchmark did not exercise requested %s mode\n",
            requestedMode);
        return 7;
    }
    return 0;
}
