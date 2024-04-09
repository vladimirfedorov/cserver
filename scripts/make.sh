#!/bin/sh
if [ ! -d "md4c" ] || [ ! -d "cjson" ] || [ ! -d "mustach" ]; then
    ./scripts/init.sh
fi
ls -l
make
ls -l
make tests
ls -l
