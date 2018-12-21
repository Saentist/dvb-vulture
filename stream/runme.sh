#!/bin/bash

#`dmalloc low -llogfile`
DMALLOC_OPTIONS=debug=0x4e48503,log=logfile
#`dmalloc high -i 100000 -l logfile`
#DMALLOC_OPTIONS=debug=0x4f4ed03,inter=100000,log=logfile
#`dmalloc runtime -i 100000 -l logfile`
#DMALLOC_OPTIONS=debug=0x4000503,inter=100000,log=logfile
export DMALLOC_OPTIONS
LD_LIBRARY_PATH=../common
LD_DEBUG=files
LD_DEBUG_OUTPUT=./ld.log
export LD_LIBRARY_PATH
export LD_DEBUG
export LD_DEBUG_OUTPUT
#gdb --args ./main -nd -cfg /etc/dvbvulture/sa_test.cfg $*
#./main -cfg /etc/dvbvulture/sa_test.cfg "$@"
gdb --args ./main -nd -d svt=255 -d pidbuf=255
