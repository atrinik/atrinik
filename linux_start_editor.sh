#!/bin/sh

cd editor
PATH=$PATH:../tools/map-checker-qt
java -jar AtrinikEditor.jar
cd ..
