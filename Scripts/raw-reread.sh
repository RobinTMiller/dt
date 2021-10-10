#!/bin/bash

IOTOOLS=${IOTOOLS:-'/auto/share/IOGenerationTools'}
alias dt="${IOTOOLS}/Dt/Linux/dt"
alias spt="${IOTOOLS}/Spt/Linux/spt"
#alias dt=~romiller/Linux/dt
#alias spt=~romiller/Linux/spt

if [[ -z "${DISKS}" ]]; then
    disks=$(spt show devices spaths=/dev/mapper/mpath sfmt='%paths' vid=Nimble) 
    export DISKS=$(echo $disks | sed 's/ /,/g')
fi
##echo $DISKS

# Make this a required parameter so this script is general purpose!
DISKS=${DISKS:?Please define DISKS to list of disks to test!}
#
LOGDIR=${LOGDIR:-/var/tmp/dtlogs}
RUNTIME=${RUNTIME:-24h}

# Additional options plus overrides:
OPTIONS="max=1m hdsize=64 runtime=${RUNTIME} keepalivet=30 job_log=${LOGDIR}/dt_job%job.log log=${LOGDIR}/dt_thread-j%jobt%thread.log enable=htiming,stopimmed"

rm -rf ${LOGDIR}
mkdir -p ${LOGDIR}
dt of=${DISKS} workload=disk_read_after_write workload=log_timestamps ${OPTIONS}
ls -ls ${LOGDIR}
