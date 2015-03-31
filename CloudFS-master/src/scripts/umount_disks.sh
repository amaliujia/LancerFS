#!/bin/bash

## Script to unmount the SSD in the VirtualBox machine
##

SSD=$1
SSD_DEV="/dev/sdb1"

sudo /bin/umount ${SSD_DEV}

mount | grep ^$SSD_DEV
if [ $? -eq 0 ]; then
  echo "unmounting of $SSD_DEV on $SSD failed!!!!."
  exit 1
fi

