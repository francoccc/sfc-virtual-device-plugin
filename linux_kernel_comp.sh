#! /bin/bash

me=$(basename "$0")
err()  { echo >&2 "$*"; }

usearch() {
  case $(uname -s) in
    Linux) gcc -dumpmachine | sed s/-.*$// | sed s/powerpc/ppc/;;
    FreeBSD) uname -m;;
    Darwin)  uname -m;;
    *)			err "userarch: unknown system type"; exit 1;;
    esac
}

platform() {
  uname -s | tr '[A-Z]' '[a-z]'
}

userbuild_linux() {
  local ua=$(usearch)
  case "$ua" in
    i*86)   echo gnu;;
    *)      echo "gnu_$ua";;
  esac
}

[ -z "$KVER" ] && KVER=`uname -r`
[ -z "$KPATH" ] && KPATH=/lib/modules/$KVER/build
[ -z "$KPATH_SRC" ] && KPATH_SRC=$(echo `readlink -f "$KPATH"`|sed 's/[a-z|0-9]*$/common/')

err "KPATH: $KPATH"
rm -rf contrib/linux-headers-common && ln -sf $KPATH_SRC contrib/linux-headers-common