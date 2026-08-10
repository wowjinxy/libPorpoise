#define LIBPORPOISE_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <dolphin.h>
#include <dolphin/dvd.h>
#include <simulator/sim.hpp>
#include <simulator/sim.h>
#include <simulator/sim_gx_State.hpp>
#include <simulator/sim_pad.hpp>
#include <simulator/sim_gx_Thread.hpp>
#include <simulator/sim_vi.h>
#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>
#ifdef LIBPORPOISE_BUILD_LINUX
#include <signal.h>
#endif
#ifdef LIBPORPOISE_BUILD_WIN
#undef IsDebuggerPresent
#include <windows.h>
#endif
#include <format>
#include <string>
#ifdef TRACY_ENABLE
#include "tracy/Tracy.hpp"
#endif

static SDL_GLContext context;
static SDL_Window * window;
static SDL_Thread* s_dolphinMainThread;

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

static int RunDolphinMainThread(void * arg) {
    DolphinMain();
    return 0;
}


namespace SIM {

static SDL_threadID s_renderContextThread = 0;
static SDL_mutex * s_renderContextMutex = nullptr;

bool ReleaseRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();

    if (s_renderContextMutex == NULL) {
        return FALSE;
    }
    SDL_LockMutex(s_renderContextMutex);

    if (context == NULL || s_renderContextThread == 0) {
        SDL_UnlockMutex(s_renderContextMutex);
        return TRUE;
    }
    if (s_renderContextThread != currentThread) {
        //fprintf(stderr,
        //        "libPorpoise SIM: render context release requested from "
        //        "a non-owner thread.\n");
        SDL_UnlockMutex(s_renderContextMutex);
        return FALSE;
    }
    if (SDL_GL_GetCurrentContext() == context &&
        SDL_GL_MakeCurrent(NULL, NULL) != 0) {
        //fprintf(stderr, "libPorpoise SIM: OpenGL context release failed: %s\n",
        //        SDL_GetError());
        SDL_UnlockMutex(s_renderContextMutex);
        return FALSE;
    }
    s_renderContextThread = 0;
    SDL_UnlockMutex(s_renderContextMutex);
    return TRUE;
}

bool AcquireRenderContext(void) {
    const SDL_threadID currentThread = SDL_ThreadID();

    if (s_renderContextMutex == NULL) {
        return FALSE;
    }
    SDL_LockMutex(s_renderContextMutex);

    if (window == NULL || context == NULL) {
        SDL_UnlockMutex(s_renderContextMutex);
        return FALSE;
    }
    if (s_renderContextThread != 0 && s_renderContextThread != currentThread) {
        //fprintf(stderr,
        //        "libPorpoise SIM: render context acquisition requested "
        //        "before its previous owner released it.\n");
        SDL_UnlockMutex(s_renderContextMutex);
        return FALSE;
    }
    if (SDL_GL_GetCurrentContext() != context &&
        SDL_GL_MakeCurrent(window, context) != 0) {
        fprintf(stderr, "libPorpoise SIM: OpenGL context acquisition failed: %s\n",
                SDL_GetError());
        SDL_UnlockMutex(s_renderContextMutex);
        return FALSE;
    }
    glUseProgram(gxShaderProgramId);
    //SIM::GX::GetGlRenderer().SetShaderProgram(gxShaderProgramId);
    s_renderContextThread = currentThread;
    //UpdateDrawableViewport();
    SDL_UnlockMutex(s_renderContextMutex);
    return TRUE;
}


