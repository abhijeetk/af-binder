#!/bin/bash

ROOT=$(dirname $0)

AFB=$ROOT/build/src/afb-daemon
CLI=$ROOT/build/src/afb-client-demo
HELLO=$ROOT/build/bindings/samples/helloWorld.so
PORT=12345
TEST=$ROOT/test
TOKEN=knock-knock-knoc
OUT=$ROOT/stress-out

rm $OUT*

echo -n launch afb...
$AFB --session-max=100 --port=$PORT --rootdir=$ROOT --roothttp=$TEST --tracereq=all --token=$TOKEN --ldpaths=/tmp --binding=$HELLO --verbose --verbose --verbose > $OUT.0 2>&1 &
afbpid=$!
strace -tt -f -o $OUT-strace.0 -p $afbpid &
echo done

sleep 3

CMDS='
hello ping true
HELLO PING false
hello pIngNull true
hello PingBug true
hello PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
hello subcall {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello subcall {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello subcallsync {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello subcallsync {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello subcall {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello subcallsync {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello eventadd {"tag":"ev1","name":"event-A"}
hello eventadd {"tag":"ev2","name":"event-B"}
hello eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello eventpush {"tag":"ev2","data":{"item":0}}
hello eventsub {"tag":"ev2"}
hello eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello eventpush {"tag":"ev2","data":{"item":0}}
hello eventsub {"tag":"ev1"}
hello subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}
hello subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
hello subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}
hello subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
hello eventunsub {"tag":"ev2"}
hello eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello eventpush {"tag":"ev2","data":{"item":0}}
hello eventdel {"tag":"ev1"}
hello eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello eventpush {"tag":"ev2","data":{"item":0}}
'

r() {
	while :; do echo "$CMDS"; done | while read x; do echo $x; sleep 0.005; done | strace -tt -f -o $OUT-strace.$1 $CLI "localhost:$PORT/api?token=$TOKEN" > $OUT.$1 2>&1 &
}

echo -n launch clients...
r 1
r 2
r 3
r 4
r 5
r 7
r 8
r 9
r a
r b
r c
echo done

sleep 3

kill $afbpid

