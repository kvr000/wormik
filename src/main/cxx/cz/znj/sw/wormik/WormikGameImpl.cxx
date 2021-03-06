/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * Game class
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include <limits.h>
#include <fcntl.h>
#include <time.h>

#include <sys/stat.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "cz/znj/sw/wormik/platform.hxx"

#include "cz/znj/sw/wormik/WormikGame.hxx"
#include "cz/znj/sw/wormik/WormikGui.hxx"

namespace cz { namespace znj { namespace sw { namespace wormik {


class WormikGameImpl: public WormikGame
{
protected:
	/* gui interface */
	WormikGui *			gui;

	/* game state */
	int				state_game;			/* game state */

	unsigned			state_level;
	unsigned			state_season;
	unsigned			state_exitscore;
	unsigned			state_levscore;
	unsigned			state_totscore;

	typedef struct element_pos
	{
		unsigned char			x, y;
	} element_pos;

	/* stats */
	int				stats_record;
	time_t				stats_rectime;

	/* snake data */
	unsigned			snake_dir;
	element_pos			snake_pos[GAME_XSIZE*GAME_YSIZE];
	unsigned			snake_len;
	int				snake_grow;
	unsigned			snake_health;

	/* board state */
	enum {
		DEFCNTSMAX			= 4,
	};

	typedef struct def_state
	{
		board_def			def;
		unsigned			cnt;
		unsigned			max;
		float				timeout;
	} def_state;

	typedef struct new_def {
		short				x, y;
		unsigned			defsi;
		float				timeout;
	} new_def;

	board_def			board[GAME_YSIZE][GAME_XSIZE];	/* game board */
	def_state			defcnts[DEFCNTSMAX];	/* regenerable defs count */
	unsigned			freecnt;		/* count of empty tiles */
	new_def				newdefs[32];		/* newly generated defs */
	unsigned			ndlen;			/* (and their count) */

	bool				isDebug;

public:
	/* constructor */		WormikGameImpl();

public:
	virtual void			setGui(WormikGui *gui);
	virtual void			run(void);

	virtual int			getConfigStr(const char *name, char *buf, int blen);
	virtual int			getConfigInt(const char *name, int defval);
	virtual void			setConfig(const char *name, int value);
	virtual void			setConfig(const char *name, const char *value);

	virtual int			debug(const char *fmt, ...);
	virtual int			error(const char *fmt, ...);
	virtual void			fatal();
	virtual void			fatal(const char *fmt, ...);

	virtual void			changeDirection(int dir);
	virtual int			getState(int *level, int *season);
	virtual int			getScore(int *score, int *total);
	virtual void			getSnakeInfo(int *health, int *length);
	virtual bool 			getRecord(int *record, time_t *rectime);

	virtual void			outPoint(void *gc, unsigned x, unsigned y);
	virtual void			outStatic(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1);
	virtual void			outGame(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1);
	virtual int			outNewdefs(void *gc);

protected:
	void				initBoard();
	void				generateType(board_def type, int num, board_def old);
	int				generateWalls(unsigned headx, unsigned heady);

	void				decDefs(board_def def);
	int				genDef(float latency);
	bool				deleteNewDef(unsigned x, unsigned y);

	void				saveRecord();

	void				exit(int n);

