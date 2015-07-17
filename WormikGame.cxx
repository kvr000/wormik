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
#include <unistd.h>
#include <time.h>

#include <sys/stat.h>

#include "WormikGame_If.hxx"
#include "WormikGui_If.hxx"

class WormikGame: public WormikGame_If
{
protected:
	/* gui interface */
	WormikGui_If *			gui;

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

public:
	/* constructor */		WormikGame();

public:
	virtual void			setGui(WormikGui_If *gui);
	virtual void			run(void);

	virtual int			getConfigStr(const char *name, char *buf, int blen);
	virtual int			getConfigInt(const char *name, int defval);
	virtual void			setConfig(const char *name, int value);
	virtual void			setConfig(const char *name, const char *value);

	virtual int			error(const char *fmt, ...);
	virtual void			fatal();
	virtual void			fatal(const char *fmt, ...);

	virtual void			changeDirection(int dir);
	virtual int			getState(int *level, int *season);
	virtual int			getScore(int *score, int *total);
	virtual void			getSnakeInfo(int *health, int *length);
	virtual int 			getRecord(int *record, time_t *rectime);

	virtual void			outPoint(void *gc, unsigned x, unsigned y);
	virtual void			outGame(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1);
	virtual int			outNewdefs(void *gc);

protected:
	void				initBoard();
	void				generateType(board_def type, int num, board_def old);
	int				generateWalls(unsigned headx, unsigned heady);

	void				decDefs(board_def def);
	int				genDef(float latency);
	int				delNewdef(unsigned x, unsigned y);

	void				saveRecord();

	void				exit(int n);
};

static const int direction_moves[4][2] = { { 1, 0 }, { 0, -1 }, { -1, 0 }, { 0, 1} };

static int randrange(int min, int max)
{
#if 0
	int r = rand()%3;
	switch (r) {
	case 0:
		if (min+2 > max)
			break;
		return min+2;
	case 1:
		return max;
	default:
		break;
	}
#endif
	return min+(long long)rand()*(max-min+1)/((unsigned)RAND_MAX+1);
}

WormikGame::WormikGame():
	WormikGame_If()
{
	char buf[1024];
	int i = 0;
	if ((unsigned)getConfigStr("record", buf, sizeof(buf)) >= sizeof(buf) || sscanf(buf, "%d/%ld", &stats_record, &stats_rectime) < 2) {
		stats_record = 0;
		stats_rectime = 0;
	}

	defcnts[i].def = GR_EXIT; /*defcnts[i].max = 0;*/ defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_POSIT; defcnts[i].max = 50; defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_POSIT2; defcnts[i].max = 30; defcnts[i].timeout = 2.0; i++;
	defcnts[i].def = GR_NEGAT; defcnts[i].max = 20; defcnts[i].timeout = 2.0; i++;
	assert(i == DEFCNTSMAX);
}

void WormikGame::setGui(WormikGui_If *gui_)
{
	gui = gui_;
}

/* *config*: stupid unsafe implementation, but written really quickly ;) */
FILE *openConfig(int mode)
{
	int fd;
	const char *home;
	char buf[PATH_MAX];
	if ((home = getenv("HOME")) == NULL)
		home = ".";
	if (snprintf(buf, sizeof(buf), "%s/.config", home) < (int)sizeof(buf))
		mkdir(buf, 0777);
	if (snprintf(buf, sizeof(buf), "%s/.config/.wormikrc", home) >= (int)sizeof(buf))
		return NULL;
	if ((fd = open(buf, mode|O_CREAT, 0666)) < 0)
		return NULL;
	return fdopen(fd, (mode == O_RDONLY)?"r":"r+");
}

void WormikGame::setConfig(const char *name, int value)
{
	char buf[32];
	sprintf(buf, "%d", value);
	setConfig(name, buf);
}

void WormikGame::setConfig(const char *name, const char *value)
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

int WormikGame::getConfigInt(const char *name, int defval)
{
	char buf[16];
	if ((unsigned)getConfigStr(name, buf, sizeof(buf)) >= sizeof(buf))
		return defval;
	return strtol(buf, NULL, 0);
}

