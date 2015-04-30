#!/bin/bash

#
# from kgdb
#
 
if [ $# != 2 ]
then
    echo disasfun objectfile functionname
    exit 1
fi
OBJFILE=$1
FUNNAME=$2
ADDRSZ=`objdump -t $OBJFILE | gawk -- "{
    if (\\\$3 == \\"F\\" && \\\$6 == \\"$FUNNAME\\") {
        printf(\\"%s %s\\", \\\$1, \\\$5)
    }
}"`
ADDR=`echo $ADDRSZ | gawk "{ printf(\\"%s\\",\\\$1)}"`
SIZE=`echo $ADDRSZ | gawk "{ printf(\\"%s\\",\\\$2)}"`
#echo $ADDRSZ
echo ADDR = $ADDR
echo SIZE = $SIZE
if [ -z "$ADDR" -o -z "$SIZE" ]
then
    echo Cannot find address or size of function $FUNNAME
    exit 2
fi
#set -x
objdump -S $OBJFILE --start-address=0x$ADDR --stop-address=$((0x$ADDR + 0x$SIZE))

