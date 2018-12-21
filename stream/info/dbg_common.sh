#!/bin/bash

#squeezes all those debugAddModule lines from source files into readable documentation
#extracts the string argument and the (optional) one-line comment afterwards
set -e

echo @table @asis >dbg_common.txi
sed 's,.*= *debugAddModule *(\"\(.*\)\")\; *//\(.*\),@item \1\n\2 ,g;s,.*= *debugAddModule *(\"\(.*\)\")\;,@item \1,g;t;d' < ../../common/debug.c >>dbg_common.txi
echo @end table >>dbg_common.txi

