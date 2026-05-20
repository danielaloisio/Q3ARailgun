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

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>

#include "../renderer/tr_local.h"
#include "../unix/unix_glw.h"

/* Extension function pointers are defined in renderer/tr_init.c.
   We just populate them here at QGL_Init time. */

/* -------------------------------------------------------------------------- */

void *qwglGetProcAddress(char *symbol)
{
    return SDL_GL_GetProcAddress(symbol);
}

qboolean QGL_Init(const char *dllname)
{
    ri.Printf(PRINT_ALL, "...initializing QGL (SDL2 linked)\n");

    qglMultiTexCoord2fARB    = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
    qglActiveTextureARB      = SDL_GL_GetProcAddress("glActiveTextureARB");
    qglClientActiveTextureARB = SDL_GL_GetProcAddress("glClientActiveTextureARB");
    qglLockArraysEXT         = SDL_GL_GetProcAddress("glLockArraysEXT");
    qglUnlockArraysEXT       = SDL_GL_GetProcAddress("glUnlockArraysEXT");

    return qtrue;
}

void QGL_Shutdown(void)
{
    ri.Printf(PRINT_ALL, "...shutting down QGL\n");

    qglMultiTexCoord2fARB     = NULL;
    qglActiveTextureARB       = NULL;
    qglClientActiveTextureARB = NULL;
    qglLockArraysEXT          = NULL;
    qglUnlockArraysEXT        = NULL;

    if (glw_state.log_fp) {
        fclose(glw_state.log_fp);
        glw_state.log_fp = NULL;
    }
}

void QGL_EnableLogging(qboolean enable)
{
    static qboolean isEnabled = qfalse;

    if (isEnabled == enable)
        return;
    isEnabled = enable;

    if (enable) {
        if (!glw_state.log_fp)
            glw_state.log_fp = fopen("gl.log", "wt");
        ri.Printf(PRINT_ALL, "QGL logging enabled\n");
    } else {
        if (glw_state.log_fp) {
            fclose(glw_state.log_fp);
            glw_state.log_fp = NULL;
        }
        ri.Printf(PRINT_ALL, "QGL logging disabled\n");
    }
}

void GLimp_LogNewFrame(void)
{
    if (glw_state.log_fp)
        fprintf(glw_state.log_fp, "*** R_BeginFrame ***\n");
}
