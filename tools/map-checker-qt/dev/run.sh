#!/bin/sh

cd $(dirname $0)

for f in ../ui/*.ui
do
    f_new=$(echo $f | sed 's/\.ui$/.py/')

    if [ ! -f $f_new ]; then
        pyuic5 $f -o $f_new
    else
        f_new_tmp=$f_new.tmp
        pyuic5 $f -o $f_new_tmp

        diff=`grep -Fxvf $f_new $f_new_tmp | grep -ve "^#"`

        if [ -n "$diff" ]; then
            mv $f_new_tmp $f_new
        else
            rm $f_new_tmp
        fi
    fi
done
