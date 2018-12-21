#!/bin/bash

#generate invocation help string in help.h from 
#texinfo documentation
set -e

echo "char help_txt[]={\"\\" >../help.h
makeinfo invoking.txi --plaintext | sed 's,	,\\t,g;s,\",\\",g;s,\(.*\),\1\\n\\,g' >>../help.h
echo "\"};" >>../help.h
