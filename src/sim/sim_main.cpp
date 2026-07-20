#define LIBPORPOISE_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#include <dolphin.h>
#include <simulator/sim.h>
#include <simulator/sim_gpu.h>
#include <SDL2/SDL.h>
#include <simulator/glad/glad.h>

static SDL_GLContext context;
static SDL_Window * window;

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
}

void SIM_Render() {
    SDL_GL_SwapWindow(window);
}