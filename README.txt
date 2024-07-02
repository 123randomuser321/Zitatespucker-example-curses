## What

This is a very quick and dirty frontent for Zitatespucker, based on Curses.
I just made it to share it with my mates :)


## Building

Against NCurses:
[any gcc] -lZitatespucker -lpanel -lcurses main.c [any other options you like]

Against PDCurses:
[any gcc] -lZitatespucker -lpdcurses main.c [any other options you like]


## Limitations

The window must be at least Y=24 and X=68 in size, otherwise the program will (cleanly) terminate.

Any printed line beyond 56 characters will have its content split across multiple lines automatically.
However, you can still influence this behavior by manually putting "\n" somewhere within the strings.

Any quote exceeding 14 lines will be truncated.

The file paths to be read are hardcoded:
"zitate.json"
"zitate.sqlite"

Currently, CJK characters cause problems (J are not properly centered, Windows sometimes does not display them, etc...)
