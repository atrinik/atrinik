#!/bin/bash
#
# This script is for building AtrinikEditor.jar from Gridarta source code. It
# supports various features, such as uploading the generated .jar to a server
# through rsync, including generated update.properties (if any).
#
# In order for Gradle (Gridarta build system) to generate update.properties,
# create a file called build.developer in root Gridarta source code directory,
# and configure it like so:
#
#   buildDeveloper="<your name>"
#   updateUrlAtrinik="<URL of AtrinikEditor.jar>"

usage()
{
cat << EOF
Usage: $0 [options] path-to-gridarta-repo

OPTIONS:
   -s      Set sync destination using rsync. WARNING: rsync is configured with '-a --delete', so don't set the destination to something that has files you don't want to lose.
   -b      Set maximum number of builds
   -h      Show this message and exit
EOF
}

DEVELOPER=
URL=
BUILDS=
SYNC=
BUILD_PATH=atrinik-builds

while getopts "hs:b:" OPTION
do
    case $OPTION in
        h)
            usage
            exit 0
            ;;
        s)
            SYNC=$OPTARG
            ;;
        b)
            BUILDS=$OPTARG
            ;;
        ?)
            usage
            exit 1
            ;;
    esac
done

shift $(($OPTIND - 1))
GRIDARTA_PATH=$1

if [[ -z $GRIDARTA_PATH ]]; then
    echo "ERROR  : Path to Gridarta repository must be set."
    usage
    exit 1
fi

command -v git >/dev/null 2>&1 || { echo "WARNING: git not found, will not update repository" >&2; }

if [[ ! -z $SYNC ]]; then
    command -v rsync >/dev/null 2>&1 || { echo "ERROR  : Synchronization requested but rsync is not installed" >&2; exit 1; }
fi

echo "INFO   : Gridarta build started: $(date)"

echo "INFO   : Entering Gridarta directory..."
cd $GRIDARTA_PATH || exit 1

echo "INFO   : Updating git..."
git pull || echo "ERROR  : Could not update git." >&2

if [ -h AtrinikEditor.jar ]; then
    echo "INFO   : Removing old AtrinikEditor.jar symlink..."
    rm AtrinikEditor.jar || exit 1
fi

echo "INFO   : Cleaning workspace..."
./gradlew clean || exit 1

echo "INFO   : Building project..."
./gradlew :src:atrinik:preparePublish || exit 1

if [ ! -d $BUILD_PATH ]; then
    mkdir $BUILD_PATH
fi

_filename="AtrinikEditor-$(date +'%Y-%m-%d').jar" || exit 1

echo "INFO   : Copying jar to $BUILD_PATH/$_filename..."
cp src/atrinik/build/libs/AtrinikEditor.jar $BUILD_PATH/$_filename || exit 1

if [ -f src/atrinik/build/libs/update.properties ]; then
    echo "INFO   : Copying update properties to $BUILD_PATH..."
    cp src/atrinik/build/libs/update.properties $BUILD_PATH/ || exit 1
fi

echo "INFO   : Creating symlinks..."
ln -sf $BUILD_PATH/$_filename AtrinikEditor.jar || exit 1
ln -sf $_filename $BUILD_PATH/AtrinikEditor.jar || exit 1

if [[ ! -z $BUILDS ]]; then
    echo "INFO   : Removing old builds..."

    for file in $(ls $BUILD_PATH/AtrinikEditor-* | head -n -$BUILDS); do
        rm $file || exit 1
    done
fi

if [[ ! -z $SYNC ]]; then
    echo "INFO   : Synchronizing builds..."
    rsync -a --delete --exclude=.* $BUILD_PATH/ $SYNC/ || exit 1
fi

echo "INFO   : Gridarta build completed: $(date)"
