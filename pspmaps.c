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
#include <sys/stat.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_rotozoom.h>
#include <curl/curl.h>

#define VERSION "0.4"
#define BLANK SDL_MapRGB(screen->format, 0, 0, 0)
#define WIDTH 480
#define HEIGHT 272
#define BPP 32
#define BUFFER_SIZE 100 * 1024
#define MEMORY_CACHE_SIZE 32
#define DISK_CACHE_SIZE 1000
#define DIGITAL_STEP 0.5
#define JOYSTICK_STEP 0.05
#define NUM_FAVORITES 9

#ifdef _PSP_FW_VERSION
#define DEBUG(x...) {}
#else
#define DEBUG(x...) printf(x);
#endif

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
SDL_Surface *logo, *loading, *font, *na;
SDL_Joystick *joystick;
char response[BUFFER_SIZE];
int z = 16, s = 0;
float x = 1, y = 1, dx, dy;
CURL *curl;

/* cache in memory, for recent history and smooth moves */
struct
{
	int x, y, z, s;
	SDL_Surface *tile;
} memory[MEMORY_CACHE_SIZE];
int memory_idx = 0;

/* cache on disk, for offline browsing and to limit requests */
struct
{
	int x, y, z, s;
} disk[DISK_CACHE_SIZE];
int disk_idx = 0;

/* user's configuration */
struct
{
	int use_disk_cache;
	int use_effects;
} config;

/* user's favorite places */
struct
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
	
	/* save disk cache */
	if ((f = fopen("disk.dat", "wb")) != NULL)
	{
		fwrite(&disk_idx, sizeof(disk_idx), 1, f);
		fwrite(disk, sizeof(disk), 1, f);
		fclose(f);
	}
	
	/* save configuration */
	if ((f = fopen("config.dat", "wb")) != NULL)
	{
		fwrite(&config, sizeof(config), 1, f);
		fclose(f);
	}
	
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

/* save tile in memory cache */
void savememory(int x, int y, int z, int s, SDL_Surface *tile)
{
	DEBUG("savememory(%d, %d, %d, %d)\n", x, y, z, s);
	SDL_FreeSurface(memory[memory_idx].tile);
	memory[memory_idx].x = x;
	memory[memory_idx].y = y;
	memory[memory_idx].z = z;
	memory[memory_idx].s = s;
	memory[memory_idx].tile = tile;
	memory_idx = (memory_idx + 1) % MEMORY_CACHE_SIZE;
}

/* save tile in disk cache */
void savedisk(int x, int y, int z, int s, SDL_RWops *rw, int n)
{
	FILE *f;
	char name[50];
	char buffer[BUFFER_SIZE];
	
	if (!config.use_disk_cache) return;
	
	DEBUG("savedisk(%d, %d, %d, %d)\n", x, y, z, s);
	
	if (rw == NULL)
	{
		printf("warning: savedisk(NULL)!\n");
		return;
	}
	
	disk[disk_idx].x = x;
	disk[disk_idx].y = y;
	disk[disk_idx].z = z;
	disk[disk_idx].s = s;
	
	SDL_RWseek(rw, 0, SEEK_SET);
	sprintf(name, "cache/%d.dat", disk_idx);
	if ((f = fopen(name, "wb")) != NULL)
	{
		SDL_RWread(rw, buffer, 1, n);
		fwrite(buffer, 1, n, f);
		fclose(f);
	}
	
	disk_idx = (disk_idx + 1) % DISK_CACHE_SIZE;
}

/* curl callback to save in memory */
size_t curl_write(void *ptr, size_t size, size_t nb, SDL_RWops *rw)
{
	int t = nb * size;
	rw->write(rw, ptr, size, nb);
	return t;
}

