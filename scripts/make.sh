#!/bin/sh
rm -rf md4c
git clone git@github.com:mity/md4c.git md4c
make
make clean
