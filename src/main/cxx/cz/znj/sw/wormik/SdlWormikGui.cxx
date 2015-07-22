/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * SDL GUI class
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>

#include "cz/znj/sw/wormik/WormikGame.hxx"

#include "cz/znj/sw/wormik/WormikGui.hxx"

#include "cz/znj/sw/wormik/gui_common.hxx"

namespace cz { namespace znj { namespace sw { namespace wormik {


using namespace gui4x6x16;


class SdlWormikGui: public WormikGui
{
public:
	enum {
		INVO_DESC		= INVO_NBASE,
		INVO_MENU		= INVO_NBASE<<1,
		INVO_MFULL		= INVO_FULL|INVO_DESC|INVO_MENU,
		INVO_DYNFLAGS		= INVO_NEWDEFS,
	};

	enum {
		UEV_GAMETOUT,
		UEV_DRAWTOUT,
	};

	enum {
		CLR_MENUBG		= 0,		/**< menu background */
		CLR_FONTMENU		= 1,		/**< menu font color */
		CLR_FONTEXC		= 2,		/**< exceptions font color */
		CLR_FONTANC		= 3,		/**< announcement font color */
		CLR_COUNT		= 4,
	};

protected:
	SDL_Window *			window;			/**< main window */
	SDL_Renderer *			windowRenderer;		/**< main renderer */
	SDL_PixelFormat *		windowPixelFormat;	/**< main window pixel format */

	SDL_Renderer *			textureRenderer;	/**< generic texture renderer */

	SDL_PixelFormat *		argbPixelFormat;	/**< ARGB8888 pixel format */

	SDL_Texture *			seasonImage;		/**< season image */
	SDL_Texture *			bgSeasonImage;		/**< season image (without alpha, with drawn background) */
	TTF_Font *			font;			/**< output font */

	WormikGame *			game;			/**< game interface */

	unsigned			colors[CLR_COUNT];	/**< colors (see CLR_* definitions) */

	double				diffgtime;		/**< difference to game time */
	double				lastmove;		/**< time of last game update */

	InvalList			invlist[2];		/**< invalid regions list */
	unsigned			currentInvList;		/**< current invlist */
	bool				redraw;			/**< screen needs redraw */

public:
	/* constructor */		SdlWormikGui();
	virtual				~SdlWormikGui();

public:
	virtual int			init(WormikGame *game);
	virtual void			shutdown(WormikGame *game);
	virtual int			newLevel(int season);

	virtual void			drawPoint(void *gc, unsigned x, unsigned y, unsigned short type);
	virtual int			drawNewdef(void *gc, unsigned x, unsigned y, unsigned short type, double timeout, double total);

	virtual void			invalOut(int len, unsigned (*points)[2]);

	virtual int			waitNext(double interval);
	virtual int			announce(int type);

protected:
	int				initSurface();
	int				initSeasonImage(SDL_Texture *img);

	int				initGui();
	void				closeGui();

	void				drawLTextf(int x, int y, Uint32 color, const char *fmt, ...);
	void				drawText(int x, int y, Uint32 color, const char *text);

	/**
	 * @return
	 * 	number of pending "newdefs"
	 */
	unsigned			drawBase();
	unsigned			drawAnnounce(unsigned n, const char *text[]);
	void				drawFinish(unsigned rflags);
	int				showPopup(int stde);

	SDL_TimerID			createDrawTimer(volatile int *timed);