void MainLoop() {
    int waitForRetraceTimeout = 0;

    static constexpr auto HalfFrametime = 8;
    static constexpr auto WaitRetraceMax = 3;
    while(true) {

        // Wait a few MS (maybe half the target frame time)
        SDL_Delay(HalfFrametime);


        // Wait for at least one thread to call VI_WaitForRetrace, there is also a timeout in case no threads call it.
        waitForRetraceTimeout = 0;
        while(!SIM::VI::GetWaitForRetraceCount() && waitForRetraceTimeout < WaitRetraceMax) {
            SDL_Delay(1);
            waitForRetraceTimeout++;
        }


        // Ensure GX is Done
        while(!SIM::GX::IsThreadDone()) {
            SDL_Delay(1);
        }


        //Call Pre Vblank Callback
        SIM::VI::HandlePreRetrace();

        // Read PAD

        // Steal GL context from GX thread
        SIM::GX::TakeRenderContext();
        AcquireRenderContext();
        //Render & Vblank
        SIM_Render();

        ReleaseRenderContext();
        // Tell GX thread it can have its context back
        SIM::GX::GiveRenderContext();


        //Call Post Vblank callback
        SIM::VI::HandlePostRetrace();
    }
}
}

int main(int argc, char** argv) {
#ifdef LIBPORPOISE_BUILD_WIN
    //Set Unicode Codepage
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    #ifdef TRACY_ENABLE
    //Start up Tracy profiler
    tracy::StartupProfiler();

    while(!TracyIsStarted) {
        SDL_Delay(1);
    }
    FrameMarkStart("GameLoop");
    #endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    SIM::s_renderContextMutex = SDL_CreateMutex();

    int windowWidth = 640;
    int windowHeight = 480;

    auto* dvdId = DVDGetCurrentDiskID();
    char gameId[5] = {0};
    char dvdCompany[3] = {0};
    strncpy(gameId, dvdId->gameName, sizeof(dvdId->gameName));
    strncpy(dvdCompany, dvdId->company, sizeof(dvdId->company));
    std::string windowTitle = std::format("libPorpoise Application [{}{}]", gameId, dvdCompany);

    window = SDL_CreateWindow( windowTitle.c_str(), 100, 100, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
    if (window == NULL) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        return 1;
    }

    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
        return 1;
    }

    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        fprintf(stderr, "OpenGL function loading failed\n");
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    glViewport(0, 0, windowWidth, windowHeight);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    gxShaderProgramId = glCreateProgram();
    CompileVertexShader(gxVertexShader, SIM_GXVertexShader);
    CompileFragmentShader(gxFragmentShader, SIM_GXFragmentShader);
    LinkShader(gxShaderProgramId, gxVertexShader, gxFragmentShader);
    glUseProgram(gxShaderProgramId);

    SIM::GX::Init();

    SIM::VI::Init();

    // Spawn a new thread for DolphinMain
    s_dolphinMainThread = SDL_CreateThread(RunDolphinMainThread, "DolphinMain", NULL);
    SIM::MainLoop();
}

void SIM_VIInit() {
}

void SIM_Render() {
    #ifdef TRACY_ENABLE
    FrameMarkEnd("GameLoop");
    #endif
    SDL_Event Event;

    while( SDL_PollEvent(&Event))
    {
        switch(Event.type) {
            case SDL_KEYDOWN:
                SIM::PAD::HandleKey(Event.key.keysym.sym, true);
                break;
            case SDL_KEYUP:
                SIM::PAD::HandleKey(Event.key.keysym.sym, false);
                break;
            case SDL_WINDOWEVENT:
                if(Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    glViewport(0, 0, Event.window.data1, Event.window.data2);
                }
                break;
            case SDL_QUIT:
                SDL_GL_DeleteContext(context);
                SDL_DestroyWindow(window);
                SDL_Quit();
                exit(0);
                break;
            default:
                break;
        }
    }

    //DrawTestTriangle();
    SDL_GL_SwapWindow(window);

    #ifdef TRACY_ENABLE
    FrameMarkNamed("VBlank");
    FrameMark;
    #endif

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
    #ifdef TRACY_ENABLE
    FrameMarkStart("GameLoop");
    #endif
}

void SIM_DebugBreak() {
#ifdef LIBPORPOISE_BUILD_LINUX
    raise(SIGTRAP);
#elif LIBPORPOISE_BUILD_WIN
    __debugbreak();
#else
    OSReport("Warning: SIM_DebugBreak called but it is not supported on this platform!\n");
#endif
}
