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

#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_ttf.h>

#include "cz/znj/sw/wormik/platform.hxx"

#include "cz/znj/sw/wormik/WormikGame.hxx"

#include "cz/znj/sw/wormik/WormikGui.hxx"

#include "cz/znj/sw/wormik/gui_common.hxx"

namespace cz { namespace znj { namespace sw { namespace wormik {


using namespace gui4x6x16;


class SdlWormikGui: public WormikGui
{
public:
	enum {
		WINDOW_WIDTH = 640,
		WINDOW_HEIGHT = 480,
		AREA_INFO_X = 480,
	};

	enum {
		INVO_DESC		= INVO_NEXT_BASE,
		INVO_MENU		= INVO_NEXT_BASE<<1,
		INVO_MESSAGE		= INVO_NEXT_BASE<<2,
		INVO_SDL_FULL		= INVO_FULL|INVO_DESC|INVO_MENU|INVO_MESSAGE,
		INVO_DYN_FLAGS		= INVO_NEW_DEFS,
	};

	enum {
		CLR_MENU_BG		= 0,		/**< menu background */
		CLR_MENU_FONT		= 1,		/**< menu font color */
		CLR_EXCEPTION_FONT	= 2,		/**< exceptions font color */
		CLR_ANNOUNCEMENT_FONT	= 3,		/**< announcement font color */
		CLR_COUNT		= 4,
	};

	enum {
		MENU_PADDING            = 2,
		MENU_WIDTH_POINTS       = 10,
		MENU_X_POINTS           = WormikGame::GAME_XSIZE,
		MENU_HEIGHT_POINTS      = WormikGame::GAME_YSIZE,
		MENU_SEP_FIRST_POINTS   = 6,
		MENU_SEP_SCORE_POINTS   = 6,
		MENU_SEP_SNAKE_POINTS   = 12,
		MENU_SEP_INFO_POINTS    = 18,
		MENU_FONT_HEIGHT_PX     = GRECT_YSIZE-4,
		MENU_TEXT_RIGHT_PX      = WINDOW_WIDTH-GRECT_XSIZE-MENU_PADDING,
		MENU_DESC_RIGHT_PX      = WINDOW_WIDTH-GRECT_XSIZE-GRECT_XSIZE,
		MENU_DESC_SPACING_PX    = 8,
	};

	static const double		REDRAW_TIME;

protected:
	SDL_Window *			window;			/**< main window */
	SDL_Renderer *			windowRenderer;		/**< main renderer */
	SDL_PixelFormat *		windowPixelFormat;	/**< main window pixel format */

	SDL_Renderer *			textureRenderer;	/**< generic texture renderer */

	Uint32				alphaPixelFormat;	/**< preferred alpha pixel format */

	SDL_Texture *			seasonImage;		/**< season image */
	SDL_Texture *			bgSeasonImage;		/**< season image (without alpha, with drawn background) */

	SDL_Texture *                   basicScreen;            /**< screen rendered with basic level decoration */

	TTF_Font *			font;			/**< output font */

	WormikGame *			game;			/**< game interface */

	unsigned			colors[CLR_COUNT];	/**< colors (see CLR_* definitions) */

	double				diffGameTime;		/**< difference to game time */
	double				lastMove;		/**< time of last game update */

	InvalidatedList			invalidatedList[2];	/**< invalid regions list */
	unsigned			nextInvalidatedList;		/**< current invlist */
	bool				redraw;			/**< screen needs redraw */

public:
	/* constructor */		SdlWormikGui();
	virtual				~SdlWormikGui();

public:
	virtual int			init(WormikGame *game);
	virtual void			shutdown(WormikGame *game);
	virtual int			newLevel(int season);

	virtual void			drawStatic(void *gc, unsigned x, unsigned y, unsigned short type);
	virtual void			drawPoint(void *gc, unsigned x, unsigned y, unsigned short type);
	virtual int			drawNewdef(void *gc, unsigned x, unsigned y, unsigned short type, double timeout, double total);

	virtual void			invalidateOutput(int len, unsigned (*points)[2]);
	virtual void			invalidateAll();

	virtual bool			waitStart();
	virtual bool			waitNext(double interval);
	virtual bool			announce(int type);

protected:
	int				initWindow();
	int				initSeasonImage(SDL_Texture *img);
	int				initLevelImage(int season);

	int				initGui();
	void				closeGui();

	void				drawLinedTextf(int x, int y, Uint32 color, const char *fmt, ...);
	void				drawText(int x, int y, Uint32 color, const char *text);

	/**
	 * @return
	 * 	next refresh flags
	 */
	void                            drawStaticScreen(int flags);
	unsigned			drawBase();
	unsigned			drawAnnounce(unsigned n, const char *const text[]);
	void				drawFinish(unsigned renderFlags);
	int				showPopup(int stde);

	SDL_TimerID			createDrawTimer();

	int				processStandardEvent(SDL_Event *ev);

