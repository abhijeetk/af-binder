#!/bin/bash

ROOT=$(dirname $0)
cd ${ROOT:-.}
ROOT=$(pwd)
echo ROOT=$ROOT

AFB=build/src/afb-daemon
HELLO=build/bindings/samples/helloWorld.so
PORT=12345
TEST=test
TOKEN=knock-knock-knock
OUT=stress-out-server

rm $OUT*

case "$1" in
 --ws)
  shift
  ARGS="-q --ldpaths=/tmp --binding=$HELLO --session-max=100 --ws-server=unix:@afw:hello --no-httpd --exec $AFB --session-max=100 --port=$PORT --ldpaths=/tmp --roothttp=$TEST --token=$TOKEN --ws-client=unix:@afw:hello "
#  ARGS="$ARGS -vvv --tracereq=all"
  ;;
 *)
  ARGS="-q --session-max=100 --port=$PORT --workdir=$ROOT --roothttp=$TEST --token=$TOKEN --ldpaths=/tmp --binding=$HELLO"
  ;;
esac


echo $AFB $ARGS $@
echo -n launch afb...
case "$1" in
 gdb) shift; gdb $AFB -ex "run $ARGS $@";;
 valgrind) shift; valgrind --leak-check=full $AFB $ARGS $@ 2>&1 | tee $OUT;;
 strace) shift; strace -tt -f -o $OUT.strace $AFB $ARGS $@ 2>&1 | tee $OUT;;
 *) $AFB $ARGS $@ 2>&1 | tee $OUT;;
esac
#$AFB -q --session-max=100 --port=$PORT --workdir=$ROOT --roothttp=$TEST --token=$TOKEN --ldpaths=/tmp --binding=$HELLO  > $OUT.0 2>&1 &
#afbpid=$!
#strace -tt -f -o $OUT-strace.0 -p $afbpid &
wait
