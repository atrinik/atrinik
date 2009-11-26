#!/bin/sh
#
# This is the Atrinik installation script.
# Use it after compiling or when using a binary package

prgname="atrinik_server"
pythonplug="plugin_python.so.0.1"
arenaplug="plugin_arena.so.0.1"

basedir="./../.."
datadir="/data"

echo "Copy binaries"
cp ./../../src/server/$prgname ./../../$prgname
cp ./../../src/plugins/plugin_python/$pythonplug ./../../plugins/$pythonplug
cp ./../../src/plugins/plugin_arena/$arenaplug ./../../plugins/$arenaplug
cp $basedir/src/utils/atrinikloop $basedir

echo "Create data directories"
mkdir $basedir/$datadir
mkdir $basedir/$datadir/tmp
mkdir $basedir/$datadir/log
mkdir $basedir/$datadir/unique-items
mkdir $basedir/$datadir/players

echo "Copy server data"
cp $basedir/install/* $basedir/$datadir

mkdir $basedir/lib
echo "Copy arch and lib files"
cp ./../../../arch/* $basedir/lib
echo "done."