	int				stdEvent(SDL_Event *ev);
};

static double getdtime(void)
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec+t.tv_usec/1000000.0;
}

SdlWormikGui::SdlWormikGui()
{
	window = NULL;
	windowRenderer = NULL;
	windowPixelFormat = NULL;
	bgSeasonImage = NULL;
	seasonImage = NULL;
	font = NULL;
	game = NULL;
}

SdlWormikGui::~SdlWormikGui()
{
}

int SdlWormikGui::initSurface()
{
	SDL_SetWindowTitle(window, "Wormik");
	SDL_ShowCursor(SDL_DISABLE);


	if (!(bgSeasonImage = SDL_CreateTexture(windowRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SIMG_WIDTH, SIMG_HEIGTH))) {
		game->error("couldn't create bgSeasonImage texture: %s\n", SDL_GetError());
		return -1;
	}

#if 0
	colors[CLR_MENUBG] = SDL_MapRGB(windowPixelFormat, 0, 0, 0);
	colors[CLR_MENUFNT] = SDL_MapRGB(windowPixelFormat, 255, 255, 255);
	colors[CLR_MENUEXC] = SDL_MapRGB(windowPixelFormat, 255, 255, 200);
#endif

	currentInvList = 0;
	invlist[0].reset(INVO_MFULL); invlist[1].reset(INVO_MFULL);

	return 0;
}

static SDL_RWops *findopenfile(const char *fname, ...)
{
	SDL_RWops *f = NULL;
	const char *p;
	va_list va;
	va_start(va, fname);

	while (!f && (p = va_arg(va, const char *))) {
		switch (*p++) {
		case 'l':
			{
				FILE *lp;
				char buf[PATH_MAX+16];
				unsigned fl = strlen(fname);
				if (snprintf(buf, sizeof(buf), "locate /%s", fname) >= (int)sizeof(buf))
					break;
				if (!(lp = popen(buf, "r")))
					break;
				while (fgets(buf, sizeof(buf), lp)) {
					unsigned bl;
					if ((bl = strlen(buf)) <= fl+2 || buf[--bl] != '\n')
						continue;
					buf[bl] = '\0';
					if (buf[bl-fl-1] == '/' && memcmp(buf+bl-fl, fname, fl) == 0) {
						if ((f = SDL_RWFromFile(buf, "r")))
							break;
					}
				}
				pclose(lp);
			}
			break;

		case 'd':
			{
				char buf[PATH_MAX];
				if (snprintf(buf, sizeof(buf), "%s/%s", p, fname) >= (int)sizeof(buf))
					break;
				f = SDL_RWFromFile(buf, "r");
			}
			break;

		case 's':
			{
				FILE *fp;
				char buf[PATH_MAX+32];
				unsigned fl = strlen(fname);
				if (snprintf(buf, sizeof(buf), "find %s -follow -name %s -print", p, fname) >= (int)sizeof(buf))
					break;
				if (!(fp = popen(buf, "r")))
					break;
				while (fgets(buf, sizeof(buf), fp)) {
					unsigned bl;
					if ((bl = strlen(buf)) <= fl+1 || buf[--bl] != '\n')
						continue;
					buf[bl] = '\0';
					if ((f = SDL_RWFromFile(buf, "r")))
						break;
				}
				pclose(fp);
			}
			break;

		default:
			assert(0);
			break;
		}
	}
	va_end(va);
	return f;
}

int SdlWormikGui::init(WormikGame *game_)
{
	game = game_;

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		game->error("Couldn't init SDL: %s\n", SDL_GetError());
		return -1;
	}
	if (initGui() < 0) {
		shutdown(game);
		return -1;
	}
	return 0;
}

int SdlWormikGui::initGui()
{
	char buf[PATH_MAX];
	SDL_RWops *ffo = NULL;

	argbPixelFormat = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
	if (SDL_CreateWindowAndRenderer(640, 480, (game->getConfigInt("fullscreen", 1)?SDL_WINDOW_FULLSCREEN:0), &window, &windowRenderer) != 0) {
		game->error("Couldn't create window: %s\n", SDL_GetError());
		goto err;
	}
	if ((textureRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_TARGETTEXTURE)) != NULL) {
		game->error("Couldn't create texture renderer: %s\n", SDL_GetError());
		goto err;
	}
	SDL_RenderClear(windowRenderer);
	SDL_SetRenderDrawColor(windowRenderer, 255, 255, 255, 255);
	SDL_RenderFillRect(windowRenderer, NULL);
	windowPixelFormat = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));
	SDL_RenderPresent(windowRenderer);
	if (TTF_Init() < 0) {
		game->error("Couldn't init TTF lib: %s\n", TTF_GetError());
		goto err;
	}
	if ((unsigned)game->getConfigStr("font", buf, sizeof(buf)) < sizeof(buf)) {
		if (!(ffo = SDL_RWFromFile(buf, "r")))
			game->error("Couldn't open font file specified in config (trying default): %s\n", strerror(errno));
	}
	if (!ffo && !(ffo = findopenfile("FreeMonoBold.ttf", "d/usr/share/fonts/truetype/freefont", "l", "s/usr/share/fonts", "s/usr/lib/X11/fonts", NULL))) {
		game->fatal("Couldn't find/open output font\n");
		goto err;
	}
	if (!(font = TTF_OpenFontRW(ffo, 1, game->getConfigInt("fontsize", 15)))) {
		game->error("Couldn't open output font: %s\n", TTF_GetError());
		goto err;
	}
	if (initSurface() < 0) {
		goto err;
	}
	return 0;

err:
	return -1;
}

