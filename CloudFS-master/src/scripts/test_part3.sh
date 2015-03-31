#!/bin/bash
#
# A script to test if the basic functions of the files 
# in CloudFS. Has to be run from the ./src/scripts/ 
# directory.
# 

CLOUDFS="../cloudfs/bin/cloudfs"
FUSE="/mnt/fuse"
SSD="/mnt/ssd"
CLOUD="/tmp/s3"
CLOUDFSOPTS=" "

SSDSIZE=""
THRESHOLD="64"
AVGSEGSIZE="4096"
RABINWINDOWSIZE=""
CACHESIZE=""
NODEDUP="0"
NOCACHE="0"

CACHESIZELIMIT="1024"
# cachesizelimit = 1MB 

TESTDIR="$FUSE"
TEMPDIR="/tmp/cloudfstest"
LOGDIR="/tmp/testrun-`date +"%Y-%m-%d-%H%M%S"`"
STATFILE="$LOGDIR/stats"

WORKLOADSIZE="200"
WPPERCENTILE="30"

FILENUM="50"
CACHEDIR="/mnt/ssd/.cache/"

TEMPDIRWL="/tmp/cloudfsworkload"

source ./functions.sh
function usage()
{
	echo "test_part3.sh <test-data.tar.gz> [cloudfs_options]"
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
function execute_part3_tests()
{
        echo ""
	echo "Executing part3 tests"

	rm -rf $CACHEDIR
	reinit_env
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size 200

	untar $TARFILE $TESTDIR 
	untar $TARFILE $TEMPDIR
	# get rid of disk cache
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size 200
	PWDSAVE=$PWD
	cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
	collect_stats > $STATFILE.md5sum
	cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
	collect_stats >> $STATFILE.md5sum
	cd $PWDSAVE

	echo -ne "@@ Checking for file integrity : "
	diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
	print_result $?
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
	reinit_env
	echo "Running cloudfs with --no-cache"
	./cloudfs_controller.sh x $CLOUDFSOPTS --no-cache
	collect_stats > $STATFILE.nocache
	untar $TARFILE $TESTDIR 	    
	collect_stats >> $STATFILE.nocache
	max_cloud_storage_nocache=`get_cloud_max_usage $STATFILE.nocache`
	    
	rm -rf $CACHEDIR	    	    	    
	reinit_env
	echo "Running cloudfs with cache enabled with 300KB cache space"
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size 300
	collect_stats > $STATFILE.cache
	untar $TARFILE $TESTDIR 
	collect_stats >> $STATFILE.cache
	max_cloud_storage_cache=`get_cloud_max_usage $STATFILE.cache`
	
	echo "Cloud capacity usage with cache    : $max_cloud_storage_cache bytes"
	echo "Cloud capacity usage without cache : $max_cloud_storage_nocache bytes"
	echo -ne "@@ Checking for basic cache functionality I : "
	test $max_cloud_storage_cache -le $max_cloud_storage_nocache
	print_result $? 

	rm -rf $TEMPDIR/*
	rm -rf $CACHEDIR	    	    	    
	reinit_env

	echo "Stress test with cache - I"
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    
	for((i=1; i <= $FILENUM ; i++))
	do
	    dd if=/dev/urandom of=$TEMPDIR/file$i bs=$[($RANDOM%AVGSEGSIZE)+($THRESHOLD*1024)] count=1 > /dev/null 2>&1	    
	done
	
	for((i=1; i <= $FILENUM ; i++))
	do	   	    
	    
	    cat $TEMPDIR/file$i $TEMPDIR/file$i > $TEMPDIR/tfile$i
	    rm  $TEMPDIR/file$i
	    #cat $TEMPDIR/file$i $TEMPDIR/file$i > $TESTDIR/file$i
	    cat $TEMPDIR/tfile$i > $TESTDIR/tfile$i
	done        

	echo -ne "file content test(md5sum) after copy   "
	PWDSAVE=$PWD
	cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
	cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
	cd $PWDSAVE
	diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
	print_result $?
	
	success=0
	for((j=1; j < 50; j++))
	do
	    cat $TESTDIR/tfile$[($RANDOM%(($FILENUM-1)))+1] > /dev/null 2>&1	

	    cachesizeb=$(ls -alR $SSD/.cache/ | grep -v '^d' | awk '{ sum += $5 } END { print sum }')
	    #echo "$[($cachesizeb + 4096)]   $[($CACHESIZELIMIT*1024)] "
	    if [ $cachesizeb -gt $[($CACHESIZELIMIT*1024)] ]
	    then 
		echo "     Fatal: $cachesizeb greater than cache size limit $[($CACHESIZELIMIT*1024)]   "
		success=1
	    fi
	    
	done  	    
	echo "@@ read intensive test : "
	print_result $success	 

	success=0
	for((j=1; j < 50; j++))
	do
	    #echo $j iteration 
	    fileidx=$[($RANDOM%($FILENUM-1)+1)]
	    #echo "write to a file $fileidx "
	    echo "Modified" >>  $TESTDIR/tfile$fileidx	
	    echo "Modified" >>  $TEMPDIR/tfile$fileidx	
	    
	    cachesizeb=$(ls -alR $SSD/.cache/ | grep -v '^d' | awk '{ sum += $5 } END { print sum }')
	    #echo "$[($cachesizeb + 4096)]   $[($CACHESIZELIMIT*1024)] "
	    if [ $cachesizeb -gt $[($CACHESIZELIMIT*1024)] ]
	    then 
		echo "     Fatal: out of cache size limit"
		success=1
	    fi
	done 
	echo "@@ writ intensive test : "
	print_result $success
	
	echo -ne "file content test(md5sum) after write intensive test "
	PWDSAVE=$PWD
	cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
	cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
	cd $PWDSAVE
	diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
	print_result $?

	# check persistency of reference count information 
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    
	echo -ne "@@ File removal test (rm -rf) : "
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

	rm -rf $CACHEDIR	    	    	    
	reinit_env
	echo "@@ Running a rw mixed workload"
	echo "Warning: We strongly recommend to make your own workloads to develop and optimize your cache policy"
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    

	for((i=1; i <= (($FILENUM-1)) ; i++))
	do	   	    
	    cat $TEMPDIR/tfile$i $TEMPDIR/tfile$i > $TESTDIR/tfile$i
	done

	echo "Generate rw mixed workload : "
	for((j=1; j < $WORKLOADSIZE; j++))
	do
	    OPERATION="r"
	    RVSR=$(($RANDOM%100))
	    if [ $RVSR -lt $WPPERCENTILE ] 
	    then 
		OPERATION="w"
	    fi
	    #echo $RVSR
	    echo tfile$[($RANDOM%(($FILENUM-1)))+1]"             "$OPERATION >> $TEMPDIR/workload.list	
	done 
	echo "workload.list file is in $TEMPDIR"
	#ls -al /mnt/fuse/
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	

	collect_stats > $STATFILE.md5sum
	
	input=$TEMPDIR/workload.list 
	while read line 
	do
	    filename=$(echo "$line" | cut -c 1-10)
	    rwop=$(echo "$line" | cut -c 11-30)
	#echo "FILE" $filename "OP  " $rwop
	    
	    if [ $rwop == "r" ]
	    then 	   
		#echo "read  $filename $rwop"
		cat $TESTDIR/$filename > /dev/null 2>&1
		#cat $TESTDIR/$filename	
            fi 

	    if [ $rwop == "w" ]
	    then 	   
		#echo "write $filename $rwop"
		echo "Modified" >>  $TESTDIR/$filename
            fi        
	done < "$input"

	collect_stats >> $STATFILE.md5sum
	echo "Requests to cloud       : `get_cloud_requests $STATFILE.md5sum`"
	echo "Bytes read from cloud   : `get_cloud_read_bytes $STATFILE.md5sum`"
	echo "Capacity usage in cloud : `get_cloud_max_usage $STATFILE.md5sum`"	    
	echo ""
	echo "Reads to SSD     : `get_ssd_reads $STATFILE.md5sum`"
	echo "Writes to SSD    : `get_ssd_writes $STATFILE.md5sum`"
	echo "Sectors read     : `get_ssd_read_sectors $STATFILE.md5sum`"
	echo "Sectors written  : `get_ssd_write_sectors $STATFILE.md5sum`"	    
	echo "Cloud cost = `calculate_cloud_cost $STATFILE.md5sum`"	    

	# check persistency of reference count information 
	./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    

	echo -ne "Checking for big files on SSD except for .cache directory : "
	find $SSD \( ! -regex '.*/\..*' \) -type f -size +${THRESHOLD}k > $LOGDIR/find-above-treshold.out 
	nfiles=`wc -l $LOGDIR/find-above-treshold.out|cut -d" " -f1`
	print_result $nfiles

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


    #######################################################################
    # Extension to cover more cases including duplicated data across files  
    ######################################################################
    echo "@@ Stress test with cache - II (more duplicated workload)."

    rm -rf $CLOUD/*
    rm -rf $TEMPDIR/*
    mkdir -p $TEMPDIRWL
    # generate random files 
    for((i=1; i <= $FILENUM ; i++))
    do
	dd if=/dev/urandom of=$TEMPDIR/dfile$i bs=$[($RANDOM%AVGSEGSIZE)+($THRESHOLD*1024)] count=1 > /dev/null 2>&1	    
    done

    dd if=/dev/urandom of=$TEMPDIR/head.tmp   bs=$AVGSEGSIZE count=1 > /dev/null 2>&1	
    dd if=/dev/urandom of=$TEMPDIR/middle.tmp bs=$AVGSEGSIZE count=1 > /dev/null 2>&1	
    dd if=/dev/urandom of=$TEMPDIR/tail.tmp   bs=$AVGSEGSIZE count=1 > /dev/null 2>&1	

    for((i=1; i <= (($FILENUM-1)) ; i++))
    do	   	    
	cat  $TEMPDIR/head.tmp $TEMPDIR/dfile$i $TEMPDIR/middle.tmp $TEMPDIR/dfile$((i+1)) $TEMPDIR/tail.tmp > $TEMPDIR/tfile$i 
	rm   $TEMPDIR/dfile$i
    done
    rm $TEMPDIR/dfile$FILENUM
    rm $TEMPDIR/head.tmp
    rm $TEMPDIR/middle.tmp
    rm $TEMPDIR/tail.tmp

    rm -rf $CACHEDIR	    	    	    
    reinit_env
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    

    echo "copy test files from $TEMPDIR into $TESTDIR "
    for((i=1; i <= (($FILENUM-1)) ; i++))
    do	   	    	
	cat  $TEMPDIR/tfile$i > $TESTDIR/tfile$i
    done

    echo -ne "file content test(md5sum) after copy   "
    PWDSAVE=$PWD
    cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
    cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
    cd $PWDSAVE
    diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
    print_result $?

    echo "   Generate rw mixed workload : "
    for((j=1; j < $WORKLOADSIZE; j++))
    do
	OPERATION="r"
	RVSR=$(($RANDOM%100))
	if [ $RVSR -lt $WPPERCENTILE ] 
	then 
	    OPERATION="w"
	fi
	    #echo $RVSR
	echo tfile$[($RANDOM%(($FILENUM-1)))+1]"             "$OPERATION >> $TEMPDIR/workload.list	
    done 
    echo "workload.list file is in $TEMPDIR"
    cp $TEMPDIR/workload.list $TEMPDIRWL/workload.list

    echo " test out-of-cache space while running a workload : " 
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	
    input=$TEMPDIR/workload.list 
    success=0
    while read line 
    do
	filename=$(echo "$line" | cut -c 1-10)
	rwop=$(echo "$line" | cut -c 11-30)
	if [ $rwop == "r" ]
	then 	   
		#echo "read  $filename $rwop"
	    cat $TESTDIR/$filename > /dev/null 2>&1
		#cat $TESTDIR/$filename
	fi 
	if [ $rwop == "w" ]
	then 	   
		#echo "write $filename $rwop"
	    echo "MODIFY" >>  $TESTDIR/$filename
	    echo "MODIFY" >>  $TEMPDIR/$filename
	fi        

	cachesizeb=$(ls -alR $SSD/.cache/ | grep -v '^d' | awk '{ sum += $5 } END { print sum }')
	#echo "$[($cachesizeb + 4096)]   $[($CACHESIZELIMIT*1024)] "
	if [ $cachesizeb -gt $[($CACHESIZELIMIT*1024)] ]
	then 
	    echo "     Fatal: size of cache directory $cachesize GT cache-size $CACHESIZELIMIT"
	    success=1
	fi
    done < "$input"
    print_result $success	

    rm $TEMPDIR/workload.list
    echo "file content test(md5sum) after running workload"
    PWDSAVE=$PWD
    cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
    cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
    cd $PWDSAVE
    diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
    print_result $?
    cp $TEMPDIRWL/workload.list $TEMPDIR/workload.list

    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    
    echo -ne "Checking for big files on SSD except for .cache directory : "
    find $SSD \( ! -regex '.*/\..*' \) -type f -size +${THRESHOLD}k > $LOGDIR/find-above-treshold.out 
    nfiles=`wc -l $LOGDIR/find-above-treshold.out|cut -d" " -f1`
    print_result $nfiles

    echo "@@ unmount fuse and check persistency of cache data structure "
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    
    rm $TEMPDIR/workload.list
    echo "file content test(md5sum) after running workload"
    PWDSAVE=$PWD
    cd $TEMPDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out.master
    cd $TESTDIR && find .  \( ! -regex '.*/\..*' \) -type f -exec md5sum \{\} \; | sort -k2 > $LOGDIR/md5sum.out
    cd $PWDSAVE
    diff $LOGDIR/md5sum.out.master $LOGDIR/md5sum.out
    print_result $?
    cp $TEMPDIRWL/workload.list $TEMPDIR/workload.list

    sync 
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    
    echo -ne "Checking for big files on SSD except for .cache directory : "
    find $SSD \( ! -regex '.*/\..*' \) -type f -size +${THRESHOLD}k > $LOGDIR/find-above-treshold.out 
    nfiles=`wc -l $LOGDIR/find-above-treshold.out|cut -d" " -f1`
    print_result $nfiles

    echo -ne "File removal test (rm -rf) : "
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

    rm -rf $CACHEDIR	    	    	    
    reinit_env
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	    

    echo "copy test files from $TEMPDIR into $TESTDIR "
    for((i=1; i <= (($FILENUM-1)) ; i++))
    do	   	    	
	cat  $TEMPDIR/tfile$i > $TESTDIR/tfile$i
    done

    echo "run a rw mixed workload for benchmarking "
    ./cloudfs_controller.sh x $CLOUDFSOPTS --cache-size $CACHESIZELIMIT	
    collect_stats > $STATFILE.md5sum	
    input=$TEMPDIR/workload.list 
    while read line 
    do
	filename=$(echo "$line" | cut -c 1-10)
	rwop=$(echo "$line" | cut -c 11-30)
	    #echo "FILE" $filename "OP  " $rwop
	
	if [ $rwop == "r" ]
	then 	   
	    #echo "read  $filename $rwop"
	    cat $TESTDIR/$filename > /dev/null 2>&1
		#cat $TESTDIR/$filename
	fi 

	if [ $rwop == "w" ]
	then 	   
	    #echo "write $filename $rwop"
	    echo "Modified" >>  $TESTDIR/$filename
	fi        
    done < "$input"

    collect_stats >> $STATFILE.md5sum
    echo "Requests to cloud       : `get_cloud_requests $STATFILE.md5sum`"
    echo "Bytes read from cloud   : `get_cloud_read_bytes $STATFILE.md5sum`"
    echo "Capacity usage in cloud : `get_cloud_max_usage $STATFILE.md5sum`"	    
    echo ""
    echo "Reads to SSD     : `get_ssd_reads $STATFILE.md5sum`"
    echo "Writes to SSD    : `get_ssd_writes $STATFILE.md5sum`"
    echo "Sectors read     : `get_ssd_read_sectors $STATFILE.md5sum`"
    echo "Sectors written  : `get_ssd_write_sectors $STATFILE.md5sum`"	    
    echo "Cloud cost = `calculate_cloud_cost $STATFILE.md5sum`"	    
    rm -rf $TEMPDIRWL

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
execute_part3_tests
#----
# test cleanup
rm -rf $TEMPDIR
rm -rf $LOGDIR

exit 0
