#!/bin/bash

ROOT=$(dirname $0)
echo ROOT=$ROOT

AFB=$ROOT/build/src/afb-daemon
CLI=$ROOT/build/src/afb-client-demo
HELLO=build/bindings/samples/helloWorld.so
PORT=12345
TEST=test
TOKEN=knock-knock-knoc
OUT=$ROOT/stress-out-clients

rm $OUT*

CMDS=
add() {
	CMDS="$CMDS
$1"
}

add 'hello ping true'
add 'HELLO PING false'
add 'hello pIngNull true'
#add 'hello PingBug true'
add 'hello PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}'
add 'hello subcall {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}'
add 'hello subcall {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}'
add 'hello subcallsync {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}'
add 'hello subcallsync {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}'
add 'hello subcall {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}'
add 'hello subcallsync {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}'
add 'hello eventadd {"tag":"ev1","name":"event-A"}'
add 'hello eventadd {"tag":"ev2","name":"event-B"}'
add 'hello eventpush {"tag":"ev1","data":[1,2,"hello"]}'
add 'hello eventpush {"tag":"ev2","data":{"item":0}}'
add 'hello eventsub {"tag":"ev2"}'
add 'hello eventpush {"tag":"ev1","data":[1,2,"hello"]}'
add 'hello eventpush {"tag":"ev2","data":{"item":0}}'
add 'hello eventsub {"tag":"ev1"}'
add 'hello subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}'
add 'hello subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}'
add 'hello subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}'
add 'hello subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}'
add 'hello eventunsub {"tag":"ev2"}'
add 'hello eventpush {"tag":"ev1","data":[1,2,"hello"]}'
add 'hello eventpush {"tag":"ev2","data":{"item":0}}'
add 'hello eventdel {"tag":"ev1"}'
add 'hello eventpush {"tag":"ev1","data":[1,2,"hello"]}'
add 'hello eventpush {"tag":"ev2","data":{"item":0}}'
add 'hello eventdel {"tag":"ev2"}'

r() {
	while :; do echo "$CMDS"; done |
	while read x; do echo $x; sleep 0.001; done |
	$CLI "localhost:$PORT/api?token=$TOKEN" > $OUT.$1 2>&1 &
#	while read x; do echo $x; sleep 0.001; done |
#	strace -tt -f -o $OUT-strace.$1 $CLI "localhost:$PORT/api?token=$TOKEN" > $OUT.$1 2>&1 &
}

echo -n launch clients...
for x in 0 1 2 3 4 5 6 7 8 9 a b c d e f; do r $x; done
echo done

wait
