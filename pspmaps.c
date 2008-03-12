/*
 * PSP-Maps
 * A homebrew to browse Google Maps with your PSP!
 *
 * Copyright (C) 2008  Antoine Jacquet <royale@zerezo.com>
 * http://royale.zerezo.com/psp/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_rotozoom.h>
#include <curl/curl.h>

#define VERSION "0.3"
#define BLANK SDL_MapRGB(screen->format, 0, 0, 0)
#define WIDTH 480
#define HEIGHT 272
#define BPP 32
#define BUFFER_SIZE 100 * 1024
#define CACHE_SIZE 32
#define DIGITAL_STEP 0.5
#define JOYSTICK_STEP 0.05
#define NUM_FAVORITES 9

#ifdef _PSP_FW_VERSION
#include <pspkernel.h>
#include <pspsdk.h>
#include <psputility.h>
#include <pspnet_apctl.h>
#define printf pspDebugScreenPrintf
#define MODULE_NAME "PSP-Maps"
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(20480);
void quit();
#include "netdialog.c"
#endif

SDL_Surface *screen, *prev, *next;
SDL_Surface *logo, *loading, *font;
int z = 16, s = 0;
float x = 1, y = 1, dx, dy;
CURL *curl;

/* cache in RAM, for recent history and smooth moves */
struct _cache
{
	int x, y, z, s;
	SDL_Surface *tile;
} cache[CACHE_SIZE];
int cur = 0;

/* user's favorite places */
struct _favorite
{
	float x, y;
	int z, s;
	char name[50];
} favorite[NUM_FAVORITES];

/* Google Maps images type */
enum
{
	GG_MAP,		/* zoom =  1 .. 16 */
	GG_SATELLITE,	/* zoom = -4 .. 16 */
	GG_TERRAIN,	/* zoom =  1 .. 16 */
	GG_NUM_VIEWS
};

/* PSP buttons list */
enum
{
	PSP_BUTTON_Y,
	PSP_BUTTON_B,
	PSP_BUTTON_A,
	PSP_BUTTON_X,
	PSP_BUTTON_L,
	PSP_BUTTON_R,
	PSP_BUTTON_DOWN,
	PSP_BUTTON_LEFT,
	PSP_BUTTON_UP,
	PSP_BUTTON_RIGHT,
	PSP_BUTTON_SELECT,
	PSP_BUTTON_START,
	PSP_NUM_BUTTONS
};

/* transition effects */
enum
{
	FX_NONE,
	FX_LEFT,
	FX_RIGHT,
	FX_UP,
	FX_DOWN,
	FX_IN,
	FX_OUT,
	FX_FADE,
	FX_NUM
};

/* quit */
void quit()
{
	FILE *f;
	
	/* save favorites */
	if ((f = fopen("favorite.dat", "wb")) != NULL)
	{
		fwrite(favorite, sizeof(favorite), 1, f);
		fclose(f);
	}
	
	/* quit SDL and curl */
	SDL_FreeSurface(prev);
	SDL_FreeSurface(next);
	SDL_Quit();
	curl_easy_cleanup(curl);
	
	/* boom */
	#ifdef _PSP_FW_VERSION
	sceKernelExitGame();
	#else
	printf("quit!\n");
	exit(0);
	#endif
}

/* returns in buffer "b" the name of the satellite tile for location (x,y,z) */
void sattile(int x, int y, int z, char *b)
{
	int c = 18 - z;
	b[c] = '\0';
	while (z++ < 17)
	{
		c--;
		if (x % 2)
		{
			if (y % 2)
				b[c] = 's';
			else
				b[c] = 'r';
		}
		else
		{
			if (y % 2)
				b[c] = 't';
			else
				b[c] = 'q';
		}
		x/=2;
		y/=2;
	}
	b[0] = 't';
}

/* curl callback to save in memory */
size_t curl_write(void *ptr, size_t size, size_t nb, SDL_RWops *rw)
{
	int t = nb * size;
	rw->write(rw, ptr, size, nb);
	return t;
}

/* return the tile from cache if available, or NULL */
SDL_Surface *getcache(int x, int y, int z, int s)
{
	int i;
	for (i = 0; i < CACHE_SIZE; i++)
		if (cache[i].tile && cache[i].x == x && cache[i].y == y && cache[i].z == z && cache[i].s == s)
			return cache[i].tile;
	return NULL;
}