	static Uint32			drawTimerCallback(Uint32 timeout, void *this_);
	static Uint32			gameTimerCallback(Uint32 timeout, void *this_);
};

static double getDoubleTime(void)
{
#if (defined _WIN32) || (defined _WIN64)
	return GetTickCount()/1000.0;
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec+t.tv_usec/1000000.0;
#endif
}

const double SdlWormikGui::REDRAW_TIME = 1/20.0;

SdlWormikGui::SdlWormikGui()
{
	window = NULL;
	windowRenderer = NULL;
	windowPixelFormat = NULL;
	bgSeasonImage = NULL;
	seasonImage = NULL;
	font = NULL;
	game = NULL;

	static_assert((MENU_X_POINTS+MENU_WIDTH_POINTS)*GRECT_XSIZE == WINDOW_WIDTH);
	static_assert((MENU_HEIGHT_POINTS)*GRECT_XSIZE == WINDOW_HEIGHT);
}

SdlWormikGui::~SdlWormikGui()
{
}

int SdlWormikGui::initWindow()
{
	SDL_SetWindowTitle(window, "Wormik");
	SDL_ShowCursor(SDL_DISABLE);

	if (!(bgSeasonImage = SDL_CreateTexture(windowRenderer, alphaPixelFormat, SDL_TEXTUREACCESS_TARGET, SIMG_WIDTH, SIMG_HEIGTH))) {
		game->error("couldn't create bgSeasonImage texture: %s\n", SDL_GetError());
		return -1;
	}
	SDL_SetTextureBlendMode(bgSeasonImage, SDL_BLENDMODE_BLEND);

#if 0
	colors[CLR_MENU_BG] = SDL_MapRGB(windowPixelFormat, 0, 0, 0);
	colors[CLR_MENUFNT] = SDL_MapRGB(windowPixelFormat, 255, 255, 255);
	colors[CLR_MENUEXC] = SDL_MapRGB(windowPixelFormat, 255, 255, 200);
#endif

	nextInvalidatedList = 0;
	invalidatedList[0].resetFlags(INVO_SDL_FULL); invalidatedList[1].resetFlags(INVO_SDL_FULL);

	return 0;
}

static SDL_RWops *findopenfile(const char *fname, ...)
{
	SDL_RWops *f = NULL;
	const char *p;
	va_list va;
	va_start(va, fname);

	while (!f && (p = va_arg(va, const char *))) {
		const char *arg = va_arg(va, const char *);
		switch (*p) {
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
				if (snprintf(buf, sizeof(buf), "%s/%s", arg, fname) >= (int)sizeof(buf))
					break;
				f = SDL_RWFromFile(buf, "r");
			}
			break;

		case 's':
			{
				FILE *fp;
				char buf[PATH_MAX+32];
				unsigned fl = strlen(fname);
				if (snprintf(buf, sizeof(buf), "find %s -follow -name %s -print", arg, fname) >= (int)sizeof(buf))
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

	if ((window = SDL_CreateWindow("Wormik", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, (game->getConfigInt("fullscreen", 1) ? SDL_WINDOW_FULLSCREEN : 0))) == NULL) {
		game->error("Couldn't create window: %s\n", SDL_GetError());
		goto err;
	}
	if ((windowRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE)) == NULL) {
		game->error("Couldn't create window renderer: %s\n", SDL_GetError());
		goto err;
	}
	//SDL_RenderSetLogicalSize(windowRenderer, WINDOW_WIDTH, WINDOW_HEIGHT);
	//SDL_RenderSetIntegerScale(windowRenderer, SDL_TRUE);
	alphaPixelFormat = SDL_PIXELFORMAT_ARGB8888;
	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(windowRenderer, &rendererInfo);
	game->error("Using renderer %s\n", rendererInfo.name);
	windowPixelFormat = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));
	for (size_t i = 0; i < rendererInfo.num_texture_formats; ++i) {
		if (SDL_ISPIXELFORMAT_ALPHA(rendererInfo.texture_formats[i])) {
			alphaPixelFormat = rendererInfo.texture_formats[i];
			break;
		}
	}

	textureRenderer = windowRenderer;

	if ((basicScreen = SDL_CreateTexture(textureRenderer, windowPixelFormat->format, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT)) == NULL) {
		game->error("Couldn't get basic screen texture: %s\n", SDL_GetError());
		goto err;
	}

