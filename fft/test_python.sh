#!/bin/bash

SERVERIP="${1}"

rm -f iq.dat psd.dat plot.png

source venv/bin/activate && python fft/python/main.py --host ${SERVERIP} --freq 100e6 --rate 2.4e6 --n 2000000 --nfft 2048 2>&1

source venv/bin/activate && ./scripts/plot_data --freq 100e6 --rate 2.4e6 --file iq.dat --psd-file psd.dat