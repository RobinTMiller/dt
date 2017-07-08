#!/bin/ksh -p
#/****************************************************************************
# *                                                                          *
# *                       COPYRIGHT (c) 2006 - 2007                          *
# *                        This Software Provided                            *
# *                                  By                                      *
# *                       Robin's Nest Software Inc.                         *
# *                                                                          *
# * Permission to use, copy, modify, distribute and sell this software and   *
# * its documentation for any purpose and without fee is hereby granted      *
# * provided that the above copyright notice appear in all copies and that   *
# * both that copyright notice and this permission notice appear in the      *
# * supporting documentation, and that the name of the author not be used    *
# * in advertising or publicity pertaining to distribution of the software   *
# * without specific, written prior permission.                              *
# *                                                                          *
# * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
# * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
# * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
# * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
# * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
# * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
# * THIS SOFTWARE.                                                           *
# *                                                                          *
# ****************************************************************************/
#
# Author:  Robin Miller
# Date:    July 23rd, 2013
#
# Description:
#	Korn shell wrapper for 'dt' program to allow commands
# to be sent to detached 'dt' through pipes.  This method makes
# 'dt' appear as though it's a Korn shell builtin command.
#
# How do you use this script?
# Startup via: % . ./dh.ksh
#
# This defines required functions and starts dt process talking to a pipe!
#
# Caveats/Notes:
#	The stderr stream is redirected to stdout stream.
#	Exit status of 'dt' is return value from functions.
#	You cannot pipe output to pager (writing is to a pipe).
#
typeset -fx dt dtIntr dtGetPrompt dtPromptUser dtStartup dtSetStatus
typeset -i dtIntrFlag=0
typeset -x HISTFILE=${HISTFILE:-"$HOME/.dt_history"}
typeset -x HISTSIZE=${HISTSIZE:-100}
typeset -x VISUAL=${VISUAL:-"emacs"}
# For now, this will change!
typeset -x DT_TOOL=${DT_TOOL:-"dt.experimental"}
typeset -x DT_PROMPT=${DT_PROMPT:-"dt.experimental"}
#typeset -x DT_TOOL=${DT_TOOL:-"dt.experimental"}
#typeset -x DT_PROMPT=${DT_PROMPT:-"$DT_TOOL"}
typeset -x DT_PATH=${DT_PATH:-"/usr/software/test/bin/$DT_TOOL"}
#typeset -x DT_PATH=${DT_PATH:-`whence dt`}
#typeset -x DT_PATH=${DT_PATH:-"/sbin/dt"}
typeset -x DT_PROMPT=${DT_PROMPT:-"dt"}
typeset -i DT_PID
typeset -x dtCmd
typeset -i CmdStatus

#readonly dt dtIntr dtGetPrompt
#
# Check for arrow keys being defined for editing.
#
whence __A > /dev/null ||
{
    # These first 4 allow emacs history editing to use the arrow keys
    alias __A="" \
	  __B="" \
	  __C="" \
	  __D=""
}

#
# Catch signals, and forward on to 'dt' process.
#
function dtIntr
{
	dtIntrFlag=1
	kill -INT $DT_PID
}

#
# This function loops reading input from the 'dt' process
# until we read the prompt string.  This is important so we
# keep things in sync between 'dt' commands for the main loop.
#
function dtGetPrompt
{
	status=1
	while read -r -u1 -p
	do
	    case $REPLY in

		${DT_PROMPT}\>\ \?\ *)
			dtSetStatus $REPLY
			status=$?
			break;;
		*)
			print -r - "$REPLY"
			;;
	    esac
	done
	return $status
}

#
# This function is used to get input from the terminal to
# parse and/or send to the 'dt' process.  It _must_ be a
# simple function (as it is), so signals get delivered to
# our trap handler properly.  Basically signals are _not_
# delivered until a function returns (as opposed to async
# signal delivery in C programs.
#
function dtPromptUser
{
	read -s dtCmd?"$1"
	return $?
}

#
#            $1    $2   $3
# Expect: progname> ? %status
#
function dtSetStatus
{
	saved_IFS=$IFS
	CmdOutput="$*"
	CmdOutput=`print -- $CmdOutput | tr -d '\r'`
	#print "CmdOutput: ##${CmdOutput}##" | cat -v
	IFS=" "
	set -- $*
	# Windows adds \r\n to lines, we strip \r!
	CmdStatus=`print -- $3 | tr -d '\r'`
	# Note: This will fail, if $3 is NOT numeric!
	#CmdStatus=$3
	IFS=$saved_IFS
	return $CmdStatus
}

function dt
{
	trap 'dtIntr' HUP INT QUIT TERM
	print -p - "$*" || return $?
	IFS=''
	status=0
	dtIntrFlag=0
	while read -r -u1 -p
	do
	    case $REPLY in

		${DT_PROMPT}\>\ \?\ *)
			dtSetStatus $REPLY
			status=$?
			break;;

		*)	#[[ $dtIntrFlag -eq 1 ]] && break
			print -r - "$REPLY"
			;;
	    esac
	    dtIntrFlag=0
	done
	IFS="$SavedIFS"
	dtIntrFlag=0
	trap - HUP INT QUIT TERM
	return $status
}

function dtStartup
{
    if [[ -n "$DT_REMOTE" ]]; then
	rsh ${DT_REMOTE} $DT_PATH enable=pipes $* 2<&1 |&
    else
	$DT_PATH enable=pipes $* 2<&1 |&
    fi
    [[ $? -ne 0 ]] && return $?
    DT_PID=$!
    dtGetPrompt
    return $?
}

#
# This is main()...
#
set +o nounset
unalias dt
dtStartup
#dtStartup $*
typeset -x SavedIFS="$IFS "
return $?
