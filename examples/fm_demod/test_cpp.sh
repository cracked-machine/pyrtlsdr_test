#!/bin/bash

SERVERIP="${1}"

rm -f audio.raw

COMMON="examples/common/cpp"
SRC="examples/fm_demod/cpp"
BUILD="examples/fm_demod/cpp/build"

mkdir -p ${BUILD}
g++ -std=c++17 -O2 ${SRC}/main.cpp ${COMMON}/rtltcp.cpp -I./examples/common/cpp -o ${BUILD}/client
chmod +x ${BUILD}/client

./${BUILD}/client --host ${SERVERIP} --freq 100.4e6 --seconds 10
