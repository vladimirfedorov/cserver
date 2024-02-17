#!/bin/sh
if [ ! -d "md4c" ] || [ ! -d "mustach" ]; then
    ./scripts/init.sh
fi
make
make clean