void SdlWormikGui::closeGui()
{
	if (font) {
		TTF_CloseFont(font);
		font = NULL;
	}
	if (TTF_WasInit())
		TTF_Quit();
	if (seasonImage) {
		SDL_DestroyTexture(seasonImage);
		seasonImage = NULL;
	}
	SDL_ShowCursor(SDL_ENABLE);
	if (bgSeasonImage) {
		SDL_DestroyTexture(bgSeasonImage);
		bgSeasonImage = NULL;
	}
	if (textureRenderer) {
		SDL_DestroyRenderer(textureRenderer);
		textureRenderer = NULL;
	}
	if (windowPixelFormat) {
		SDL_FreeFormat(windowPixelFormat);
		windowPixelFormat = NULL;
	}
	if (windowRenderer) {
		SDL_DestroyRenderer(windowRenderer);
		windowRenderer = NULL;
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
}

void SdlWormikGui::shutdown(WormikGame *game)
{
	closeGui();
	SDL_Quit();
}

int SdlWormikGui::initSeasonImage(SDL_Texture *img)
{
	SDL_Rect s, d;
	if (seasonImage) {
		SDL_DestroyTexture(seasonImage);
	}
	if (!(seasonImage = SDL_CreateTexture(windowRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, SIMG_WIDTH, SIMG_HEIGTH))) {
		game->error("Failed to convert season image to current video surface: %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetRenderTarget(textureRenderer, seasonImage);
	s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
	for (d.y = 0; d.y < SIMG_HEIGTH; d.y += GRECT_YSIZE) {
		for (d.x = 0; d.x < SIMG_WIDTH; d.x += GRECT_XSIZE) {
			SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
		}
	}
	SDL_RenderCopy(textureRenderer, bgSeasonImage, NULL, NULL);
	SDL_SetRenderTarget(textureRenderer, NULL);
	return 0;
}

int SdlWormikGui::newLevel(int season)
{
	SDL_RWops *sf;
	int err;
	char fname[PATH_MAX];
	char dpath[PATH_MAX];
	SDL_Texture *img;
	SDL_Color c[sizeof(colors)/sizeof(colors[0])];
	unsigned i;

	if ((unsigned)game->getConfigStr("datapath", dpath+1, sizeof(dpath)-1) >= sizeof(dpath)-1)
		dpath[0] = '\0';
	else
		dpath[0] = 'd';

	for (;;) {
		if (snprintf(fname, sizeof(fname), "wormik_%d.png", season) >= (int)sizeof(fname)) {
			game->fatal("filename too long\n");
		}
		if (!(sf = findopenfile(fname, "d/usr/share/games/wormik", (dpath[0] == '\0')?"d.":dpath, NULL))) {
			if (season == 0)
				game->fatal("failed to open %s: %s\n", fname, strerror(errno));
			season = 0;
		}
		else
			break;
	}
	if (!(img = IMG_LoadTexture_RW(windowRenderer, sf, 1))) {
		game->fatal("failed to process image %s: %s\n", fname, SDL_GetError());
	}
	Uint32 imgFormat; int imgWidth; int imgHeight;
	if (SDL_QueryTexture(img, &imgFormat, NULL, &imgWidth, &imgHeight) != 0) {
		game->fatal("failed to query texture data");
	}
	SDL_PixelFormat *imgPixelFormat = SDL_AllocFormat(imgFormat);
	if (imgWidth != SIMG_WIDTH || imgHeight != SIMG_HEIGTH || imgFormat != SDL_PIXELFORMAT_ARGB8888) {
		game->fatal("%s: image has to be %dx%dx32 sized (is %dx%dx%d)\n", fname, SIMG_WIDTH, SIMG_HEIGTH, imgWidth, imgHeight, imgFormat);
		SDL_DestroyTexture(img);
	}
	//void *imgPixels; int imgPitch;
	//if (SDL_LockTexture(img, NULL, &imgPixels, &imgPitch) != 0) {
	//	game->fatal("failed to lock texture: %s\n", SDL_GetError());
	//}
	SDL_SetRenderTarget(textureRenderer, img);

	// find basic drawing colors, these have alpha 0 in original image
#if 1
	i = 0;
	SDL_Rect rec = { 0, 0, 1, 1 };
	for (unsigned y = 0; i < sizeof(c)/sizeof(c[0]) && y < (unsigned)imgHeight; y++) {
		for (unsigned x = 0; i < sizeof(c)/sizeof(c[0]) && x < (unsigned)imgWidth; x++) {
			rec.x = x; rec.y = y;
			Uint32 pixel;
			if (SDL_RenderReadPixels(textureRenderer, &rec, SDL_PIXELFORMAT_ARGB8888, &pixel, 1) != 0) {
				game->fatal("cannot read pixel from texture: %s\n", SDL_GetError());
			}
			game->error("found pixel %x\n", pixel);
			if ((pixel&0xff000000) == 0) {
				//printf("found %dx%d: (%d, %d, %d)\n", y, x, c[i].r, c[i].g, c[i].b);
				c[i].r = (Uint8)(pixel>>16);
				c[i].g = (Uint8)(pixel>>8);
				c[i].b = (Uint8)(pixel>>0);
				i++;
			}
		}
	}
	if (i != sizeof(c)/sizeof(c[0])) {
		game->fatal("%s: Didn't find enough hidden pixels to get font colors: %d\n", fname, (int)i);
	}
#else
	for (i = 0; i < sizeof(c)/sizeof(c[0]); i++) {
		Uint8 a;
		SDL_GetRGBA(*(Uint32 *)((char *)imgPixels+5*GRECT_YSIZE*imgPitch+(2*GRECT_XSIZE+i)*SDL_BYTESPERPIXEL(imgFormat)), imgFormat, &c[i].r, &c[i].g, &c[i].b, &a);
		printf("found %dx%d: (%d, %d, %d)\n", y, x, c[i].r, c[i].g, c[i].b);
	}
#endif
	for (i = 0; i < sizeof(c)/sizeof(c[0]); i++) {
		colors[i] = SDL_MapRGB(windowPixelFormat, c[i].r, c[i].g, c[i].b);
	}

	//SDL_UnlockTexture(img);
	SDL_SetRenderTarget(textureRenderer, NULL);
	err = initSeasonImage(img);
	SDL_FreeFormat(imgPixelFormat);
	SDL_DestroyTexture(img);
	if (err < 0) {
		game->fatal();
	}
	invalOut(-INVO_MFULL, NULL);
	return season;
}

void SdlWormikGui::drawPoint(void *gc, unsigned x, unsigned y, unsigned short cont)
{
	SDL_Rect s, d;
	unsigned sx, sy;
	d.x = x*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
	s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
	findImagePos(cont, &sx, &sy);
	s.x = sx; s.y = sy;
	SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
}

int SdlWormikGui::drawNewdef(void *gc, unsigned x, unsigned y, unsigned short cont, double timeout, double total)
{
	SDL_Rect s, d;
	unsigned sx, sy;
	int alpha = (int)(255*(timeout-diffgtime)/total);
	d.x = x*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
	s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
	findImagePos(cont, &sx, &sy);
	if (alpha <= 0) {
		s.x = sx; s.y = sy;
		SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
		return 0;
	}
	else {
		if (alpha >= 256) // possible because of newdef latency
			alpha = 255;
		s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		s.x = sx; s.y = sy;
		SDL_SetTextureAlphaMod(bgSeasonImage, 255-alpha);
		SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
		SDL_SetTextureAlphaMod(bgSeasonImage, 255);
		return 1;
	}
}

void SdlWormikGui::invalOut(int len, unsigned (*points)[2])
{
	if (len == 0) {
		return;
	}
	else if (len < 0) {
		invlist[0].flags |= -len;
		invlist[1].flags |= -len;
	}
	else {
		while (len-- > 0) {
			invlist[0].add(points[len][0], points[len][1]);
			invlist[1].add(points[len][0], points[len][1]);
		}
	}
	redraw = true;
}

void SdlWormikGui::drawText(int x, int y, Uint32 color, const char *text)
{
	SDL_Color clr;
	SDL_Surface *fs;
	SDL_Rect d;
	SDL_GetRGB(color, windowPixelFormat, &clr.r, &clr.g, &clr.b);
	fs = TTF_RenderText_Blended(font, text, clr);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs);
	d.x = (x >= 0)?x:(-x-fs->w); d.y = (y >= 0)?y:(-y-fs->h);
	SDL_RenderCopy(windowRenderer, texture, NULL, &d);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(fs);
}

void SdlWormikGui::drawLTextf(int x, int y, Uint32 color, const char *fmt, ...)
{
	SDL_Color clr;
	va_list va;
	char buf[256];
	int nrows;
	SDL_Rect d;
	SDL_Surface *fs[256];
	char *p;
	int mw, th;
	SDL_GetRGB(color, windowPixelFormat, &clr.r, &clr.g, &clr.b);
	va_start(va, fmt);
	if (vsnprintf(buf, sizeof(buf), fmt, va) >= (int)sizeof(buf))
		buf[sizeof(buf)-1] = '\0';

	mw = 0; th = 0;
	for (p = buf, nrows = 0; *p != '\0'; nrows++) {
		char *o = p;
		while (*p != '\0' && *p != '\n')
			p++;
		if (*p != '\0')
			*p++ = '\0';
		fs[nrows] = TTF_RenderText_Blended(font, o, clr);
		if (fs[nrows]->w > mw)
			mw = fs[nrows]->w;
		th += fs[nrows]->h;
	}
	d.y = (y >= 0)?y:(-y-th);
	fs[nrows] = NULL;
	for (nrows = 0; fs[nrows]; nrows++) {
		d.x = (x >= 0)?x:(-x-fs[nrows]->w);
		SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs[nrows]);
		SDL_RenderCopy(windowRenderer, texture, NULL, &d);
		d.y += fs[nrows]->h;
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(fs[nrows]);
	}
}

unsigned SdlWormikGui::drawBase(void)
{
	unsigned ret = 0;
	unsigned x, y;
	SDL_Rect s, d;
	InvalList *ic = &invlist[currentInvList];

	if ((ic->flags&INVO_BOARD) != 0) {
		s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
		game->outGame(NULL, 0, 0, game->GAME_YSIZE-1, game->GAME_XSIZE-1);
	}
	else {
		unsigned i;
		for (i = 0; i < ic->ilength; i++) {
			game->outPoint(NULL, ic->invlist[i][0], ic->invlist[i][1]);
		}
	}

	if ((ic->flags&INVO_NEWDEFS) != 0) {
		if (game->outNewdefs(NULL) > 0)
			ret |= INVO_NEWDEFS;
	}

	if ((ic->flags&INVO_MENU) != 0) {
		findImagePos(WormikGame::GR_WALL, &x, &y);
		s.x = x; s.y = y; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
		d.w = GRECT_XSIZE; d.h = GRECT_YSIZE;
		SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENUBG]>>16), (Uint8)(colors[CLR_MENUBG]>>8), (Uint8)(colors[CLR_MENUBG]>>0), (Uint8)(colors[CLR_MENUBG]>>24));
		for (y = 1; y < 29; y++) {
			d.x = 480+9*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
		for (x = 0; x < 10; x++) {
			d.x = 480+x*GRECT_XSIZE;
			d.y = 0;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = 6*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = 12*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = 18*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = 29*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
	}

	if ((ic->flags&(INVO_RECORD|INVO_SCORE|INVO_GSTATE|INVO_HEALTH|INVO_LENGTH)) != 0) {
		d.w = 144; d.h = 80; d.x = 480;
		if ((ic->flags&INVO_RECORD) != 0) {
			struct tm t; char tc[32];
			int record; time_t rectime; int isnow;
			isnow = game->getRecord(&record, &rectime);
			t = *localtime(&rectime); strftime(tc, sizeof(tc), "%Y-%m-%d %H:%M", &t);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENUBG]>>16), (Uint8)(colors[CLR_MENUBG]>>8), (Uint8)(colors[CLR_MENUBG]>>0), (Uint8)(colors[CLR_MENUBG]>>24));
			d.y = 16; SDL_RenderFillRect(windowRenderer, &d);
			drawLTextf(-622, 28, colors[isnow?CLR_FONTEXC:CLR_FONTMENU], "Record: %d\n%s\n", record, (rectime == 0)?" ":tc);
		}
		if ((ic->flags&(INVO_SCORE|INVO_GSTATE)) != 0) {
			int score, total, exit;
			int level, season;
			game->getState(&level, &season);
			exit = game->getScore(&score, &total);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[0]>>16), (Uint8)(colors[0]>>8), (Uint8)(colors[0]>>0), (Uint8)(colors[0]>>24));
			d.y = 112; SDL_RenderFillRect(windowRenderer, &d);
			drawLTextf(-622, 124, colors[(score >= exit)?CLR_FONTEXC:CLR_FONTMENU], "Score: %d\nLevel: %d/%d\n", total, level, (total-score)+exit);
		}
		if ((ic->flags&(INVO_HEALTH|INVO_LENGTH)) != 0) {
			int health, length;
			game->getSnakeInfo(&health, &length);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[0]>>16), (Uint8)(colors[0]>>8), (Uint8)(colors[0]>>0), (Uint8)(colors[0]>>24));
			d.y = 208; SDL_RenderFillRect(windowRenderer, &d);
			drawLTextf(-622, 220, colors[(health <= 1)?CLR_FONTEXC:CLR_FONTMENU], "Health: %d\nLength: %d\n", health, length);
		}
	}

	if ((ic->flags&INVO_DESC) != 0) {
		s.w = GRECT_XSIZE; s.h = GRECT_YSIZE; d.x = 496;
		d.x = 480; d.y = 19*GRECT_YSIZE; d.w = 9*GRECT_XSIZE; d.h = 10*GRECT_YSIZE;
		SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENUBG]>>16), (Uint8)(colors[CLR_MENUBG]>>8), (Uint8)(colors[CLR_MENUBG]>>0), (Uint8)(colors[CLR_MENUBG]>>24));
		SDL_RenderFillRect(windowRenderer, &d);
		findImagePos(WormikGame::GR_POSIT, &x, &y); s.x = x; s.y = y; d.y = 312; SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-608, 304, colors[CLR_FONTMENU], "S+2");
		findImagePos(WormikGame::GR_POSIT2, &x, &y); s.x = x; s.y = y; d.y = 344; SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-608, 336, colors[CLR_FONTMENU], "S+5");
		findImagePos(WormikGame::GR_NEGAT, &x, &y); s.x = x; s.y = y; d.y = 376; SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-608, 368, colors[CLR_FONTMENU], "H-1");
		findImagePos(WormikGame::GR_DEATH, &x, &y); s.x = x; s.y = y; d.y = 408; SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-608, 400, colors[CLR_FONTMENU], "Death");
		findImagePos(WormikGame::GR_EXIT, &x, &y); s.x = x; s.y = y; d.y = 440; SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-608, 432, colors[CLR_FONTMENU], "Exit");
	}
	return ret;
}

