/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * GUI interface
 */

#ifndef WormikGui_If_hxx__
# define WormikGui_If_hxx__


class WormikGame_If;

class WormikGui_If
{
public:
	enum {		/* announcements */
		ANC_DEAD,
		ANC_EXIT,
	};

	enum {
		INVO_BOARD = 1,
		INVO_NEWDEFS = 2,
		INVO_RECORD = 4,
		INVO_SCORE = 8,
		INVO_HEALTH = 16,
		INVO_LENGTH = 32,
		INVO_GSTATE = 64,
		INVO_NBASE = 128,
		INVO_FULL = (INVO_BOARD|INVO_NEWDEFS|INVO_RECORD|INVO_SCORE|INVO_HEALTH|INVO_LENGTH|INVO_GSTATE),
	};

public:
	virtual				~WormikGui_If() = 0;

	/* init/shutdown functions */
	virtual int			init(WormikGame_If *game) = 0;
	virtual void			shutdown(WormikGame_If *game) = 0;

	/* season init function */
	virtual int			newLevel(int season) = 0;

	/* output functions */
	/*  draws "static" point */
	virtual void			drawPoint(void *gc, unsigned x, unsigned y, unsigned short cont) = 0;
	/*  draws "newdef" point, returns 1 if it hasn't yet timed out */
	virtual int			drawNewdef(void *gc, unsigned x, unsigned y, unsigned short newcont, double left, double total) = 0;

	/* output changes functions */
	/*  invalidates points, or adds INVO_* flags if length is negative */
	virtual void			invalOut(int length, unsigned (*points)[2]) = 0;

	/* wait for next move */
	virtual int			waitNext(double interval) = 0;
	/* announce */
	virtual int			announce(int type) = 0;
};


#endif
