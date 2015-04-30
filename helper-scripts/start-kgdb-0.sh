#echo 1 >/sys/module/kgdb/parameters/kgdb_use_con
echo "ttyS0,115200" >/sys/module/kgdboc/parameters/kgdboc
echo 9 >/proc/sys/kernel/sysrq
sysctl kernel.softlockup_panic=1

echo kgdboc=`cat /sys/module/kgdboc/parameters/kgdboc`
#echo kgdbcon=`cat /sys/module/kgdb/parameters/kgdb_use_con`
echo ""
echo "use Alt-SysRq-g to enter the debugger"
echo "or echo g to /proc/sysrq-trigger"