unsigned SdlWormikGui::drawAnnounce(unsigned n, const char *text[])
{
	SDL_Color clr;
	unsigned i;
	SDL_Surface **fs = (SDL_Surface **)alloca(n*sizeof(SDL_Surface *));
	SDL_Rect s, d;
	int w, h;
	int ey, ex;

	SDL_GetRGB(colors[CLR_FONTANC], windowPixelFormat, &clr.r, &clr.g, &clr.b);

	w = h = 0;
	for (i = 0; i < n; i++) {
		fs[i] = TTF_RenderText_Blended(font, text[i], clr);
		if (fs[i]->w > w)
			w = fs[i]->w;
		h += fs[i]->h;
	}
	w += 32; h += 16;
	s.x = SP_MSG_X*GRECT_XSIZE; s.y = SP_MSG_Y*GRECT_YSIZE; s.h = GRECT_YSIZE;
	for (d.y = (480-h)/2, ey = d.y+h; d.y < ey; d.y += GRECT_YSIZE) {
		if (d.y+s.h > ey)
			s.h = ey-d.y;
		s.w = GRECT_XSIZE;
		for (d.x = (480-w)/2, ex = d.x+w; d.x < ex; d.x += GRECT_XSIZE) {
			if (d.x+s.w > ex)
				s.w = ex-d.x;
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
	}
	d.y = (480-h+16)/2;
	for (i = 0; i < n; i++) {
		d.x = (480-fs[i]->w)/2;
		SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs[i]);
		SDL_RenderCopy(windowRenderer, texture, NULL, &d);
		d.y += fs[i]->h;
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(fs[i]);
	}
	return 0;
}

