/*
Copyright (C) 1994-1995 Apogee Software, Ltd.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "modexlib.h"
#include "rt_util.h"
#include "rt_net.h" // for GamePaused
#include "isr.h"	// for VBLCOUNTER

static void StretchMemPicture();
// GLOBAL VARIABLES

boolean StretchScreen = 0; // bna++
extern boolean iG_aimCross;
extern boolean sdl_fullscreen;
extern int iG_X_center;
extern int iG_Y_center;
byte *iG_buf_center;

int linewidth;
// int    ylookup[MAXSCREENHEIGHT];
int ylookup[MAXSCREENHEIGHT]; // just set to max res
byte *SCREEN_BUFFER;
int screensize;
byte *bufferofs;
byte *displayofs;
boolean graphicsmode = false;
byte *bufofsTopLimit;
byte *bufofsBottomLimit;

void DrawCenterAim();

#include "SDL.h"

/*
====================
=
= GraphicsMode
=
====================
*/
SDL_Surface *sdl_surface = NULL;
SDL_Surface *unstretch_sdl_surface = NULL;

static SDL_Window *screen;
static SDL_Renderer *renderer;
static SDL_Surface *argbbuffer;
static SDL_Texture *texture;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// kampvogn.dk: high resolution. The game runs at its 480-line resolution
// (status bar, text, menus as they are); only the 3D view is rendered at
// hires_w x hires_h into hires_surface (rt_view.c: HiRes_Begin/End around
// the 3D part of ThreeDRefresh). A frame is the 3D layer with the game's
// screen scaled over it, HIRES_KEY pixels (the view area) transparent.
SDL_Surface *hires_surface = NULL;
static SDL_Surface *hires_argb;
static SDL_Texture *hires_texture;
int hires_w = 0, hires_h = 0;
boolean hires_valid = false; // the 3D layer belongs to the frame on screen

EM_JS(int, web_hires_size, (int *w, int *h), {
	const r = Module.hires;
	if (!r || !r[0] || !r[1]) return 0;
	HEAP32[w >> 2] = r[0]; HEAP32[h >> 2] = r[1];
	return 1;
});

static Uint32 VL_TexFormat(void)
{
	return hires_w ? SDL_PIXELFORMAT_ARGB8888 : SDL_GetWindowPixelFormat(screen);
}

static void VL_MakeHiResSurfaces(void)
{
	if (hires_surface)
	{
		SDL_FreeSurface(hires_surface);
		SDL_FreeSurface(hires_argb);
		SDL_DestroyTexture(hires_texture);
	}
	hires_surface = SDL_CreateRGBSurface(0, hires_w, hires_h, 8, 0, 0, 0, 0);
	if (sdl_surface)
		SDL_SetPaletteColors(hires_surface->format->palette,
							 sdl_surface->format->palette->colors, 0, 256);
	SDL_FillRect(hires_surface, NULL, 0);
	hires_argb = SDL_CreateRGBSurfaceWithFormatFrom(NULL, hires_w, hires_h, 0, 0,
													 SDL_PIXELFORMAT_ARGB8888);
	hires_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
									  SDL_TEXTUREACCESS_STREAMING, hires_w, hires_h);
	SDL_SetTextureBlendMode(hires_texture, SDL_BLENDMODE_NONE);
	hires_valid = false;
}
#else
#define VL_TexFormat() SDL_GetWindowPixelFormat(screen)
#endif

SDL_Window *VL_GetVideoWindow(void)
{
	return screen;
}

SDL_Surface *VL_GetVideoSurface(void)
{
	return sdl_surface;
}

int VL_SaveBMP(const char *file)
{
	return SDL_SaveBMP(sdl_surface, file);
}

void SetShowCursor(int show)
{
	SDL_SetRelativeMouseMode(!show);
	SDL_GetRelativeMouseState(NULL, NULL);
}

static union {
	int (*BlitScaled)(SDL_Surface *src, const SDL_Rect *srcrect,
					  SDL_Surface *dst, SDL_Rect *dstrect);
	int (*SoftStretch)(SDL_Surface *src, const SDL_Rect *srcrect,
					   SDL_Surface *dst, const SDL_Rect *dstrect);
} StretchFunc;

