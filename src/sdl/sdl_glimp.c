/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
** SDL_GLIMP.C
**
** SDL2 replacement for linux_glimp.c.
** Implements: GLimp_Init, GLimp_Shutdown, GLimp_EndFrame, GLimp_SetGamma,
**             IN_Init, IN_Shutdown, IN_Frame, Sys_SendKeyEvents,
**             QGL_Init, QGL_Shutdown, QGL_EnableLogging
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../unix/linux_local.h"

/* -------------------------------------------------------------------------- */
/* Globals visible to sdl_qgl.c                                               */
/* -------------------------------------------------------------------------- */

typedef struct {
    void   *OpenGLLib;
    FILE   *log_fp;
} glwstate_t;

glwstate_t glw_state;

static SDL_Window   *sdl_window  = NULL;
static SDL_GLContext sdl_glctx   = NULL;

static cvar_t *in_mouse    = NULL;
static cvar_t *in_nograb   = NULL;
static cvar_t *in_joystick = NULL;

/* in_subframe is referenced by unix_shared.c (Sys_XTimeToSysTime).
   We provide it here; Sys_XTimeToSysTime itself falls back to Sys_Milliseconds
   when in_subframe is 0, so initialising it to 0 is correct for SDL2. */
cvar_t *in_subframe = NULL;

static qboolean mouse_avail  = qfalse;
static qboolean mouse_active = qfalse;

/* -------------------------------------------------------------------------- */
/* Key translation                                                             */
/* -------------------------------------------------------------------------- */

