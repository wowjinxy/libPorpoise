#define LIBPORPOISE_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <dolphin.h>
#include <simulator/sim.h>
#include <simulator/sim_gpu.h>
#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>

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
    SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK );
    
    SIM_GPU_Init();

    DolphinMain();
}

void SIM_VIInit() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetSwapInterval(1);

    int windowWidth = 640;
    int windowHeight = 480;

    window = SDL_CreateWindow( "libPorpoise Game", 100, 100, windowWidth, windowHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
    context = SDL_GL_CreateContext(window);

    gladLoadGLLoader(SDL_GL_GetProcAddress);

    glViewport(0, 0, windowWidth, windowHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f );
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    gxShaderProgramId = glCreateProgram();
    CompileVertexShader(gxVertexShader, SIM_GXVertexShader);
    CompileFragmentShader(gxFragmentShader, SIM_GXFragmentShader);
    LinkShader(gxShaderProgramId, gxVertexShader, gxFragmentShader);

    glUseProgram(gxShaderProgramId);
}

void SIM_Render() {
    DrawTestTriangle();
    SDL_GL_SwapWindow(window);
}