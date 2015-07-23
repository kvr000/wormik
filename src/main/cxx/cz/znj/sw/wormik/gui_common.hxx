/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * GUI common declarations
 */

#ifndef gui_common_hxx__
# define gui_common_hxx__

namespace cz { namespace znj { namespace sw { namespace wormik { namespace gui4x6x16
{


/* canvas sizes */
enum {
	GRECT_XSIZE = 16,
	GRECT_YSIZE = 16,
};

enum {
	SP_BACK_X = 2,
	SP_BACK_Y = 6,
	SP_MSG_X = 3,
	SP_MSG_Y = 6,
};

enum {
	SIMG_WIDTH = 4*GRECT_XSIZE,
	SIMG_HEIGTH = 7*GRECT_YSIZE,
};

/* snake body: in, out -> { x, y } */
extern const int body_image_pos[4][4][2];
/* snake head: in -> { x, y } */
extern const int head_image_pos[4][2];
/* snake tail: out -> { x, y } */
extern const int tail_image_pos[4][2];

/* returns image positions for board-type */
void findImagePos(WormikGame::board_def t, unsigned *x, unsigned *y);

class InvalList
{
public:
	unsigned	flags;

	short		invlist[128][2];
	unsigned	ilength;

	void		reset(unsigned flags);
	void		add(short x, short y);
};

inline void InvalList::reset(unsigned flags_)
{
	flags = flags_;
	ilength = 0;
}


} } } } }; /* namespace cz::znj::sw::wormik::gui4x6x16 */

#endif
