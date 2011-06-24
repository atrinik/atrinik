#!/bin/sh

cp arch/* server/lib > /dev/null 2>&1
cd server/data/tmp
rm -f *
cd ../..
chmod u+x atrinik_server
./atrinik_server -log logfile.log
cd ..