void GraphicsMode(void)
{
#ifdef __EMSCRIPTEN__
	// Fixed-size canvas; the web page scales it to a 4:3 box
	uint32_t flags = 0;
#else
	uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		Error("Could not initialize SDL\n");
	}

#ifndef __EMSCRIPTEN__ // the page has its own fullscreen button
	if (sdl_fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
#endif

#ifdef __EMSCRIPTEN__
	if (!web_hires_size(&hires_w, &hires_h) ||
		hires_w < iGLOBAL_SCREENWIDTH || hires_h <= iGLOBAL_SCREENHEIGHT ||
		hires_w > MAXSCREENWIDTH || hires_h > MAXSCREENHEIGHT)
		hires_w = hires_h = 0;
	{
		const int ww = hires_w ? hires_w : iGLOBAL_SCREENWIDTH;
		const int wh = hires_w ? hires_h : iGLOBAL_SCREENHEIGHT;
		screen = SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED,
								  SDL_WINDOWPOS_CENTERED, ww, wh, flags);
		SDL_SetWindowMinimumSize(screen, ww, wh);
	}
#else
	screen =
		SDL_CreateWindow(NULL, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
						 iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT, flags);
	SDL_SetWindowMinimumSize(screen, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
#endif
	SDL_SetWindowTitle(screen, PACKAGE_STRING);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
	{
		renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_SOFTWARE);
	}
#ifdef __EMSCRIPTEN__
	if (hires_w)
		SDL_RenderSetLogicalSize(renderer, hires_w, hires_h);
	else
#endif
		SDL_RenderSetLogicalSize(renderer, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	sdl_surface = SDL_CreateRGBSurface(0, iGLOBAL_SCREENWIDTH,
									   iGLOBAL_SCREENHEIGHT, 8, 0, 0, 0, 0);
	SDL_FillRect(sdl_surface, NULL, 0);

	argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
		NULL, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT, 0, 0,
		VL_TexFormat());

	texture = SDL_CreateTexture(renderer, VL_TexFormat(),
								SDL_TEXTUREACCESS_STREAMING,
								iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
#ifdef __EMSCRIPTEN__
	if (hires_w)
	{
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
		VL_MakeHiResSurfaces();
	}
#endif

	SetShowCursor(!sdl_fullscreen);

	const char *driver = SDL_GetCurrentVideoDriver();
	if (driver && strcmp(driver, "wayland") == 0)
	{
		StretchFunc.BlitScaled = SDL_BlitScaled;
	}
	else
	{
		StretchFunc.SoftStretch = SDL_SoftStretch;
	}
}

