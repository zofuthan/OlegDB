#!/usr/bin/env bash

export LD_LIBRARY_PATH=./build/lib:$LD_LIBRARY_PATH
./build/bin/oleg_test test
