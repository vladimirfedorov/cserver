#!/bin/sh
rm -rf md4c
git clone https://github.com/mity/md4c.git
rm -rf cjson
git clone https://github.com/DaveGamble/cJSON.git cjson
rm -rf mustach
# cc fails on master with multiple "Undefined symbol" errors
git clone -b 1.2.9 https://gitlab.com/jobol/mustach.git
