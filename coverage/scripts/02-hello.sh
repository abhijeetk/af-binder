#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
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
hello broadcast {"name":"xev","data":"true"}
hello broadcast {"tag":"ev2","data":"true"}
hello eventdel {"tag":"ev2"}
hello hasperm {"perm":"some-permissison"}
hello appid true
hello uid true
hello set-loa 1
hello set-loa 3
hello close true
hello setctx "some-text-0"
hello setctxif "some-text-1"
hello getctx 
hello setctx "some-text-2"
hello getctx
#------------------------
# TODO bug to be fixed!
#hello info
#hello verbose {"level":2,"message":"hello"}
#------------------------
EOC

