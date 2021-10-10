#!/bin/bash -p

# Variables for dt scripts:
export SRC_DIR=/mnt/localhost/iscsi-rtp-smc-qa18-4-v9-2a22895287d11c80d6c9ce900fa42bd6b
export DST_DIR=/mnt/localhost/iscsi-rtp-smc-qa18-4-v10-2d12ed6a3394567a56c9ce900fa42bd6b
export DIRECTORY=/mnt/localhost/iscsi-rtp-smc-qa18-4-v10-2d12ed6a3394567a56c9ce900fa42bd6b
export DEBUG_OPTIONS="enable=fdebug"
export CLUS1_SRC=/dev/mapper/mpathbi
export CLUS1_DST=/dev/mapper/mpathbj
export CLUS2_SRC=/dev/mapper/mpathbk
export CLUS2_DST=/dev/mapper/mpathbl

#/dev/dm-9 /dev/mapper/mpathbp /dev/nimblestorage/iscsi-rtp-smc-qa18-4-v8-2737d0a5bd42d7b6d6c9ce900fa42bd6b
#/dev/sdl   /dev/sg11  Nimble   5.1         360448000   512  737d0a5bd42d7b6d6c9ce900fa42bd6b False Active    iSCSI   iscsi-rtp-smc-qa18-4-v8
#/dev/sdu   /dev/sg20  Nimble   5.1         360448000   512  737d0a5bd42d7b6d6c9ce900fa42bd6b False Active    iSCSI   iscsi-rtp-smc-qa18-4-v8
#/dev/sdad  /dev/sg29  Nimble   5.1         360448000   512  737d0a5bd42d7b6d6c9ce900fa42bd6b False Active    iSCSI   iscsi-rtp-smc-qa18-4-v8
#/dev/sdam  /dev/sg38  Nimble   5.1         360448000   512  737d0a5bd42d7b6d6c9ce900fa42bd6b False Active    iSCSI   iscsi-rtp-smc-qa18-4-v8

export WRITER=/dev/sdl
export MIRROR=/dev/sdam

