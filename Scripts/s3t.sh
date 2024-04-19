#!/usr/bin/bash
#
# Date: December 7th, 2023
# Author: Robin T. Miller
#
# Description:
#  Simple script to use dt as a data generation tool for testing S3 object storage.
#  Note: Assumes S3 bucket already exists, and credentials have been created.\
#  This script uses the default profile, aka 's3'.
#
# Modification History:
#   April 19th, 2024
#     Minor updates, fix bash error redirection.
#
#   December 18th, 2023 by Robin T. Miller
#     When reading files, remove the min/max limit options to ensure
#     the whole file is verified, otherwise only a portion is read.
#
#   December 13th, 2023 by Robin T. Miller
#     Add S3 bucket name to dt prefix string.
#     Change the default S3 bucket name to "dt-bucket".
#     If the S3 bucket does not exist, make the bucket.
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
bucket=${BUCKET:-dt-bucket}
dtdir=${DTDIR:-dtfiles}
s3dir=${s3DIR:-s3files}
files=${FILES:-10}
limit=${LIMIT:-10m}
passes=${PASSES:-3}
threads=${THREADS:-5}
s3uri="s3://${bucket}"

echo "--> Verify Bucket ${s3uri} Exists <--"
aws s3 ls ${s3uri} 2>/dev/null >/dev/null
if [[ $? -ne 0 ]]; then
    echo "--> Making Bucket ${s3uri} <--"
    aws s3 mb ${s3uri}
    check_error
fi

for pass in $(seq $passes);
do
    echo "--> Starting Pass ${pass} <--"
    echo "--> Removing previous test files <--"
    rm -rf ${dtdir} ${s3dir}
    echo "--> Creating dt files <--"
    ${dtpath}  of=${dtdir}/dt.data bs=random min_limit=4k max_limit=${limit} incr_limit=vary workload=high_validation threads=${threads} files=${files} dispose=keep iotpass=$pass disable=pstats prefix="%d@%h@${bucket}"
    check_error
    ls -lsR ${dtdir}
    echo "--> Uploading dt files to S3 server <--"
    aws s3 cp ${dtdir} ${s3uri}/ --recursive
    check_error
    echo "--> Downloading S3 dt files <--"
    aws s3 cp ${s3uri}/ ${s3dir} --recursive
    check_error
    echo "--> Verifying downloaded S3 dt files <--"
    ${dtpath}  if=${s3dir}/dt.data bs=random workload=high_validation vflags=~inode threads=${threads} files=${files} iotpass=${pass} disable=verbose prefix="%d@%h@${bucket}"
    check_error
    echo "--> Removing S3 dt files <--"
    aws s3 rm --recursive ${s3uri}
    check_error
done
