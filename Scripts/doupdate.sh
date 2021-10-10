#!/bin/bash

# Note: This method is required since auto-mounter and DNS are NOT configured on each OS! :-(

DT_CURRENT=${DT_CURRENT:?Please define DT_CURRENT with the previous dt version! (e.g. v23.03)}

# Define Associative Arrays:
declare -A OSdir
declare -A OSdest

# Operating Systems: (we support today)
OSs="AIX HPUX LINUX SOLARIS_SPARC SOLARIS_X86 SOLARIS_X64 WINDOWS"

OSdir[AIX]='aix-6.1'
OSdest[AIX]='Aix'

OSdir[HPUX]='hpux-ia64'
OSdest[HPUX]='hpux-ia64'

OSdir[LINUX]='linux-rhel6x64'
OSdest[LINUX]='Linux'

OSdir[SOLARIS_SPARC]='solaris-sparc'
OSdest[SOLARIS_SPARC]='Solaris/sparc'

OSdir[SOLARIS_X86]='solaris-x86'
OSdest[SOLARIS_X86]='Solaris/x86'

OSdir[SOLARIS_X64]='solaris-x64'
OSdest[SOLARIS_X64]='Solaris/x64'

OSdir[WINDOWS]='windows/x64/Release'
OSdest[WINDOWS]='Windows'

DTVER='dt.v23'
OSBIN='/usr/bin/'
TOPDIR='/auto/share/IOGenerationTools/Dt'
# TODO: Automate version extraction:
##find ${TOPDIR} \( -name 'dt' -o -name 'dt.exe' \) | xargs strings | fgrep "Version:"

for os in ${OSs};
do
    ##echo "OS: ${os}"
    OSDIR=${OSdir[${os}]}
    OSDEST=${OSdest[${os}]}
    if [[ "${os}" == "WINDOWS" ]]; then
        TOOL="dt.exe"
    else
        TOOL="dt"
    fi
    OSPATH="${TOPDIR}/${OSDEST}/${TOOL}"
    ##echo "PATH: ${OSPATH}"
    if [[ -e ${OSPATH} ]]; then
        echo "Renaming ${TOPDIR}/${OSDEST}/${TOOL} to ${OSPATH}-${DT_CURRENT}..."
        mv ${OSPATH} ${OSPATH}-${DT_CURRENT}
    fi
    echo "Copying ${OSDIR}/${TOOL} to ${TOPDIR}/${OSDEST}..."
    cp -p ${OSDIR}/${TOOL} ${OSPATH}
    if [ $? -ne  0 ]; then
        echo "The copy failed, please correct issue and retry!"
        break
    fi
    strings ${OSPATH} | fgrep "Version:"
done
##find . -mtime -1 -ls
find ${TOPDIR} \( -name 'dt' -o -name 'dt.exe' \) -ls
find ${TOPDIR} \( -name 'dt' -o -name 'dt.exe' \) | xargs strings | fgrep "Version:"
