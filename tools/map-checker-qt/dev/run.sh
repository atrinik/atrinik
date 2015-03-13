#!/bin/sh

cd $(dirname $0)

for f in ../ui/*.ui
do
    pyuic5 $f -o $(echo $f | sed 's/\.ui$/.py/')
done
