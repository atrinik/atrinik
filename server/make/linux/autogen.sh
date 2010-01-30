#!/bin/sh

aclocal -I macros --install || exit 1
autoconf || exit 1
./configure $*
