#define LIBPORPOISE_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <dolphin.h>
#include <simulator/sim.h>
#include <simulator/sim_gx_CommandProcessor.h>
#include <simulator/sim_gx_State.hpp>
#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>
#ifdef LIBPORPOISE_BUILD_LINUX
#include <signal.h>
#endif

static SDL_GLContext context;
static SDL_Window * window;

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

void DolphinMain();

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
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
    
    SIM::GX::InitGlobalState();
    SIM_GX_CommandProcessor_Init();

    DolphinMain();
}

void SIM_VIInit() {
}

void SIM_Render() {
    SDL_Event Event;

    while( SDL_PollEvent(&Event))
    {
        if (Event.type == SDL_QUIT) {
            SDL_GL_DeleteContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
            exit(0);
        }

        if (Event.type == SDL_WINDOWEVENT &&
            Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            glViewport(0, 0, Event.window.data1, Event.window.data2);
        }
    }

    //DrawTestTriangle();
    SDL_GL_SwapWindow(window);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
}

void SIM_DebugBreak() {
#ifdef LIBPORPOISE_BUILD_LINUX
    raise(SIGTRAP);
#elif LIBPORPOISE_BUILD_WIN64
    __debugbreak();
#else
    OSReport("Warning: SIM_DebugBreak called but it is not supported on this platform!\n");
#endif
}