/* get the image on internet and return a buffer */
SDL_RWops *getnet(int x, int y, int z, int s)
{
	char request[1024];
	SDL_RWops *rw;
	
	DEBUG("getnet(%d, %d, %d, %d)\n", x, y, z, s);
	
	switch (s)
	{
		case GG_MAP:
			sprintf(request, "http://mt0.google.com/mt?n=404&v=w2.69&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_TERRAIN:
			sprintf(request, "http://mt0.google.com/mt?n=404&v=w2p.64&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_SATELLITE:
			sprintf(request, "http://kh0.google.com/kh?n=404&v=25&t=");
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
	
	return rw;
}

/* return the tile from disk if available, or NULL */
SDL_Surface *getdisk(int x, int y, int z, int s)
{
	int i;
	char name[50];
	DEBUG("getdisk(%d, %d, %d, %d)\n", x, y, z, s);
	for (i = 0; i < DISK_CACHE_SIZE; i++)
		if (disk[i].x == x && disk[i].y == y && disk[i].z == z && disk[i].s == s)
		{
			sprintf(name, "cache/%d.dat", i);
			return IMG_Load(name);
		}
	return NULL;
}

/* return the tile from memory if available, or NULL */
SDL_Surface *getmemory(int x, int y, int z, int s)
{
	int i;
	DEBUG("getmemory(%d, %d, %d, %d)\n", x, y, z, s);
	for (i = 0; i < MEMORY_CACHE_SIZE; i++)
		if (memory[i].tile && memory[i].x == x && memory[i].y == y && memory[i].z == z && memory[i].s == s)
			return memory[i].tile;
	return NULL;
}

/* downloads the image from Google for location (x,y,z) with mode (s) */
SDL_Surface* gettile(int x, int y, int z, int s)
{
	SDL_RWops *rw;
	SDL_Surface *tile;
	int n;
	
	/* try memory cache */
	if ((tile = getmemory(x, y, z, s)) != NULL)
		return tile;
	
	/* try disk cache */
	if ((tile = getdisk(x, y, z, s)) != NULL)
	{
		if (tile == NULL)
			tile = zoomSurface(na, 1, 1, 0);
		savememory(x, y, z, s, tile);
		return tile;
	}
	
	/* try internet */
	rw = getnet(x, y, z, s);
	
	/* load the image */
	n = SDL_RWtell(rw);
	SDL_RWseek(rw, 0, SEEK_SET);
	tile = IMG_Load_RW(rw, 0);
	SDL_RWseek(rw, 0, SEEK_SET);
	
	/* if there is no tile, copy the n/a image
	 * I use a dummy call to zoomSurface to copy the surface
	 * because I had issues with SDL_DisplayFormat() on PSP */
	if (tile == NULL)
		tile = zoomSurface(na, 1, 1, 0);
	/* only save on disk if not n/a
	 * to avoid filling the cache with wrong images
	 * when we are offline */
	else
		savedisk(x, y, z, s, rw, n);
	savememory(x, y, z, s, tile);
	
	SDL_RWclose(rw);
	
	return tile;
}

/* fx for transition between prev and next screen */
void effect(int fx)
{
	SDL_Surface *tmp;
	SDL_Rect r;
	int i;
	float t;
	
	if (!config.use_effects) return;
	
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
	SDL_Surface *tile;
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
	
	/* check if everything is in memory cache */
	ok = 1;
	for (j = y-1; j < y+1; j++)
		for (i = x-1; i < x+1; i++)
			if (!getmemory(i, j, z, s))
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
			tile = gettile(i, j, z, s);
			SDL_BlitSurface(tile, NULL, next, &r);
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
	int action, active = 0, fav = 0;
	#define MENU_OPTIONS 7

	void update()
	{
		char temp[50];
		SDL_Rect pos;
		SDL_FillRect(next, NULL, BLANK);
		pos.x = 70;
		pos.y = 0;
		SDL_BlitSurface(logo, NULL, next, &pos);
		print(next, 280, 30, "version " VERSION);
		print(next, 120, HEIGHT/(MENU_OPTIONS+4)*(3+active), ">");
		sprintf(temp, "Load favorite: %d", fav+1);
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*3, temp);
		sprintf(temp, "Save favorite: %d", fav+1);
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*4, temp);
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*5, "Default view");
		sprintf(temp, "Disk cache: %s", config.use_disk_cache ? "Yes" : "No");
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*6, temp);
		sprintf(temp, "Transition effects: %s", config.use_effects ? "Yes" : "No");
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*7, temp);
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*8, "Exit menu");
		print(next, 140, HEIGHT/(MENU_OPTIONS+4)*9, "Quit PSP-Maps");
		print(next, 90, HEIGHT/(MENU_OPTIONS+4)*10.4, "http://royale.zerezo.com/psp/");
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
						action = event.key.keysym.sym;
					else
						action = event.jbutton.button;
					switch (action)
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
								/* disk cache */
								case 3:
									config.use_disk_cache = !config.use_disk_cache;
									break;
								/* effects */
								case 4:
									config.use_effects = !config.use_effects;
									break;
								/* exit menu */
								case 5:
									return;
								/* quit PSP-Maps */
								case 6:
									quit();
							}
							update();
							break;
						case SDLK_LEFT:
						case PSP_BUTTON_LEFT:
						case PSP_BUTTON_L:
							switch (active)
							{
								/* favorites */
								case 0:
								case 1:
									fav--;
									if (fav < 0) fav = NUM_FAVORITES-1;
									break;
								/* disk cache */
								case 3:
									config.use_disk_cache = !config.use_disk_cache;
									break;
								/* effects */
								case 4:
									config.use_effects = !config.use_effects;
									break;
							}
							update();
							break;
						case SDLK_RIGHT:
						case PSP_BUTTON_RIGHT:
						case PSP_BUTTON_R:
							switch (active)
							{
								/* favorites */
								case 0:
								case 1:
									fav++;
									if (fav > NUM_FAVORITES-1) fav = 0;
									break;
								/* disk cache */
								case 3:
									config.use_disk_cache = !config.use_disk_cache;
									break;
								/* effects */
								case 4:
									config.use_effects = !config.use_effects;
									break;
							}
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

/* init */
void init()
{
	int flags;
	FILE *f;
	
	/* clear cache */
	bzero(memory, sizeof(memory));
	
	/* load disk cache if available */
	bzero(disk, sizeof(disk));
	if ((f = fopen("disk.dat", "rb")) != NULL)
	{
		fread(&disk_idx, sizeof(disk_idx), 1, f);
		fread(disk, sizeof(disk), 1, f);
		fclose(f);
	}
	
	/* create disk cache directory if needed */
	mkdir("cache", 0755);
	
	/* default options */
	config.use_disk_cache = 1;
	config.use_effects = 1;
	
	/* load configuration if available */
	if ((f = fopen("config.dat", "rb")) != NULL)
	{
		fread(&config, sizeof(config), 1, f);
		fclose(f);
	}
	
	/* load favorites if available */
	bzero(favorite, sizeof(favorite));
	if ((f = fopen("favorite.dat", "rb")) != NULL)
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
	na = IMG_Load("na.png");
	
	/* display initial map */
	display(FX_FADE);
}

/* main loop */
void loop()
{
	int action;
	SDL_Event event;
	
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
						action = event.key.keysym.sym;
					else
						action = event.jbutton.button;
					switch (action)
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
	init();
	loop();
	return 0;
}