/* downloads the image from Google for location (x,y,z) with mode (s) */
SDL_Surface* gettile(int x, int y, int z, int s)
{
	char request[1024], response[BUFFER_SIZE];
	SDL_RWops *rw;
	SDL_Surface *tmp;
	
	if ((tmp = getcache(x, y, z, s)) != NULL)
		return tmp;
	
	switch (s)
	{
		case GG_MAP:
			sprintf(request, "http://mt0.google.com/mt?n=404&v=w2.69&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_TERRAIN:
			sprintf(request, "http://mt0.google.com/mt?n=404&v=w2p.64&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_SATELLITE:
			sprintf(request, "http://kh0.google.com/kh?n=404&v=20&t=");
			sattile(x, y, z, request + strlen(request));
			break;
	}
	
	rw = SDL_RWFromMem(response, BUFFER_SIZE);
	
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
	curl_easy_setopt(curl, CURLOPT_URL, request);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, rw);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
	curl_easy_perform(curl);
	
	rw->seek(rw, 0, SEEK_SET);
	tmp = IMG_Load_RW(rw, 1);
	
	SDL_FreeSurface(cache[cur].tile);
	cache[cur].x = x;
	cache[cur].y = y;
	cache[cur].z = z;
	cache[cur].s = s;
	cache[cur].tile = tmp;
	cur = (cur + 1) % CACHE_SIZE;
	
	return tmp;
}

/* fx for transition between prev and next screen */
void effect(int fx)
{
	SDL_Surface *tmp;
	SDL_Rect r;
	int i;
	float t;
	
	/* effects */
	switch (fx)
	{
		case FX_IN:
			t = 1.0;
			while (t < 2)
			{
				tmp = zoomSurface(prev, t, t, 0);
				r.x = -WIDTH/2 * (t-1);
				r.y = -HEIGHT/2 * (t-1);
				SDL_BlitSurface(tmp, NULL, screen, &r);
				SDL_FreeSurface(tmp);
				SDL_Flip(screen);
				t += 0.1;
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_OUT:
			t = 2.0;
			while (t > 1)
			{
				tmp = zoomSurface(next, t, t, 0);
				r.x = -WIDTH/2 * (t-1);
				r.y = -HEIGHT/2 * (t-1);
				SDL_BlitSurface(tmp, NULL, screen, &r);
				SDL_FreeSurface(tmp);
				SDL_Flip(screen);
				t -= 0.1;
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_FADE:
			for (i = 0; i < 255; i+=10)
			{
				tmp = zoomSurface(next, 1, 1, 0);
				SDL_SetAlpha(tmp, SDL_SRCALPHA, i);
				SDL_BlitSurface(prev, NULL, screen, NULL);
				SDL_BlitSurface(tmp, NULL, screen, NULL);
				SDL_FreeSurface(tmp);
				SDL_Flip(screen);
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_LEFT:
			r.y = 0;
			for (i = 0; i < DIGITAL_STEP * 256; i+=5)
			{
				r.x = i;
				SDL_BlitSurface(prev, NULL, screen, &r);
				r.x = DIGITAL_STEP * 256 - i;
				SDL_BlitSurface(next, &r, screen, NULL);
				SDL_Flip(screen);
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_RIGHT:
			r.y = 0;
			for (i = 0; i < DIGITAL_STEP * 256; i+=5)
			{
				r.x = i;
				SDL_BlitSurface(prev, &r, screen, NULL);
				r.x = DIGITAL_STEP * 256 - i;
				SDL_BlitSurface(next, NULL, screen, &r);
				SDL_Flip(screen);
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_UP:
			r.x = 0;
			for (i = 0; i < DIGITAL_STEP * 256; i+=5)
			{
				r.y = i;
				SDL_BlitSurface(prev, NULL, screen, &r);
				r.y = DIGITAL_STEP * 256 - i;
				SDL_BlitSurface(next, &r, screen, NULL);
				SDL_Flip(screen);
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
		case FX_DOWN:
			r.x = 0;
			for (i = 0; i < DIGITAL_STEP * 256; i+=5)
			{
				r.y = i;
				SDL_BlitSurface(prev, &r, screen, NULL);
				r.y = DIGITAL_STEP * 256 - i;
				SDL_BlitSurface(next, NULL, screen, &r);
				SDL_Flip(screen);
				#ifndef _PSP_FW_VERSION
				SDL_Delay(20);
				#endif
			}
			break;
	}
}

/* updates the display */
void display(int fx)
{
	SDL_Surface *tmp;
	SDL_Rect r;
	int i, j, ok;
	
	/* fix the bounds
	 * disable the special effect to avoid map jumps */
	if (x < 1) { x = 1; fx = FX_NONE; }
	if (x > pow(2, 17-z)-1) { x = pow(2, 17-z)-1; fx = FX_NONE; }
	if (y < 1) { y = 1; fx = FX_NONE; }
	if (y > pow(2, 17-z)-1) { y = pow(2, 17-z)-1; fx = FX_NONE; }
	
	/* save the old screen */
	SDL_BlitSurface(next, NULL, prev, NULL);
	
	/* check if everything is in cache */
	ok = 1;
	for (j = y-1; j < y+1; j++)
		for (i = x-1; i < x+1; i++)
			if (!getcache(i, j, z, s))
				ok = 0;
	
	/* if not, display loading notice */
	if (!ok)
	{
		r.x = 160;
		r.y = 110;
		SDL_BlitSurface(prev, NULL, screen, NULL);
		SDL_BlitSurface(loading, NULL, screen, &r);
		SDL_Flip(screen);
	}
	
	/* build the new screen */
	for (j = y-1; j < y+1; j++)
		for (i = x-1; i < x+1; i++)
		{
			r.x = WIDTH/2 + (i-x)*256;
			r.y = HEIGHT/2 + (j-y)*256;
			tmp = gettile(i, j, z, s);
			SDL_BlitSurface(tmp, NULL, next, &r);
		}
	
	/* nicer transition */
	effect(fx);
	
	/* restore the good screen */
	SDL_BlitSurface(next, NULL, screen, NULL);
	SDL_Flip(screen);
}

/* prints a message using the bitmap font */
void print(SDL_Surface *dst, int x, int y, char *text)
{
	SDL_Rect srcpos, dstpos;
	/* on the first call, load the font picture */
	while (*text != '\0')
	{
		srcpos.x = (*text-' ') % 16 * 10;
		srcpos.y = (*text-' ') / 16 * 19;
		srcpos.w = 10;
		srcpos.h = 19;
		dstpos.x = x;
		dstpos.y = y;
		SDL_BlitSurface(font, &srcpos, dst, &dstpos);
		x += 10;
		text++;
	}
}

/* menu to load/save favorites */
void menu()
{
	SDL_Event event;
	int flags, active = 0, fav = 0;
	#define MENU_OPTIONS 5

	void update()
	{
		char temp[20];
		SDL_Rect pos;
		SDL_FillRect(next, NULL, BLANK);
		pos.x = 70;
		pos.y = 0;
		SDL_BlitSurface(logo, NULL, next, &pos);
		print(next, 280, 30, "version " VERSION);
		print(next, 120, HEIGHT/(MENU_OPTIONS+5)*(3+active), ">");
		sprintf(temp, "Load favorite: %d", fav+1);
		print(next, 140, HEIGHT/(MENU_OPTIONS+5)*3, temp);
		sprintf(temp, "Save favorite: %d", fav+1);
		print(next, 140, HEIGHT/(MENU_OPTIONS+5)*4, temp);
		print(next, 140, HEIGHT/(MENU_OPTIONS+5)*5, "Default view");
		print(next, 140, HEIGHT/(MENU_OPTIONS+5)*6, "Exit menu");
		print(next, 140, HEIGHT/(MENU_OPTIONS+5)*7, "Quit PSP-Maps");
		print(next, 90, HEIGHT/(MENU_OPTIONS+5)*9, "http://royale.zerezo.com/psp/");
		SDL_BlitSurface(next, NULL, screen, NULL);
		SDL_Flip(screen);
	}
	
	update();
	for (;;)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					quit();
					break;
				case SDL_KEYDOWN:
				case SDL_JOYBUTTONDOWN:
					if (event.type == SDL_KEYDOWN)
						flags = event.key.keysym.sym;
					else
						flags = event.jbutton.button;
					switch (flags)
					{
						case SDLK_ESCAPE:
						case PSP_BUTTON_START:
							return;
						case SDLK_RETURN:
						case SDLK_SPACE:
						case PSP_BUTTON_A:
						case PSP_BUTTON_B:
						case PSP_BUTTON_X:
						case PSP_BUTTON_Y:
							switch (active)
							{
								/* load favorite */
								case 0:
									if (favorite[fav].z)
									{
										x = favorite[fav].x;
										y = favorite[fav].y;
										z = favorite[fav].z;
										s = favorite[fav].s;
									}
									return;
								/* save favorite */
								case 1:
									favorite[fav].x = x;
									favorite[fav].y = y;
									favorite[fav].z = z;
									favorite[fav].s = s;
									return;
								/* default view */
								case 2:
									x = 1;
									y = 1;
									z = 16;
									s = 0;
									return;
								/* exit menu */
								case 3:
									return;
								/* quit PSP-Maps */
								case 4:
									quit();
							}
							update();
							break;
						case SDLK_LEFT:
						case PSP_BUTTON_LEFT:
						case PSP_BUTTON_L:
							fav--;
							if (fav < 0) fav = NUM_FAVORITES-1;
							update();
							break;
						case SDLK_RIGHT:
						case PSP_BUTTON_RIGHT:
						case PSP_BUTTON_R:
							fav++;
							if (fav > NUM_FAVORITES-1) fav = 0;
							update();
							break;
						case SDLK_UP:
						case PSP_BUTTON_UP:
							active--;
							if (active < 0) active = MENU_OPTIONS-1;
							update();
							break;
						case SDLK_DOWN:
						case PSP_BUTTON_DOWN:
							active++;
							if (active > MENU_OPTIONS-1) active = 0;
							update();
							break;
						default:
							break;
					}
					break;
			}
		}
		SDL_Delay(10);
	}
}

/* init and main loop */
void go()
{
	int flags;
	SDL_Event event;
	SDL_Joystick *joystick;
	FILE *f;
	
	/* clear cache */
	bzero(cache, sizeof(cache));
	
	/* load favorites if available */
	bzero(favorite, sizeof(favorite));
	if ((f = fopen("favorite.dat", "r")) != NULL)
	{
		fread(favorite, sizeof(favorite), 1, f);
		fclose(f);
	}	
	
	/* setup curl */
	curl = curl_easy_init();
	
	/* setup SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1)
		quit();
	joystick = SDL_JoystickOpen(0);
	SDL_JoystickEventState(SDL_ENABLE);
	
	/* setup screen */
	flags = SDL_HWSURFACE | SDL_ANYFORMAT | SDL_DOUBLEBUF;
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, BPP, flags);
	SDL_FillRect(screen, NULL, BLANK);
	prev = zoomSurface(screen, 1, 1, 0);
	next = zoomSurface(screen, 1, 1, 0);
	if (screen == NULL)
		quit();
	
	/* load textures */
	logo = IMG_Load("logo.png");
	loading = IMG_Load("loading.png");
	font = IMG_Load("font.png");
	
	/* display initial map */
	display(FX_FADE);
	
	/* main loop */
	while (1)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					quit();
					break;
				case SDL_KEYDOWN:
				case SDL_JOYBUTTONDOWN:
					if (event.type == SDL_KEYDOWN)
						flags = event.key.keysym.sym;
					else
						flags = event.jbutton.button;
					switch (flags)
					{
						case SDLK_LEFT:
						case PSP_BUTTON_LEFT:
							x -= DIGITAL_STEP;
							display(FX_LEFT);
							break;
						case SDLK_RIGHT:
						case PSP_BUTTON_RIGHT:
							x += DIGITAL_STEP;
							display(FX_RIGHT);
							break;
						case SDLK_UP:
						case PSP_BUTTON_UP:
							y -= DIGITAL_STEP;
							display(FX_UP);
							break;
						case SDLK_DOWN:
						case PSP_BUTTON_DOWN:
							y += DIGITAL_STEP;
							display(FX_DOWN);
							break;
						case SDLK_PAGEUP:
						case PSP_BUTTON_R:
							if (z > 1 || (s && z > -4))
							{
								z--;
								x*=2;
								y*=2;
								display(FX_IN);
							}
							break;
						case SDLK_PAGEDOWN:
						case PSP_BUTTON_L:
							if (z < 16)
							{
								z++;
								x/=2;
								y/=2;
								display(FX_OUT);
							}
							break;
						case SDLK_SPACE:
						case PSP_BUTTON_A:
						case PSP_BUTTON_B:
						case PSP_BUTTON_X:
						case PSP_BUTTON_Y:
							s = (s + 1) % GG_NUM_VIEWS;
							/* zoom out if needed */
							while (z < 1)
							{
								x/=2;
								y/=2;
								z++;
							}
							display(FX_FADE);
							break;
						case SDLK_ESCAPE:
						case PSP_BUTTON_START:
							menu();
							display(FX_FADE);
							break;
						default:
							break;
					}
					break;
			}
		}
		
		dx = SDL_JoystickGetAxis(joystick, 0) / (32768/16) / 16.0 * JOYSTICK_STEP;
		x += dx;
		dy = SDL_JoystickGetAxis(joystick, 1) / (32768/16) / 16.0 * JOYSTICK_STEP;
		y += dy;
		if (dx || dy) display(FX_NONE);
		
		SDL_Delay(50);
	}
}

int main(void)
{
	#ifdef _PSP_FW_VERSION
	pspDebugScreenInit();
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	netInit();
	SetupCallbacks();
	setupGu();
	netDialog();
	#endif
	go();
	return 0;
}
