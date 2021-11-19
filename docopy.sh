#!/bin/bash

# Note: This method is required since auto-mounter and DNS are NOT configured on each OS! :-(

DT_FILES=${DT_FILES:?Please define DT_FILES with list of files to copy to each build system!}

declare -A OSdir
declare -A OShost
declare -A OSuser

OSs="AIX HPUX SOLARIS_SPARC SOLARIS_X64"
# Note: No need to copy files to Linux today!
# Also Note: The Solaris VM is used for x86 and x64 builds.
##OSs="AIX HPUX LINUX SOLARIS_SPARC SOLARIS_X86 SOLARIS_X64"

OSdir[AIX]='aix-6.1'
OShost[AIX]='rtp-ps814-dev01p1.rtplab.nimblestorage.com'
OSuser[AIX]='root'

OSdir[HPUX]='hpux-ia64'
OShost[HPUX]='rh-d8-u24.rtplab.nimblestorage.com'
OSuser[HPUX]='root'

##OSdir[LINUX]='linux-rhel6x64'
##OShost[LINUX]='rtpcycl01.rtplab.nimblestorage.com'
##OSuser[LINUX]='romiller'

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
DTDIR="/var/tmp/romiller/GitHub/dt/"
# Please configure ssh keys on each OS server for easier updates!

for os in ${OSs};
do
    OSDIR=${OSdir[${os}]}
    OSHOST=${OShost[${os}]}
    OSUSER=${OSuser[${os}]}
    echo "Host: ${OSHOST}, Directory: ${OSDIR}, User: ${OSUSER}"
    # Copy updated files to each OS build system.
    scp -p ${DT_FILES} ${OSUSER}@${OSHOST}:${DTDIR}/
    # Note: The actual dt building is done separately.
done
