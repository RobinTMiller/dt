#!/bin/bash

RGITHUB="romiller@rtm-rtp-rhel7.rtpvlab.nimblestorage.com:GitHub/dt"

scp -p ${RGITHUB}/aix-6.1/dt aix-6.1/
scp -p ${RGITHUB}/hpux-ia64/dt hpux-ia64/
scp -p ${RGITHUB}/linux-rhel6x64/dt linux-rhel6x64/
scp -p ${RGITHUB}/linux-rhel7x64/dt linux-rhel7x64/
scp -p ${RGITHUB}/solaris-x64/dt solaris-x64/
scp -p ${RGITHUB}/solaris-sparc/dt solaris-sparc/
scp -p ${RGITHUB}/solaris-x86/dt solaris-x86/
