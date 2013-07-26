#!/bin/bash

ROOTFS_PATH="/home/simone/neak-n7100/ramdisk-samsung"

echo "Building SAMMY GSM N.E.A.K. version..."

# Cleanup
./clean.sh

# Making .config
make neak_defconfig

# Compiling
./build.sh $ROOTFS_PATH
