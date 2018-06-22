#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
x-hello ping true
x-HELLO PING false
x-hello pIngNull true
x-hello PingBug true
x-hello PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
x-hello subcall {"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}
x-hello subcall {"api":"x-hello","verb":"subcall","args":{"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}}
x-hello subcallsync {"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}
x-hello subcallsync {"api":"x-hello","verb":"subcall","args":{"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}}
x-hello subcall {"api":"x-hello","verb":"subcallsync","args":{"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}}
x-hello subcallsync {"api":"x-hello","verb":"subcallsync","args":{"api":"x-hello","verb":"pingjson","args":[{"key1":"value1"}]}}
x-hello eventadd {"tag":"ev1","name":"event-A"}
x-hello eventadd {"tag":"ev2","name":"event-B"}
x-hello eventpush {"tag":"ev1","data":[1,2,"x-hello"]}
x-hello eventpush {"tag":"ev2","data":{"item":0}}
x-hello eventsub {"tag":"ev2"}
x-hello eventpush {"tag":"ev1","data":[1,2,"x-hello"]}
x-hello eventpush {"tag":"ev2","data":{"item":0}}
x-hello eventsub {"tag":"ev1"}
x-hello subcall {"api":"x-hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"x-hello"]}}
x-hello subcall {"api":"x-hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
x-hello subcallsync {"api":"x-hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"x-hello"]}}
x-hello subcallsync {"api":"x-hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
x-hello eventunsub {"tag":"ev2"}
x-hello eventpush {"tag":"ev1","data":[1,2,"x-hello"]}
x-hello eventpush {"tag":"ev2","data":{"item":0}}
x-hello eventdel {"tag":"ev1"}
x-hello eventpush {"tag":"ev1","data":[1,2,"x-hello"]}
x-hello eventpush {"tag":"ev2","data":{"item":0}}
x-hello eventdel {"tag":"ev2"}
EOC

