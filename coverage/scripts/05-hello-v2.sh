#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
hello-v2 ping true
HELLO-v2 PING false
hello-v2 pIngNull true
hello-v2 PingBug true
hello-v2 PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
hello-v2 call {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello-v2 callsync {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello-v2 subcall {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello-v2 subcall {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello-v2 subcallsync {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello-v2 subcallsync {"api":"hello","verb":"subcall","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello-v2 subcall {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello-v2 subcallsync {"api":"hello","verb":"subcallsync","args":{"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}}
hello-v2 eventadd {"tag":"ev1","name":"event-A"}
hello-v2 eventadd {"tag":"ev2","name":"event-B"}
hello-v2 eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello-v2 eventpush {"tag":"ev2","data":{"item":0}}
hello-v2 eventsub {"tag":"ev2"}
hello-v2 eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello-v2 eventpush {"tag":"ev2","data":{"item":0}}
hello-v2 eventsub {"tag":"ev1"}
hello-v2 subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}
hello-v2 subcall {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
hello-v2 subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"hello"]}}
hello-v2 subcallsync {"api":"hello","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
hello-v2 eventunsub {"tag":"ev2"}
hello-v2 eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello-v2 eventpush {"tag":"ev2","data":{"item":0}}
hello-v2 eventdel {"tag":"ev1"}
hello-v2 eventpush {"tag":"ev1","data":[1,2,"hello"]}
hello-v2 eventpush {"tag":"ev2","data":{"item":0}}
hello-v2 broadcast {"name":"xev","data":"true"}
hello-v2 broadcast {"tag":"ev2","data":"true"}
hello-v2 eventdel {"tag":"ev2"}
hello-v2 hasperm {"perm":"some-permissison"}
hello-v2 appid true
hello-v2 uid true
hello-v2 set-loa 1
hello-v2 set-loa 3
hello-v2 close true
hello-v2 setctx "some-text-0"
hello-v2 setctxif "some-text-1"
hello-v2 getctx 
hello-v2 setctx "some-text-2"
hello-v2 getctx
hello-v2 info
hello-v2 verbose {"level":2,"message":"hello"}
EOC

