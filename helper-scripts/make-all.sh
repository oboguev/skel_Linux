#!/bin/sh
echo '***************************************************'
grep CMGROUP .config
echo '***************************************************'
rm -f nohup.out
rm -f modules.out
rm -f modules.err
set -x
make clean
make kernelversion
nohup make -j8 bzImage
set +x
echo '***************************************************'
grep error nohup.out
grep warning nohup.out
echo '***************************************************'
tail nohup.out
echo '***************************************************'
set -x
nohup make -j8 modules 1>modules.out 2>modules.err
set +x
echo '***************************************************'
tail modules.out
echo '***************************************************'
tail modules.err
echo '***************************************************'
grep error modules.err
echo '***************************************************'