void ToggleFullScreen(void)
{
	unsigned int flags = 0;

	sdl_fullscreen = !sdl_fullscreen;

	if (sdl_fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	SDL_SetWindowFullscreen(screen, flags);
	SetShowCursor(!sdl_fullscreen);
}

/*
====================
=
= SetTextMode
=
====================
*/
void SetTextMode(void)
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == SDL_INIT_VIDEO)
	{
		if (sdl_surface)
			SDL_FreeSurface(sdl_surface);
		if (unstretch_sdl_surface)
			SDL_FreeSurface(unstretch_sdl_surface);
		if (argbbuffer)
			SDL_FreeSurface(argbbuffer);
		if (texture)
			SDL_DestroyTexture(texture);
		if (renderer)
			SDL_DestroyRenderer(renderer);
		if (screen)
			SDL_DestroyWindow(screen);

		sdl_surface = NULL;
		unstretch_sdl_surface = NULL;
		argbbuffer = NULL;
		texture = NULL;
		renderer = NULL;
		screen = NULL;

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

/*
====================
=
= TurnOffTextCursor
=
====================
*/
void TurnOffTextCursor(void)
{
}

/*
====================
=
= WaitVBL
=
====================
*/

static Uint64 next_time = 0;

static Uint64 time_left(void)
{
	Uint64 now = SDL_GetTicks();

	if (next_time <= now)
		return 0;
	else
		return next_time - now;
}

void WaitVBL(void)
{
	// was:
	// SDL_Delay (16667/1000);

	if (next_time == 0)
		next_time = SDL_GetTicks() + VBLCOUNTER;

	SDL_Delay(time_left());
	next_time += VBLCOUNTER;
}

/*
=======================
=
= VL_SetVGAPlaneMode
=
=======================
*/

static void VL_SetupScreenTables(void)
{
	int i, offset;

	//
	// set up lookup tables
	//
	// bna--   linewidth = 320;
	linewidth = iGLOBAL_SCREENWIDTH;

	offset = 0;

	for (i = 0; i < iGLOBAL_SCREENHEIGHT; i++)
	{
		ylookup[i] = offset;
		offset += linewidth;
	}

	//    screensize=MAXSCREENHEIGHT*MAXSCREENWIDTH;
	screensize = iGLOBAL_SCREENHEIGHT * iGLOBAL_SCREENWIDTH;

	SCREEN_BUFFER = displayofs = bufferofs = sdl_surface->pixels;

	iG_X_center = iGLOBAL_SCREENWIDTH / 2;
	iG_Y_center = (iGLOBAL_SCREENHEIGHT / 2) + 10; //+10 = move aim down a bit

	iG_buf_center =
		bufferofs +
		(screensize / 2); //(iG_Y_center*iGLOBAL_SCREENWIDTH);//+iG_X_center;

	bufofsTopLimit = bufferofs + screensize - iGLOBAL_SCREENWIDTH;
	bufofsBottomLimit = bufferofs + iGLOBAL_SCREENWIDTH;
}

void VL_SetVGAPlaneMode(void)
{
	GraphicsMode();
	VL_SetupScreenTables();

	// start stretched
	EnableScreenStretch();
	XFlipPage();
}

#ifdef __EMSCRIPTEN__
// kampvogn.dk: a new width while playing (the page asks for one when the
// window turns or is resized). The surfaces, the texture and the lookup
// tables are made again at the new width, with the same palette; the caller
// sets up the view and redraws (SetupScreen). Only while the 3D view is up:
// the 320x200 screens (menus) are drawn into the unstretched surface.
static SDL_Surface *VL_NewPalettedSurface(SDL_Surface *old)
{
	SDL_Surface *s = SDL_CreateRGBSurface(0, iGLOBAL_SCREENWIDTH,
										  iGLOBAL_SCREENHEIGHT, 8, 0, 0, 0, 0);
	SDL_SetPaletteColors(s->format->palette, old->format->palette->colors, 0, 256);
	SDL_FillRect(s, NULL, 0);
	SDL_FreeSurface(old);
	return s;
}

// a new size for the 3D layer (the page asks when the window changes); 0x0: none
boolean VL_ChangeHiRes(int w, int h)
{
	if (w == hires_w && h == hires_h)
		return false;
	if (w && (w < iGLOBAL_SCREENWIDTH || h <= iGLOBAL_SCREENHEIGHT ||
			  w > MAXSCREENWIDTH || h > MAXSCREENHEIGHT))
		return false;
	hires_w = w;
	hires_h = h;
	if (hires_w)
	{
		SDL_SetWindowMinimumSize(screen, hires_w, hires_h);
		SDL_SetWindowSize(screen, hires_w, hires_h);
		SDL_RenderSetLogicalSize(renderer, hires_w, hires_h);
		VL_MakeHiResSurfaces();
	}
	else
	{
		SDL_SetWindowMinimumSize(screen, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
		SDL_SetWindowSize(screen, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
		SDL_RenderSetLogicalSize(renderer, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
		hires_valid = false;
	}
	// the game's texture: with or without see-through pixels
	SDL_FreeSurface(argbbuffer);
	argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
		NULL, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT, 0, 0, VL_TexFormat());
	SDL_DestroyTexture(texture);
	texture = SDL_CreateTexture(renderer, VL_TexFormat(), SDL_TEXTUREACCESS_STREAMING,
								iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT);
	if (hires_w)
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	return true;
}

boolean VL_ChangeScreenWidth(int width)
{
	if (width == iGLOBAL_SCREENWIDTH || StretchScreen ||
		!IS_WIDE_RES(width, iGLOBAL_SCREENHEIGHT))
		return false;

	iGLOBAL_SCREENWIDTH = width;
	if (!hires_w)
	{
		SDL_SetWindowMinimumSize(screen, width, iGLOBAL_SCREENHEIGHT);
		SDL_SetWindowSize(screen, width, iGLOBAL_SCREENHEIGHT);
		SDL_RenderSetLogicalSize(renderer, width, iGLOBAL_SCREENHEIGHT);
	}

	sdl_surface = VL_NewPalettedSurface(sdl_surface);
	if (unstretch_sdl_surface)
		unstretch_sdl_surface = VL_NewPalettedSurface(unstretch_sdl_surface);

	SDL_FreeSurface(argbbuffer);
	argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
		NULL, width, iGLOBAL_SCREENHEIGHT, 0, 0, VL_TexFormat());
	SDL_DestroyTexture(texture);
	texture = SDL_CreateTexture(renderer, VL_TexFormat(),
								SDL_TEXTUREACCESS_STREAMING, width,
								iGLOBAL_SCREENHEIGHT);
	if (hires_w)
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	hires_valid = false;

	VL_SetupScreenTables();
	return true;
}
#endif

/*
=======================
=
= VL_CopyPlanarPage
=
=======================
*/
void VL_CopyPlanarPage(byte *src, byte *dest)
{
	memcpy(dest, src, screensize);
}

/*
=======================
=
= VL_CopyPlanarPageToMemory
=
=======================
*/
void VL_CopyPlanarPageToMemory(byte *src, byte *dest)
{
	memcpy(dest, src, screensize);
}

/*
=================
=
= VL_ClearBuffer
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearBuffer(byte *buf, byte color)
{
	memset((byte *)buf, color, screensize);
}

/*
=================
=
= VL_ClearVideo
=
= Fill the entire video buffer with a given color
=
=================
*/

void VL_ClearVideo(byte color)
{
	memset(sdl_surface->pixels, color,
		   iGLOBAL_SCREENWIDTH * iGLOBAL_SCREENHEIGHT);
}

/* C version of rt_vh_a.asm */

static void VL_Present(void)
{
#ifdef __EMSCRIPTEN__
	if (hires_w && hires_valid)
	{
		// the 3D layer, then the game's screen with the view area see-through
		if (SDL_LockTexture(hires_texture, NULL, &hires_argb->pixels,
							&hires_argb->pitch) == 0)
		{
			SDL_BlitSurface(hires_surface, NULL, hires_argb, NULL);
			SDL_UnlockTexture(hires_texture);
		}
		SDL_SetColorKey(sdl_surface, SDL_TRUE, HIRES_KEY);
		if (SDL_LockTexture(texture, NULL, &argbbuffer->pixels,
							&argbbuffer->pitch) == 0)
		{
			SDL_FillRect(argbbuffer, NULL, 0);
			SDL_BlitSurface(sdl_surface, NULL, argbbuffer, NULL);
			SDL_UnlockTexture(texture);
		}
		SDL_SetColorKey(sdl_surface, SDL_FALSE, 0);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, hires_texture, NULL, NULL);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
		return;
	}
#endif
	if (SDL_LockTexture(texture, NULL, &argbbuffer->pixels,
						&argbbuffer->pitch) == 0)
	{
		SDL_BlitSurface(sdl_surface, NULL, argbbuffer, NULL);
		SDL_UnlockTexture(texture);
	}

	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void VH_UpdateScreen(void)
{
	if (StretchScreen)
	{ // bna++
		StretchMemPicture();
	}
	else
	{
		DrawCenterAim();
	}

	VL_Present();
}

/*
=================
=
= XFlipPage
=
=================
*/

void XFlipPage(void)
{
	if (StretchScreen)
	{ // bna++
		StretchMemPicture();
	}
	else
	{
		DrawCenterAim();
	}

	VL_Present();
}

void EnableScreenStretch(void)
{
	if (iGLOBAL_SCREENWIDTH <= 320 || StretchScreen)
		return;
#ifdef __EMSCRIPTEN__
	hires_valid = false; // the 320x200 screens have no 3D layer
#endif

	if (unstretch_sdl_surface == NULL)
	{
		/* should really be just 320x200, but there is code all over the
		   places which crashes then */
		unstretch_sdl_surface = SDL_CreateRGBSurface(
			0, iGLOBAL_SCREENWIDTH, iGLOBAL_SCREENHEIGHT, 8, 0, 0, 0, 0);
	}

	displayofs = (byte *)unstretch_sdl_surface->pixels +
				 (displayofs - (byte *)sdl_surface->pixels);
	SCREEN_BUFFER = bufferofs = unstretch_sdl_surface->pixels;
	StretchScreen = 1;
}

void DisableScreenStretch(void)
{
	if (iGLOBAL_SCREENWIDTH <= 320 || !StretchScreen)
		return;

	displayofs = (byte *)sdl_surface->pixels +
				 (displayofs - (byte *)unstretch_sdl_surface->pixels);
	SCREEN_BUFFER = bufferofs = sdl_surface->pixels;
	StretchScreen = 0;
}

// bna section -------------------------------------------
static void StretchMemPicture()
{
	SDL_Rect src;
	SDL_Rect dest;

	src.x = 0;
	src.y = 0;
	src.w = 320;
	src.h = 200;

	// wide screens: the 320x200 screens (menus, titles) keep 4:3 in the
	// middle, black at the sides
	dest.h = iGLOBAL_SCREENHEIGHT;
	dest.w = iGLOBAL_SCREENHEIGHT * 4 / 3; // a 4:3 box: 640x480, 1440x1080
	if (dest.w > iGLOBAL_SCREENWIDTH)
		dest.w = iGLOBAL_SCREENWIDTH;
	dest.x = (iGLOBAL_SCREENWIDTH - dest.w) / 2;
	dest.y = 0;
	if (dest.x > 0)
	{
		SDL_Rect side = { 0, 0, dest.x, dest.h };
		SDL_FillRect(sdl_surface, &side, 0);
		side.x = dest.x + dest.w;
		side.w = iGLOBAL_SCREENWIDTH - side.x;
		SDL_FillRect(sdl_surface, &side, 0);
	}
	StretchFunc.BlitScaled(unstretch_sdl_surface, &src, sdl_surface, &dest);
}

// bna function added start
extern boolean ingame;
int iG_playerTilt;

void DrawCenterAim()
{
	int x;

	int percenthealth = (locplayerstate->health * 10) /
						MaxHitpointsForCharacter(locplayerstate);
	int color = percenthealth < 3	? egacolor[RED]
				: percenthealth < 4 ? egacolor[YELLOW]
									: egacolor[GREEN];

	if (iG_aimCross && !GamePaused)
	{
		if ((ingame == true) && (iGLOBAL_SCREENWIDTH > 320))
		{
			if ((iG_playerTilt < 0) ||
				(iG_playerTilt > iGLOBAL_SCREENHEIGHT / 2))
			{
				iG_playerTilt = -(2048 - iG_playerTilt);
			}
			if (iGLOBAL_SCREENWIDTH >= 640)
			{
				x = iG_playerTilt;
				iG_playerTilt = x / 2;
			}
			iG_buf_center = bufferofs + ((iG_Y_center - iG_playerTilt) *
										 iGLOBAL_SCREENWIDTH); //+iG_X_center;

			for (x = iG_X_center - 10; x <= iG_X_center - 4; x++)
			{
				if ((iG_buf_center + x < bufofsTopLimit) &&
					(iG_buf_center + x > bufofsBottomLimit))
				{
					*(iG_buf_center + x) = color;
				}
			}
			for (x = iG_X_center + 4; x <= iG_X_center + 10; x++)
			{
				if ((iG_buf_center + x < bufofsTopLimit) &&
					(iG_buf_center + x > bufofsBottomLimit))
				{
					*(iG_buf_center + x) = color;
				}
			}
			for (x = 10; x >= 4; x--)
			{
				if (((iG_buf_center - (x * iGLOBAL_SCREENWIDTH) + iG_X_center) <
					 bufofsTopLimit) &&
					((iG_buf_center - (x * iGLOBAL_SCREENWIDTH) + iG_X_center) >
					 bufofsBottomLimit))
				{
					*(iG_buf_center - (x * iGLOBAL_SCREENWIDTH) + iG_X_center) =
						color;
				}
			}
			for (x = 4; x <= 10; x++)
			{
				if (((iG_buf_center + (x * iGLOBAL_SCREENWIDTH) + iG_X_center) <
					 bufofsTopLimit) &&
					((iG_buf_center + (x * iGLOBAL_SCREENWIDTH) + iG_X_center) >
					 bufofsBottomLimit))
				{
					*(iG_buf_center + (x * iGLOBAL_SCREENWIDTH) + iG_X_center) =
						color;
				}
			}
		}
	}
}
// bna function added end

// bna section -------------------------------------------
