#!/bin/bash

rtl_discovery="$(which rtl_tcp && lsusb | grep -i rtl)"
if [[ -n ${rtl_discovery} ]]; then printf "Found dongle: \n\t"; fi
echo ${rtl_discovery}

kill $(pgrep -a rtl_tcp) 2>/dev/null; echo "stopped test rtl_tcp instance"
rtl_tcp -a 0.0.0.0 -p 1234 > /tmp/rtl_tcp.log 2>&1 &
echo "started pid $!"
sleep 2
cat /tmp/rtl_tcp.log

if ! pgrep -a rtl_tcp; then
    echo "NOT RUNNING"
    exit 1
fi