#!/bin/bash

source ./paths.sh
pkill -9 cloudfs
./kill_server.sh
fusermount -u $fuse_mnt
./umount_disks.sh
./format_disks.sh
rm -r $s3_dir
rm -r $fuse_mnt
rm -r $ssd_mnt
mkdir -p $s3_dir
mkdir -p $fuse_mnt
mkdir -p $ssd_mnt

echo "Cleaned up everything to its default state."
