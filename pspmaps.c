/*
 * PSP maps
 * cache local ?
 */

#include <math.h>
#include <stdlib.h>

#include <SDL.h>
#include <SDL_image.h>
#include <curl/curl.h>

#define WIDTH 480
#define HEIGHT 272
#define BPP 32

#ifdef _PSP_FW_VERSION
#include <pspkernel.h>
#define printf pspDebugScreenPrintf
#define MODULE_NAME "PSP-Maps"
PSP_MODULE_INFO(MODULE_NAME, 0, 1, 0);
PSP_HEAP_SIZE_KB(12*1024);
PSP_MAIN_THREAD_ATTR(0);
PSP_MAIN_THREAD_STACK_SIZE_KB(32);
#endif

SDL_Surface *screen;
int x = 1, y = 1, z = 16, s = 0;
CURL* curl;

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

/* downloads the image from Google for location (x,y,z) with mode (s) to file "temp" */
void gettile(int x, int y, int z, int s)
{
	char buffer[1024];
	FILE *fic;
	fic = fopen("temp", "wb");
	switch (s)
	{
		case GG_MAP:
			sprintf(buffer, "http://mt0.google.com/mt?n=404&v=w2.69&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_TERRAIN:
			sprintf(buffer, "http://mt0.google.com/mt?n=404&v=w2p.64&x=%d&y=%d&zoom=%d", x, y, z);
			break;
		case GG_SATELLITE:
			sprintf(buffer, "http://kh0.google.com/kh?n=404&v=20&t=");
			sattile(x, y, z, buffer + strlen(buffer));
			break;
	}
	
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
	curl_easy_setopt(curl, CURLOPT_URL, buffer);
	curl_easy_setopt(curl, CURLOPT_FILE, fic);
	curl_easy_perform(curl);
	
	fclose(fic);
}

/* updates the display */
void display()
{
	SDL_Surface *tmp;
	SDL_Rect r;
	int i, j;
	
	#ifndef _PSP_FW_VERSION
	printf("Display: x=%d y=%d z=%d s=%d\n", x, y, z, s);
	#endif
	
	for (j = -1; j < 1; j++)
		for (i = -1; i < 1; i++)
		{
			r.x = WIDTH/2 + i*256;
			r.y = HEIGHT/2 + j*256;
			gettile(x+i, y+j, z, s);
			tmp = IMG_Load("temp");
			SDL_BlitSurface(tmp, NULL, screen, &r);
			SDL_FreeSurface(tmp);
		}
	
	SDL_Flip(screen);
}

/* main loop */
void go()
{
	int flags, finish = 0;
	SDL_Event event;
	
	curl = curl_easy_init();
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1)
		quit();
	
	SDL_JoystickOpen(0);
	SDL_JoystickEventState(SDL_ENABLE);
	
	flags = SDL_HWSURFACE | SDL_ANYFORMAT | SDL_DOUBLEBUF;
	//flags |= SDL_FULLSCREEN;
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, BPP, flags);
		
	if (screen == NULL)
		quit();
	
	display();
	
	while (!finish)
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				finish = 1;
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
						if (x > 1)
						{
							x--;
							display();
						}
						break;
					case SDLK_RIGHT:
					case PSP_BUTTON_RIGHT:
						if (x < pow(2, 17-z)-1)
						{
							x++;
							display();
						}
						break;
					case SDLK_UP:
					case PSP_BUTTON_UP:
						if (y > 1)
						{
							y--;
							display();
						}
						break;
					case SDLK_DOWN:
					case PSP_BUTTON_DOWN:
						if (y < pow(2, 17-z)-1)
						{
							y++;
							display();
						}
						break;
					case SDLK_PAGEUP:
					case PSP_BUTTON_R:
						if (z > 1 || (s && z > -4))
						{
							z--;
							x*=2;
							y*=2;
							display();
						}
						break;
					case SDLK_PAGEDOWN:
					case PSP_BUTTON_L:
						if (z < 16)
						{
							z++;
							x/=2;
							y/=2;
							display();
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
						display();
						break;
					case SDLK_ESCAPE:
					case PSP_BUTTON_START:
						#ifdef _PSP_FW_VERSION
						sceKernelExitGame();
						#endif
						quit();
						break;
					default:
						break;
				}
				break;
		}
		SDL_Delay(100);
	}
}

#ifdef _PSP_FW_VERSION
/* Connect to an access point */
int connect_to_apctl(int config)
{
	int err;
	int stateLast = -1;

	/* Connect using the first profile */
	err = sceNetApctlConnect(config);
	if (err != 0)
	{
		printf(MODULE_NAME ": sceNetApctlConnect returns %08X\n", err);
		return 0;
	}

	printf(MODULE_NAME ": Connecting...\n");
	while (1)
	{
		int state;
		err = sceNetApctlGetState(&state);
		if (err != 0)
		{
			printf(MODULE_NAME ": sceNetApctlGetState returns $%x\n", err);
			break;
		}
		if (state > stateLast)
		{
			printf("  connection state %d of 4\n", state);
			stateLast = state;
		}
		if (state == 4)
			break;  // connected with static IP

		// wait a little before polling again
		sceKernelDelayThread(50*1000); // 50ms
	}
	printf(MODULE_NAME ": Connected!\n");

	if(err != 0)
	{
		return 0;
	}

	return 1;
}

int net_thread(SceSize args, void *argp)
{
	int err;

	do
	{
		if((err = pspSdkInetInit()))
		{
			printf(MODULE_NAME ": Error, could not initialise the network %08X\n", err);
			break;
		}

		if(connect_to_apctl(1))
		{
			// connected, get my IPADDR and run test
			char szMyIPAddr[32];
			if (sceNetApctlGetInfo(8, szMyIPAddr) != 0)
				strcpy(szMyIPAddr, "unknown IP address");

			go();
		}
	}
	while(0);

	return 0;
}
#endif

int main(void)
{
	#ifdef _PSP_FW_VERSION
	SceUID thid;
	pspDebugScreenInit();
	#endif
	printf(
		"PSP-Maps 0.1\n"
		"http://royale.zerezo.com/psp/\n"
		"\n"
		"This software is using Google Maps images.\n"
		"\n"
	);
	#ifdef _PSP_FW_VERSION
	sceUtilityLoadNetModule(1);
	sceUtilityLoadNetModule(3);
	thid = sceKernelCreateThread("net_thread", net_thread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if(thid < 0)
	{
		printf("Error, could not create thread\n");
		sceKernelSleepThread();
	}
	sceKernelStartThread(thid, 0, NULL);
	sceKernelExitDeleteThread(0);
	#else
	go();
	#endif
	
	return 0;
}