void SdlWormikGui::drawFinish(unsigned rflags)
{
	game->error("SDL_RenderPresent\n");
	SDL_RenderPresent(windowRenderer);
	invlist[currentInvList].reset(rflags);
#if 0
	if ((SDL_GetWindowFlags(window)&SDL_DOUBLEBUF) != 0) {
		currentInvList ^= 1;
		invlist[currentInvList].flags |= rflags;
	}
#endif
	redraw = false;
}

static Uint32 push_game_timeout(Uint32 inter, void *par)
{
	if (*(volatile int *)par == 0) {
		SDL_Event ev;
		ev.type = SDL_USEREVENT;
		ev.user.code = SdlWormikGui::UEV_GAMETOUT;
		SDL_PushEvent(&ev);
		*(volatile int *)par = 1;
	}
	return inter;
}

static Uint32 push_draw_timeout(Uint32 inter, void *par)
{
	if (*(volatile int *)par == 0) {
		SDL_Event ev;
		ev.type = SDL_USEREVENT;
		ev.user.code = SdlWormikGui::UEV_DRAWTOUT;
		SDL_PushEvent(&ev);
		*(volatile int *)par = 1;
	}
	return inter;
}

SDL_TimerID SdlWormikGui::createDrawTimer(volatile int *timed)
{
	SDL_TimerID t;
	if (!(t = SDL_AddTimer(1000/16, &push_draw_timeout, (void *)timed)))
		game->error("unable to create drawtimer: %s\n", SDL_GetError());
	return t;
}

