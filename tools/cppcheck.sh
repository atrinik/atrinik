#!/bin/bash

GIT_PATH=`which git`
CPPCHECK_PATH=`which cppcheck`

if [ -z $GIT_PATH ]; then
    echo "Git not found, ensure it's installed"
    exit 1
fi

if [ -z $CPPCHECK_PATH ]; then
    echo "cppcheck not found, ensure it's installed"
    exit 1
fi

REPO_ROOT=`git rev-parse --show-toplevel`

find "$REPO_ROOT" -name "*.[ch]" -and -not -path "*CMakeFiles*" -and \
    -not -path "*src/loaders*" -exec "$CPPCHECK_PATH" -q --force \
    --inline-suppr {} \;
