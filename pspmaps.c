/*
 * PSP maps
 * cache local ?
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_image.h>
#include <curl/curl.h>

#define WIDTH 480
#define HEIGHT 272
#define BPP 32
#define BUFFER_SIZE 100 * 1024
#define CACHE_SIZE 32
#define DIGITAL_STEP 0.1
#define JOYSTICK_STEP 0.05

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
#include "netdialog.c"
#else
static int running = 1;
#endif

SDL_Surface *screen;
SDL_Surface *loading;
int z = 16, s = 0;
float x = 1, y = 1;
CURL* curl;

struct _cache
{
	int loaded;
	int x, y, z, s;
	SDL_Surface *tile;
} cache[CACHE_SIZE];
int cur = 0;

enum
{
	GG_MAP,		// zoom =  1 .. 16
	GG_SATELLITE,	// zoom = -4 .. 16
	GG_TERRAIN,	// zoom =  1 .. 16
	GG_NUM_VIEWS
};

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

void quit()
{
	SDL_Quit();
	curl_easy_cleanup(curl);
	exit(0);
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

size_t curl_write(void *ptr, size_t size, size_t nb, SDL_RWops *rw)
{
	int t = nb * size;
	rw->write(rw, ptr, size, nb);
	return t;
}

/* downloads the image from Google for location (x,y,z) with mode (s) to file "temp" */
SDL_Surface* gettile(int x, int y, int z, int s)
{
	char request[1024], response[BUFFER_SIZE];
	SDL_RWops *rw;
	SDL_Surface *tmp;
	
	int i;
	for (i = 0; i < CACHE_SIZE; i++)
		if (cache[i].loaded && cache[i].x == x && cache[i].y == y && cache[i].z == z && cache[i].s == s)
			return cache[i].tile;
	
	SDL_Rect r;
	r.x = 160;
	r.y = 110;
	SDL_BlitSurface(loading, NULL, screen, &r);
	SDL_Flip(screen);
	
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
	
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
	curl_easy_setopt(curl, CURLOPT_URL, request);
	//curl_easy_setopt(curl, CURLOPT_FILE, fic);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, rw);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_perform(curl);
	
	rw->seek(rw, 0, SEEK_SET);
	tmp = IMG_Load_RW(rw, 1);
	
	if (cache[cur].loaded) SDL_FreeSurface(cache[cur].tile);
	cache[cur].loaded = 1;
	cache[cur].x = x;
	cache[cur].y = y;
	cache[cur].z = z;
	cache[cur].s = s;
	cache[cur].tile = tmp;
	cur = (cur + 1) % CACHE_SIZE;
	
	return tmp;
}

/* updates the display */
void display()
{
	SDL_Surface *tmp;
	SDL_Rect r;
	int i, j;
	
	if (x < 1) x = 1;
	if (x > pow(2, 17-z)-1) x = pow(2, 17-z)-1;
	if (y < 1) y = 1;
	if (y > pow(2, 17-z)-1) y = pow(2, 17-z)-1;
	
	// pre-load the tiles
	for (j = y-1; j < y+1; j++)
		for (i = x-1; i < x+1; i++)
		{
			//~ r.x = WIDTH/2 + (i-x)*256;
			//~ r.y = HEIGHT/2 + (j-y)*256;
			tmp = gettile(i, j, z, s);
			//~ SDL_BlitSurface(tmp, NULL, screen, &r);
		}
	
	// display the tiles
	for (j = y-1; j < y+1; j++)
		for (i = x-1; i < x+1; i++)
		{
			r.x = WIDTH/2 + (i-x)*256;
			r.y = HEIGHT/2 + (j-y)*256;
			tmp = gettile(i, j, z, s);
			SDL_BlitSurface(tmp, NULL, screen, &r);
		}
	
	SDL_Flip(screen);
}

/* main loop */
void mainLoop()
{
	int flags;
	SDL_Event event;
	SDL_Joystick *joystick;
	
	bzero(cache, sizeof(cache));
	
	curl = curl_easy_init();
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1)
		quit();
	
	joystick = SDL_JoystickOpen(0);
	SDL_JoystickEventState(SDL_ENABLE);
	
	flags = SDL_HWSURFACE | SDL_ANYFORMAT | SDL_DOUBLEBUF;
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, BPP, flags);
	
	if (screen == NULL)
		quit();
	
	loading = IMG_Load("loading.png");
	
	//display();
	
	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					running = 0;
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
							//display();
							break;
						case SDLK_RIGHT:
						case PSP_BUTTON_RIGHT:
							x += DIGITAL_STEP;
							//display();
							break;
						case SDLK_UP:
						case PSP_BUTTON_UP:
							y -= DIGITAL_STEP;
							//display();
							break;
						case SDLK_DOWN:
						case PSP_BUTTON_DOWN:
							y += DIGITAL_STEP;
							//display();
							break;
						case SDLK_PAGEUP:
						case PSP_BUTTON_R:
							if (z > 1 || (s && z > -4))
							{
								z--;
								x*=2;
								y*=2;
								//display();
							}
							break;
						case SDLK_PAGEDOWN:
						case PSP_BUTTON_L:
							if (z < 16)
							{
								z++;
								x/=2;
								y/=2;
								//display();
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
							//display();
							break;
						case SDLK_ESCAPE:
						case PSP_BUTTON_START:
							running = 0;
							break;
						default:
							break;
					}
					break;
			}
		}
		
		x += SDL_JoystickGetAxis(joystick, 0) / (32768/16) / 16.0 * JOYSTICK_STEP;
		y += SDL_JoystickGetAxis(joystick, 1) / (32768/16) / 16.0 * JOYSTICK_STEP;
		
		display();
		SDL_Delay(50);
	}
}

int main(void)
{
	#ifdef _PSP_FW_VERSION
	pspDebugScreenInit();
	#endif
	printf(
		"\n"
		"\n"
		"\n"
		"        _____   _____ _____        __  __                     \n"
		"       |  __ \\ / ____|  __ \\      |  \\/  |                    \n"
		"       | |__) | (___ | |__) |_____| \\  / | __ _ _ __  ___     \n"
		"       |  ___/ \\___ \\|  ___/______| |\\/| |/ _` | '_ \\/ __|    \n"
		"       | |     ____) | |          | |  | | (_| | |_) \\__ \\    \n"
		"       |_|    |_____/|_|          |_|  |_|\\__,_| .__/|___/    \n"
		"                                               | |            \n"
		"                                               |_|            \n"
		"\n"
		"\n"
		"           PSP-Maps 0.2\n"
		"\n"
		"           http://royale.zerezo.com/psp/\n"
		"\n"
		"\n"
		"           This software is using Google Maps images.\n"
	);
	SDL_Delay(3000);
	#ifdef _PSP_FW_VERSION
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	netInit();
	SetupCallbacks();
	setupGu();
	netDialog();
	mainLoop();
	sceKernelExitGame();
	netTerm();
	#else
	mainLoop();
	#endif
	return 0;
}
