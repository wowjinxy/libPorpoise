#define LIBPORPOISE_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <dolphin.h>
#include <simulator/sim.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_GlRenderer.hpp>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim_host_Benchmark.h>
#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>
#include <atomic>
static SDL_GLContext context;
static SDL_Window * window;
static SDL_threadID contextThread;
static SDL_mutex* contextMutex;
static std::atomic<bool> drawableViewportChanged{false};
static int currentDrawableWidth;
static int currentDrawableHeight;

static void UpdateDrawableViewport() {
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    if (drawableWidth > 0 && drawableHeight > 0) {
        currentDrawableWidth = drawableWidth;
        currentDrawableHeight = drawableHeight;
        glViewport(0, 0, drawableWidth, drawableHeight);
        SIM::GX::GetGlRenderer().SetDrawableSize(
            drawableWidth,
            drawableHeight);
    }
}

extern const char * SIM_GXVertexShader;
extern const char * SIM_GXFragmentShader;

GLenum glErrCode;
GLint status;
GLint logSize = 0;
void * errorBuf;
static GLuint gxVertexArray;
static GLuint gxVertexBuffer;

static void CompileShaderCommon(GLuint& id, const char * source) {
    glShaderSource( id, 1, &source, NULL );
    glCompileShader( id );
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);
    if( status != GL_TRUE )
    {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logSize);
        errorBuf = malloc( logSize * sizeof( GLchar ) );
        glGetShaderInfoLog(id, logSize, &logSize, (GLchar *)errorBuf);
        printf( "ERROR COMPILING SHADER:\n%s\n", (GLchar *)errorBuf );
        free( errorBuf );
    }
}

static void CompileVertexShader(GLuint& id, const char * source) {
    id = glCreateShader(GL_VERTEX_SHADER);
    CompileShaderCommon(id, source);
}


static void CompileFragmentShader(GLuint& id, const char * source) {
    id = glCreateShader(GL_FRAGMENT_SHADER);
    CompileShaderCommon(id,source);
}

void LinkShader(GLuint id,GLuint vertex,GLuint fragment) {
    glAttachShader( id, vertex );
    glAttachShader( id, fragment );
    glLinkProgram( id );
    glErrCode = glGetError();
    glGetProgramiv( id, GL_LINK_STATUS, &status );
    if( status != GL_TRUE )
    {
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logSize);
        errorBuf = malloc( logSize * sizeof( GLchar ) );
        glGetProgramInfoLog(id, logSize, &logSize, (GLchar *)errorBuf);
        glErrCode = glGetError();
        printf( "ERROR LINKING SHADER:\n%s\n", (GLchar*)errorBuf );
        free( errorBuf );
    }
    glValidateProgram(id);
    glUseProgram(id);

    glGenVertexArrays(1, &gxVertexArray);
    glBindVertexArray(gxVertexArray);
    glGenBuffers(1, &gxVertexBuffer);
}

static void DrawTestTriangle()
{
    glBindVertexArray(gxVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, gxVertexBuffer);


    float verts[] = {
        0.0f, 0.0f, 0.0f,   // Vertex A : bottom‑left
        1.0f, 0.0f, 0.0f,   // Vertex B : bottom‑right
        0.5f, 1.0f, 0.0f   // Vertex C : top‑center
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 12, (void*)0);

    glDrawArrays(GL_TRIANGLES,0, sizeof(verts) / sizeof(float));
}


static GLuint gxShaderProgramId;
static GLuint gxVertexShader;
static GLuint gxFragmentShader;

extern "C" {
void DolphinMain();
}

struct GameThreadState {
    std::atomic<bool> finished{false};
    int result = 1;
};

static BOOL IsRenderContextOwner() {
    BOOL isOwner;

    SDL_LockMutex(contextMutex);
    isOwner = contextThread != 0 && contextThread == SDL_ThreadID();
    SDL_UnlockMutex(contextMutex);
    return isOwner;
}

static int RunDolphinMain(void* data) {
    auto* state = static_cast<GameThreadState*>(data);

    if (!SIM_HostAcquireRenderContext()) {
        fprintf(stderr,
                "libPorpoise SIM: game thread could not acquire the "
                "initial render context.\n");
    } else {
        DolphinMain();
        state->result = 0;
        if (IsRenderContextOwner()) {
            SIM_HostReleaseRenderContext();
        }
    }
    state->finished.store(true, std::memory_order_release);
    return state->result;
}

static void ProcessWindowEvent(const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        /* Console applications generally have no orderly main-loop exit. */
        exit(0);
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        drawableViewportChanged.store(true, std::memory_order_release);
    }
}

