#!/bin/bash -p

# /dev/sdb /dev/sg5|0|150|0|Direct|SPC-4|HGST    |HUH721010AL5204 |C202|N

export DISKS=$(scu show edt raw | fgrep Direct | fgrep "HGST" | \
       awk 'BEGIN {FS = "|"} {print $1 }' | awk '{printf "%s,", $1 }')

DISKS=$( echo ${DISKS} | sed 's/,$//' )
if [[ -z ${DISKS} ]] ;
then
    echo "Warning: No disks were found!"
    unset DISKS
else
    echo "Disks: ${DISKS}"
    rm -f ERRORS.log
    rm -rf logs ; mkdir logs
    ./dt if=${DISKS} workload=san_disk slices=0 disable=compare enable=raw,scsiio runtime=15m error_log=ERRORS.log log=logs/thread-%dsf-%thread.log job_log=logs/job-%dsf.log
    #./dt of=${DISKS} workload=san_disk enable=raw,scsiio runtime=1h error_log=ERRORS.log log=logs/thread-%dsf-%thread.log job_log=logs/job-%dsf.log
fi
