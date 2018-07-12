#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
salam ping true
x-HELLO PING false
salam pIngNull true
salam PingBug true
salam PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
salam subcall {"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}
salam subcall {"api":"salam","verb":"subcall","args":{"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}}
salam subcallsync {"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}
salam subcallsync {"api":"salam","verb":"subcall","args":{"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}}
salam subcall {"api":"salam","verb":"subcallsync","args":{"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}}
salam subcallsync {"api":"salam","verb":"subcallsync","args":{"api":"salam","verb":"pingjson","args":[{"key1":"value1"}]}}
salam eventadd {"tag":"ev1","name":"event-A"}
salam eventadd {"tag":"ev2","name":"event-B"}
salam eventpush {"tag":"ev1","data":[1,2,"salam"]}
salam eventpush {"tag":"ev2","data":{"item":0}}
salam eventsub {"tag":"ev2"}
salam eventpush {"tag":"ev1","data":[1,2,"salam"]}
salam eventpush {"tag":"ev2","data":{"item":0}}
salam eventsub {"tag":"ev1"}
salam subcall {"api":"salam","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"salam"]}}
salam subcall {"api":"salam","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
salam subcallsync {"api":"salam","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"salam"]}}
salam subcallsync {"api":"salam","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
salam eventunsub {"tag":"ev2"}
salam eventpush {"tag":"ev1","data":[1,2,"salam"]}
salam eventpush {"tag":"ev2","data":{"item":0}}
salam eventdel {"tag":"ev1"}
salam eventpush {"tag":"ev1","data":[1,2,"salam"]}
salam eventpush {"tag":"ev2","data":{"item":0}}
salam eventdel {"tag":"ev2"}
EOC

