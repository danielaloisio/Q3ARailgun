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
** SDL_SND.C
**
** SDL2 audio backend replacing linux_snd.c (OSS /dev/dsp).
** The engine uses a DMA-style ring buffer (dma.buffer).  SDL2 delivers
** audio via a callback, so we maintain the ring buffer ourselves and let
** SDL pull from it.
*/

#include <SDL2/SDL.h>
#include <string.h>

#include "../game/q_shared.h"
#include "../client/snd_local.h"

/* SDL audio device handle */
static SDL_AudioDeviceID sdl_audio_dev = 0;

/* Ring-buffer state */
#define SDL_SND_BUFSIZE  (1 << 15)   /* 32 KB — must be power of two */
static byte        sdl_dma_buffer[SDL_SND_BUFSIZE];
static int         sdl_dma_sample_pos = 0; /* current write position in samples */

/* -------------------------------------------------------------------------- */
/* SDL audio callback                                                          */
/* -------------------------------------------------------------------------- */

static void SDLCALL sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    int bytes_per_sample = dma.samplebits / 8;
    int total_bytes      = dma.samples * bytes_per_sample;

    /* Compute byte read position from the sample cursor */
    int read_pos = (sdl_dma_sample_pos * bytes_per_sample) % total_bytes;

    /* Copy audio data, wrapping around the ring buffer */
    int remaining = len;
    Uint8 *dst    = stream;

    while (remaining > 0) {
        int chunk = total_bytes - read_pos;
        if (chunk > remaining)
            chunk = remaining;
        memcpy(dst, dma.buffer + read_pos, chunk);
        read_pos  = (read_pos + chunk) % total_bytes;
        dst      += chunk;
        remaining -= chunk;
    }
}

/* -------------------------------------------------------------------------- */
/* SNDDMA interface                                                            */
/* -------------------------------------------------------------------------- */

qboolean SNDDMA_Init(void)
{
    SDL_AudioSpec desired, obtained;
    cvar_t *sndbits, *sndspeed, *sndchannels;

    sndbits    = Cvar_Get("sndbits",    "16", CVAR_ARCHIVE);
    sndspeed   = Cvar_Get("sndspeed",   "0",  CVAR_ARCHIVE);
    sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        Com_Printf("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s\n", SDL_GetError());
        return qfalse;
    }

    dma.samplebits = (int)sndbits->value;
    if (dma.samplebits != 16 && dma.samplebits != 8)
        dma.samplebits = 16;

    dma.speed = (int)sndspeed->value;
    if (!dma.speed)
        dma.speed = 22050;

    dma.channels = (int)sndchannels->value;
    if (dma.channels < 1 || dma.channels > 2)
        dma.channels = 2;

    memset(&desired, 0, sizeof(desired));
    desired.freq     = dma.speed;
    desired.format   = (dma.samplebits == 16) ? AUDIO_S16LSB : AUDIO_U8;
    desired.channels = (Uint8)dma.channels;
    desired.samples  = 512;   /* callback buffer size */
    desired.callback = sdl_audio_callback;
    desired.userdata = NULL;

    sdl_audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained,
                                        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (sdl_audio_dev == 0) {
        Com_Printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return qfalse;
    }

    /* Accept what SDL negotiated */
    dma.speed    = obtained.freq;
    dma.channels = obtained.channels;
    dma.samplebits = (obtained.format == AUDIO_U8) ? 8 : 16;

    dma.samples          = SDL_SND_BUFSIZE / (dma.samplebits / 8);
    dma.submission_chunk = 1;
    dma.buffer           = sdl_dma_buffer;

    memset(sdl_dma_buffer, 0, sizeof(sdl_dma_buffer));

    Com_Printf("SDL audio: %d Hz, %d ch, %d-bit\n",
               dma.speed, dma.channels, dma.samplebits);

    SDL_PauseAudioDevice(sdl_audio_dev, 0); /* start playback */
    return qtrue;
}

int SNDDMA_GetDMAPos(void)
{
    /* Return position in *samples* (mono), as the engine expects */
    sdl_dma_sample_pos = (sdl_dma_sample_pos + 256) % dma.samples;
    return sdl_dma_sample_pos;
}

void SNDDMA_Shutdown(void)
{
    if (sdl_audio_dev) {
        SDL_CloseAudioDevice(sdl_audio_dev);
        sdl_audio_dev = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    dma.buffer = NULL;
}

void SNDDMA_BeginPainting(void)
{
    /* Lock the device so the callback won't read while we write */
    if (sdl_audio_dev)
        SDL_LockAudioDevice(sdl_audio_dev);
}

void SNDDMA_Submit(void)
{
    if (sdl_audio_dev)
        SDL_UnlockAudioDevice(sdl_audio_dev);
}

/* Snd_Memset — keep the same override used in the original linux port */
void Snd_Memset(void *dest, const int val, const size_t count)
{
    Com_Memset(dest, val, count);
}
