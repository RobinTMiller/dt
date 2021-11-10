#!/bin/bash


# Note: This method is required since auto-mounter and DNS are NOT configured on each OS! :-(

declare -A OSdir
declare -A OShost
declare -A OSuser

OSs="AIX HPUX SOLARIS_SPARC SOLARIS_X64"
# Note: We also test Linux and Windows separately/manually!
##OSs="AIX HPUX LINUX SOLARIS_SPARC SOLARIS_X86 SOLARIS_X64"

OSdir[AIX]='aix-6.1'
OShost[AIX]='rtp-ibm-dev01p1.rtplab.nimblestorage.com'
OSuser[AIX]='root'

OSdir[HPUX]='hpux-ia64'
OShost[HPUX]='rh-d8-u24.rtplab.nimblestorage.com'
OSuser[HPUX]='root'

OSdir[LINUX]='linux-rhel6x64'
OShost[LINUX]='rtpcycl01.rtplab.nimblestorage.com'
OSuser[LINUX]='romiller'

OSdir[SOLARIS_SPARC]='solaris-sparc'
OShost[SOLARIS_SPARC]='rs-d7-u22.rtplab.nimblestorage.com'
OSuser[SOLARIS_SPARC]='root'

OSdir[SOLARIS_X86]='solaris-x86'
OShost[SOLARIS_X86]='rtm-rtp-solaris11x86.rtpvlab.nimblestorage.com'
OSuser[SOLARIS_X86]='root'

OSdir[SOLARIS_X64]='solaris-x64'
OShost[SOLARIS_X64]='rtm-rtp-solaris11x86.rtpvlab.nimblestorage.com'
OSuser[SOLARIS_X64]='root'

##DTVER='dt.v23'
DTDIR="/var/tmp/romiller/GitHub/dt"
TESTDIR="/var/tmp/dtfiles/dt.data"
DTWORKLOAD="of=${TESTDIR} workload=longevity_file_system runtime=0 limit=100m disable=stats enable=job_stats"

# Please configure ssh keys on each OS server for easier updates!

for os in ${OSs};
do
    OSDIR=${OSdir[${os}]}
    OSHOST=${OShost[${os}]}
    OSUSER=${OSuser[${os}]}
    echo "Host: ${OSHOST}, Directory: ${OSDIR}, User: ${OSUSER}"
    # Do quick test to verify dt build.
    ssh ${OSUSER}@${OSHOST} "( cd ${DTDIR}/${OSDIR} ; ./dt ${DTWORKLOAD} )"
    if [ $? -ne  0 ]; then
        echo "dt test failed, fix and re-test!"
        break
    fi
done