int main(int argc, char** argv) {
    if (!SIM_HostBenchmarkConfigureFromEnvironment()) {
        return 2;
    }
    if (SDL_Init(
            SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
            SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    contextMutex = SDL_CreateMutex();
    if (contextMutex == NULL) {
        fprintf(stderr, "OpenGL context mutex creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int windowWidth = 640;
    int windowHeight = 480;

    window = SDL_CreateWindow( "libPorpoise Game", 100, 100, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
    if (window == NULL) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        return 1;
    }

    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
        return 1;
    }
    contextThread = SDL_ThreadID();

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        fprintf(stderr, "OpenGL function loading failed\n");
        return 1;
    }
    if (!SIM_HostBenchmarkInitializeGl()) {
        return 2;
    }
    /* The emulated VI provides the hardware cadence after presentation.
     * Disable driver v-sync because a frame just slower than the monitor can
     * otherwise be quantized from (for example) 42 FPS down to 30 FPS. */
    SDL_GL_SetSwapInterval(0);

    UpdateDrawableViewport();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    gxShaderProgramId = glCreateProgram();
    CompileVertexShader(gxVertexShader, SIM_GXVertexShader);
    CompileFragmentShader(gxFragmentShader, SIM_GXFragmentShader);
    LinkShader(gxShaderProgramId, gxVertexShader, gxFragmentShader);
    glUseProgram(gxShaderProgramId);
    SIM::GX::GetGlRenderer().SetShaderProgram(gxShaderProgramId);
    
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    /*
     * Keep the native window-creator thread available for SDL's event pump.
     * The emulated console bootstrap owns GL while it initializes, and VI can
     * later transfer that context explicitly to the long-lived render thread.
     */
    if (!SIM_HostReleaseRenderContext()) {
        return 1;
    }

    GameThreadState gameState;
    SDL_Thread* gameThread =
        SDL_CreateThread(RunDolphinMain, "DolphinMain", &gameState);
    if (gameThread == NULL) {
        fprintf(stderr, "Game thread creation failed: %s\n", SDL_GetError());
        return 1;
    }

    while (!gameState.finished.load(std::memory_order_acquire)) {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 8)) {
            ProcessWindowEvent(event);
            while (SDL_PollEvent(&event)) {
                ProcessWindowEvent(event);
            }
        }
    }

    int gameResult = 1;
    SDL_WaitThread(gameThread, &gameResult);
    if (!SIM_HostAcquireRenderContext()) {
        fprintf(stderr,
                "libPorpoise SIM: render context still belongs to a game "
                "thread after DolphinMain returned.\n");
        return 1;
    }
    SDL_LockMutex(contextMutex);
    SDL_GL_DeleteContext(context);
    context = NULL;
    contextThread = 0;
    SDL_UnlockMutex(contextMutex);
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_DestroyMutex(contextMutex);
    contextMutex = NULL;
    SDL_Quit();
    return gameResult;
}

BOOL SIM_HostReleaseRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();

    if (contextMutex == NULL) {
        return FALSE;
    }
    SDL_LockMutex(contextMutex);

    if (context == NULL || contextThread == 0) {
        SDL_UnlockMutex(contextMutex);
        return TRUE;
    }
    if (contextThread != currentThread) {
        fprintf(stderr,
                "libPorpoise SIM: render context release requested from "
                "a non-owner thread.\n");
        SDL_UnlockMutex(contextMutex);
        return FALSE;
    }
    if (SDL_GL_GetCurrentContext() == context &&
        SDL_GL_MakeCurrent(NULL, NULL) != 0) {
        fprintf(stderr, "libPorpoise SIM: OpenGL context release failed: %s\n",
                SDL_GetError());
        SDL_UnlockMutex(contextMutex);
        return FALSE;
    }
    contextThread = 0;
    SDL_UnlockMutex(contextMutex);
    return TRUE;
}

BOOL SIM_HostAcquireRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();

    if (contextMutex == NULL) {
        return FALSE;
    }
    SDL_LockMutex(contextMutex);

    if (window == NULL || context == NULL) {
        SDL_UnlockMutex(contextMutex);
        return FALSE;
    }
    if (contextThread != 0 && contextThread != currentThread) {
        fprintf(stderr,
                "libPorpoise SIM: render context acquisition requested "
                "before its previous owner released it.\n");
        SDL_UnlockMutex(contextMutex);
        return FALSE;
    }
    if (SDL_GL_GetCurrentContext() != context &&
        SDL_GL_MakeCurrent(window, context) != 0) {
        fprintf(stderr, "libPorpoise SIM: OpenGL context acquisition failed: %s\n",
                SDL_GetError());
        SDL_UnlockMutex(contextMutex);
        return FALSE;
    }
    glUseProgram(gxShaderProgramId);
    SIM::GX::GetGlRenderer().SetShaderProgram(gxShaderProgramId);
    contextThread = currentThread;
    UpdateDrawableViewport();
    SDL_UnlockMutex(contextMutex);
    return TRUE;
}

void SIM_VIInit() {
}

void SIM_Render() {
    if (!IsRenderContextOwner() ||
        SDL_GL_GetCurrentContext() != context) {
        fprintf(stderr,
                "libPorpoise SIM: presentation requested without the render "
                "context on the calling thread.\n");
        return;
    }
    if (drawableViewportChanged.exchange(
            false,
            std::memory_order_acq_rel)) {
        UpdateDrawableViewport();
    }

    const BOOL benchmarkEnabled = SIM_HostBenchmarkEnabled();
    const u32 retraceId = VIGetRetraceCount();
    Uint64 swapStart = 0u;
    Uint64 swapTicks = 0u;
    Uint64 performanceFrequency = 0u;
    if (benchmarkEnabled) {
        SIM_HostBenchmarkBeforeSwap(
            retraceId,
            currentDrawableWidth,
            currentDrawableHeight);
        performanceFrequency = SDL_GetPerformanceFrequency();
        swapStart = SDL_GetPerformanceCounter();
    }

    // Present the EFB contents, then apply a requested GX copy clear to the
    // next frame just as GXCopyDisp(..., GX_TRUE) does on the console.
    SDL_GL_SwapWindow(window);
    if (benchmarkEnabled) {
        swapTicks = SDL_GetPerformanceCounter() - swapStart;
    }
    auto& gxState = SIM::GX::GetGlobalState();
    if (gxState.ConsumeCopyClearRequest()) {
        const auto& clearColor = gxState.GetCopyClearColor();
        const GLboolean scissorEnabled =
            glIsEnabled(GL_SCISSOR_TEST);
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
    if (benchmarkEnabled) {
        SIM_HostBenchmarkAfterSwap(
            retraceId,
            swapTicks,
            performanceFrequency);
    }
}
