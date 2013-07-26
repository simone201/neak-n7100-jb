#!/bin/bash

ROOTFS_PATH="/home/simone/neak-n7100/ramdisk-samsung-lte"

echo "Building SAMMY LTE N.E.A.K. version..."

# Cleanup
./clean.sh

# Making .config
make neak_lte_defconfig

# Compiling
./build.sh $ROOTFS_PATH