int WormikGame::getConfigStr(const char *name, char *str, int buflen)
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

void WormikGame::changeDirection(int dir_)
{
	if (dir_ != GR_GET_IN(board[snake_pos[0].y][snake_pos[0].x]))
		snake_dir = dir_;
}

int WormikGame::getState(int *level, int *season)
{
	*level = state_level;
	*season = state_season;
	return state_game;
}

int WormikGame::getScore(int *score, int *total)
{
	*score = state_levscore;
	*total = state_totscore;
	return state_exitscore;
}

void WormikGame::getSnakeInfo(int *health, int *length)
{
	*health = snake_health;
	*length = snake_len;
}

int WormikGame::getRecord(int *record, time_t *rectime)
{
	*rectime = stats_rectime;
	if ((*record = stats_record) < 0) {
		*record = -*record;
		return 1;
	}
	return 0;
}

void WormikGame::outPoint(void *gc, unsigned x, unsigned y)
{
	if (board[y][x] != GR_NEWDEF)
		gui->drawPoint(gc, x, y, board[y][x]);
}

void WormikGame::outGame(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1)
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

int WormikGame::outNewdefs(void *gc)
{
	int ret = 0;
	unsigned i;
	for (i = 0; i < ndlen; i++) {
		ret += gui->drawNewdef(gc, newdefs[i].x, newdefs[i].y, defcnts[newdefs[i].defsi].def, newdefs[i].timeout, defcnts[newdefs[i].defsi].timeout);
	}
	return ret;
}

