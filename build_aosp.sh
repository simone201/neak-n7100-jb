#!/bin/bash

ROOTFS_PATH="/home/simone/neak-n7100/ramdisk-aosp"

echo "Building AOSP GSM N.E.A.K. version..."

# Cleanup
./clean.sh

# Making .config
make neak_aosp_defconfig

# Compiling
./build.sh $ROOTFS_PATH
