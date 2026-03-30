#!/bin/bash
echo "#define WTCORE_VERSION_STR \"`git rev-list HEAD | head -n 1 | cut -b -8`\"" > ./api/version.h
echo "#define WTCORE_VERSION `git rev-list HEAD | wc -l`" >> ./api/version.h
if [ -e ../../inc/version.h ];then
    cat ../../inc/version.h >> ./api/version.h
fi