	if (TTF_Init() < 0) {
		game->error("Couldn't init TTF lib: %s\n", TTF_GetError());
		goto err;
	}
	if ((unsigned)game->getConfigStr("font", buf, sizeof(buf)) < sizeof(buf)) {
		if (!(ffo = SDL_RWFromFile(buf, "r"))) {
			game->error("Couldn't open font file specified in config (trying default): %s\n", strerror(errno));
		}
	}
#if (defined _WIN32) || (defined _WIN64)
	if (!ffo) {
		if (GetSystemDirectory(buf, sizeof(buf)) <= sizeof(buf)) {
			strcat(buf, "/../fonts");
			ffo = findopenfile("courbd.ttf", "d", buf, NULL);
		}
	}
#elif (defined __APPLE__)
	if (!ffo) {
		ffo = findopenfile("Andale Mono.ttf", "d", "/Library/Fonts", "l", "", NULL);
	}
#else
	if (!ffo) {
		ffo = findopenfile("FreeMonoBold.ttf", "d", "/usr/share/fonts/truetype/freefont", "l", "", "s", "/usr/share/fonts", "s", "/usr/lib/X11/fonts", NULL);
	}
#endif
	if (!ffo) {
		game->fatal("Couldn't find/open output font\n");
		goto err;
	}
#if (defined _WIN32) || (defined _WIN64)
	strcat(buf, "/courbd.ttf");
	SDL_RWclose(ffo);
	font = TTF_OpenFont(buf, 15);
#else
	font = TTF_OpenFontRW(ffo, 1, game->getConfigInt("fontsize", 15));
#endif
	if (!font) {
		game->error("Couldn't open output font: %s\n", TTF_GetError());
		goto err;
	}
	if (initWindow() < 0) {
		goto err;
	}
	redraw = true;
	game->debug("Initialized GUI\n");
	return 0;

err:
	closeGui();
	return -1;
}

void SdlWormikGui::closeGui()
{
	if (font) {
		TTF_CloseFont(font);
		font = NULL;
	}
	if (TTF_WasInit()) {
		TTF_Quit();
	}
	if (seasonImage) {
		SDL_DestroyTexture(seasonImage);
		seasonImage = NULL;
	}
	SDL_ShowCursor(SDL_ENABLE);
	if (bgSeasonImage) {
		SDL_DestroyTexture(bgSeasonImage);
		bgSeasonImage = NULL;
	}
	if (basicScreen) {
		SDL_DestroyTexture(basicScreen);
		basicScreen = NULL;
	}
	if (textureRenderer) {
		if (textureRenderer != windowRenderer)
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
	if (true) {
		seasonImage = img;
	}
	else {
		if (!(seasonImage = SDL_CreateTexture(windowRenderer, alphaPixelFormat, SDL_TEXTUREACCESS_TARGET, SIMG_WIDTH, SIMG_HEIGTH))) {
			game->error("Failed to convert season image to current video texture: %s\n", SDL_GetError());
			return -1;
		}
		if (SDL_SetRenderTarget(textureRenderer, seasonImage) < 0) {
			game->fatal("failed to set rendering to bgSeasonImage: %s\n", SDL_GetError());
		}
		SDL_RenderCopy(textureRenderer, img, NULL, NULL);
	}
	if (SDL_SetRenderTarget(textureRenderer, bgSeasonImage) < 0) {
		game->fatal("failed to set rendering to bgSeasonImage: %s\n", SDL_GetError());
	}
	s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
	d.w = s.w; d.h = s.h;
	for (d.y = 0; d.y < SIMG_HEIGTH; d.y += GRECT_YSIZE) {
		for (d.x = 0; d.x < SIMG_WIDTH; d.x += GRECT_XSIZE) {
			if (SDL_RenderCopy(textureRenderer, seasonImage, &s, &d) < 0) {
				game->fatal("failed to render to bgSeasonImage from seasonImage: %s\n", SDL_GetError());
			}
		}
	}
	SDL_RenderCopy(textureRenderer, seasonImage, NULL, NULL);
	SDL_SetRenderTarget(textureRenderer, NULL);
	return 0;
}

int SdlWormikGui::initLevelImage(int season)
{
	SDL_RWops *sf;
	int err;
	char fname[PATH_MAX];
	char dpath[PATH_MAX];
	// we have to use SDL_Surface as the texture does not allow us to read
	// pixel values
	SDL_Surface *img;
	SDL_Texture *texture;
	SDL_Color c[sizeof(colors)/sizeof(colors[0])];
	unsigned i;

	if ((unsigned)game->getConfigStr("datapath", dpath, sizeof(dpath)) >= sizeof(dpath))
		dpath[0] = '\0';

	for (;;) {
		if (snprintf(fname, sizeof(fname), "wormik_%d.png", season) >= (int)sizeof(fname)) {
			game->fatal("filename too long\n");
		}
		if (!(sf = findopenfile(fname, "d", RESOURCE_DIR, "d", (dpath[0] == '\0') ? "." : dpath, NULL))) {
			if (season == 0)
				game->fatal("failed to open %s: %s\n", fname, strerror(errno));
			season = 0;
		}
		else
			break;
	}
#if (defined _WIN32) || (defined _WIN64)
	sprintf(fname, "wormik_%d.png", season);
	SDL_RWclose(sf);
	img = IMG_Load(fname);
#else
	img = IMG_Load_RW(sf, 1);
#endif
	if (!img) {
		game->fatal("failed to process image %s: %s\n", fname, SDL_GetError());
	}
	if (img->w != SIMG_WIDTH || img->h != SIMG_HEIGTH || img->format->BytesPerPixel != 4) {
		game->fatal("%s: image has to be %dx%dx32 sized (is %dx%dx%d)\n", fname, SIMG_WIDTH, SIMG_HEIGTH, img->w, img->h, img->format->BytesPerPixel*4);
	}

	if (SDL_LockSurface(img) < 0) {
		game->fatal("cannot lock surface: %s\n", SDL_GetError());
	}
	// find basic drawing colors, these have alpha 0 in original image
#if 1
	i = 0;
	for (unsigned y = 0; i < sizeof(c)/sizeof(c[0]) && y < (unsigned)img->h; y++) {
		for (unsigned x = 0; i < sizeof(c)/sizeof(c[0]) && x < (unsigned)img->w; x++) {
			Uint8 a;
			SDL_GetRGBA(*(Uint32 *)((char *)img->pixels+y*img->pitch+x*img->format->BytesPerPixel), img->format, &c[i].r, &c[i].g, &c[i].b, &a);
			if (a == 0) {
				//printf("found %dx%d: (%d, %d, %d)\n", y, x, c[i].r, c[i].g, c[i].b);
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

	SDL_UnlockSurface(img);
	if ((texture = SDL_CreateTextureFromSurface(windowRenderer, img)) == NULL) {
		game->fatal("failed to convert surface to texture: %s\n", SDL_GetError());
	}
	SDL_FreeSurface(img);
	err = initSeasonImage(texture);
	if (err < 0)
		return err;

	SDL_SetRenderTarget(textureRenderer, basicScreen);
	drawStaticScreen(INVO_SDL_FULL);
	SDL_SetRenderTarget(textureRenderer, NULL);

	invalidateOutput(-INVO_SDL_FULL, NULL);
	return season;
}

int SdlWormikGui::newLevel(int season)
{
	int err = initLevelImage(season);
	if (err < 0) {
		game->fatal();
	}
	return err;
}

void SdlWormikGui::drawStatic(void *gc, unsigned x, unsigned y, unsigned short cont)
{
	SDL_Rect s, d;
	unsigned sx, sy;
	d.x = x*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
	d.w = s.w = GRECT_XSIZE; d.h = s.h = GRECT_YSIZE;
	findImagePos(cont, &sx, &sy);
	s.x = sx; s.y = sy;
	SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
}

void SdlWormikGui::drawPoint(void *gc, unsigned x, unsigned y, unsigned short cont)
{
	if (cont == WormikGame::GR_NONE || cont == WormikGame::GR_WALL)
		return;
	SDL_Rect s, d;
	unsigned sx, sy;
	d.x = x*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
	d.w = s.w = GRECT_XSIZE; d.h = s.h = GRECT_YSIZE;
	findImagePos(cont, &sx, &sy);
	s.x = sx; s.y = sy;
	SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
}

int SdlWormikGui::drawNewdef(void *gc, unsigned x, unsigned y, unsigned short cont, double timeout, double total)
{
	SDL_Rect s, d;
	unsigned sx, sy;
	int alpha = (int)(255*(timeout-diffGameTime)/total);
	d.x = x*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
	d.w = s.w = GRECT_XSIZE; d.h = s.h = GRECT_YSIZE;
	findImagePos(cont, &sx, &sy);
	if (alpha <= 0) {
		s.x = sx; s.y = sy;
		SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
		return 0;
	}
	else {
		if (alpha >= 256) // possible because of newdef latency
			alpha = 255;
		// not needed as we start from fresh copy always
		//s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE;
		//SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		s.x = sx; s.y = sy;
		SDL_SetTextureAlphaMod(bgSeasonImage, 255-alpha);
		SDL_RenderCopy(windowRenderer, bgSeasonImage, &s, &d);
		SDL_SetTextureAlphaMod(bgSeasonImage, 255);
		return 1;
	}
}

void SdlWormikGui::invalidateOutput(int len, unsigned (*points)[2])
{
	if (len == 0) {
		return;
	}
	else if (len < 0) {
		invalidatedList[0].addFlags(-len);
		invalidatedList[1].addFlags(-len);
	}
	else {
		while (len-- > 0) {
			invalidatedList[0].addObject(points[len][0], points[len][1]);
			invalidatedList[1].addObject(points[len][0], points[len][1]);
		}
	}
	redraw = true;
}

void SdlWormikGui::invalidateAll()
{
	invalidateOutput(-INVO_SDL_FULL, NULL);
}

void SdlWormikGui::drawText(int x, int y, Uint32 color, const char *text)
{
	SDL_Color clr;
	SDL_Surface *fs;
	SDL_Rect d;
	SDL_GetRGB(color, windowPixelFormat, &clr.r, &clr.g, &clr.b);
	fs = TTF_RenderText_Blended(font, text, clr);
	SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs);
	d.x = (x >= 0) ? x : (-x-fs->w); d.y = (y >= 0) ? y : (-y-fs->h);
	d.w = fs->w; d.h = fs->h;
	SDL_RenderCopy(windowRenderer, texture, NULL, &d);
	SDL_DestroyTexture(texture);
	SDL_FreeSurface(fs);
}

void SdlWormikGui::drawLinedTextf(int x, int y, Uint32 color, const char *fmt, ...)
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
	d.y = (y >= 0) ? y : (-y-th);
	fs[nrows] = NULL;
	for (nrows = 0; fs[nrows]; nrows++) {
		d.x = (x >= 0) ? x : (-x-fs[nrows]->w);
		d.w = fs[nrows]->w; d.h = fs[nrows]->h;
		SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs[nrows]);
		SDL_RenderCopy(windowRenderer, texture, NULL, &d);
		d.y += fs[nrows]->h;
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(fs[nrows]);
	}
}

void SdlWormikGui::drawStaticScreen(int flags)
{
	unsigned x, y;
	SDL_Rect s, d;

	if ((flags&INVO_BOARD) != 0) {
		s.x = SP_BACK_X*GRECT_XSIZE; s.y = SP_BACK_Y*GRECT_YSIZE; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
		game->outStatic(NULL, 0, 0, game->GAME_XSIZE-1, game->GAME_YSIZE-1);
	}

	if ((flags&INVO_MENU) != 0) {
		findImagePos(WormikGame::GR_WALL, &x, &y);
		s.x = x; s.y = y; s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
		d.w = GRECT_XSIZE; d.h = GRECT_YSIZE;
		SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENU_BG]>>16), (Uint8)(colors[CLR_MENU_BG]>>8), (Uint8)(colors[CLR_MENU_BG]>>0), (Uint8)(colors[CLR_MENU_BG]>>24));
		for (y = 1; y < MENU_HEIGHT_POINTS-1; y++) {
			d.x = AREA_INFO_X+(MENU_WIDTH_POINTS-1)*GRECT_XSIZE; d.y = y*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
		for (x = 0; x < MENU_WIDTH_POINTS; x++) {
			d.x = AREA_INFO_X+x*GRECT_XSIZE;
			d.y = 0;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = MENU_SEP_SCORE_POINTS*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = MENU_SEP_SNAKE_POINTS*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
			d.y = (MENU_HEIGHT_POINTS-1)*GRECT_YSIZE;
			SDL_RenderFillRect(windowRenderer, &d);
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
	}

	if ((flags&INVO_DESC) != 0) {
		s.w = GRECT_XSIZE; s.h = GRECT_YSIZE;
		d.x = AREA_INFO_X; d.y = (MENU_SEP_INFO_POINTS+1)*GRECT_YSIZE; d.w = (MENU_WIDTH_POINTS-1)*GRECT_XSIZE; d.h = (MENU_HEIGHT_POINTS-MENU_SEP_INFO_POINTS-2)*GRECT_YSIZE;
		SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENU_BG]>>16), (Uint8)(colors[CLR_MENU_BG]>>8), (Uint8)(colors[CLR_MENU_BG]>>0), (Uint8)(colors[CLR_MENU_BG]>>24));
		SDL_RenderFillRect(windowRenderer, &d);
		d.w = GRECT_XSIZE; d.h = GRECT_YSIZE;
		findImagePos(WormikGame::GR_POSITIVE, &x, &y); s.x = x; s.y = y; d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE+GRECT_YSIZE+MENU_DESC_SPACING_PX;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-MENU_DESC_RIGHT_PX, d.y, colors[CLR_MENU_FONT], "S+2");
		findImagePos(WormikGame::GR_POSITIVE_2, &x, &y); s.x = x; s.y = y; d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE+GRECT_YSIZE+MENU_DESC_SPACING_PX+GRECT_YSIZE*2;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-MENU_DESC_RIGHT_PX, d.y, colors[CLR_MENU_FONT], "S+5");
		findImagePos(WormikGame::GR_NEGATIVE, &x, &y); s.x = x; s.y = y; d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE+GRECT_YSIZE+MENU_DESC_SPACING_PX+GRECT_YSIZE*4;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-MENU_DESC_RIGHT_PX, d.y, colors[CLR_MENU_FONT], "H-1");
		findImagePos(WormikGame::GR_DEATH, &x, &y); s.x = x; s.y = y; d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE+GRECT_YSIZE+MENU_DESC_SPACING_PX+GRECT_YSIZE*6;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-MENU_DESC_RIGHT_PX, d.y, colors[CLR_MENU_FONT], "Death");
		findImagePos(WormikGame::GR_EXIT, &x, &y); s.x = x; s.y = y; d.y = MENU_SEP_INFO_POINTS*GRECT_YSIZE+GRECT_YSIZE+MENU_DESC_SPACING_PX+GRECT_YSIZE*8;
		SDL_RenderCopy(windowRenderer, seasonImage, &s, &d); drawText(-MENU_DESC_RIGHT_PX, d.y, colors[CLR_MENU_FONT], "Exit");
	}
}

