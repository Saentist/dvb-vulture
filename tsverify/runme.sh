#!/bin/bash

#for running with dmalloc

DMALLOC_OPTIONS=debug=0x4e48503,log=logfile
export DMALLOC_OPTIONS
LD_LIBRARY_PATH=../common
export LD_LIBRARY_PATH
cd `dirname $0`
./main "$@"
#gdb --args ./main $*
