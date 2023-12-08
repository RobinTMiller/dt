#!/usr/bin/bash
#
# Date: December 7th, 2023
# Author: Robin T. Miller
# Description:
#  Simple script to use dt as a data generation tool for testing S3 object storage.
#  Note: Assumes S3 bucket already exists, and credentials have been created.\
#  This script uses the default profile, aka 's3'.
#
function check_error
{
    exit_status=$?
    if [[ $exit_status -ne 0 ]]; then
        echo "Error occurred, last exit status is ${exit_status}"
        exit $exit_status
    fi
    return
}

# Set defaults:
dtpath=${DTPATH:-~/dt}
bucket=${BUCKET:-robin-bucket}
dtdir=${DTDIR:-dtfiles}
s3dir=${s3DIR:-s3files}
files=${FILES:-10}
passes=${PASSES:-3}
threads=${THREADS:-5}

for pass in $(seq $passes);
do
    echo $pass
    rm -rf ${dtdir} ${s3dir}
    ${dtpath}  of=${dtdir}/dt.data bs=random min_limit=4k max_limit=1m incr_limit=vary workload=high_validation threads=${threads} files=${files} dispose=keep iotpass=$pass disable=pstats
    check_error
    aws s3 cp ${dtdir} s3://${bucket}/ --recursive
    check_error
    aws s3 cp s3://${bucket}/ ${s3dir} --recursive
    check_error
    ${dtpath}  if=${s3dir}/dt.data bs=random workload=high_validation min_limit=4k max_limit=1m incr_limit=vary vflags=~inode threads=${threads} files=${files} iotpass=${pass} disable=verbose
    check_error
    aws s3 rm --recursive s3://${bucket}
    check_error
done
