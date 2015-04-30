#!/bin/sh

echo Installing modules ...
make modules_install

echo Installing kernel ...
V=`make kernelversion`
ARCH=`uname -m`
cp arch/$ARCH/boot/bzImage /boot/vmlinuz-$V
cp .config /boot/config-$V
cp System.map /boot/System.map-$V
mkinitramfs -o /boot/initrd.img-$V $V
ls -l /boot/*$V*

echo Updating GRUB ...
update-grub

sync
