#!/bin/bash

## Script to mount the SSD in the VirtualBox machine
##

source ./paths.sh

mkdir -p $ssd_mnt
mount | grep "$ssd_mnt" &> /dev/null
before=$?

sudo mount.ssd &> /dev/null

mount | grep "$ssd_mnt" &> /dev/null
after=$?

if [ $before -eq $after ]; 
then
  echo "Mounting failed. Maybe it is not mounted?"
  exit 1
fi

echo "Mounting successful!"
