#!/bin/ksh
#
# Script: TestVolumes.ksh
# Author: Robin T. Miller
# Date:   January 31st, 2009
#
# Description:
#	This script is a wrapper around the DiskTests.ksh script to
# start this script on a list of volumes.
#

function usage
{
	print "Usage: $prog list_of_volumes"
	print "Example: /vol0 /vol1 ..."
	exit 1
}

[[ $# -lt 1 ]] && usage

typeset volumes=$*

#
# Start DiskScript on each volume and background the process.
#
for volume in $volumes
do
    volname=$(basename $volume)
    ./DiskTests.ksh $volume/dt.data >$volname.log 2>&1 &
done 

#
# Keep it simple (for now), just wait for all jobs.
# Note: Exit status should be gathered for each process.
#
set -m
wait
exit $?
