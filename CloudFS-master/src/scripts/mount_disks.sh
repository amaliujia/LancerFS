#!/bin/bash

## Script to mount the SSD in the VirtualBox machine
##

SSD=$1
SSD_DEV="/dev/sdb1"

sudo mkdir -p ${SSD}
sudo /bin/mount -t ext2 -o user_xattr ${SSD_DEV} ${SSD}
sudo chown guest ${SSD}
sudo chgrp guest ${SSD}
sudo chmod a+rwx ${SSD}

mount | grep ^$SSD_DEV
if [ $? -ne 0 ]; then
  echo "Mounting of $SSD_DEV on $SSD failed!!!"
  exit 1
fi
