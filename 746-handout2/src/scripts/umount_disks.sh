#!/bin/bash

## Script to unmount the SSD in the VirtualBox machine
##

source ./paths.sh
mount | grep "$ssd_mnt" &> /dev/null
before=$?

sudo umount.ssd &> /dev/null

mount | grep "$ssd_mnt" &> /dev/null
after=$?

if [ $before -eq $after ]; 
then
  echo "Unmounting failed. Maybe it is not mounted?"
  exit 1
fi

echo "Unmounting successful!"