static void gwCheckAccess(short (*access)[WormikGame::GAME_XSIZE], unsigned x, unsigned y)
{
	unsigned tlen;
	short tlist[WormikGame::GAME_YSIZE*WormikGame::GAME_XSIZE][2];

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

int WormikGame::generateWalls(unsigned headx, unsigned heady)
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

	for (wallcnt = 0; wallcnt < 180; ) {
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
	for (deathcnt = 0; deathcnt < 20; deathcnt++) {
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

void WormikGame::generateType(board_def type, int num, unsigned char old)
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

void WormikGame::initBoard()
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

	snake_dir = 3;
	//snake_grow = 0;
	snake_health = 4;

	snake_pos[0].x = GAME_XSIZE/2; snake_pos[0].y = GAME_YSIZE/2+1; 
	snake_pos[1].x = GAME_XSIZE/2; snake_pos[1].y = GAME_YSIZE/2+0; 
	snake_pos[2].x = GAME_XSIZE/2; snake_pos[2].y = GAME_YSIZE/2-1; 
	snake_pos[3].x = GAME_XSIZE/2; snake_pos[3].y = GAME_YSIZE/2-2; 
	snake_len = 4;

	board[GAME_YSIZE/2-2][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKETAIL, SDIR_UP, SDIR_DOWN);
	board[GAME_YSIZE/2-1][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKEBODY, SDIR_UP, SDIR_DOWN);
	board[GAME_YSIZE/2+0][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKEBODY, SDIR_UP, SDIR_DOWN);
	board[GAME_YSIZE/2+1][GAME_XSIZE/2] = GR_SNAKE(GSF_SNAKEHEAD, SDIR_UP, SDIR_DOWN);
	board[GAME_YSIZE/2+2][GAME_XSIZE/2] = GR_NONE;
	board[GAME_YSIZE/2+3][GAME_XSIZE/2] = GR_NONE;
	freecnt -= 6;

	//{ board[2][1] = GR_WALL; board[2][2] = GR_WALL; freecnt -= 2; } // test

	generateWalls(GAME_XSIZE/2, GAME_YSIZE/2+1);

#if 0
	generateType(GR_POSIT, 50, GR_INVALID);
	generateType(GR_POSIT2, 30, GR_INVALID);
	generateType(GR_NEGAT, 20, GR_INVALID);
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

void WormikGame::decDefs(board_def def)
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

int WormikGame::genDef(float latency)
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
	board[y][x] = GR_NEWDEF;
	freecnt--;
	gui->invalOut(-WormikGui_If::INVO_NEWDEFS, NULL);
	return 1;
}

int WormikGame::delNewdef(unsigned x, unsigned y)
{
	unsigned p[2];
	board_def def;
	unsigned i;
	unsigned di;
	bool delit;
	assert(board[y][x] == GR_NEWDEF);
	for (i = 0; ; i++) {
		assert(i < ndlen);
		if (newdefs[i].x == (int)x && newdefs[i].y == (int)y)
			break;
	}
	di = newdefs[i].defsi;
	def = defcnts[di].def;
	switch (def) {
	case GR_EXIT:
		delit = newdefs[i].timeout/defcnts[di].timeout > 0.75;
		break;
	case GR_POSIT:
	case GR_POSIT2:
		delit = newdefs[i].timeout/defcnts[di].timeout > 0.3;
		break;
	default:
		delit = true;
		break;
	}
	memmove(newdefs+i, newdefs+i+1, (--ndlen-i)*sizeof(newdefs[0]));
	if (delit) {
		decDefs(def);
		board[y][x] = GR_NONE;
	}
	else {
		p[0] = x; p[1] = y;
		gui->invalOut(1, &p);
		board[y][x] = def;
	}
	return delit;
}

void WormikGame::run(void)
{
	int action = 2; /* exit: 1; dead: 2, quit: 3 */

	while (action != 3) {
		int gret;
		double inter = 0.400000;
		double tadd_health = 5.0;
		double tout_health = tadd_health+inter;

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

		if ((gret = gui->waitNext(0)) == 1)
			goto quit;
		state_game = GS_RUNNING;
		for (;;) {
			int invof = 0;
			unsigned npos[2];
			unsigned oldscore = state_levscore;

			if ((tout_health -= inter) <= 0) {
				snake_health++;
				invof |= WormikGui_If::INVO_HEALTH;
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
					if ((newdefs[ni].timeout -= inter) <= 0) {
						board[newdefs[ni].y][newdefs[ni].x] = defcnts[newdefs[ni].defsi].def;
						inval[il][0] = newdefs[ni].x; inval[il][1] = newdefs[ni].y;
						il++;
					}
					else {
						newdefs[di++] = newdefs[ni];
					}
				}
				ndlen = di;
				gui->invalOut(il, inval);
			}

			npos[0] = snake_pos[0].x+direction_moves[snake_dir][0];
			npos[1] = snake_pos[0].y+direction_moves[snake_dir][1];

step_hit:
			switch (board[npos[1]][npos[0]]) {
			case GR_WALL:
			case GR_DEATH:
				action = 2;
				invof |= WormikGui_If::INVO_HEALTH;
				break;
			default: // snake
				if (GR_GET_FTYPE(board[npos[1]][npos[0]]) == GR_BSNAKE+GSF_SNAKETAIL)
					snake_health--;
				else
					snake_health /= 2;
				if (snake_health > 0) {
					unsigned il = 0;
					unsigned inval[17][2];
					for (;;) {
						element_pos *p = &snake_pos[--snake_len];
						if (il == sizeof(inval)/sizeof(inval[0])-1) {
							gui->invalOut(il, inval);
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
					board[snake_pos[snake_len-1].y][snake_pos[snake_len-1].x] = GR_SNAKE(GSF_SNAKETAIL, 0, GR_GET_OUT(board[snake_pos[snake_len-1].y][snake_pos[snake_len-1].x]));
					inval[il][0] = snake_pos[snake_len-1].x; inval[il][1] = snake_pos[snake_len-1].y; il++;
					gui->invalOut(il, inval);
					gui->invalOut(-WormikGui_If::INVO_LENGTH, NULL);
				}
				invof |= WormikGui_If::INVO_HEALTH;
				break;
			case GR_NEWDEF:
				if (delNewdef(npos[0], npos[1]) == 0)
					goto step_hit;
				// fall through
			case GR_NONE:
				break;
			case GR_POSIT:
				snake_grow += 1;
				state_levscore += 2;
				decDefs(GR_POSIT);
				break;
			case GR_POSIT2:
				snake_grow += 2;
				state_levscore += 5;
				decDefs(GR_POSIT2);
				break;
			case GR_NEGAT:
				invof |= WormikGui_If::INVO_HEALTH;
				if ((snake_health -= 1) <= 0)
					break;
				decDefs(GR_NEGAT);
				snake_grow--;
				break;
			case GR_EXIT:
				state_levscore += 12*state_level+8*(state_level/4);
				invof |= WormikGui_If::INVO_SCORE;
				action = 1;
				break;
			}
			if (snake_health == 0) {
				action = 2;
			}
			if (state_levscore != oldscore) {
				state_totscore += state_levscore-oldscore;
				gui->invalOut(-WormikGui_If::INVO_SCORE, NULL);
				if ((stats_record < 0 || state_totscore > (unsigned)stats_record)) {
					stats_record = -state_totscore;
					stats_rectime = time(NULL);
					gui->invalOut(-WormikGui_If::INVO_RECORD, NULL);
				}
			}
			if (action == 0) {
				unsigned old_len = snake_len;
				memmove(snake_pos+1, snake_pos+0, snake_len*sizeof(snake_pos[0]));
				snake_pos[0].x = npos[0]; snake_pos[0].y = npos[1];
				board[npos[1]][npos[0]] = GR_SNAKE(GSF_SNAKEHEAD, (snake_dir+2)&3, snake_dir);
				{
					unsigned inval[2][2];
					element_pos *p;

					p = &snake_pos[1];
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKEBODY, GR_GET_IN(board[p->y][p->x]), snake_dir);
					inval[0][0] = p->x; inval[0][1] = p->y;

					p = &snake_pos[0];
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKEHEAD, (snake_dir+2)&3, snake_dir);
					inval[1][0] = p->x; inval[1][1] = p->y;

					gui->invalOut(2, inval);
				}
				if (--snake_grow >= 0) {
					snake_len++;
					invof |= WormikGui_If::INVO_LENGTH;
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
					board[p->y][p->x] = GR_SNAKE(GSF_SNAKETAIL, 0, GR_GET_OUT(board[p->y][p->x]));
					inval[il][0] = p->x; inval[il][1] = p->y; il++;
					gui->invalOut(il, inval);
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
					if (inter > 0.30)
						c = 0.002;
					else if (inter > 0.25)
						c = 0.0012;
					else if (inter > 0.20)
						c = 0.0007;
					else if (inter > 0.15)
						c = 0.0005;
					else
						c = 0;
					if (snake_len < old_len)
						c *= 1.5;
					if ((inter -= c) < 0.15)
						inter = 0.15;
					//printf("speed: %4.2f (%6.4f)\n", 1.0/inter, inter);
				}
				for (float latency = (float)inter/8; latency < inter; latency += (float)inter/4) {
					if (genDef(latency) == 0 || 0)
						break;
				}
			}
			gui->invalOut(-invof, NULL);
			if (action != 0)
				break;
			if ((gret = gui->waitNext(inter)) == 1)
				goto quit;
		}
		if (stats_record < 0)
			saveRecord();
		switch (action) {
		case 1:
			if (gui->announce(WormikGui_If::ANC_EXIT) > 0)
				goto quit;
			break;
		case 2:
			if (gui->announce(WormikGui_If::ANC_DEAD) > 0)
				goto quit;
			break;
		}
		if (0) {
quit:
			action = 3;
		}
	}
	exit(0);
}

int WormikGame::error(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	return 0;
}

void WormikGame::fatal()
{
	exit(1);
}

void WormikGame::fatal(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "fatal error occured: ");
	vfprintf(stderr, fmt, va);
	fatal();
}

void WormikGame::saveRecord()
{
	int record; time_t rectime;
	char recs[64];
	getRecord(&record, &rectime);
	sprintf(recs, "%d/%ld", record, rectime);
	setConfig("record", recs);
}

void WormikGame::exit(int n)
{
	gui->shutdown(this);
	delete gui;
	delete this;
	::exit(n);
}

WormikGame_If *create_WormikGame()
{
	return new WormikGame();
}
