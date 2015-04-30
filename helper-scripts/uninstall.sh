#!/bin/sh

V=`make kernelversion`
CV=`uname -r`

if [ "$V" = "" ]; then
    echo Cannot get kernel version number
    exit 1
fi

if [ "$V" = "$CV" ]; then
    echo Cannot unistall kernel $V because it is the currently running version
    exit 1
fi

echo Uninstalling kernel version \"$V\" ...

echo Removing modules ...
set -x
rm -rf /lib/modules/$V
set +x

echo Removing kernel ...
set -x
rm -f /boot/vmlinuz-$V
rm -f /boot/config-$V
rm -f /boot/System.map-$V
rm -f /boot/initrd.img-$V
set +x

echo Updating GRUB ...
update-grub

sync

