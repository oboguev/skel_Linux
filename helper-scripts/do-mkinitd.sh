# mkinitrd -v -L -B -b /sapmnt/HOME/i800461/kernel/myboot -k vmlinuz-3.12.0-1.0-my-0.5-default -i initrd-3.12.0-1.0-my-0.5-default -D eth1 -M /boot/System.map-3.12.0-1.0-my-0.5-default -f acpi block dm ifup kpartx lvm2 network netconsole resume.kernel usb
# mkinitrd -v -L -B -b /sapmnt/HOME/i800461/kernel/myboot -k vmlinuz-3.12.0-1.0-my-0.5-default -i initrd-3.12.0-1.0-my-0.5-default -D eth1 -M /boot/System.map-3.12.0-1.0-my-0.5-default -f acpi,block,dm,ifup,kpartx,lvm2,network,netconsole,resume.kernel,usb
mkinitrd -v -B -k vmlinuz-3.12.0-1.0-my-0.5-default -i initrd-3.12.0-1.0-my-0.5-default -D eth1 -M /boot/System.map-3.12.0-1.0-my-0.5-default -f 'acpi block dm ifup kpartx lvm2 network netconsole resume.kernel usb'
