#!/bin/ksh
#
# Script: butterfly.ksh
# Author: Robin T. Miller
# Date:   May 1st, 2008
#
# Description:
#	This script uses the data test (dt) program, to force I/O to be
# like a butterfly, that is from front to back and visa versa.
#
# Please Note: The I/O pattern won't alternate every other I/O, due to
# process scheduling, and possibly reordering by the I/O subsystem.
#
# Note: On Linux, block device special files (DSF's) should either be
# bound to /dev/rawNN DSF's via raw(8) interface, or Direct I/O (DIO)
# should be enabled to bypass the buffer cache and its' affects.
#
alias dt=/u/rtmiller/Tools/dt.d-WIP/linux2.6-x86/dt
rm -f butterfly_slice[12].log
#
# Create two processes writing from each end of the disk to the middle.
#
dt of=/dev/sdc bs=256k flags=direct slices=2 slice=1 iodir=forward enable=debug pattern=0xaaaaaaaa log=butterfly_slice1.log &
dt of=/dev/sdc bs=256k flags=direct slices=2 slice=2 iodir=reverse enable=debug pattern=0xbbbbbbbb log=butterfly_slice2.log &
#
# Keep it simple (for now), just wait for all jobs.
# Note: Exit status should be gathered for each process.
#
set -m
wait
exit $?
