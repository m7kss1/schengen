#!/bin/bash

export schengencfg='cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo'

export schengenbldfullspeed='cmake --build build -- -j $(nproc)'