	void				printLogTimestamp();
};

static const int direction_moves[4][2] = { { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1} };

static int randrange(int min, int max)
{
	return min+(int)(rand()*(uint64_t)(max-min+1)/((uint64_t)RAND_MAX+1));
}

WormikGameImpl::WormikGameImpl()
{
	char buf[1024];
	int i = 0;
	if ((unsigned)getConfigStr("record", buf, sizeof(buf)) >= sizeof(buf) || sscanf(buf, "%d/%ld", &stats_record, &stats_rectime) < 2) {
		stats_record = 0;
		stats_rectime = 0;
	}

	isDebug = getConfigInt("debug", 0) != 0;

	defcnts[i].def = GR_EXIT; defcnts[i].max = 0; defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_POSITIVE; defcnts[i].max = TILES_COUNT_POSITIVE; defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_POSITIVE_2; defcnts[i].max = TILES_COUNT_POSITIVE_2; defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_NEGATIVE; defcnts[i].max = TILES_COUNT_NEGATIVE; defcnts[i].timeout = 2.0; i++;
	assert(i == DEFCNTSMAX);
}

void WormikGameImpl::setGui(WormikGui *gui_)
{
	gui = gui_;
}

/* *config*: stupid simple implementation, but written really quickly ;) */
FILE *openConfig(int mode)
{
	int fd;
	const char *home;
	char buf[PATH_MAX];
	if ((home = getenv("HOME")) == NULL)
		home = ".";
	if (snprintf(buf, sizeof(buf), "%s/.config", home) < (int)sizeof(buf))
		mkdir(buf, 0777);
	if (snprintf(buf, sizeof(buf), "%s/.config/wormikrc", home) >= (int)sizeof(buf))
		return NULL;
#if (defined _WIN32) || (defined _WIN64)
	if ((fd = open(buf, mode|O_CREAT|O_BINARY, 0666)) < 0)
		return NULL;
	return fdopen(fd, (mode == O_RDONLY) ? "rb" : "r+b");
#else
	if ((fd = open(buf, mode|O_CREAT, 0666)) < 0)
		return NULL;
	return fdopen(fd, (mode == O_RDONLY) ? "r" : "r+");
#endif
}

void WormikGameImpl::setConfig(const char *name, int value)
{
	char buf[32];
	sprintf(buf, "%d", value);
	setConfig(name, buf);
}

void WormikGameImpl::setConfig(const char *name, const char *value)
{
	char buf[256];
	FILE *cf;
	int nl;
	int ll;
	int rc;
	if (!(cf = openConfig(O_RDWR)))
		return;
	nl = strlen(name);
	while (fgets(buf, sizeof(buf), cf) || (buf[0] = '\0', 0)) {
		if (memcmp(buf, name, nl) != 0)
			continue;
		if (buf[nl] != '=')
			continue;
		break;
	}
	ll = strlen(buf);
	while ((rc = fread(buf, 1, sizeof(buf), cf)) > 0) {
		if (fseek(cf, -ll-rc, SEEK_CUR) < 0)
			goto err;
		if (fwrite(buf, 1, rc, cf) < 0)
			goto err;
		if (fseek(cf, +ll, SEEK_CUR) < 0)
			goto err;
	}
	fseek(cf, -ll, SEEK_CUR);
	fprintf(cf, "%s=%s\n", name, value);
	fflush(cf);
	if (ftruncate(fileno(cf), ftell(cf)) != 0) {
		(void)0; // ignore error
	}
err:
	fclose(cf);
}

int WormikGameImpl::getConfigInt(const char *name, int defval)
{
	char buf[16];
	if ((unsigned)getConfigStr(name, buf, sizeof(buf)) >= sizeof(buf))
		return defval;
	return strtol(buf, NULL, 0);
}

int WormikGameImpl::getConfigStr(const char *name, char *str, int buflen)
{
	char buf[256];
	FILE *cf;
	int nl;
	int vl = -1;
	if (!(cf = openConfig(O_RDONLY)))
		return -1;
	nl = strlen(name);
	while (fgets(buf, sizeof(buf), cf)) {
		char *p;
		for (p = buf; isspace(*p); p++);
		if (memcmp(p, name, nl) != 0)
			continue;
		for (p += nl; isspace(*p); p++);
		if (*p != '=')
			continue;
		for (p++; isspace(*p); p++);
		if (((vl = strlen(p)) == 0 || p[--vl] != '\n') && (vl = -1, 1))
			continue;
		memcpy(str, p, (vl >= buflen)?buflen:vl);
		(vl < buflen) && (str[vl] = '\0');
		break;
	}
	fclose(cf);
	return vl;
}

void WormikGameImpl::changeDirection(int dir_)
{
	if (dir_ != GR_GET_IN(board[snake_pos[0].y][snake_pos[0].x]))
		snake_dir = dir_;
}

int WormikGameImpl::getState(int *level, int *season)
{
	if (level != NULL)
		*level = state_level;
	if (season != NULL)
		*season = state_season;
	return state_game;
}

int WormikGameImpl::getScore(int *score, int *total)
{
	*score = state_levscore;
	*total = state_totscore;
	return state_exitscore;
}

void WormikGameImpl::getSnakeInfo(int *health, int *length)
{
	*health = snake_health;
	*length = snake_len;
}

bool WormikGameImpl::getRecord(int *record, time_t *rectime)
{
	*rectime = stats_rectime;
	if ((*record = stats_record) < 0) {
		*record = -*record;
		return true;
	}
	return false;
}

void WormikGameImpl::outPoint(void *gc, unsigned x, unsigned y)
{
	if (board[y][x] != GR_NEW_DEF)
		gui->drawPoint(gc, x, y, board[y][x]);
}

void WormikGameImpl::outStatic(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
	assert(x0 >= 0);
	assert(y0 >= 0);
	assert(x1 < GAME_XSIZE);
	assert(y1 < GAME_YSIZE);
	for (unsigned y = y0; y <= y1; y++) {
		for (unsigned x = x0; x <= x1; x++) {
			gui->drawStatic(gc, x, y, board[y][x] == GR_WALL ? board[y][x] : GR_NONE);
		}
	}
}

void WormikGameImpl::outGame(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
	assert(x0 >= 0);
	assert(y0 >= 0);
	assert(x1 < GAME_XSIZE);
	assert(y1 < GAME_YSIZE);
	for (unsigned y = y0; y <= y1; y++) {
		for (unsigned x = x0; x <= x1; x++) {
			outPoint(gc, x, y);
		}
	}
}

int WormikGameImpl::outNewdefs(void *gc)
{
	int ret = 0;
	unsigned i;
	for (i = 0; i < ndlen; i++) {
		ret += gui->drawNewdef(gc, newdefs[i].x, newdefs[i].y, defcnts[newdefs[i].defsi].def, newdefs[i].timeout, defcnts[newdefs[i].defsi].timeout);
	}
	return ret;
}

static void gwCheckAccess(short (*access)[WormikGameImpl::GAME_XSIZE], unsigned x, unsigned y)
{
	unsigned tlen;
	short tlist[WormikGameImpl::GAME_YSIZE*WormikGameImpl::GAME_XSIZE][2];

	tlen = 0;
	tlist[tlen][0] = x; tlist[tlen][1] = y; tlen++;

	while (tlen > 0) {
		unsigned bi = 0; unsigned bx, by;
		for (unsigned i = 1; i < tlen; i++) {
			if (access[tlist[i][1]][tlist[i][0]] < access[tlist[bi][1]][tlist[bi][0]])
				bi = i;
		}
		bx = tlist[bi][0]; by = tlist[bi][1];
		memmove(tlist+bi, tlist+bi+1, (--tlen-bi)*sizeof(tlist[0]));
		for (unsigned dm = 0; dm < 4; dm++) {
			unsigned cx, cy;
			cx = bx+direction_moves[dm][0]; cy = by+direction_moves[dm][1];
			if (access[cy][cx] < 0)
				continue;
			if (access[cy][cx] == SHRT_MAX) {
				tlist[tlen][0] = cx; tlist[tlen][1] = cy;
				tlen++;
			}
			if (access[cy][cx] > access[by][bx]+1)
				access[cy][cx] = access[by][bx]+1;
		}
	}
}

int WormikGameImpl::generateWalls(unsigned headx, unsigned heady)
{
	unsigned wallcnt, deathcnt;
	short access[GAME_YSIZE][GAME_XSIZE];

	for (unsigned y = 0; y < GAME_YSIZE; y++) {
		access[y][0] = -1; access[y][GAME_XSIZE-1] = -1;
		for (unsigned x = 0; x < GAME_XSIZE; x++) {
			if (board[y][x] == GR_INVALID || board[y][x] == GR_NONE)
				access[y][x] = SHRT_MAX;
			else {
				access[y][x] = -1;
			}
		}
	}
	access[heady][headx] = 0;
	access[heady+1][headx] = -1;

	for (wallcnt = 0; wallcnt < TILES_COUNT_WALLS; ) {
		unsigned x, y;
		unsigned l;
		bool isok = true;
		unsigned cacc;
		l = randrange(1, freecnt);
		assert(l > 0 && l <= freecnt);
		for (y = 1; ; y++) {
			assert(y < GAME_YSIZE-1);
			for (x = 1; x < GAME_XSIZE-1; x++) {
				if (board[y][x] != GR_INVALID)
					continue;
				if (--l == 0)
					break;
			}
			if (l == 0)
				break;
		}
		assert(board[y][x] == GR_INVALID);
		cacc = access[y][x];
		access[y][x] = -1;
		if (cacc == SHRT_MAX) {
			isok = false;
		}
		else {
			for (unsigned dm = 0; dm < 4; dm++) {
				unsigned dm2;
				unsigned cx, cy;
				cx = x+direction_moves[dm][0]; cy = y+direction_moves[dm][1];
				if (access[cy][cx] < 0)
					continue;
				if (access[cy][cx] <= (short)cacc)
					continue;
				for (dm2 = 0; dm2 < 4; dm2++) {
					unsigned tx, ty;
					if (dm2 == ((dm+2)&3))
						continue;
					tx = cx+direction_moves[dm2][0]; ty = cy+direction_moves[dm2][1];
					if (access[ty][tx] < 0)
						continue;
					if ((unsigned)access[ty][tx] <= cacc+1)
						break;
				}
				if (dm2 < 4)
					continue;
				isok = false;
				break;
			}
		}
		if (!isok) {
			{ // keep gcc quiet about ISO C++
				for (unsigned y = 1; y < GAME_YSIZE-1; y++) {
					for (unsigned x = 1; x < GAME_XSIZE-1; x++) {
						if (access[y][x] > 0)
							access[y][x] = SHRT_MAX;
					}
				}
			}
			gwCheckAccess(access, headx, heady);
			isok = true;
			for (unsigned dm = 0; dm < 4; dm++) {
				unsigned cx, cy;
				cx = x+direction_moves[dm][0]; cy = y+direction_moves[dm][1];
				if (access[cy][cx] < 0)
					continue;
				if (access[cy][cx] < SHRT_MAX)
					continue;
				isok = false;
				break;
			}
		}
		if (!isok) {
			board[y][x] = GR_NONE;
			freecnt--;
			access[y][x] = cacc;
		}
		else {
			board[y][x] = GR_WALL;
			freecnt--;
			wallcnt++;
		}
	}
	for (deathcnt = 0; deathcnt < TILES_COUNT_DEATH; deathcnt++) {
		unsigned x, y;
		unsigned l;
		l = randrange(1, wallcnt-deathcnt);
		assert(l > 0 && l <= wallcnt-deathcnt);
		for (y = 1; ; y++) {
			assert(y < GAME_YSIZE-1);
			for (x = 1; x < GAME_XSIZE-1; x++) {
				if (board[y][x] != GR_WALL)
					continue;
				if (--l == 0)
					break;
			}
			if (l == 0)
				break;
		}
		board[y][x] = GR_DEATH;
	}
	return wallcnt+deathcnt;
}

void WormikGameImpl::generateType(board_def type, int num, unsigned char old)
{
	unsigned x, y;
	while (num--) {
		do {
			x = rand()%GAME_XSIZE;
			y = rand()%GAME_YSIZE;
		} while (board[y][x] != old);
		board[y][x] = type;
		freecnt--;
	}
}

void WormikGameImpl::initBoard()
{
	unsigned x, y;

	for (y = 1; y < GAME_YSIZE-1; y++) {
		for (x = 1; x < GAME_XSIZE-1; x++)
			board[y][x] = GR_INVALID;
	}
	for (x = 0; x < GAME_XSIZE; x++) {
		board[0][x] = GR_WALL;
		board[GAME_YSIZE-1][x] = GR_WALL;
	}
	for (y = 1; y < GAME_YSIZE-1; y++) {
		board[y][0] = GR_WALL;
		board[y][GAME_XSIZE-1] = GR_WALL;
	}
	freecnt = GAME_YSIZE*GAME_XSIZE-2*GAME_XSIZE-2*(GAME_YSIZE-2);

	snake_dir = SDIR_SOUTH;
	//snake_grow = 0;
	snake_health = 4;

	snake_pos[0].x = GAME_XSIZE/2; snake_pos[0].y = GAME_YSIZE/2+1;
	snake_pos[1].x = GAME_XSIZE/2; snake_pos[1].y = GAME_YSIZE/2+0;
	snake_pos[2].x = GAME_XSIZE/2; snake_pos[2].y = GAME_YSIZE/2-1;
	snake_pos[3].x = GAME_XSIZE/2; snake_pos[3].y = GAME_YSIZE/2-2;
	snake_len = 4;

	board[GAME_YSIZE/2-2][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKE_TAIL, SDIR_NORTH, SDIR_SOUTH);
	board[GAME_YSIZE/2-1][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKE_BODY, SDIR_NORTH, SDIR_SOUTH);
	board[GAME_YSIZE/2+0][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKE_BODY, SDIR_NORTH, SDIR_SOUTH);
	board[GAME_YSIZE/2+1][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKE_HEAD, SDIR_NORTH, SDIR_SOUTH);
	board[GAME_YSIZE/2+2][GAME_XSIZE/2] = GR_NONE;
	board[GAME_YSIZE/2+3][GAME_XSIZE/2] = GR_NONE;
	freecnt -= 6;

	//{ board[2][1] = GR_WALL; board[2][2] = GR_WALL; freecnt -= 2; } // test

	generateWalls(GAME_XSIZE/2, GAME_YSIZE/2+1);

#if 0
	generateType(GR_POSITIVE, 50, GR_INVALID);
	generateType(GR_POSITIVE_2, 30, GR_INVALID);
	generateType(GR_NEGATIVE, 20, GR_INVALID);
#endif
	assert(defcnts[0].def == GR_EXIT); defcnts[0].max = 0;
	for (int i = 0; i < DEFCNTSMAX; i++)
		defcnts[i].cnt = 0;

	for (y = 0; y < GAME_YSIZE; y++) {
		for (x = 0; x < GAME_XSIZE; x++) {
			if (board[y][x] == GR_INVALID)
				board[y][x] = GR_NONE;
			else if (board[y][x] == GR_NONE)
				freecnt++;
		}
	}
	ndlen = 0;

	state_season = gui->newLevel(state_season);
}

void WormikGameImpl::decDefs(board_def def)
{
	unsigned i;
	for (i = 0; ; i++) {
		assert(i < DEFCNTSMAX);
		if (defcnts[i].def == def)
			break;
	}
	assert(defcnts[i].cnt > 0);
	defcnts[i].cnt--;
	freecnt++;
}

int WormikGameImpl::genDef(float latency)
{
	unsigned x, y;
	unsigned l;
	int i;
	int bi = -1;
	int br = INT_MAX;
	if (freecnt == 0)
		return 0;
	if (ndlen == sizeof(newdefs)/sizeof(newdefs[0]))
		return 0;
	for (i = 0; i < DEFCNTSMAX; i++) {
		int r;
		if (defcnts[i].cnt == defcnts[i].max && (1 || defcnts[i].max == 0)) // another test - generate full board (switch 0/1)
			continue;
		if ((r = 65536*defcnts[i].cnt/defcnts[i].max) >= br)
			continue;
		bi = i; br = r;
	}
	if (bi < 0)
		return 0;
	l = randrange(1, freecnt);
	for (y = 1; ; y++) {
		assert(y < GAME_YSIZE-1);
		for (x = 1; x < GAME_XSIZE-1; x++) {
			if (board[y][x] != GR_NONE)
				continue;
			if (--l == 0)
				break;
		}
		if (l == 0)
			break;
	}
	if (0) { // another testing code
		unsigned nx, ny;
		nx = snake_pos[0].x+4*direction_moves[snake_dir][0]; ny = snake_pos[0].y+4*direction_moves[snake_dir][1];
		if (board[ny][nx] == GR_NONE)
			x = nx, y = ny;
	}
	assert(board[y][x] == GR_NONE);
	newdefs[ndlen].defsi = bi;
	newdefs[ndlen].x = x; newdefs[ndlen].y = y;
	newdefs[ndlen].timeout = defcnts[bi].timeout+latency;
	ndlen++;
	defcnts[bi].cnt++;
	board[y][x] = GR_NEW_DEF;
	freecnt--;
	gui->invalidateOutput(-WormikGui::INVO_NEW_DEFS, NULL);
	return 1;
}

bool WormikGameImpl::deleteNewDef(unsigned x, unsigned y)
{
	unsigned p[2];
	board_def def;
	unsigned i;
	unsigned di;
	bool deleteIt;
	assert(board[y][x] == GR_NEW_DEF);
	for (i = 0; ; i++) {
		assert(i < ndlen);
		if (newdefs[i].x == (int)x && newdefs[i].y == (int)y)
			break;
	}
	di = newdefs[i].defsi;
	def = defcnts[di].def;
	switch (def) {
	case GR_EXIT:
		deleteIt = newdefs[i].timeout/defcnts[di].timeout > TIMEOUT_EXIT;
		break;

	case GR_POSITIVE:
	case GR_POSITIVE_2:
		deleteIt = newdefs[i].timeout/defcnts[di].timeout > TIMEOUT_POSITIVE;
		break;

	default:
		deleteIt = true;
		break;
	}
	memmove(newdefs+i, newdefs+i+1, (--ndlen-i)*sizeof(newdefs[0]));
	if (deleteIt) {
		decDefs(def);
		board[y][x] = GR_NONE;
	}
	else {
		p[0] = x; p[1] = y;
		gui->invalidateOutput(1, &p);
		board[y][x] = def;
	}
	return deleteIt;
}

void WormikGameImpl::run(void)
{
	int action = 2; /* exit: 1; dead: 2, quit: 3 */

	while (action != 3) {
		double interval = 0.400000;
		double tadd_health = 5.0;
		double tout_health = tadd_health+interval;

		if (action == 2) {
			snake_grow = 0;
			state_level = 0;
			state_season = 0;
#ifdef TESTOPTS
			state_season = getConfigInt("initseason", state_season);
#endif
			state_exitscore = 80;
#ifdef TESTOPTS
			state_exitscore = getConfigInt("exitscore", state_exitscore);
#endif
			state_totscore = 0;
			stats_record = abs(stats_record);
		}
		else {
			state_level++;
			state_season++;
			state_exitscore += 16+8*(state_level/4);
		}
		state_levscore = 0;
		state_game = GS_WAITING;
		action = 0;
		initBoard();

		if (gui->waitStart())
			goto quit;
		state_game = GS_RUNNING;
		for (;;) {
			int invof = 0;
			unsigned npos[2];
			unsigned oldscore = state_levscore;

			if ((tout_health -= interval) <= 0) {
				snake_health++;
				invof |= WormikGui::INVO_HEALTH;
				if (tadd_health < 8.0)
					tadd_health += 2.0;
				else if (tadd_health < 12.0)
					tadd_health += 1.5;
				else if (tadd_health < 16.0)
					tadd_health += 1.0;
				else if (tadd_health < 20.0)
					tadd_health += 0.7;
				else if (tadd_health < 25.0)
					tadd_health += 0.5;
				tout_health += tadd_health;
			}

			if (ndlen > 0) {
				unsigned il = 0;
				unsigned inval[sizeof(newdefs)/sizeof(newdefs[0])][2];
				unsigned ni, di;
				for (di = ni = 0; ni < ndlen; ni++) {
					if ((newdefs[ni].timeout -= interval) <= 0) {
						board[newdefs[ni].y][newdefs[ni].x] = defcnts[newdefs[ni].defsi].def;
						inval[il][0] = newdefs[ni].x; inval[il][1] = newdefs[ni].y;
						il++;
					}
					else {
						newdefs[di++] = newdefs[ni];
					}
				}
				ndlen = di;
				gui->invalidateOutput(il, inval);
			}

			npos[0] = snake_pos[0].x+direction_moves[snake_dir][0];
			npos[1] = snake_pos[0].y+direction_moves[snake_dir][1];

step_hit:
			switch (board[npos[1]][npos[0]]) {
			case GR_WALL:
			case GR_DEATH:
				action = 2;
				invof |= WormikGui::INVO_HEALTH;
				break;

			default: // snake
				if (GR_GET_FULL_TYPE(board[npos[1]][npos[0]]) == GR_BASE_SNAKE+GSF_SNAKE_TAIL)
					snake_health--;
				else
					snake_health /= 2;
				if (snake_health > 0) {
					unsigned il = 0;
					unsigned inval[17][2];
					for (;;) {
						element_pos *p = &snake_pos[--snake_len];
						if (il == sizeof(inval)/sizeof(inval[0])-1) {
							gui->invalidateOutput(il, inval);
							il = 0;
						}
						inval[il][0] = p->x; inval[il][1] = p->y; il++;
						board[p->y][p->x] = GR_NONE; freecnt++;
						if (p->x == npos[0] && p->y == npos[1])
							break;
						assert(defcnts[0].def == GR_EXIT);
						if (defcnts[0].max != 0)
							state_levscore++;
					}
					board[snake_pos[snake_len-1].y][snake_pos[snake_len-1].x] = GR_SNAKE(GSF_SNAKE_TAIL, 0, GR_GET_OUT(board[snake_pos[snake_len-1].y][snake_pos[snake_len-1].x]));
					inval[il][0] = snake_pos[snake_len-1].x; inval[il][1] = snake_pos[snake_len-1].y; il++;
					gui->invalidateOutput(il, inval);
					gui->invalidateOutput(-WormikGui::INVO_LENGTH, NULL);
				}
				invof |= WormikGui::INVO_HEALTH;
				break;

			case GR_NEW_DEF:
				if (!deleteNewDef(npos[0], npos[1]))
					goto step_hit;
				// fall through
			case GR_NONE:
				break;

			case GR_POSITIVE:
				snake_grow += 1;
				state_levscore += 2;
				decDefs(GR_POSITIVE);
				break;

			case GR_POSITIVE_2:
				snake_grow += 2;
				state_levscore += 5;
				decDefs(GR_POSITIVE_2);
				break;

			case GR_NEGATIVE:
				invof |= WormikGui::INVO_HEALTH;
				if ((snake_health -= 1) <= 0)
					break;
				decDefs(GR_NEGATIVE);
				snake_grow--;
				break;

			case GR_EXIT:
				state_levscore += 12*state_level+8*(state_level/4);
				invof |= WormikGui::INVO_SCORE;
				action = 1;
				break;
			}
			if (snake_health == 0) {
				action = 2;
			}
			if (state_levscore != oldscore) {
				state_totscore += state_levscore-oldscore;
				gui->invalidateOutput(-WormikGui::INVO_SCORE, NULL);
				if ((stats_record < 0 || state_totscore > (unsigned)stats_record)) {
					stats_record = -state_totscore;
					stats_rectime = time(NULL);
					gui->invalidateOutput(-WormikGui::INVO_RECORD, NULL);
				}
			}
			if (action == 0) {
				unsigned old_len = snake_len;
				memmove(snake_pos+1, snake_pos+0, snake_len*sizeof(snake_pos[0]));
				snake_pos[0].x = npos[0]; snake_pos[0].y = npos[1];
				board[npos[1]][npos[0]] = GR_SNAKE(GSF_SNAKE_HEAD, (snake_dir+2)&3, snake_dir);
				{
					unsigned inval[2][2];
					element_pos *p;

					p = &snake_pos[1];
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKE_BODY, GR_GET_IN(board[p->y][p->x]), snake_dir);
					inval[0][0] = p->x; inval[0][1] = p->y;

					p = &snake_pos[0];
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKE_HEAD, (snake_dir+2)&3, snake_dir);
					inval[1][0] = p->x; inval[1][1] = p->y;

					gui->invalidateOutput(2, inval);
				}
				if (--snake_grow >= 0) {
					snake_len++;
					invof |= WormikGui::INVO_LENGTH;
					freecnt--;
				}
				else {
					unsigned il = 0;
					unsigned inval[8][2];
					element_pos *p;

					for (;;) {
						p = &snake_pos[snake_len];
						board[p->y][p->x] = GR_NONE;
						inval[il][0] = p->x; inval[il][1] = p->y; il++;
						if (++snake_grow == 0 || snake_len <= 2)
							break;
						snake_len--;
						freecnt++;
					}
					p = &snake_pos[snake_len-1];
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKE_TAIL, 0, GR_GET_OUT(board[p->y][p->x]));
					inval[il][0] = p->x; inval[il][1] = p->y; il++;
					gui->invalidateOutput(il, inval);
				}
				if (snake_health > snake_len) {
					snake_health = snake_len;
				}
				if (state_levscore >= state_exitscore) {
					assert(defcnts[0].def == GR_EXIT);
					defcnts[0].max = 1;
				}
				if (old_len != snake_len) {
					double c;
					if (interval > 0.30)
						c = 0.002;
					else if (interval > 0.25)
						c = 0.0012;
					else if (interval > 0.20)
						c = 0.0007;
					else if (interval > 0.15)
						c = 0.0005;
					else
						c = 0;
					if (snake_len < old_len)
						c *= 1.5;
					if ((interval -= c) < 0.15)
						interval = 0.15;
					//printf("speed: %4.2f (%6.4f)\n", 1.0/interval, interval);
				}
				for (float latency = (float)interval/8; latency < interval; latency += (float)interval/4) {
					if (genDef(latency) == 0 || 0)
						break;
				}
			}
			gui->invalidateOutput(-invof, NULL);
			if (action != 0)
				break;
			if (gui->waitNext(interval))
				goto quit;
		}
		if (stats_record < 0)
			saveRecord();
		switch (action) {
		case 1:
			if (gui->announce(WormikGui::ANC_EXIT))
				goto quit;
			break;

		case 2:
			if (gui->announce(WormikGui::ANC_DEAD))
				goto quit;
			break;
		}
		if (false) {
quit:
			action = 3;
		}
	}
	exit(0);
}

void WormikGameImpl::printLogTimestamp()
{
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t nowAsTimeT = std::chrono::system_clock::to_time_t(now);
	std::chrono::milliseconds nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	std::stringstream nowString;
	nowString
		<< std::put_time(std::localtime(&nowAsTimeT), "%Y-%m-%d %H:%M:%S")
		<< '.' << std::setfill('0') << std::setw(3) << nowMs.count() << " ";
	fputs(nowString.str().c_str(), stderr);
}

int WormikGameImpl::debug(const char *fmt, ...)
{
	if (isDebug) {
		printLogTimestamp();
		va_list va;
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
	}
	return 0;
}

int WormikGameImpl::error(const char *fmt, ...)
{
	printLogTimestamp();
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	return 0;
}

void WormikGameImpl::fatal()
{
	printLogTimestamp();
	fprintf(stderr, "\n");
	exit(126);
}

void WormikGameImpl::fatal(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	printLogTimestamp();
	fprintf(stderr, "fatal error occured: ");
	vfprintf(stderr, fmt, va);
	fatal();
}

void WormikGameImpl::saveRecord()
{
	int record; time_t rectime;
	char recs[64];
	getRecord(&record, &rectime);
	sprintf(recs, "%d/%ld", record, rectime);
	setConfig("record", recs);
}

void WormikGameImpl::exit(int n)
{
	gui->shutdown(this);
	delete gui;
	delete this;
	::exit(n);
}

WormikGame *create_WormikGame()
{
	return new WormikGameImpl();
}


} } } };
