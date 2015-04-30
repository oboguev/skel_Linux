#!/bin/sh

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 src-dir postfix" >&2
  exit 1
fi

if ! [ -d "$1" ]; then
  echo "$0: Directory $1 does not exist" >&2
  exit 1
fi

if ! [ -d "$1-$2" ]; then
  echo "$0: Directory $1-$2 does not exist" >&2
  exit 1
fi

diff -uprN $1 $1-$2 >$1-$2.patch

