#!/bin/bash

## Script to create the Ext2 file system on two different disks
##

SSD_DEV="/dev/sdb1"

echo "*** Formatting SSD device ${SSD_DEV} with ext2 ..."
echo "***"
sudo mke2fs ${SSD_DEV}

echo "***NOTE: If an error shows up, you have not formatted the disk"
echo "***"
echo "***Run 'sudo fdisk /dev/sdb' to setup the SSD"

