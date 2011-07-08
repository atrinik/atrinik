#!/bin/sh

cd client

if [ -t 1 ]; then
	./atrinik "$@"
else
	./atrinik
fi
