#!/bin/bash

##
## Script to start the CloudFS file system
##
source ./paths.sh
cfs_fuse_mount=$fuse_mnt
cfs_ssd_mount=$ssd_mnt
cfs_process=$cloudfs_bin
MOUNT_OR_UMOUNT_FLAG=$1     ## "m" for mount, "u" for unmount, "x" for both

if [ "$1" = "m" ]
then
    mkdir -p ${cfs_fuse_mount}
    shift             # discard the first arg
    ./mount_disks.sh
    ${cfs_process} "$@"
elif [ "$1" = "u" ]
then
    sync
    ./umount_disks.sh
    fusermount -u ${cfs_fuse_mount}
    if [ $? -ne 0 ]; then
        echo "Trying to do a lazy unmount for cloudfs"
        fusermount -u -z ${cfs_fuse_mount}
    fi
elif [ "$1" = "x" ]
then
    sync
    fusermount -u ${cfs_fuse_mount}
    if [ $? -ne 0 ]; then
        echo "Trying to do a lazy unmount for cloudfs"
        fusermount -u -z ${cfs_fuse_mount}
    fi
    ./umount_disks.sh ${cfs_ssd_mount} 
    ./mount_disks.sh ${cfs_ssd_mount} 
    shift             # discard the first arg
    ${cfs_process} "$@"
else
    echo "***ERROR"
    echo "Usage: ./scriptName <MODE>"
    echo ""
    echo "where, MODE is one of the following ..."
    echo "m for mount, u for unmount, x for (unmount+mount) to drop caches"
fi
