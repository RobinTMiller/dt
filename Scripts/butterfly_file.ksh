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
# process scheduling, and possibly reordering by the file system or the
# I/O subsystem.  Ideally, a raw disk should be used, but this example
# utilizes a file within a file system.
#
# Note: On Linux, block device special files (DSF's) should either be
# bound to /dev/rawNN DSF's via raw(8) interface, or Direct I/O (DIO)
# should be enabled to bypass the buffer cache and its' affects.
#
alias dt=/u/rtmiller/Tools/dt.d-WIP/linux2.6-x86/dt
#
# Create a file of 100m, but mostly empty (sparse file).
#
rm -f dt.data butterfly_slice[12].log
dt of=dt.data position=100m-b count=1 dispose=keep pattern=0xdeadbeef stats=none
ls -ls dt.data
#
# Create two processes writing from each end of the disk to the middle.
#
dt of=dt.data bs=256k flags=direct slices=2 slice=1 iodir=forward enable=debug pattern=0xaaaaaaaa log=butterfly_slice1.log &
dt of=dt.data bs=256k flags=direct slices=2 slice=2 iodir=reverse enable=debug pattern=0xbbbbbbbb log=butterfly_slice2.log &
#
# Keep it simple, just wait for all jobs.
#
set -m
wait
#
# Ok, let's dump the data in hex.
#
hexdump -x dt.data
exit $?
