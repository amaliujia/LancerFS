#!/bin/bash

## Script to create the Ext2 file system on two different disks
##

source ./paths.sh

echo "*** Formatting SSD device ${SSD_DEV} with ext4 ..."
echo "***"
mkfs.ext4 -F -b 4096 $ssd_disk

