#!/bin/bash


# Note: This method is required since auto-mounter and DNS are NOT configured on each OS! :-(

declare -A OSdir
declare -A OShost
declare -A OSuser

OSs="AIX HPUX LINUX SOLARIS_SPARC SOLARIS_X86 SOLARIS_X64"

OSdir[AIX]='aix-6.1'
OShost[AIX]='rtp-ibm-dev01p1.rtplab.nimblestorage.com'
OSuser[AIX]='root'

OSdir[HPUX]='hpux-ia64'
OShost[HPUX]='rh-d8-u24.rtplab.nimblestorage.com'
OSuser[HPUX]='root'

# Note: We still do Linux builds manually.
OSdir[LINUX]='linux-rhel6x64'
OShost[LINUX]='rtm-rtp-centos6'
# Note: All cycle servers are CentOS 7 now!
# Actual cycle servers renamed as "duh-access*"
##OShost[LINUX]='rtpcycl05.rtplab.nimblestorage.com'
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

##DTVER='dt.v24'
GITHUB="/auto/home.nas04/romiller/GitHub/dt"
DTDIR="/var/tmp/romiller/GitHub/dt"
##OSBIN='/usr/bin/'
# Please configure ssh keys on each OS server for easier updates!

for os in ${OSs};
do
    OSDIR=${OSdir[${os}]}
    OSHOST=${OShost[${os}]}
    OSUSER=${OSuser[${os}]}
    # Linux is built in my home directory (a VM)!
    if [[ "${os}" == "LINUX" ]]; then
        BUILD_DIR="${GITHUB}/${OSDIR}"
    else
        BUILD_DIR="${DTDIR}/${OSDIR}"
    fi
    echo "Host: ${OSHOST}, OS Build Directory: ${OSDIR}, User: ${OSUSER}"
    # Do build, test, and copy.
    ssh ${OSUSER}@${OSHOST} "( uname -a ; cd ${BUILD_DIR} ; ./domake )"
    if [ $? -eq  0 ]; then
        # Quick test to ensure built Ok.
        ssh ${OSUSER}@${OSHOST} "( cd ${BUILD_DIR} ; ./dt version )"
        if [ $? -eq  0 ]; then
            if [ "${os}" != "LINUX" ]; then
                # Update the OS binary directory.
                ##ssh ${OSUSER}@${OSHOST} cp -p ${BUILD_DIR}/dt ${OSBIN}
                # Update the build tree in my home directory.
                scp -p ${OSUSER}@${OSHOST}:${BUILD_DIR}/dt ${GITHUB}/${OSDIR}/
            fi
        fi
    fi
done

# Windows (Robin's Laptop using Visual Studio Community Edition 2017) Manual Build!
# millerob@OVCQP3UPGX ~/dt.v23
# $ scp -p windows/x64/Release/dt.exe romiller@rtm-rtp-rhel7.rtpvlab.nimblestorage.com:Windows/

# Note: Update tools on San Jose Cycle Server: sjccycl.lab.nimblestorage.com

find . \( -name dt -o -name dt.exe \) -ls
find . \( -name dt -o -name dt.exe \) | xargs file

