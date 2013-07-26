#!/bin/bash

ROOTFS_PATH="/home/simone/neak-n7100/ramdisk-aosp-lte"

echo "Building AOSP LTE N.E.A.K. version..."

# Cleanup
./clean.sh

# Making .config
make neak_lte_aosp_defconfig

# Compiling
./build.sh $ROOTFS_PATH
