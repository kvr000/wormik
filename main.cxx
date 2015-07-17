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
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include "WormikGame_If.hxx"
#include "WormikGui_If.hxx"

extern WormikGui_If *create_Wormik_SDL();
extern WormikGame_If *create_WormikGame();

int main(void)
{
	WormikGame_If *game;
	WormikGui_If *gui;

	srand(time(NULL));

	game = create_WormikGame();
	gui = create_Wormik_SDL();
	game->setGui(gui);
	if (gui->init(game) < 0) {
		delete game;
		delete gui;
		return 1;
	}
	game->run();

	return 0;
}
