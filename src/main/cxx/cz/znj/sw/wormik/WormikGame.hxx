/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * Game interface
 */

#ifndef WormikGame_hxx__
# define WormikGame_hxx__

namespace cz { namespace znj { namespace sw { namespace wormik {


class WormikGui;

class WormikGame
{
public:
	typedef unsigned char board_def;

	/* game size */
	enum {
		GAME_XSIZE			= 30,
		GAME_YSIZE			= 30,
	};

	/* game states */
	enum {
		GS_WAITING,
		GS_RUNNING,
		GS_ASKAGAIN,
	};

	enum {
		SDIR_RIGHT,
		SDIR_UP,
		SDIR_LEFT,
		SDIR_DOWN,
	};

	enum {
		/* field types */
		GR_INVALID			= 0,
		GR_NONE				= 1,
		GR_WALL				= 2,
		GR_POSIT			= 3,
		GR_POSIT2			= 4,
		GR_NEGAT			= 5,
		GR_DEATH			= 6,
		GR_EXIT				= 7,
		GR_NEWDEF			= 8,
		GR_BSNAKE			= 12,

		/* snake body types */
		GSF_SNAKEBODY			= 0,
		GSF_SNAKEHEAD			= 1,
		GSF_SNAKETAIL			= 2,
	};

public:
	/* board macros */
	/* get base type GR_INVALID, ..., GR_BSNAKE */
	static board_def		GR_GET_BTYPE(board_def n);
	/* get full type GR_INVALID, ..., GR_BSNAKE/head, ... */
	static board_def		GR_GET_FTYPE(board_def n);
	/* get snake type */
	static board_def		GR_GET_STYPE(board_def n);
	/* get snake in-direction */
	static board_def		GR_GET_IN(board_def n);
	/* get snake out-direction */
	static board_def		GR_GET_OUT(board_def n);
	/* create snake definition */
	static board_def		GR_SNAKE(board_def type, int in, int out);

public:
	virtual				~WormikGame() {}

public:
	/* main game functions */
	virtual void			setGui(WormikGui *gui) = 0;
	virtual void			run(void) = 0;

	/* config functions */
	/*  returns full string length (as sprintf) */
	virtual int			getConfigStr(const char *name, char *buf, int blen) = 0;
	/*  returns int-ed string or defvalue if not found */
	virtual int			getConfigInt(const char *name, int defval) = 0;
	/*  sets config int */
	virtual void			setConfig(const char *name, int value) = 0;
	/*  sets config string */
	virtual void			setConfig(const char *name, const char *value) = 0;

	/* error reporting functions */
	/*  debug message */
	virtual int			debug(const char *fmt, ...) = 0;
	/*  error occured, probably able to continue */
	virtual int			error(const char *fmt, ...) = 0;
	/*  fatal io error occured, unable to continue */
	virtual void			fatal() = 0;
	virtual void			fatal(const char *fmt, ...) = 0;

	/* handling functions */
	/*  input handling */
	virtual void			changeDirection(int dir) = 0;
	/*  get level and season, returns game state */
	virtual int			getState(int *level, int *season) = 0;
	/*  get current level score and total score, returns exit score */
	virtual int			getScore(int *score, int *total) = 0;
	/*  get health, length */
	virtual void			getSnakeInfo(int *health, int *length) = 0;
	/*  get record, returns if current is record */
	virtual bool			getRecord(int *record, time_t *rectime) = 0;

	/* output functions */
	/*  draws one point (if not newdef) */
	virtual void			outPoint(void *gc, unsigned x, unsigned y) = 0;
	/*  draws interval (non-newdef points only) */
	virtual void			outGame(void *gc, unsigned x0, unsigned y0, unsigned x1, unsigned y1) = 0;
	/*  draws all newdefs, returns number of still timing */
	virtual int			outNewdefs(void *gc) = 0;
};

inline WormikGame::board_def WormikGame::GR_SNAKE(board_def type, int in, int out)
{
	return (GR_BSNAKE+type)|(in<<4)|(out<<6);
}

inline WormikGame::board_def WormikGame::GR_GET_BTYPE(board_def n)
{
	return ((n&15) >= GR_BSNAKE)?GR_BSNAKE:n;
}

inline WormikGame::board_def WormikGame::GR_GET_FTYPE(board_def n)
{
	return n&15;
}

inline WormikGame::board_def WormikGame::GR_GET_STYPE(board_def n)
{
	return n&3;
}

inline WormikGame::board_def WormikGame::GR_GET_IN(board_def n)
{
	return (n>>4)&3;
}

inline WormikGame::board_def WormikGame::GR_GET_OUT(board_def n)
{
	return (n>>6)&3;
}


} } } };

#endif
