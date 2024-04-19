# Defaults:
# Simple script to start multiple s3t.sh instances.

#bucket=${BUCKET:-dt-bucket}
#dtdir=${DTDIR:-dtfiles}
#s3dir=${s3DIR:-s3files}

unset BUCKET
unset DTDIR
unset s3DIR

for instance in {1..3};
do
    export BUCKET="dt-bucket-${instance}"
    export DTDIR="dtfiles-${instance}"
    export s3DIR="s3files-${instance}"
    ./s3t.sh -x 2>&1 >s3t-${BUCKET}.log &
done
