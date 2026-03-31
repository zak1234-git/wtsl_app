#!/bin/bash

. envsetup.sh
echo "make for x64"
make clean && make ARCH=x64

echo "make for arm64"
make clean && make ARCH=arm64

echo "make for arm"
make clean && make