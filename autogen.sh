#!/bin/sh

AUTORECONF_BIN=`which autoreconf`


if ! test -x "$AUTORECONF_BIN"; then
	echo "Oh Oh! I can't find Autotools."
	echo "Make sure Autotools is installed." 
fi

echo "Running Autotools..."

if autoreconf --install ; then
	echo "\nGreat! It worked!!!\n"
	echo "Now run:"
	echo "./configure"
	echo "make"
	echo "make install"
else
	echo "Ops! Something went wrong"
fi
