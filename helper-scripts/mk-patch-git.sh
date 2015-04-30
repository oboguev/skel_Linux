#!/bin/sh

###############################################################################
#
#  Make patch (diff between two trees) using git
#
#  Example:
#
#      mk-git-patch 3.10 3.10-mypatch 
#
#  Creates:
#
#       <patched>.patch = patch file for (<orig> -> <patched>)
#
#
###############################################################################

usage()
{
    echo "Usage: $0 <orig-tree> <patched-tree>" >&2
    exit 1
}

canonic_dir()
{
    if ! [ -d "$1" ]; then
        echo "$1" is not a directory
        usage
    fi

    retval=$(readlink -ne "$1")
}

if [ "$#" -ne 2 ]; then
    usage
fi

orig_dir="$1"
patched_dir="$2"

canonic_dir "$orig_dir"
orig_dir="$retval"

canonic_dir "$patched_dir"
patched_dir="$retval"

if [ -e "$orig_dir/.git" ]; then
    echo "Error: $orig_dir/.git exists"
    exit 1
fi

if [ -e "$patched_dir/.git" ]; then
    echo "Error: $patched_dir/.git exists"
    exit 1
fi

rm -rf mkpatch-temp
mkdir mkpatch-temp
cd mkpatch-temp

echo "------------------------------------------------"
echo Creating temporary git in $(readlink -ne .)

git init

echo Checking in old version "$orig_dir"

rsync -a "$orig_dir/" .
git add --all .
git commit -q -m "orig"

echo Copying in patched version "$patched_dir"

rm -rf [a-z]* [A-Z]* [0-9]* .gitignore .mailmap
rsync -a "$patched_dir/" .
git add --all .
git diff -p --stat HEAD  >"$patched_dir.patch"
cd ..
rm -rf mkpatch-temp

echo "------------------------------------------------"
echo ""
echo "Patch was created in $patched_dir.patch"
echo ""

