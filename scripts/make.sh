#!/bin/sh
if [ ! -d "md4c" ] || [ ! -d "cjson" ] || [ ! -d "mustach" ]; then
    ./scripts/init.sh
fi
make
make clean