enum {
	STDE_UNKNOWN = -1,
	STDE_PROCESSED = 0,
	STDE_QUIT = 1,
	STDE_REDRAW = 2,
	STDE_DRAWTIMER = 3,
	STDE_SHOWBASE = 4,
	STDE_SHOWHELP = 4,
	STDE_SHOWABOUT = 5,
	STDE_SHOWMAX = 5,
};

int SdlWormikGui::stdEvent(SDL_Event *ev)
{
	switch (ev->type) {
	case SDL_QUIT:
		return STDE_QUIT;

	case SDL_KEYDOWN:
		switch (ev->key.keysym.sym) {
		case SDLK_ESCAPE:
		case SDLK_q:
			return 1;

		case SDLK_f:
			{
				closeGui();
				game->setConfig("fullscreen", game->getConfigInt("fullscreen", 0) == 0);
				if (initGui() < 0) {
					game->error("Failed to set video mode, trying to set the old one: %s\n", SDL_GetError());
					game->setConfig("fullscreen", game->getConfigInt("fullscreen", 0) == 0);
					if (initGui() < 0) {
						game->fatal("Failed to set video mode: %s\n", SDL_GetError());
					}
				}
			}
			invalOut(-INVO_MFULL, NULL);
			return STDE_REDRAW;

		case SDLK_h:
		case SDLK_HELP:
		case SDLK_F1:
			return STDE_SHOWHELP;

		case SDLK_a:
			return STDE_SHOWABOUT;

		default:
			break;
		}
		break;
	case SDL_WINDOWEVENT:
		invlist[0].reset(INVO_MFULL); invlist[1].reset(INVO_MFULL);
		return STDE_REDRAW;

	case SDL_USEREVENT:
		switch (ev->user.code) {
		case UEV_DRAWTOUT:
			return STDE_DRAWTIMER;

		}
		break;
	}
	return STDE_UNKNOWN;
}

