#!/bin/sh

# Wrapper script that wraps gengallery.sh safely, preventing it from
# blocking the slowrxd process.

nohup $( dirname $( realpath $0 ) )/gengallery.sh "$@" > gengallery.log 2>&1 &
