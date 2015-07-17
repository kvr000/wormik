-- ABOUT

Simple worm-like game.

You have to collect some food to get points and get to the next level.
Interesting anomaly is you can eat yourself and get the points for that.


-- LICENSE
Licence is GPL v2 (or newer), see http://www.gnu.org/licenses/ for details.

In windos distributions there are added some libraries - libpng, libjpeg,
libSDL, libSDL-ttf, libSDL-image. I think (hope) they all are written under
LGPL license...


-- INSTALLATION

SDL, SDL_image and SDL_ttf libraries are required for running and appropriate
development files for compiling.
Makefile is simple, no autoconf or other stuff, just run make and it should
work properly.
You can install it locally into any directory and run it from there, or install
data files (wormik_*.png) to /usr/share/games/wormik.


-- CONFIGURATION

Usually no need to configure it manually.

The configuration is stored in ~/.wormikrc, following is supported:
fullscreen=<number>		# sets fullscreen mode
datapath=path			# path to game data
font=/usr/.../fontfile.ttf	# use if game cannot find font
fontsize=<number>		# if fonts are too big, change it
record=...			# you can modify your records ;o)


-- CONTROLS

arrows	- changing snake direction
f	- toggle fullscreen mode
p	- pause
q, Esc	- quit
h	- show help
a	- about
return	- close dialog

-- RULES

Description of tiles is available when playing the game.

Positive: first advances your score by one and length by one, the second
advances your score by five and length by two.

Negative: only one, decrements your health by one and your length by one.

All the others tiles will kill you.

You can eat yourself too, then your health is halved (except when you eat only
your tail - then health is decremented by one). If you have enough points to
exit, then score is advanced by number of eated cells.

More points are added when reaching exit (8*level_num) etc.


-- AUTHORS etc.

Zbynek Vyskovsky <kvr000@gmail.com> , https://github.com/kvr000/wormik/


That's all, enjoy playing it,
	Zbynek Vyskovsky