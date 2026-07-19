#!/bin/bash

SERVERIP="${1}"

rm -f iq.dat psd.dat plot.png

SRC="cpp"
BUILD="cpp/build"

g++ ${SRC}/main.cpp ${SRC}/rtltcp.cpp -o ${BUILD}/client
chmod +x ${BUILD}/client  

./${BUILD}/client --host ${SERVERIP} --freq 100e6 --rate 2.4e6 --n 2000000 --nfft 2048

kill $(pgrep -a rtl_tcp) 2>/dev/null; echo "stopped test rtl_tcp instance"

source venv/bin/activate && ./scripts/plot_data --freq 100e6 --rate 2.4e6 --file iq.dat --psd-file psd.dat