unsigned SdlWormikGui::drawBase(void)
{
	unsigned ret = 0;
	SDL_Rect d;
	InvalidatedList *currentIl = &invalidatedList[nextInvalidatedList];

	// reset all screen prior to drawing, reusing old one does not work everywhere correctly
	currentIl->resetFlags(INVO_SDL_FULL);

	SDL_RenderCopy(windowRenderer, basicScreen, NULL, NULL);

	if ((currentIl->flags&INVO_BOARD) != 0) {
		game->outGame(NULL, 0, 0, WormikGame::GAME_XSIZE-1, WormikGame::GAME_YSIZE-1);
	}
	else {
		unsigned i;
		for (i = 0; i < currentIl->invalidatedLength; i++) {
			game->outPoint(NULL, currentIl->invalidatedList[i][0], currentIl->invalidatedList[i][1]);
		}
	}
	currentIl->invalidatedLength = 0;

	if ((currentIl->flags&INVO_NEW_DEFS) != 0) {
		if (game->outNewdefs(NULL) > 0)
			ret |= INVO_NEW_DEFS;
	}

	if ((currentIl->flags&(INVO_RECORD|INVO_SCORE|INVO_GAME_STATE|INVO_HEALTH|INVO_LENGTH)) != 0) {
		d.w = (MENU_WIDTH_POINTS-1)*GRECT_XSIZE; d.x = AREA_INFO_X;
		if ((currentIl->flags&INVO_RECORD) != 0) {
			struct tm t; char tc[32];
			int record; time_t rectime; bool isNow;
			isNow = game->getRecord(&record, &rectime);
			t = *localtime(&rectime); strftime(tc, sizeof(tc), "%Y-%m-%d %H:%M", &t);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[CLR_MENU_BG]>>16), (Uint8)(colors[CLR_MENU_BG]>>8), (Uint8)(colors[CLR_MENU_BG]>>0), (Uint8)(colors[CLR_MENU_BG]>>24));
			d.y = GRECT_YSIZE; d.h = (MENU_SEP_FIRST_POINTS-1)*GRECT_YSIZE; SDL_RenderFillRect(windowRenderer, &d);
			drawLinedTextf(-MENU_TEXT_RIGHT_PX, d.y+MENU_FONT_HEIGHT_PX, colors[isNow ? CLR_EXCEPTION_FONT : CLR_MENU_FONT], "Record: %d\n%s\n", record, (rectime == 0) ? " " : tc);
		}
		if ((currentIl->flags&(INVO_SCORE|INVO_GAME_STATE)) != 0) {
			int score, total, exit;
			int level, season;
			game->getState(&level, &season);
			exit = game->getScore(&score, &total);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[0]>>16), (Uint8)(colors[0]>>8), (Uint8)(colors[0]>>0), (Uint8)(colors[0]>>24));
			d.y = (MENU_SEP_FIRST_POINTS+1)*GRECT_YSIZE; d.h = (MENU_SEP_SNAKE_POINTS-MENU_SEP_FIRST_POINTS-1)*GRECT_YSIZE; SDL_RenderFillRect(windowRenderer, &d);
			drawLinedTextf(-MENU_TEXT_RIGHT_PX, d.y+MENU_FONT_HEIGHT_PX, colors[(score >= exit)?CLR_EXCEPTION_FONT:CLR_MENU_FONT], "Score: %d\nLevel: %d/%d\n", total, level, (total-score)+exit);
		}
		if ((currentIl->flags&(INVO_HEALTH|INVO_LENGTH)) != 0) {
			int health, length;
			game->getSnakeInfo(&health, &length);
			SDL_SetRenderDrawColor(windowRenderer, (Uint8)(colors[0]>>16), (Uint8)(colors[0]>>8), (Uint8)(colors[0]>>0), (Uint8)(colors[0]>>24));
			d.y = (MENU_SEP_SNAKE_POINTS+1)*GRECT_YSIZE; d.h = (MENU_SEP_INFO_POINTS-MENU_SEP_SNAKE_POINTS-1)*GRECT_YSIZE; SDL_RenderFillRect(windowRenderer, &d);
			drawLinedTextf(-MENU_TEXT_RIGHT_PX, d.y+MENU_FONT_HEIGHT_PX, colors[(health <= 1)?CLR_EXCEPTION_FONT:CLR_MENU_FONT], "Health: %d\nLength: %d\n", health, length);
		}
	}

	return ret;
}

