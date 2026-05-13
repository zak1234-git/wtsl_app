#!/bin/bash
echo "#define Version \"`git rev-list HEAD | head -n 1 | cut -b -8`\"" > ./inc/version.h
echo "#define VersionCode `git rev-list HEAD | wc -l`" >> ./inc/version.h