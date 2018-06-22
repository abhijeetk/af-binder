#!/bin/sh

export PORT=$1
export TOKEN=$2
export URL=localhost:$PORT
export WSURL="$URL/api?token=$TOKEN"
export R=$(realpath $(dirname $0)/..)

ls $R/scripts/[0-9][0-9]-*.sh |
sort -n |
while read x
do
	echo
	echo
	echo
	echo
	echo
	echo
	echo ===========================================================================
	echo ===========================================================================
	echo ==
	echo == $(basename $x)
	echo ==
	echo ===========================================================================
	$x
done
