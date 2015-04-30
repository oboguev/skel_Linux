#
# ttyS0 - on-board
# ttyS4 - Intel AMT
# ttys5 - right RS232 looking from front
# ttys6 - left RS232 looking from front
#

#echo 1 >/sys/module/kgdb/parameters/kgdb_use_con
echo "ttyS6,115200" >/sys/module/kgdboc/parameters/kgdboc
echo 9 >/proc/sys/kernel/sysrq
sysctl kernel.softlockup_panic=1

echo kgdboc=`cat /sys/module/kgdboc/parameters/kgdboc`
#echo kgdbcon=`cat /sys/module/kgdb/parameters/kgdb_use_con`
echo ""
echo "use ALT-SYSRQ-G to enter the debugger"
echo "or echo g to /proc/sysrq-trigger"
