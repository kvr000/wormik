/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * GUI interface
 */

#ifndef WormikGui_hxx__
# define WormikGui_hxx__

namespace cz { namespace znj { namespace sw { namespace wormik {


class WormikGame;

class WormikGui
{
public:
	enum {
		WINDOW_WIDTH = 640,
		WINDOW_HEIGHT = 480,
		AREA_INFO_X = 480,
	};

	enum {		/* announcements */
		ANC_DEAD,
		ANC_EXIT,
	};

	enum {
		INVO_BOARD = 1,
		INVO_NEW_DEFS = 2,
		INVO_RECORD = 4,
		INVO_SCORE = 8,
		INVO_HEALTH = 16,
		INVO_LENGTH = 32,
		INVO_GAME_STATE = 64,
		INVO_NEXT_BASE = 128,
		INVO_FULL = (INVO_BOARD|INVO_NEW_DEFS|INVO_RECORD|INVO_SCORE|INVO_HEALTH|INVO_LENGTH|INVO_GAME_STATE),
	};

public:
	virtual				~WormikGui() {}

	/* init/shutdown functions */
	virtual int			init(WormikGame *game) = 0;
	virtual void			shutdown(WormikGame *game) = 0;

	/* season init function */
	virtual int			newLevel(int season) = 0;

	/* output functions */

	/*  draws "static" point */
	virtual void			drawPoint(void *gc, unsigned x, unsigned y, unsigned short cont) = 0;
	/*  draws "newdef" point, returns 1 if it hasn't yet timed out */
	virtual int			drawNewdef(void *gc, unsigned x, unsigned y, unsigned short newcont, double left, double total) = 0;

	/* output changes functions */

	/* invalidates points, or adds INVO_* flags if length is negative */
	virtual void			invalidateOutput(int length, unsigned (*points)[2]) = 0;

	/* wait for first move, returns false to continue, true for quit  */
	virtual bool			waitStart() = 0;
	/* wait for next move, returns false to continue, true for quit  */
	virtual bool			waitNext(double interval) = 0;

	/* announce */
	virtual bool			announce(int type) = 0;
};


} } } };

#endif