static int SDLKeyToQ3Key(SDL_Keycode sym)
{
    switch (sym) {
    case SDLK_TAB:          return K_TAB;
    case SDLK_RETURN:       return K_ENTER;
    case SDLK_ESCAPE:       return K_ESCAPE;
    case SDLK_SPACE:        return K_SPACE;
    case SDLK_BACKSPACE:    return K_BACKSPACE;
    case SDLK_UP:           return K_UPARROW;
    case SDLK_DOWN:         return K_DOWNARROW;
    case SDLK_LEFT:         return K_LEFTARROW;
    case SDLK_RIGHT:        return K_RIGHTARROW;
    case SDLK_LALT:
    case SDLK_RALT:         return K_ALT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:        return K_CTRL;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:       return K_SHIFT;
    case SDLK_INSERT:       return K_INS;
    case SDLK_DELETE:       return K_DEL;
    case SDLK_PAGEDOWN:     return K_PGDN;
    case SDLK_PAGEUP:       return K_PGUP;
    case SDLK_HOME:         return K_HOME;
    case SDLK_END:          return K_END;
    case SDLK_F1:           return K_F1;
    case SDLK_F2:           return K_F2;
    case SDLK_F3:           return K_F3;
    case SDLK_F4:           return K_F4;
    case SDLK_F5:           return K_F5;
    case SDLK_F6:           return K_F6;
    case SDLK_F7:           return K_F7;
    case SDLK_F8:           return K_F8;
    case SDLK_F9:           return K_F9;
    case SDLK_F10:          return K_F10;
    case SDLK_F11:          return K_F11;
    case SDLK_F12:          return K_F12;
    case SDLK_PAUSE:        return K_PAUSE;
    case SDLK_KP_ENTER:     return K_KP_ENTER;
    case SDLK_KP_0:         return K_KP_INS;
    case SDLK_KP_1:         return K_KP_END;
    case SDLK_KP_2:         return K_KP_DOWNARROW;
    case SDLK_KP_3:         return K_KP_PGDN;
    case SDLK_KP_4:         return K_KP_LEFTARROW;
    case SDLK_KP_5:         return K_KP_5;
    case SDLK_KP_6:         return K_KP_RIGHTARROW;
    case SDLK_KP_7:         return K_KP_HOME;
    case SDLK_KP_8:         return K_KP_UPARROW;
    case SDLK_KP_9:         return K_KP_PGUP;
    case SDLK_KP_PERIOD:    return K_KP_DEL;
    case SDLK_KP_DIVIDE:    return K_KP_SLASH;
    case SDLK_KP_MULTIPLY:  return '*';
    case SDLK_KP_MINUS:     return K_KP_MINUS;
    case SDLK_KP_PLUS:      return K_KP_PLUS;
    default:
        if (sym >= SDLK_a && sym <= SDLK_z)
            return (int)sym; /* lowercase ASCII */
        if (sym >= 32 && sym < 127)
            return (int)sym;
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Mouse                                                                       */
/* -------------------------------------------------------------------------- */

static void IN_ActivateMouse(void)
{
    if (!mouse_avail || mouse_active)
        return;
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_SetWindowGrab(sdl_window, SDL_TRUE);
    mouse_active = qtrue;
}

static void IN_DeactivateMouse(void)
{
    if (!mouse_active)
        return;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowGrab(sdl_window, SDL_FALSE);
    mouse_active = qfalse;
}

/* -------------------------------------------------------------------------- */
/* Event pump — called every frame via Sys_SendKeyEvents → IN_Frame           */
/* -------------------------------------------------------------------------- */

static void HandleEvents(void)
{
    SDL_Event ev;
    int key;
    int t;

    while (SDL_PollEvent(&ev)) {
        t = Sys_Milliseconds();
        switch (ev.type) {

        case SDL_KEYDOWN:
            key = SDLKeyToQ3Key(ev.key.keysym.sym);
            if (key)
                Sys_QueEvent(t, SE_KEY, key, qtrue, 0, NULL);
            /* forward printable chars as SE_CHAR */
            if (ev.key.keysym.sym >= 32 && ev.key.keysym.sym < 127) {
                int ch = (int)ev.key.keysym.sym;
                Sys_QueEvent(t, SE_CHAR, ch, 0, 0, NULL);
            }
            break;

        case SDL_KEYUP:
            key = SDLKeyToQ3Key(ev.key.keysym.sym);
            if (key)
                Sys_QueEvent(t, SE_KEY, key, qfalse, 0, NULL);
            break;

        case SDL_MOUSEMOTION:
            if (mouse_active)
                Sys_QueEvent(t, SE_MOUSE, ev.motion.xrel, ev.motion.yrel, 0, NULL);
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int btn = 0;
            qboolean down = (ev.type == SDL_MOUSEBUTTONDOWN) ? qtrue : qfalse;
            switch (ev.button.button) {
            case SDL_BUTTON_LEFT:   btn = K_MOUSE1; break;
            case SDL_BUTTON_RIGHT:  btn = K_MOUSE2; break;
            case SDL_BUTTON_MIDDLE: btn = K_MOUSE3; break;
            case 4:                 btn = K_MOUSE4; break;
            case 5:                 btn = K_MOUSE5; break;
            }
            if (btn)
                Sys_QueEvent(t, SE_KEY, btn, down, 0, NULL);
            break;
        }

        case SDL_MOUSEWHEEL:
            if (ev.wheel.y > 0) {
                Sys_QueEvent(t, SE_KEY, K_MWHEELUP, qtrue,  0, NULL);
                Sys_QueEvent(t, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
            } else if (ev.wheel.y < 0) {
                Sys_QueEvent(t, SE_KEY, K_MWHEELDOWN, qtrue,  0, NULL);
                Sys_QueEvent(t, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
            }
            break;

        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                IN_ActivateMouse();
            else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                IN_DeactivateMouse();
            break;

        case SDL_QUIT:
            Sys_QueEvent(t, SE_KEY, K_ESCAPE, qtrue, 0, NULL);
            break;

        default:
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Input subsystem                                                             */
/* -------------------------------------------------------------------------- */

void IN_Init(void)
{
    Com_Printf("\n------- Input Initialization -------\n");
    in_mouse    = Cvar_Get("in_mouse",    "1", CVAR_ARCHIVE);
    in_nograb   = Cvar_Get("in_nograb",  "0", 0);
    in_joystick = Cvar_Get("in_joystick", "0", CVAR_ARCHIVE | CVAR_LATCH);
    in_subframe = Cvar_Get("in_subframe", "0", CVAR_ARCHIVE);

    mouse_avail = in_mouse->value ? qtrue : qfalse;
    Com_Printf("------------------------------------\n");
}

void IN_Shutdown(void)
{
    IN_DeactivateMouse();
    mouse_avail = qfalse;
}

void IN_Frame(void)
{
    if (cls.keyCatchers & KEYCATCH_CONSOLE) {
        if (Cvar_VariableValue("r_fullscreen") == 0) {
            IN_DeactivateMouse();
            return;
        }
    }
    IN_ActivateMouse();
}

void IN_Activate(void) {}

/* Joystick stubs — SDL2 controller support can be added here later */
void IN_StartupJoystick(void) {}
void IN_JoyMove(void) {}

void Sys_SendKeyEvents(void)
{
    HandleEvents();
}

/* -------------------------------------------------------------------------- */
/* GLimp — OpenGL window via SDL2                                              */
/* -------------------------------------------------------------------------- */

typedef enum {
    RSERR_OK,
    RSERR_INVALID_FULLSCREEN,
    RSERR_INVALID_MODE,
    RSERR_UNKNOWN
} rserr_t;

static rserr_t GLW_SetMode(int mode, qboolean fullscreen)
{
    int width, height;
    Uint32 flags;

    if (mode == -2) {
        /* custom resolution: read r_customwidth / r_customheight */
        cvar_t *cw = ri.Cvar_Get("r_customwidth",  "1024", CVAR_ARCHIVE);
        cvar_t *ch = ri.Cvar_Get("r_customheight", "768",  CVAR_ARCHIVE);
        glConfig.vidWidth    = cw->integer > 0 ? cw->integer : 640;
        glConfig.vidHeight   = ch->integer > 0 ? ch->integer : 480;
        glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight;
    } else if (!R_GetModeInfo(&glConfig.vidWidth, &glConfig.vidHeight,
                              &glConfig.windowAspect, mode)) {
        ri.Printf(PRINT_ALL, " invalid mode\n");
        return RSERR_INVALID_MODE;
    }

    width  = glConfig.vidWidth;
    height = glConfig.vidHeight;

    ri.Printf(PRINT_ALL, "...setting mode %d: %dx%d\n", mode, width, height);

    /* Set framebuffer attributes before window creation */
    int colorbits = r_colorbits->value ? (int)r_colorbits->value : 24;
    int depthbits = r_depthbits->value ? (int)r_depthbits->value : 24;
    int stencilbits = (int)r_stencilbits->value;

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     colorbits / 3);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   colorbits / 3);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    colorbits / 3);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   depthbits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencilbits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    sdl_window = SDL_CreateWindow("Quake III Arena",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  width, height, flags);
    if (!sdl_window) {
        ri.Printf(PRINT_ALL, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return RSERR_INVALID_MODE;
    }

    sdl_glctx = SDL_GL_CreateContext(sdl_window);
    if (!sdl_glctx) {
        ri.Printf(PRINT_ALL, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
        return RSERR_INVALID_MODE;
    }

    SDL_GL_MakeCurrent(sdl_window, sdl_glctx);
    SDL_GL_SetSwapInterval(1); /* vsync on; set to 0 for uncapped */

    return RSERR_OK;
}

void GLimp_Init(void)
{
    cvar_t *r_mode, *r_fullscreen;
    rserr_t err;

    ri.Printf(PRINT_ALL, "Initializing OpenGL display\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        ri.Error(ERR_FATAL, "SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
        return;
    }

    r_mode       = ri.Cvar_Get("r_mode",       "3",  CVAR_ARCHIVE);
    r_fullscreen = ri.Cvar_Get("r_fullscreen",  "0",  CVAR_ARCHIVE);

    err = GLW_SetMode((int)r_mode->value,
                      (qboolean)(r_fullscreen->integer != 0));

    if (err == RSERR_INVALID_MODE && (int)r_mode->value != 3) {
        ri.Printf(PRINT_ALL, "...WARNING: mode %d invalid, retrying with mode 3\n",
                  (int)r_mode->value);
        ri.Cvar_Set("r_mode", "3");
        err = GLW_SetMode(3, qfalse);
    }

    switch (err) {
    case RSERR_INVALID_FULLSCREEN:
        ri.Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
        break;
    case RSERR_INVALID_MODE:
        ri.Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n",
                  (int)r_mode->value);
        ri.Error(ERR_FATAL, "GLimp_Init: could not set video mode");
        return;
    default:
        break;
    }

    /* Populate glConfig with driver strings (use GL directly — QGL_LINKED
       macros are not active in this translation unit) */
    Q_strncpyz(glConfig.vendor_string,
               (const char *)glGetString(GL_VENDOR),   sizeof(glConfig.vendor_string));
    Q_strncpyz(glConfig.renderer_string,
               (const char *)glGetString(GL_RENDERER), sizeof(glConfig.renderer_string));
    Q_strncpyz(glConfig.version_string,
               (const char *)glGetString(GL_VERSION),  sizeof(glConfig.version_string));
    Q_strncpyz(glConfig.extensions_string,
               (const char *)glGetString(GL_EXTENSIONS), sizeof(glConfig.extensions_string));

    glConfig.colorBits   = 24;
    glConfig.depthBits   = 24;
    glConfig.stencilBits = 8;
    glConfig.deviceSupportsGamma = qfalse;

    /* Initialize signal handlers now that the window is up */
    InitSig();
}

void GLimp_Shutdown(void)
{
    IN_DeactivateMouse();

    if (sdl_glctx) {
        SDL_GL_DeleteContext(sdl_glctx);
        sdl_glctx = NULL;
    }
    if (sdl_window) {
        SDL_DestroyWindow(sdl_window);
        sdl_window = NULL;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    if (glw_state.OpenGLLib) {
        dlclose(glw_state.OpenGLLib);
        glw_state.OpenGLLib = NULL;
    }
}

void GLimp_EndFrame(void)
{
    if (Q_stricmp(r_drawBuffer->string, "GL_FRONT") != 0)
        SDL_GL_SwapWindow(sdl_window);

    QGL_EnableLogging((qboolean)r_logFile->integer);
}

void GLimp_SetGamma(unsigned char red[256], unsigned char green[256],
                    unsigned char blue[256])
{
    Uint16 ramp[768];
    int i;

    for (i = 0; i < 256; i++) {
        ramp[i]       = ((Uint16)red[i])   << 8;
        ramp[i + 256] = ((Uint16)green[i]) << 8;
        ramp[i + 512] = ((Uint16)blue[i])  << 8;
    }
    SDL_SetWindowGammaRamp(sdl_window, ramp, ramp + 256, ramp + 512);
}

/* Stub — SMP not supported in this port */
void GLimp_RenderThreadWrapper(void *stub) {}
qboolean GLimp_SpawnRenderThread(void (*function)(void))
{
    ri.Printf(PRINT_WARNING, "SMP support not available in SDL port\n");
    return qfalse;
}
void *GLimp_RendererSleep(void) { return NULL; }
void GLimp_FrontEndSleep(void) {}
void GLimp_WakeRenderer(void *data) {}

/* Called by tr_backend.c when GL call logging is enabled */
void GLimp_LogComment(char *comment)
{
    if (glw_state.log_fp)
        fprintf(glw_state.log_fp, "%s", comment);
}

/* Called by cl_cgame.c — snap float vector components to integers */
void Sys_SnapVector(float *v)
{
    v[0] = (float)(int)v[0];
    v[1] = (float)(int)v[1];
    v[2] = (float)(int)v[2];
}