unsigned SdlWormikGui::drawAnnounce(unsigned n, const char *const text[])
{
	SDL_Color clr;
	unsigned i;
	SDL_Surface **fs = (SDL_Surface **)alloca(n*sizeof(SDL_Surface *));
	SDL_Rect s, d;
	int w, h;
	int ey, ex;

	SDL_GetRGB(colors[CLR_ANNOUNCEMENT_FONT], windowPixelFormat, &clr.r, &clr.g, &clr.b);

	w = h = 0;
	for (i = 0; i < n; i++) {
		fs[i] = TTF_RenderText_Blended(font, text[i], clr);
		if (fs[i]->w > w)
			w = fs[i]->w;
		h += fs[i]->h;
	}
	w += 2*GRECT_XSIZE; h += GRECT_YSIZE;
	s.x = SP_MSG_X*GRECT_XSIZE; s.y = SP_MSG_Y*GRECT_YSIZE; s.h = GRECT_YSIZE;
	for (d.y = (WINDOW_HEIGHT-h)/2, ey = d.y+h; d.y < ey; d.y += GRECT_YSIZE) {
		if (d.y+s.h > ey)
			s.h = ey-d.y;
		s.w = GRECT_XSIZE;
		d.w = s.w; d.h = s.h;
		for (d.x = (MENU_X_POINTS*GRECT_XSIZE-w)/2, ex = d.x+w; d.x < ex; d.x += GRECT_XSIZE) {
			if (d.x+s.w > ex)
				s.w = ex-d.x;
			SDL_RenderCopy(windowRenderer, seasonImage, &s, &d);
		}
	}
	d.y = (WINDOW_HEIGHT-h+GRECT_YSIZE)/2;
	for (i = 0; i < n; i++) {
		d.x = (WINDOW_HEIGHT-fs[i]->w)/2;
		d.w = fs[i]->w; d.h = fs[i]->h;
		SDL_Texture *texture = SDL_CreateTextureFromSurface(windowRenderer, fs[i]);
		SDL_RenderCopy(windowRenderer, texture, NULL, &d);
		d.y += fs[i]->h;
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(fs[i]);
	}
	return 0;
}

