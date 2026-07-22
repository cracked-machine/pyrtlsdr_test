#!/bin/bash

SERVERIP="${1}"

rm -f audio.raw

source venv/bin/activate && PYTHONPATH="$(pwd)" python examples/fm_demod/python/main.py --host ${SERVERIP} --freq 100.4e6 --seconds 10 2>&1
