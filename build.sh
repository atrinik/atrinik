#!/bin/bash

DIRS="server client"
TARGET=${1:-all}

for dir in $DIRS; do
    cd $dir/
    cmake .
    make $TARGET
    cd ../
done