int SdlWormikGui::showPopup(int stdrm)
{
	int ret = -1;
	redraw = true;
	for (;;) {
		int stdr;
		SDL_Event ev;
		if (redraw) {
			int ntext = 0;
			const char *text[20];

			drawBase();
			switch (stdrm) {
			case STDE_SHOWHELP:
				text[ntext++] = "arrows - change snake direction";
				text[ntext++] = "f      - toggle fullscreen mode";
				text[ntext++] = "p      - toggle pause          ";
				text[ntext++] = "q, Esc - quit                  ";
				text[ntext++] = "h, F1  - show this help        ";
				text[ntext++] = "a      - show about            ";
				text[ntext++] = "return - close dialog          ";
				text[ntext++] = " ";
				text[ntext++] = "(see README for more info)";
				break;
			case STDE_SHOWABOUT:
				text[ntext++] = "Wormik "SVERSION;
				text[ntext++] = "See project homepage at";
				text[ntext++] = "http://atrey.karlin.mff.cuni.cz/~rat/wormik/";
				text[ntext++] = "                                            ";
				text[ntext++] = "  Enjoy the game,                           ";
				text[ntext++] = "                            Zbynek Vyskovsky";
				text[ntext++] = "(see README for more info)";
				break;
			default:
				assert(0);
			}
			drawAnnounce(ntext, text);
			drawFinish(0);
		}
		if (SDL_WaitEvent(&ev) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		stdr = stdEvent(&ev);
		if (stdr >= STDE_SHOWBASE && stdr <= STDE_SHOWMAX) {
			ret = stdr;
		}
		else switch (stdr) {
		case STDE_PROCESSED:
			break;
		case STDE_QUIT:
			ret = 1;
			break;
		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym) {
				case SDLK_SPACE:
				case SDLK_RETURN:
					ret = STDE_REDRAW;
					break;
				default:
					break;
				}
				break;
			}
			break;
		}
		if (ret >= 0)
			break;
	}
	invalOut(-INVO_MFULL, NULL);
	return ret;
}

int SdlWormikGui::announce(int anc)
{
	int ret = -1;

	redraw = true;
	for (;;) {
		int stdr;
		SDL_Event ev;
		if (redraw) {
			int ntext = 0;
			const char *text[2];

			drawBase();
			switch (anc) {
			case ANC_DEAD:
				text[ntext++] = "You are dead, hehe!";
				break;
			case ANC_EXIT:
				text[ntext++] = "You moved to next level,";
				text[ntext++] = "congratulations!";
				break;
			default:
				assert(0);
			}
			drawAnnounce(ntext, text);
			drawFinish(0);
		}
		if (SDL_WaitEvent(&ev) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		stdr = stdEvent(&ev);
reswitch:
		if (stdr >= STDE_SHOWBASE && stdr <= STDE_SHOWMAX) {
			invalOut(-INVO_MFULL, NULL);
			stdr = showPopup(stdr);
			goto reswitch;
		}
		else switch (stdr) {
		case STDE_PROCESSED:
			break;
		case STDE_QUIT:
			ret = 1;
			break;
		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym) {
				case SDLK_SPACE:
				case SDLK_RETURN:
					ret = 0;
					break;
				default:
					break;
				}
				break;
			}
			break;
		}
		if (ret >= 0)
			break;
	}
	invalOut(-INVO_MFULL, NULL);
	return ret;
}

