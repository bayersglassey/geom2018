
/*
    This file contains code from SDL2 (copyright below) which was
    either not exported by the copiled library, or didn't quite do
    what I needed, etc.
     - BAG June 2018
*/

/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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


#include <SDL2/SDL.h>
#include "sdl_util.h"


static void PaletteMappedBlit1to1Key(SDL_BlitInfo *info){
    /* Copy-paste-modified from Blit1to1Key
    (src/video/SDL_blit_1.c) - BAG */

    int c;
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    Uint8 *palmap = info->table;
    Uint32 ckey = info->colorkey;

    if (palmap) {
        while (height--) {
            for (c = width; c; --c) {
                if ( *src != ckey ) {
                  *dst = palmap[*src];
                }
                dst++;
                src++;
            }
            src += srcskip;
            dst += dstskip;
        }
    } else {
        while (height--) {
            for (c = width; c; --c) {
                if ( *src != ckey ) {
                  *dst = *src;
                }
                dst++;
                src++;
            }
            src += srcskip;
            dst += dstskip;
        }
    }
}

static void SDL_PaletteMappedSoftBlit_GetInfo(
    SDL_Surface * src, SDL_Rect * srcrect,
    SDL_Surface * dst, SDL_Rect * dstrect,
    Uint8 *table, SDL_BlitInfo *info
){
    /* Factored out of SDL_SoftBlit
    (src/video/SDL_blit.c) - BAG */

    /* Set up the blit information */
    info->src_fmt = src->format;
    info->src = (Uint8 *) src->pixels +
        (Uint16) srcrect->y * src->pitch +
        (Uint16) srcrect->x * info->src_fmt->BytesPerPixel;
    info->src_w = srcrect->w;
    info->src_h = srcrect->h;
    info->src_pitch = src->pitch;
    info->src_skip =
        info->src_pitch - info->src_w * info->src_fmt->BytesPerPixel;
    info->dst_fmt = dst->format;
    info->dst =
        (Uint8 *) dst->pixels + (Uint16) dstrect->y * dst->pitch +
        (Uint16) dstrect->x * info->dst_fmt->BytesPerPixel;
    info->dst_w = dstrect->w;
    info->dst_h = dstrect->h;
    info->dst_pitch = dst->pitch;
    info->dst_skip =
        info->dst_pitch - info->dst_w * info->dst_fmt->BytesPerPixel;

    info->colorkey = 0;
    info->table = table;
}

static int SDL_PaletteMappedSoftBlit(
    SDL_Surface * src, SDL_Rect * srcrect,
    SDL_Surface * dst, SDL_Rect * dstrect,
    Uint8 *table
){
    /* Copy-paste-modified from SDL_SoftBlit
    (src/video/SDL_blit.c) - BAG */

    int okay;
    int src_locked;
    int dst_locked;

    /* Everything is okay at the beginning...  */
    okay = 1;

    /* Lock the destination if it's in hardware */
    dst_locked = 0;
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            okay = 0;
        } else {
            dst_locked = 1;
        }
    }
    /* Lock the source if it's in hardware */
    src_locked = 0;
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) {
            okay = 0;
        } else {
            src_locked = 1;
        }
    }

    /* Set up source and destination buffer pointers, and BLIT! */
    if (okay && !SDL_RectEmpty(srcrect)) {
        SDL_BlitInfo info;
        SDL_PaletteMappedSoftBlit_GetInfo(src, srcrect, dst,
            dstrect, table, &info);
        PaletteMappedBlit1to1Key(&info);
    }

    /* We need to unlock the surfaces if they're locked */
    if (dst_locked) {
        SDL_UnlockSurface(dst);
    }
    if (src_locked) {
        SDL_UnlockSurface(src);
    }
    /* Blit is done! */
    return (okay ? 0 : -1);
}

int SDL_PaletteMappedUpperBlit(
    SDL_Surface * src, const SDL_Rect * srcrect,
    SDL_Surface * dst, SDL_Rect * dstrect,
    Uint8 *table
){
    /* Copy-paste-modified from SDL_UpperBlit
    (src/video/SDL_surface.c) - BAG */

    SDL_Rect fulldst;
    int srcx, srcy, w, h;

    /* Make sure the surfaces aren't locked */
    if (!src || !dst) {
        return SDL_SetError("SDL_UpperBlit: passed a NULL surface");
    }
    if (src->locked || dst->locked) {
        return SDL_SetError("Surfaces must not be locked during blit");
    }

    /* If the destination rectangle is NULL, use the entire dest surface */
    if (dstrect == NULL) {
        fulldst.x = fulldst.y = 0;
        fulldst.w = dst->w;
        fulldst.h = dst->h;
        dstrect = &fulldst;
    }

    /* clip the source rectangle to the source surface */
    if (srcrect) {
        int maxw, maxh;

        srcx = srcrect->x;
        w = srcrect->w;
        if (srcx < 0) {
            w += srcx;
            dstrect->x -= srcx;
            srcx = 0;
        }
        maxw = src->w - srcx;
        if (maxw < w)
            w = maxw;

        srcy = srcrect->y;
        h = srcrect->h;
        if (srcy < 0) {
            h += srcy;
            dstrect->y -= srcy;
            srcy = 0;
        }
        maxh = src->h - srcy;
        if (maxh < h)
            h = maxh;

    } else {
        srcx = srcy = 0;
        w = src->w;
        h = src->h;
    }

    /* clip the destination rectangle against the clip rectangle */
    {
        SDL_Rect *clip = &dst->clip_rect;
        int dx, dy;

        dx = clip->x - dstrect->x;
        if (dx > 0) {
            w -= dx;
            dstrect->x += dx;
            srcx += dx;
        }
        dx = dstrect->x + w - clip->x - clip->w;
        if (dx > 0)
            w -= dx;

        dy = clip->y - dstrect->y;
        if (dy > 0) {
            h -= dy;
            dstrect->y += dy;
            srcy += dy;
        }
        dy = dstrect->y + h - clip->y - clip->h;
        if (dy > 0)
            h -= dy;
    }

    if (w > 0 && h > 0) {
        SDL_Rect sr;
        sr.x = srcx;
        sr.y = srcy;
        sr.w = dstrect->w = w;
        sr.h = dstrect->h = h;
        return SDL_PaletteMappedSoftBlit(src, &sr, dst, dstrect, table);
    }
    dstrect->w = dstrect->h = 0;
    return 0;
}


