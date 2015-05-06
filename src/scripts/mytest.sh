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
AVGSEGSIZE="4096"
RABINWINDOWSIZE=""
CACHESIZE=""
NODEDUP="0"
NOCACHE="0"

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
	echo "                  -d|--no-dedup "
	echo "                  -S|--avg-seg-size in KB"
	echo "                  -w|--rabin-window-size in KB"
}

#
# Execute battery of test cases.
# expects that the test files are in $TESTDIR
# and the reference files are in $TEMPDIR
# Creates the intermediate results in $LOGDIR
#
function execute_part2_tests()
{

    #----
    # Testcases
    # assumes out test data does not have any hiddenfiles(.* files)
    # students should have all their metadata in hidden files/dirs
    echo ""
    echo "Executing part2 tests"
    rm -rf $CACHEDIR
    reinit_env
    ./cloudfs_controller.sh x $CLOUDFSOPTS

    untar $TARFILE $TESTDIR 
    untar $TARFILE $TEMPDIR
	# get rid of disk cache

    ./cloudfs_controller.sh x $CLOUDFSOPTS

    PWDSAVE=$PWD
    cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec tail \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
    collect_stats > $STATFILE.md5sum
    cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec tail \{\} \; | sort -k2 > $LOGDIR/md5sum.out
    collect_stats >> $STATFILE.md5sum
    cd $PWDSAVE

    #PWDSAVE=$PWD
    #cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec tail -c 10 \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
    #collect_stats > $STATFILE.md5sum
    #cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec tail -c 10 \{\} \; | sort -k2 > $LOGDIR/md5sum.out
    #collect_stats >> $STATFILE.md5sum
    #cd $PWDSAVE
    
		echo -ne "Checking for file integrity : "
    #diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
    #print_result $?

    echo "Requests to cloud       : `get_cloud_requests $STATFILE.md5sum`"
    echo "Bytes read from cloud   : `get_cloud_read_bytes $STATFILE.md5sum`"
    echo "Capacity usage in cloud : `get_cloud_max_usage $STATFILE.md5sum`"

    echo ""
    echo "Reads to SSD     : `get_ssd_reads $STATFILE.md5sum`"
    echo "Writes to SSD    : `get_ssd_writes $STATFILE.md5sum`"
    echo "Sectors read     : `get_ssd_read_sectors $STATFILE.md5sum`"
    echo "Sectors written  : `get_ssd_write_sectors $STATFILE.md5sum`"

    echo "Cloud cost = `calculate_cloud_cost $STATFILE.md5sum`"

    rm -rf $CACHEDIR

	#----
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
#run the actual tests
execute_part2_tests
#----
# test cleanup
rm -rf $TEMPDIR
rm -rf $LOGDIR

exit 0
