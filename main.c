#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"

char *index1, *index2, *index3;  //  =>  (Index1)<text>(Index2)value(Index3)</text>
char *name[22], *maxt[22][3], *mint[22][3], *wx[22][3], *ci[22][3], *pop[22][3]; // 22 cities
int ds18b20_fd;
char *ds18b20_str;
// ==================================================================================

SDL_Surface *screen=0, *intro=0, *cover=0, *background=0, *block[21];
SDL_Rect r, r2, src; // r2 for image
TTF_Font *font=0;
SDL_Color fg={0,0,0,255};

// ==================================================================================

// convert xml file to a string
char *file_buffer(char *filename, int *sizeptr)
{
	FILE *fp = fopen(filename, "rb");
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	rewind(fp);

	char *buffer = (char *)malloc(size+1);
	size_t status;
	if ((status=fread(buffer, size, 1, fp))<1) {
		fprintf(stderr, "fread error");
		exit(1);
	}
	fclose(fp);

	return buffer;
}

void innertext(char *inner, char *ptext, char *begintag, char *endtag)
{
	int len;
	index1 = strstr(ptext, begintag);
//	if (beginStart==NULL) return NULL;
	index2 = index1 + strlen(begintag);
	index3 = strstr(index2, endtag);
//	if(endStart<0) return NULL;
	len = index3 - index2;
	strncpy(inner, index2, len);
	inner[len] = '\0';
}


void parse_xml()
{
	int size;
	char *xmlfile = "F-C0032-001.xml"; 
	char *xml = file_buffer(xmlfile, &size);

	char tmptitle[30];	
	innertext(tmptitle, xml, "<fifowml", "zh-TW"); // get index3 address
//	printf("%p\n", beginEnd); // confirm address
//	printf("len= %d\n", len);

	char tmpname[30], tmpwx[30], tmpmaxt[3], tmpmint[3], tmpci[30], tmppop[3];
	int m, n;
	for (m=0; m<22; m++) {
		innertext(tmpname, index3, "<name>", "</name>");
		name[m] = strdup(tmpname);
		for (n=0; n<3; n++) {
			innertext(tmpwx, index3, "<text>", "</text>");
			wx[m][n] = strdup(tmpwx);
		}
		for (n=0; n<3; n++) {
			innertext(tmpmaxt, index3, "<value units=\"C\">", "</value>");
			maxt[m][n] = strdup(tmpmaxt);
		}
		for (n=0; n<3; n++) {
			innertext(tmpmint, index3, "<value units=\"C\">", "</value>");
			mint[m][n] = strdup(tmpmint);
		}
		for (n=0; n<3; n++) {
			innertext(tmpci, index3, "<text>", "</text>");
			ci[m][n] = strdup(tmpci);
		}
		for (n=0; n<3; n++) {
			innertext(tmppop, index3, "<value units=\"%\">", "</value>");
			pop[m][n] = strdup(tmppop);
		}
	}
}

// ===================================================================================


void free_font()
{
	if (font)
		TTF_CloseFont(font);
	font = 0;
}

void load_font()
{
	free_font();
	font = TTF_OpenFont("msjhbd.ttf", 20);
	if(!font) {
		fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
		exit(3);
	}
}

int get_hour()
{
	time_t timep;
	struct tm *p;
	time(&timep);
	p = localtime(&timep);

	return p->tm_hour;
}

void ask_ds18b20()
{
	unsigned char buf[2];
	char temp_str[3];
	float result;

	size_t ret = read(ds18b20_fd, buf, 1);
	if (ret!=-1) {
		result = (float)buf[0];
		result /= 16;
		result += ((float)buf[1]*16);			
		ds18b20_str = gcvt(result, 3, temp_str);
	}
	printf("\nds18b20: %s", ds18b20_str);
}

