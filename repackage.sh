#!/bin/bash
#(C) 2011 SIANA GEARZ

usage() {
	echo "Usage: repackage PLATTFORM FILEIN.tar.bz2 [FILEOUT.tar.bz2]
Repackage an archive from llautobuild format into legacy format

PLATTFORM can be one of windows, linux, mac.
"
	exit 0
}

TMP="/tmp/pak$$"
LIBPATH=""
INCPATH=""
PWD=`pwd`

shopt -s nocasematch
case "$1" in
	--windows|-w|windows|win)
		MODE=win
		LIBPATH="libraries/i686-win32/lib/release"
		LIBDPATH="libraries/i686-win32/lib/debug"
		INCPATH="libraries/i686-win32/include"
		;;
	--mac|--osx|--darwin|-x|mac|osx|darwin)
		MODE=osx
		LIBPATH="libraries/universal-darwin/lib_release"
		LIBDPATH="libraries/universal-darwin/lib_release"
		INCPATH="libraries/universal-darwin/include"
		;;
	--lin|--linux|-l|linux)
		MODE=linux
		LIBPATH="libraries/i686-linux/lib_release_client"
		INCPATH="libraries/i686-linux/include"
		;;
	--linux64|-6|linux64)
		MODE=linux64
		LIBPATH="libraries/x86_64-linux/lib_release_client"
		INCPATH="libraries/x86_64-linux/include"
		;;
	*)
		usage
		;;
esac

case "$2" in
	--commoninclude|--common-include|--common)
		INCPATH="libraries/include"
		shift
		;;
esac

test -n "$2" && FILEIN=`readlink -e $2`
test -n "$3" && FILEOUT=`readlink -f $3`

if [ -z $FILEIN ]; then
	usage
fi

if [ -z $FILEOUT ]; then
	FILEOUT=`readlink -m package.tar.bz2`
fi

mkdir "$TMP"
cd "$TMP"

case "$FILEIN" in
	http\:\/\/|https\:\/\/)
		echo "	Downloading..."
		wget "$FILEIN" -O package.tar.bz2
		echo "	Unpacking..."
		tar -xjvf package.tar.bz2
		rm package.tar.bz2
		;;
	*)
		echo "	Unpacking..."
		tar -xjvf "$FILEIN"
		;;
esac

if [ -n "$LIBDPATH" -a -d lib/debug ]; then
	mkdir -p $LIBDPATH
	mv -f lib/debug/* $LIBDPATH
fi
if [ -d lib/release ]; then
	mkdir -p $LIBPATH
	mv -f lib/release/* $LIBPATH
fi
if [ -d include ]; then
	mkdir -p $INCPATH
	mv -f include/* $LIBPATH
fi

echo "	Packing..."
tar -cjvf "$FILEOUT" libraries LICENSES
echo "	Checksum:"
cd `dirname "$FILEOUT"`
md5sum -b `basename "$FILEOUT"`
cd "$PWD"
rm -rf "$TMP"

