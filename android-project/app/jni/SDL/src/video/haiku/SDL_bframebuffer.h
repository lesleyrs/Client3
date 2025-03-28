/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_BFRAMEBUFFER_H
#define SDL_BFRAMEBUFFER_H
#include <SupportDefs.h>
#ifdef __cplusplus
extern "C" {
#endif

#define DRAWTHREAD

#include "../SDL_sysvideo.h"

extern int HAIKU_CreateWindowFramebuffer(_THIS, SDL_Window *window,
                                         Uint32 *format,
                                         void **pixels, int *pitch);
extern int HAIKU_UpdateWindowFramebuffer(_THIS, SDL_Window *window,
                                         const SDL_Rect *rects, int numrects);
extern void HAIKU_DestroyWindowFramebuffer(_THIS, SDL_Window *window);
extern int32 HAIKU_DrawThread(void *data);

#ifdef __cplusplus
}
#endif

#endif

/* vi: set ts=4 sw=4 expandtab: */
