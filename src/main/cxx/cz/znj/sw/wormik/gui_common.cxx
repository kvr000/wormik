/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * GUI common functions
 */

#include <assert.h>

#include <time.h>

#include "cz/znj/sw/wormik/WormikGame.hxx"

#include "cz/znj/sw/wormik/WormikGui.hxx"

#include "cz/znj/sw/wormik/gui_common.hxx"

namespace cz { namespace znj { namespace sw { namespace wormik { namespace gui4x6x16
{


/* position in season image */
enum {
	SP_NONE_X	= 2,
	SP_NONE_Y	= 6,
	SP_WALL_X	= 1,
	SP_WALL_Y	= 6,
	SP_POSIT_X	= 0,
	SP_POSIT_Y	= 5,
	SP_POSIT2_X	= 1,
	SP_POSIT2_Y	= 5,
	SP_NEGAT_X	= 2,
	SP_NEGAT_Y	= 5,
	SP_DEATH_X	= 3,
	SP_DEATH_Y	= 5,
	SP_EXIT_X	= 0,
	SP_EXIT_Y	= 6,
};

const int body_image_pos[4][4][2] =
{
	{
		{ 9, 9 },
		{ 0, 3 },
		{ 0, 4 },
		{ 2, 2 },
	},
	{
		{ 2, 3 },
		{ 9, 9 },
		{ 1, 3 },
		{ 1, 4 },
	},
	{
		{ 2, 4 },
		{ 3, 3 },
		{ 9, 9 },
		{ 1, 2 },
	},
	{
		{ 0, 2 },
		{ 3, 4 },
		{ 3, 2 },
		{ 9, 9 },
	},
};

const int head_image_pos[4][2] =
{
	{ 0, 0 },
	{ 1, 0 },
	{ 0, 1 },
	{ 1, 1 },
};

const int tail_image_pos[4][2] =
{
	{ 2, 0 },
	{ 3, 0 },
	{ 2, 1 },
	{ 3, 1 },
};

void findImagePos(WormikGame::board_def t, unsigned *x, unsigned *y)
{
	switch (WormikGame::GR_GET_FULL_TYPE(t)) {
	case WormikGame::GR_NONE:
		*x = SP_NONE_X; *y = SP_NONE_Y;
		break;
	case WormikGame::GR_WALL:
		*x = SP_WALL_X; *y = SP_WALL_Y;
		break;
	case WormikGame::GR_POSITIVE:
		*x = SP_POSIT_X; *y = SP_POSIT_Y;
		break;
	case WormikGame::GR_POSITIVE_2:
		*x = SP_POSIT2_X; *y = SP_POSIT2_Y;
		break;
	case WormikGame::GR_NEGATIVE:
		*x = SP_NEGAT_X; *y = SP_NEGAT_Y;
		break;
	case WormikGame::GR_DEATH:
		*x = SP_DEATH_X; *y = SP_DEATH_Y;
		break;
	case WormikGame::GR_EXIT:
		*x = SP_EXIT_X; *y = SP_EXIT_Y;
		break;
	case WormikGame::GR_BASE_SNAKE+WormikGame::GSF_SNAKE_HEAD:
		*x = head_image_pos[WormikGame::GR_GET_IN(t)][0]; *y = head_image_pos[WormikGame::GR_GET_IN(t)][1];
		break;
	case WormikGame::GR_BASE_SNAKE+WormikGame::GSF_SNAKE_BODY:
		*x = body_image_pos[WormikGame::GR_GET_IN(t)][WormikGame::GR_GET_OUT(t)][0]; *y = body_image_pos[WormikGame::GR_GET_IN(t)][WormikGame::GR_GET_OUT(t)][1];
		break;
	case WormikGame::GR_BASE_SNAKE+WormikGame::GSF_SNAKE_TAIL:
		*x = tail_image_pos[WormikGame::GR_GET_OUT(t)][0]; *y = tail_image_pos[WormikGame::GR_GET_OUT(t)][1];
		break;
	default:
		assert(0);
		break;
	}
	*x *= GRECT_XSIZE; *y *= GRECT_YSIZE;
}

void InvalidatedList::addObject(short x, short y)
{
	if ((flags&WormikGui::INVO_BOARD) == 0) {
		if (invalidatedLength >= sizeof(invalidatedList)/sizeof(invalidatedList[0])) {
			flags |= WormikGui::INVO_BOARD;
			invalidatedLength = 0;
		}
		else {
			invalidatedList[invalidatedLength][0] = x;
			invalidatedList[invalidatedLength][1] = y;
			invalidatedLength++;
		}
	}
}


} } } } };
