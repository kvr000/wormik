/*
 * Wormik, game by Zbynek Vyskovsky, under GPL license
 * http://atrey.karlin.mff.cuni.cz/~rat/wormik/
 *
 * main function
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <assert.h>
#include <time.h>

#include "cz/znj/sw/wormik/WormikGame.hxx"
#include "cz/znj/sw/wormik/WormikGui.hxx"

namespace cz { namespace znj { namespace sw { namespace wormik {


extern WormikGui *create_WormikGui();
extern WormikGame *create_WormikGame();


} } } };

using namespace cz::znj::sw::wormik;


int main(void)
{
	WormikGame *game;
	WormikGui *gui;

	srand(time(NULL));

	game = create_WormikGame();
	gui = create_WormikGui();
	game->setGui(gui);
	if (gui->init(game) < 0) {
		delete game;
		delete gui;
		return 1;
	}
	game->run();

	return 0;
}
