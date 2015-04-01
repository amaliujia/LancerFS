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

TESTDIR="$FUSE"
TEMPDIR="/tmp/cloudfstest"
LOGDIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
STATFILE="$LOGDIR/stats"

CACHEDIR=$cache_dir

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
function execute_part1_tests()
{

        echo "Executing part1 tests"
	rm -rf $CACHEDIR 
	reinit_env

	# create the test data in FUSE dir
	untar $TARFILE $TESTDIR 
	# create a reference copy on ext2
	untar $TARFILE $TEMPDIR

	# get rid of disk cache
	./cloudfs_controller.sh x

	#----
	# Testcases
	# assumes out test data does not have any hidden files(.* files)
	# students should have all their metadata in hidden files/dirs
	echo ""
	echo "Executing part1 tests"
	echo -ne "Basic file and attribute test(ls -lR) "
	collect_stats > $STATFILE.ls
	cd $TEMPDIR && ls -lR|grep -v '^total' > $LOGDIR/ls-lR.out.master
	cd $TESTDIR && ls -lR|grep -v '^total' > $LOGDIR/ls-lR.out

	collect_stats >> $STATFILE.ls

	diff $LOGDIR/ls-lR.out.master $LOGDIR/ls-lR.out 
	print_result $?

	echo -ne "Checking if cloud was used        "
	nr=`get_cloud_requests $STATFILE.ls`
	print_result $nr
	rm -f $STATFILE.ls

	#----
	echo -ne "Basic file content test(md5sum)   "
	cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
	cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out

	diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
	print_result $?

	#----
	echo -ne "Checking for big files on SSD     "
	find $SSD \( ! -regex '.*/\..*' \) -type f -size +${THRESHOLD}k > $LOGDIR/find-above-treshold.out 
	nfiles=`wc -l $LOGDIR/find-above-treshold.out|cut -d" " -f1`
	print_result $nfiles

	#----
	# Add your own tests, if you want to

	#----
	#destructive test : always do this test at the end!!
	echo -ne "File removal test (rm -rf)        "
	if [ $TESTDIR = $FUSE ]; then
		rm -rf $FUSE/*
		LF="$LOGDIR/files-remaining-after-rm-rf.out"
 
 		ls $FUSE > $LF
 		find $SSD \( ! -regex '.*/\..*' \) -type f >> $LF
 		find $CLOUD \( ! -regex '.*/\..*' \) -type f >> $LF
 		nfiles=`wc -l $LF|cut -d" " -f1`
 		print_result $nfiles
 	else
 		echo "TESTDIR($TESTDIR) != FUSEDIR($FUSE). Skipping this test."
 	fi
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
echo "Waiting for s3 server to initialize (sleep 5)..."
sleep 5

./cloudfs_controller.sh m
if [ $? -ne 0 ]; then
  echo "Unable to start cloudfs"
  exit 1
fi

#run the actual tests
execute_part1_tests

#----
# test cleanup
rm -rf $TEMPDIR
rm -rf $LOGDIR

exit 0
