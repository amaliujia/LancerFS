#!/bin/bash
#
# A script to test if the basic functions of the files 
# in CloudFS. Has to be run from the ./src/scripts/ 
# directory.
# 
source ./paths.sh

CLOUDFS=$cloudfs_bin
FUSE=$fuse_mnt
SSD=$ssd_mnt
CLOUD="/tmp/s3"
CLOUDFSOPTS=""
SSDSIZE=""
THRESHOLD="64"
AVGSEGSIZE=""
RABINWINDOWSIZE=""
CACHESIZE=""

TESTDIR="$FUSE"
TEMPDIR="/tmp/cloudfstest"
LOGDIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
STATFILE="$LOGDIR/stats"

CACHEDIR="/home/student/mnt/ssd/.cache/"

source ./functions.sh

function usage()
{
   echo "test_part1.sh <test-data.tar.gz> [cloudfs_options]"
   echo " cloudfs_options: -a|--ssd-size in KB"
   echo "                  -t|--threshold in KB"
}

#
# Execute battery of test cases.
# expects that the test files are in $TESTDIR
# and the reference files are in $TEMPDIR
# Creates the intermediate results in $LOGDIR
#
function execute_part3_tests()
{

   echo "Executing part3 basic tests"
   rm -rf $CACHEDIR 
   reinit_env
   
   # create the test data in FUSE dir
   untar $TARFILE $TESTDIR 
   # create a reference copy on ext2
   untar $TARFILE $TEMPDIR
   
   # get rid of disk cache
   ./cloudfs_controller.sh x
   snapshot_num=$(./snapshot /home/student/mnt/fuse/.snapshot s) 
   if [ $? -ne 0 ]; then
      print_result 1 
      exit
   fi
   ./cloudfs_controller.sh u 
   ./umount_disks.sh
   ./format_disks.sh
   ./mount_disks.sh

   ./cloudfs_controller.sh x
   ./snapshot /home/student/mnt/fuse/.snapshot r $snapshot_num
   
   	collect_stats > $STATFILE.ls
    cd $TEMPDIR && ls -lR --time-style=+|grep -v '^total' > $LOGDIR/ls-lR.out.master
    cd $TESTDIR && ls -lR --time-style=+|grep -v '^total' > $LOGDIR/ls-lR.out

    collect_stats >> $STATFILE.ls

    diff $LOGDIR/ls-lR.out.master $LOGDIR/ls-lR.out 
    print_result $?

}
#
# Main
#
TARFILE=$1
if [ ! -n $TARFILE ]; then
   usage
   exit 1
fi
shift
process_args $@
#----
# test setup
if [ ! -n $TESTDIR ]; then
   rm -rf "$TESTDIR/*"
fi
rm -rf $TEMPDIR
mkdir -p $TESTDIR
mkdir -p $TEMPDIR
mkdir -p $LOGDIR

#----
# tests
kill -9 `ps -lef|grep s3server.pyc|grep -v grep|awk '{print $4}'` > /dev/null 2>&1
./cloudfs_controller.sh u > /dev/null 2>&1
rm -rf $SSD/*
rm -rf $FUSE/*
rm -rf $CLOUD/*

python ../s3-server/s3server.pyc > /dev/null 2>&1 &
if [ $? -ne 0 ]; then
   echo "Unable to start S3 server"
   exit 1
fi
# wait for s3 to initialize
echo "Waiting for s3 server to initilaize(sleep 5)..."
sleep 5

./cloudfs_controller.sh m
if [ $? -ne 0 ]; then
   echo "Unable to start cloudfs"
   exit 1
fi

#run the actual tests
execute_part3_tests

#----
# test cleanup
rm -rf $TEMPDIR
rm -rf $LOGDIR

exit 0