// SDL procedure
#define SPACE 50
#define WIDTH 140
int display(int page)
{
	SDL_FillRect(screen, 0, ~0);

	background = IMG_Load("F-C0035-001.jpg");
	if (background) {
		r2.x=36, r2.y=0;
		SDL_BlitSurface(background, 0, screen, &r2);
		SDL_FreeSurface(background);
		SDL_Flip(screen);
	}
	char *items[] = {"溫度 (℃)", "天氣狀況", "舒適度", "降雨機率 (%)"};
	char *period[] = {"今日白天", "今晚至明晨", "明日白天", "明日晚上"};
	char *text[21];

	r.y = 120;
	int now = get_hour();
	int i;
	for (i=0; i<21; i++) {
		
		if (i!=0 && i<20) {
			if (i<5)
				text[i] = strdup(items[i-1]);
			else if (!(i%5)) {
				r.y += SPACE;
				if (now<18)
					text[i] = strdup(period[(i/5)-1]);
				else
					text[i] = strdup(period[i/5]);
			}
			else if (!((i-1)%5)) {
				char tmptemp[8] = {0};
				strcat(tmptemp, strdup(mint[page][(i-6)/5]));
				strcat(tmptemp, " ~ ");
				strcat(tmptemp, strdup(maxt[page][(i-6)/5])); 
				text[i] = strdup(tmptemp);
			}
			else if (!((i-2)%5)) {
				text[i] = strdup(wx[page][(i-7)/5]);
			}
			else if (!((i-3)%5))
				text[i] = strdup(ci[page][(i-8)/5]);
			else// (!((i-4)%5))
				text[i] = strdup(pop[page][(i-9)/5]);
		}
		else if (i==20) {
			r.y += SPACE;
			char ds18b20_tmp[30] = {0};
			strcat(ds18b20_tmp, "DS18B20溫度感測:  ");
			if (ds18b20_fd==-1)
				strcat(ds18b20_tmp, "null");
			else {
				ask_ds18b20();
				strcat(ds18b20_tmp, ds18b20_str);
				strcat(ds18b20_tmp, " ℃");
			}
			text[i] = strdup(ds18b20_tmp);
		}
		else
			text[i] = strdup(name[page]);

		// block[i] r.x location
		if (!((i-4)%5)) {
			if (i==4) r.x = ((i+5)%5) * WIDTH + 70;
			else r.x = ((i+5)%5) * WIDTH + 110;
		}
		else if (!((i-2)%5))
			r.x = ((i+5)%5) * WIDTH + 30;
		else
			r.x = ((i+5)%5) * WIDTH + 50;

		block[i] = TTF_RenderUTF8_Blended(font, text[i], fg);
		if (block[i]) {
			SDL_BlitSurface(block[i], 0, screen, &r);
			SDL_FreeSurface(block[i]);
		}
	}

	SDL_Flip(screen);

	int j;
	for (j=0; j<21; j++) {
		if (text[j])
			free(text[j]);
		text[j]=0;
	}

	if (ds18b20_fd!=-1)
		SDL_Delay(750); // for ds18b20

	return 0;
}

int download_file()
{
	char *cmd[] = { "wget opendata.cwb.gov.tw/opendata/MFC/F-C0032-001.xml -O F-C0032-001.xml",
		"wget opendata.cwb.gov.tw/opendata/MFC/F-C0035-001.jpg -O F-C0035-001.jpg",
		"wget opendata.cwb.gov.tw/opendata/MSC/O-B0032-002.jpg -O O-B0032-002.jpg"};
	int status[3], ret[3], R;

	for (R=0; R<3; R++) {
		status[R] = system(cmd[R]);
		ret[R] = WEXITSTATUS(status[R]);
		if (ret[R] != 0)
			return 1;
	}

	return 0;
}

int main()
{
	int ret = download_file();
	if (ret != 0)
		return 1;

	int done = 0, pages = 0;

	memset(block,0,sizeof(block));

//	SDL_ShowCursor(SDL_DISABLE); // disable mouse cursor

	if(SDL_Init(SDL_INIT_VIDEO)==-1) {
		fprintf(stderr, "SDL_Init: %s\n",SDL_GetError());
		return 1;
	}
	atexit(SDL_Quit);

	if(!(screen=SDL_SetVideoMode(800,480,16,SDL_SWSURFACE))) {
		fprintf(stderr, "SDL_SetVideoMode: %s\n",SDL_GetError());
		return 1;
	}

	if(TTF_Init()==-1) {
		fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
		return 2;
	}
	atexit(TTF_Quit);
	atexit(free_font);

	if ((ds18b20_fd=open("/dev/ds18b20", O_RDWR|O_NDELAY|O_NOCTTY))<0)
		fprintf(stderr, "open device DS18B20 failed.\n");
	else
		fprintf(stdout, "open device DS18B20 successed.\n");

	intro = IMG_Load("intro.jpg");
	if (intro) {
		SDL_FillRect(screen, 0, ~0);
		SDL_BlitSurface(intro, 0, screen, &r2);
		SDL_FreeSurface(intro);
		SDL_Flip(screen);
		SDL_Delay(3000);
	}

	cover = IMG_Load("O-B0032-002.jpg");
	if (cover) {
		SDL_FillRect(screen, 0, ~0);
		SDL_BlitSurface(cover, 0, screen, &r2);
		SDL_FreeSurface(cover);
		SDL_Flip(screen);
		SDL_Delay(5000);
	} 

    parse_xml();

	load_font();

	SDL_FillRect(screen, 0, ~0);
	display(0);

	while (!done) {
		SDL_Event event;
		r.x=0, r.y=0;

		if(SDL_WaitEvent(&event))
		do {
			switch (event.type) {
				case SDL_MOUSEBUTTONDOWN:
					switch (event.button.button) {
						case SDL_BUTTON_RIGHT:
							pages++;
							if (pages>21) pages=0;
							display(pages);
							break;
						case SDL_BUTTON_LEFT:
							pages--;
							if (pages<0) pages=21;
							display(pages);
							break;
						default:
							break;
					}
					break;
				case SDL_MOUSEBUTTONUP:
					switch (event.button.button) {
						case SDL_BUTTON_WHEELUP:
							done = 1;
						default:
							break;
					}
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case 'q':
						case SDLK_ESCAPE:
							done = 1;
							break;
						default:
							break;
					}
				default:
					break;
			}
			break;
		} while (SDL_PollEvent(&event));
	} // main loop

	close(ds18b20_fd);

	return 0;
}
