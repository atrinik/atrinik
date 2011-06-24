#!/bin/sh

cd editor
PATH=$PATH:../tools/map-checker
java -jar AtrinikEditor.jar
cd ..