void SdlWormikGui::drawFinish(unsigned rerenderFlags)
{
	SDL_RenderPresent(windowRenderer);
	invalidatedList[nextInvalidatedList].resetFlags(rerenderFlags);
	nextInvalidatedList ^= 1;
	redraw = false;
}

enum {
	STDE_UNKNOWN = -1,
	STDE_PROCESSED = 0,
	STDE_QUIT = 1,
	STDE_TIMEOUT = 2,
	STDE_SHOW_BASE = 3,
	STDE_SHOW_PAUSE = 3,
	STDE_SHOW_HELP = 4,
	STDE_SHOW_ABOUT = 5,
	STDE_SHOW_MAX = 5,
};

int SdlWormikGui::processStandardEvent(SDL_Event *ev)
{
	game->debug("Got event: %d\n", ev->type);
	switch (ev->type) {
	case SDL_QUIT:
		return STDE_QUIT;

	case SDL_KEYDOWN:
		switch (ev->key.keysym.sym) {
		case SDLK_ESCAPE:
		case SDLK_q:
			return STDE_QUIT;

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
				int season;
				game->getState(NULL, &season);
				if (initLevelImage(season) < 0) {
					game->fatal("Failed to load season image\n");
				}
			}
			invalidateAll();
			return STDE_SHOW_PAUSE;

		case SDLK_h:
		case SDLK_HELP:
		case SDLK_F1:
			return STDE_SHOW_HELP;

		case SDLK_a:
			return STDE_SHOW_ABOUT;

		default:
			break;
		}
		break;

	case SDL_WINDOWEVENT:
		invalidatedList[0].resetFlags(INVO_SDL_FULL); invalidatedList[1].resetFlags(INVO_SDL_FULL);
		redraw = true;
		return STDE_PROCESSED;
	}
	return STDE_UNKNOWN;
}

