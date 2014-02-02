#!/bin/bash

GIT_PATH=`which git`
UNCRUSTIFY_PATH=`which uncrustify`

if [ -z $GIT_PATH ]; then
    echo "Git not found, ensure it's installed"
    exit 1
fi

if [ -z $UNCRUSTIFY_PATH ]; then
    echo "Uncrustify not found, ensure it's installed"
    exit 1
fi

REPO_ROOT=`git rev-parse --show-toplevel`

find "$REPO_ROOT" -name "*.[ch]" -and -not -path "*CMakeFiles*" -and \
    -not -path "*src/loaders*" -exec "$UNCRUSTIFY_PATH" --no-backup \
    -c "$REPO_ROOT/tools/uncrustify.cfg" {} \;
