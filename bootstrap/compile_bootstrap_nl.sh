#!/bin/bash

#this has been tested with gcc; other compilers may or may not work
#this code should meet default gcc standard and gnu89 standard
#the $* is for -D DEBUG and similar

#if no C compiler is set in the environmental variables just try gcc
if [ -z "${CC}" ]
then
	CC="gcc"
fi

echo "Compiling using ${CC}..."

#the -O3 is a tailcall optimization option, and is necessary for the resultant interpreter to do tco
$CC -o bootstrap-nl *.c -O3 -Wall $*

if [ 0 -eq "$?" ]
then
	echo "Compiled!"
fi

