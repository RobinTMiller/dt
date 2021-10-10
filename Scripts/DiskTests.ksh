#!/bin/ksh -p
#
# File:         DiskTests.ksh
# Author:       Robin T. Miller
# Date:         January 31st, 2009
#
# Description:
#       A Korn shell script executing the datatest (dt) program to perform
# a series of different I/O tests.
#
# Modification History:
#
# November 30th, 2015 by Robin T. Miller
#   Added tests for block tags and percentages.
#
# March 24th, 2011 by Robin T. Miller
#   Added BufferModes tests to verify bufmodes= option.
#
# November 26th, 2009 by Robin T. Miller
#   Added DT_FILES parameter to enable multiple files.
#
# February 5th, 2009 by Robin T. Miller
#   Added run_command() to display command line.
#   Modified parameters to make them more tuneable.
#   Added "-t testcase" option to select a single tc.
#

typeset prog=${0##*/}

#
# Set Path to Executable:
#
# Override this way:
# % export DT_PATH=~rtmiller/Tools/dt.d-WIP/linux2.6-x86/dt
#
# Beware: Linux pdksh puts quotes around path from 'whence' output!
typeset DT_PATH=${DT_PATH:-$(whence dt | tr -d "'")}
# Assume Linux testing, which is normally the case for me!
typeset DT_PATH=${DT_PATH:-~rtmiller/Tools/dt.d-WIP/linux2.6-x86/dt}
typeset DT_PATH=${DT_PATH:-/usr/software/test/bin/dt.latest}

#
# Local Definitions:
#
typeset DT_AIOS=${DT_AIOS:-4}
typeset DT_BUFMODES=${DT_BUFMODES:-"buffered unbuffered cachereads cachewrites"}
typeset DT_ALLBUFMODES=${DT_ALLBUFMODES:-"buffered,unbuffered,cachereads,cachewrites"}
typeset DT_DEBUG=${DT_DEBUG:-""}    # Example: "enable=debug,edebug"
typeset DT_DISPOSE=${DT_DISPOSE:-"keeponerror"}
typeset DT_DIR=${DT_DIR:-""}        # Future, not used at present!
typeset DT_EXTRAS=${DT_EXTRAS:-""}
typeset DT_FILES=${DT_FILES:-""}
typeset DT_FLAGS=${DT_FLAGS:-"flags=direct oflags=trunc"}
typeset DT_LIMIT=${DT_LIMIT:-1g}
typeset DT_LOG=${DT_LOG:-""}
typeset DT_NOPROGT=${DT_NOPROGT-"alarm=3s keepalivet=30s noprogt=15s notime=fsync"} # add "noprogtt=125s trigger=cmd:script"
typeset DT_PROCS=${DT_PROCS:-4}
typeset DT_SLICES=${DT_SLICES:-0}
typeset DT_READ_PERCENTAGES=${DT_READ_PERCENTAGES:-"25 50 75"}
typeset DT_RANDOM_PERCENTAGES=${DT_RANDOM_PERCENTAGES:-"25 50 75 100"}
typeset DT_RUNTIME=${DT_RUNTIME:-""}
typeset DT_PATTERN=${DT_PATTERN:-""}
typeset DT_PREFIX=${DT_PREFIX:-"%d@%h"}
typeset DT_STATS=${DT_STATS:-"disable=pstats"}
typeset DT_VERIFY=${DT_VERIFY:-"enable=compare,lbdata oncerr=abort"}

typeset DT_SEQUENTIAL_BLOCKSIZES=${DT_SEQUENTAIL_BLOCKSIZES:-"256k 512k"}
typeset DT_RANDOM_BLOCKSIZES=${DT_RANDOM_BLOCKSIZES:-"8k 32k"}
typeset DT_VARIABLE_BLOCKSIZES=${DT_VARIABLE_BLOCKSIZES:-"min=d max=512k incr=var"}
typeset DT_VARIABLE_IOTYPES=${DT_VARIABLE_IOTYPES:-"sequential random"}

typeset DT_INCREMENTAL_BLOCKSIZES=${DT_INCREMENTAL_BLOCKSIZES:-"min=d max=512k incr=d"}
typeset DT_INCREMENTAL_DIRECTION=${DT_INCREMENTAL_DIRECTION:-"forward reverse"}
typeset DT_INCREMENTAL_OPTIONS=${DT_INCREMENTAL_OPTIONS:-"enable=raw"}

#
# Parameters: (tester can roll there own options)
#
typeset DT_COMMON=""
[[ -n "$DT_AIOS"  && $DT_AIOS -gt 0 ]]    && DT_COMMON="$DT_COMMON aios=$DT_AIOS"
[[ -n "$DT_PROCS" && $DT_PROCS -gt 0 ]]   && DT_COMMON="$DT_COMMON procs=$DT_PROCS"
[[ -n "$DT_SLICES" && $DT_SLICES -gt 0 ]] && DT_COMMON="$DT_COMMON slices=$DT_SLICES"
[[ -n "$DT_LIMIT" ]]                      && DT_COMMON="$DT_COMMON limit=$DT_LIMIT"
[[ -n "$DT_RUNTIME" ]]                    && DT_COMMON="$DT_COMMON runtime=$DT_RUNTIME"
[[ -n "$DT_PATTERN" ]]                    && DT_COMMON="$DT_COMMON pattern=$DT_PATTERN"
[[ -n "$DT_PREFIX" ]]                     && DT_COMMON="$DT_COMMON prefix=$DT_PREFIX"
[[ -n "$DT_STATS" ]]                      && DT_COMMON="$DT_COMMON $DT_STATS"
[[ -n "$DT_VERIFY" ]]                     && DT_COMMON="$DT_COMMON $DT_VERIFY"

[[ -n "$DT_DEBUG" ]]                      && DT_COMMON="$DT_COMMON $DT_DEBUG"
[[ -n "$DT_DISPOSE" ]]                    && DT_COMMON="$DT_COMMON dispose=$DT_DISPOSE"
[[ -n "$DT_FILES" ]]                      && DT_COMMON="$DT_COMMON files=$DT_FILES"
[[ -n "$DT_FLAGS" ]]                      && DT_COMMON="$DT_COMMON $DT_FLAGS"
[[ -n "$DT_LOG" ]]                        && DT_COMMON="$DT_COMMON logu=$DT_LOG"
[[ -n "$DT_NOPROGT" ]]                    && DT_COMMON="$DT_COMMON $DT_NOPROGT"
[[ -n "$DT_EXTRAS" ]]                     && DT_COMMON="$DT_COMMON $DT_EXTRAS"

#
# The tester can override all of our defaults by setting this parameter.
#
typeset DT_OPTS=${DT_OPTS:-"$DT_COMMON"}

typeset -i exit_status=0
typeset -i TestCasePass=0
typeset -i TestCaseFail=0
typeset -i ExitOnError=${ExitOnError:-1}
typeset -i SuccessStatus=0
typeset -i FailureStatus=255
typeset -i EndOfFileStatus=254
typeset -i WarningStatus=1

#
# Initialize the Test Case Array:
#
typeset -i tci=0
for tc in SequentialIO ReverseIO RandomIO VariableIO IncrementalIO BufferModes BlockTags Percentages
do
    TestCases[$tci]=$tc
    (( tci += 1 ))
done

#---------------------------------------------------
#                   Support Functions
#---------------------------------------------------

function terminate
{
    report_stats
    exit $exit_status
} 

function usage
{
    print "Usage: $prog [-t testcase] <filepath>"
    print ""
    print "Test Cases:"
    print -n "    "
    tci=0
    while (( tci < ${#TestCases[*]} ))
    do
        print -n "${TestCases[$tci]} "
        (( tci += 1 ))
    done
    print ""
    exit $WarningStatus
}

#
# error - Reports an error and exits with specified value.
#
# Implicit Inputs:
#    run_status = The last command run status.
#
# Explicit Outputs:
#    exit_status = The exit status set from run status.
#
function error
{
    print -u2 "$prog: error - $*"
    exit_status=$run_status
    (( ExitOnError != 0 )) && terminate
    return
}

#
# run_command - Simple function to run command. Caller handles errors.
#
# Implicit Inputs:
#    TestName = The test case name being executed.
#
function run_command
{
    print "Running: $*"
    $*

    run_status=$?
    # Success status is Success (0) or End of File (254)
    if [[ $run_status -eq $SuccessStatus || $run_status -eq $EndOfFileStatus ]] then
        (( TestCasePass+=1 ))
    else
        (( TestCaseFail+=1 ))
        error "$TestName"
    fi
    return $run_status
}

function report_stats
{
    print "Tests Passed: $TestCasePass"
    print "Tests Failed: $TestCaseFail"
    print "Finished at $(date)"
    return
}

#---------------------------------------------------
#               Start of Test Functions
#---------------------------------------------------

#
# Test Case: Sequential I/O
#
function SequentialIO
{
    TestName="Sequential I/O"
    print "Executing TestCase $TestName at $(date)"
    
    for blocksize in $DT_SEQUENTIAL_BLOCKSIZES
    do
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       bs=$blocksize                \
                       $DT_OPTS
    done
}

#
# Test Case: Reverse I/O
#
function ReverseIO
{
    TestName="Reverse I/O"
    print "Executing TestCase $TestName at $(date)"
    
    for blocksize in $DT_SEQUENTIAL_BLOCKSIZES
    do
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       bs=$blocksize                \
                       iodir=reverse                \
                       $DT_OPTS
    done
} 

#
# Test Case: Random I/O
#
function RandomIO
{
    TestName="Random I/O"
    print "Executing TestCase $TestName at $(date)"
    
    for blocksize in $DT_RANDOM_BLOCKSIZES
    do
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       bs=$blocksize                \
                       iotype=random                \
                       $DT_OPTS
    done
}

#
# Test Case: Variable I/O
#
# Note: This will force non-modulo block offsets!
#
function VariableIO
{
    TestName="Variable I/O"
    print "Executing TestCase $TestName at $(date)"
    
    for iotype in $DT_VARIABLE_IOTYPES
    do
      if [ "$OSs" == "Linux" ]; then
        #
        # For Linux direct I/O, align transfers to a block.
        #
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       ${DT_VARIABLE_BLOCKSIZES}    \
                       $DT_OPTS                     \
                       enable=fsalign
      else
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       ${DT_VARIABLE_BLOCKSIZES}    \
                       $DT_OPTS
      fi
    done
}

#
# Test Case: Incremental I/O
#
function IncrementalIO
{
    TestName="Incremental I/O"
    print "Executing TestCase $TestName at $(date)"
    
    for iodir in $DT_INCREMENTAL_DIRECTION
    do
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       ${DT_INCREMENTAL_BLOCKSIZES} \
                       iodir=$iodir                 \
                       ${DT_INCREMENTAL_OPTIONS}    \
                       $DT_OPTS
    done
}

#
# Test Case: Test Buffer Modes
#
function BufferModes
{
    TestName="Buffer Modes"
    print "Executing TestCase $TestName at $(date)"
    
    for bufmode in $DT_BUFMODES
    do
        run_command                                 \
            ${DT_PATH} of=$FILE_PATH                \
                       bufmodes=$bufmode            \
                       ${DT_INCREMENTAL_BLOCKSIZES} \
                       passes=2                     \
                       $DT_OPTS
    done
    # Run with ALL buffer modes too!
    run_command                                     \
        ${DT_PATH} of=$FILE_PATH                    \
                   bufmodes=${DT_ALLBUFMODES}       \
                   ${DT_INCREMENTAL_BLOCKSIZES}     \
                   passes=5                         \
                   $DT_OPTS
}


#
# Test Case: Test Blck Tags
#
function BlockTags
{
    TestName="Block Tags"
    print "Executing TestCase $TestName at $(date)"
    
    # Sequential I/O first.
    run_command                                     \
	${DT_PATH} of=$FILE_PATH                    \
                   ${DT_INCREMENTAL_BLOCKSIZES}     \
                   iotype=sequential                \
		   enable=btags			    \
                   passes=2                         \
                   $DT_OPTS

    # Random I/O with IOT pattern.
    run_command                                     \
        ${DT_PATH} of=$FILE_PATH                    \
                   ${DT_INCREMENTAL_BLOCKSIZES}     \
                   iotype=random                    \
		   enable=btags			    \
		   pattern=iot			    \
                   passes=2                         \
                   $DT_OPTS
}

#
# Test Case: Test I/O Percentages
#
function Percentages
{
    TestName="I/O Percentages"
    print "Executing TestCase $TestName at $(date)"
    
    for readp in $DT_READ_PERCENTAGES
    do
	for randp in $DT_RANDOM_PERCENTAGES
	do
	    run_command                             \
		${DT_PATH} of=$FILE_PATH            \
                ${DT_INCREMENTAL_BLOCKSIZES}	    \
		readp=${readp} randp=${randp}	    \
                $DT_OPTS aios=0 oflags=none
	done
    done
}

#---------------------------------------------------
#                       Main
#---------------------------------------------------

#
# Parse Options:
#
# Note: We may wish to add include/exclude options later.
#
while getopts :t: c
do  case $c in
    t)  testcase=$OPTARG
        found=0
        for tc in ${TestCases[*]}
        do
            if [[ $testcase = $tc ]] then
                found=1
                break
            fi
        done
        if (( found == 0 )) then
            print "$prog: Unknown test case: $testcase"
            usage
        fi
        ;;
    :)  print -u2 "$prog: $OPTARG requires a parameter value"
        exit $WarningStatus;;
   \?)  print "$prog: unknown option $OPTARG"
        usage;;
    esac
done
# Note: NFG on Linux w/pdksh! ;(
shift $OPTIND-1
 
#
# Expect One Argument:
#
[[ $# -ne 1 ]] && usage

#
# Only a file name path expected.
#
typeset FILE_PATH=$1

typeset OSs=$(uname -s)
if [ "$OSs" = "AIX" ] ; then
    # Ensure POSIX AIO subsystem is loaded. (MUST be root for mkdev!)
    /usr/sbin/lsdev -l posix_aio0 | fgrep Available >/dev/null || /usr/sbin/mkdev -l posix_aio0
fi

#
# Show Tool Version:
#
${DT_PATH} version
if [[ $? -ne 0 ]] then
    run_status=$?
    "${DT_PATH} not found or is not executable!"
fi

#
# Remove Old Data Files and log files (if any).
#
rm -f $FILE_PATH*
[[ -n "$DT_LOG" ]] && rm -f $DT_LOG*

trap '
    print "Signal caught, terminating..."
    (( exit_status == 0 )) && exit_status=$WarningStatus
    terminate
' HUP INT QUIT TERM

#
# Execute Test Case(s).
#
# Currently, we permit one test case or all test cases.
#
if [[ -n "$testcase" ]] then
    $testcase
else
    tci=0
    while (( tci < ${#TestCases[*]} ))
    do
        ${TestCases[$tci]}
        (( tci += 1 ))
    done
fi

#
# Ok report stats, we're done!
#
report_stats

exit $exit_status