int SdlWormikGui::waitNext(double interval)
{
	volatile int gametimed = 0;
	SDL_TimerID gametimer = 0;
	volatile int drawtimed = 0;
	SDL_TimerID drawtimer = 0;
	double time = getdtime();
	int ret = -1;
	int pausing;

	if (interval == 0) {
		pausing = -1;
	}
	else {
		int wait;
		pausing = 0;
		if ((wait = int(((lastmove+interval)-time)*(1000000/1000))) > 0) {
			if (!(gametimer = SDL_AddTimer(wait, &push_game_timeout, (void *)&gametimed)))
				game->fatal("creating SDL timer: %s\n", SDL_GetError());
		}
		else {
			push_game_timeout(0, (void *)&gametimed);
		}
	}
	drawtimer = createDrawTimer(&drawtimed);
	for (;;) {
		int stdr;
		SDL_Event ev;
		if (redraw) {
			unsigned rflags = 0;
			time = getdtime();
			if ((diffgtime = time-lastmove) > interval)
				diffgtime = interval;
			else if (diffgtime < 0) // yes, it really happens, problem is inaccuracy in SDL timers
				diffgtime = 0;
			rflags |= drawBase();
			drawFinish(rflags);
		}
		if (SDL_WaitEvent(&ev) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		stdr = stdEvent(&ev);
reswitch:
		if (stdr >= STDE_SHOWBASE && stdr <= STDE_SHOWMAX) {
			lastmove = 0;
			diffgtime = interval;
			pausing = stdr;
			ret = -1;
		}
		else switch (stdr) {
		case STDE_PROCESSED:
			break;
		case STDE_QUIT:
			ret = 1;
			break;
		case STDE_DRAWTIMER:
			if (ret < 0 && (invlist[currentInvList].flags&INVO_DYNFLAGS) != 0)
				redraw = true;
			drawtimed = 0;
			break;
		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_USEREVENT:
				switch (ev.user.code) {
				case UEV_GAMETOUT:
					if (gametimer != 0) {
						SDL_RemoveTimer(gametimer);
						gametimer = 0;
					}
					gametimed = 0;
					if (!pausing && ret < 0) {
						lastmove = lastmove+interval;
						ret = 0;
					}
					break;
				}
				break;
			case SDL_KEYDOWN:
				{
					int dir = -1;
					switch (ev.key.keysym.sym) {
					case SDLK_p:
						lastmove = 0; redraw = true;
						pausing = -1;
						ret = -1;
						break;
					case SDLK_RIGHT:
						dir = WormikGame::SDIR_RIGHT;
						break;
					case SDLK_UP:
						dir = WormikGame::SDIR_UP;
						break;
					case SDLK_LEFT:
						dir = WormikGame::SDIR_LEFT;
						break;
					case SDLK_DOWN:
						dir = WormikGame::SDIR_DOWN;
						break;
					default:
						break;
					}
					if (dir >= 0) {
						//putchar('\a'); fflush(stdout);
						game->changeDirection(dir);
						if (pausing != 0) {
							lastmove = getdtime();
							ret = 0;
							pausing = 0;
						}
					}
				}
				break;
			}
		}
		if ((invlist[currentInvList].flags&INVO_DYNFLAGS) == 0) {
			if (drawtimer != 0) {
				SDL_RemoveTimer(drawtimer);
				drawtimer = 0;
			}
		}
		if (ret >= 0 || pausing != 0) {
			if (gametimer != 0) {
				SDL_RemoveTimer(gametimer);
				gametimer = 0;
			}
			if (drawtimer != 0) {
				SDL_RemoveTimer(drawtimer);
				drawtimer = 0;
			}
			if (!gametimed && !drawtimed) {
				if (pausing > 0) {
					stdr = showPopup(pausing);
					pausing = -1;
					goto reswitch;
				}
				else if (ret >= 0) {
					break;
				}
			}
		}
	}
	diffgtime = 0;
	return ret;
}

WormikGui *create_WormikGui(void)
{
	return new SdlWormikGui();
}


} } } };
