#!/bin/bash

#squeezes all those debugAddModule lines from source files into readable documentation
#extracts the string argument and the (optional) one-line comment afterwards
set -e

echo @table @asis >dbg_main.txi
sed 's,.*= *debugAddModule *(\"\(.*\)\")\; *//\(.*\),@item \1\n\2 ,g;s,.*= *debugAddModule *(\"\(.*\)\")\;,@item \1,g;t;d' < ../main.c >>dbg_main.txi
echo @end table >>dbg_main.txi


