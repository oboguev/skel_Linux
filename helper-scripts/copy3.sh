#!/bin/sh

#
# Replicate the structure of ctl-dir from src-dir into dst-dir.
#
# Take file list/structure from ctl-dir.
# Ctl-dir is structurally a subset of src-dir.
#
# Take actual files from src-dir.
#
# Copy them to dst-dir.
#


if [ "$#" -ne 3 ]; then
    echo "Usage: $0 ctl-dir src-dir dst-dir"
    exit 1
fi

rm -rf $3/* $3/.[a-zA-Z0-9]* $3
mkdir $3

for fpath in $1/* $1/.*
do

    # echo ">>> $fpath $1 $2 $3"

    fbase=`basename $fpath`

    if [ "$fbase" = "." ] || [ "$fbase" = ".." ] ; then
        continue
    fi

    echo "  $fpath"

    if [ -d "$fpath" ]; then
        mkdir $3/$fbase
        $0 $1/$fbase $2/$fbase $3/$fbase 
        continue
    fi

    cp $2/$fbase $3/$fbase

done
