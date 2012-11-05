#!/sbin/busybox sh
# Doing some cleanup
# by Simone201

/sbin/busybox mount -o remount,rw /system

if [ -e /system/etc/init.d/S98bolt_siyah ]; then
	/sbin/busybox rm /system/etc/init.d/S98bolt_siyah
fi;

if [ -e /system/etc/init.d/s78enable_touchscreen_1 ]; then
	/sbin/busybox rm /system/etc/init.d/s78enable_touchscreen_1
fi;

if [ -e /system/etc/init.d/S98CFS ]; then
	/sbin/busybox rm /system/etc/init.d/S98CFS
fi;

# Creating init.d folder if it doesn't exist
if [ ! -d /system/etc/init.d ]; then
	/sbin/busybox mkdir /system/etc/init.d
fi;

/sbin/busybox mount -o remount,ro /system