int SdlWormikGui::showPopup(int messageEventId)
{
	int ret = -1;
	invalidateAll();
	do {
		if (redraw) {
			int ntext = 0;
			const char *text[20];

			drawBase();
			switch (messageEventId) {
			case STDE_SHOW_HELP:
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

			case STDE_SHOW_ABOUT:
				text[ntext++] = "Wormik " SVERSION;
				text[ntext++] = "See project homepage at";
				text[ntext++] = "http://github.com/kvr000/wormik/";
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
		SDL_Event ev;
		if (SDL_WaitEvent(&ev) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		int stdEvent = processStandardEvent(&ev);
		if (stdEvent >= STDE_SHOW_BASE && stdEvent <= STDE_SHOW_MAX) {
			ret = stdEvent;
		}
		else switch (stdEvent) {
		case STDE_PROCESSED:
			break;

		case STDE_QUIT:
			redraw = true;
			ret = STDE_PROCESSED;
			break;

		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym) {
				case SDLK_SPACE:
				case SDLK_RETURN:
					redraw = true;
					ret = STDE_PROCESSED;
					break;

				default:
					break;
				}
				break;
			}
			break;
		}
	} while (ret < 0);
	invalidateAll();
	return ret;
}

bool SdlWormikGui::announce(int announcement)
{
	invalidateAll();
	for (;;) {
		if (redraw) {
			int ntext = 0;
			const char *text[2];

			drawBase();
			game->debug("Drawing announce\n");
			switch (announcement) {
			case ANC_DEAD:
				text[ntext++] = "You are dead!";
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
		SDL_Event ev;
		if (SDL_WaitEvent(&ev) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		int stdEvent = processStandardEvent(&ev);
reswitch:
		if (stdEvent == STDE_SHOW_PAUSE) {
			continue;
		}
		else if (stdEvent >= STDE_SHOW_BASE && stdEvent <= STDE_SHOW_MAX) {
			stdEvent = showPopup(stdEvent);
			redraw = true;
			goto reswitch;
		}
		else switch (stdEvent) {
		case STDE_PROCESSED:
			break;

		case STDE_QUIT:
			return true;

		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_KEYDOWN:
				switch (ev.key.keysym.sym) {
				case SDLK_SPACE:
				case SDLK_RETURN:
					invalidateAll();
					return false;

				default:
					break;
				}
				break;
			}
			break;
		}
	}
}

bool SdlWormikGui::waitStart()
{
	return waitNext(INFINITY);
}

bool SdlWormikGui::waitNext(double waitInterval)
{
	double nextRedraw = invalidatedList[nextInvalidatedList^1].flags != 0 ? getDoubleTime()+REDRAW_TIME : INFINITY;
	for (;;) {
		int r;
		SDL_Event ev;
		double expire = INFINITY;
		if (redraw) {
			expire = 0;
		}
		else if ((invalidatedList[nextInvalidatedList].flags&INVO_DYN_FLAGS) != 0) {
			if (nextRedraw < expire) {
				expire = nextRedraw;
			}
		}
		else {
			nextRedraw = INFINITY;
		}
		if (lastMove+waitInterval < expire) {
			expire = lastMove+waitInterval;
		}
		double eventWaitMs = ceil((expire-getDoubleTime())*1000);
		int eventWaitMsCut = eventWaitMs < 0 ? 0 : eventWaitMs > INT_MAX ? INT_MAX : (int)eventWaitMs;
		game->debug("waiting for %d\n", eventWaitMsCut);
		if ((r = SDL_WaitEventTimeout(&ev, eventWaitMsCut)) < 0)
			game->fatal("SDL WaitEvent: %s\n", SDL_GetError());
		int stdEvent = r == 0 ? STDE_TIMEOUT : processStandardEvent(&ev);
reswitch:
		game->debug("std event: %d\n", stdEvent);
		if (stdEvent >= STDE_SHOW_BASE && stdEvent <= STDE_SHOW_MAX) {
			lastMove = 0;
			diffGameTime = waitInterval;
			waitInterval = INFINITY;
			nextRedraw = INFINITY;
			if (stdEvent == STDE_SHOW_PAUSE)
				continue;
			stdEvent = showPopup(stdEvent);
			goto reswitch;
		}
		else switch (stdEvent) {
		case STDE_PROCESSED:
			break;

		case STDE_QUIT:
			return true;

		case STDE_TIMEOUT:
			{
				double currentTime = getDoubleTime();
				if (nextRedraw <= currentTime) {
					redraw = true;
				}
				if (lastMove+waitInterval <= currentTime) {
					game->debug("Game time\n");
					lastMove = lastMove+waitInterval;
					return false;
				}
				if (redraw) {
					game->debug("Redrawing\n");
					unsigned rerenderFlags;
					double time = getDoubleTime();
					if ((diffGameTime = time-lastMove) > waitInterval) {
						diffGameTime = waitInterval;
					}
					else if (diffGameTime < 0 || waitInterval == INFINITY) {
						diffGameTime = 0;
					}
					rerenderFlags = drawBase();
					drawFinish(rerenderFlags);
					nextRedraw = rerenderFlags != 0 && waitInterval != INFINITY ? currentTime+REDRAW_TIME : INFINITY;
				}
			}
			break;

		case STDE_UNKNOWN:
			switch (ev.type) {
			case SDL_KEYDOWN:
				{
					int dir = -1;
					switch (ev.key.keysym.sym) {
					case SDLK_p:
						waitInterval = INFINITY;
						stdEvent = STDE_SHOW_PAUSE;
						goto reswitch;

					case SDLK_RIGHT:
						dir = WormikGame::SDIR_EAST;
						break;

					case SDLK_UP:
						dir = WormikGame::SDIR_NORTH;
						break;

					case SDLK_LEFT:
						dir = WormikGame::SDIR_WEST;
						break;

					case SDLK_DOWN:
						dir = WormikGame::SDIR_SOUTH;
						break;

					default:
						break;
					}
					if (dir >= 0) {
						game->changeDirection(dir);
						if (waitInterval == INFINITY) {
							lastMove = getDoubleTime();
							return false;
						}
					}
				}
				break;
			}
		}
	}
}

WormikGui *create_WormikGui(void)
{
	return new SdlWormikGui();
}


} } } };
