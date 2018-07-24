#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
salut2 ping true
SALUT2 PING false
salut2 pIngNull true
salut2 PingBug true
salut2 PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
salut2 subcall {"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}
salut2 subcall {"api":"salut2","verb":"subcall","args":{"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}}
salut2 subcallsync {"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}
salut2 subcallsync {"api":"salut2","verb":"subcall","args":{"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}}
salut2 subcall {"api":"salut2","verb":"subcallsync","args":{"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}}
salut2 subcallsync {"api":"salut2","verb":"subcallsync","args":{"api":"salut2","verb":"pingjson","args":[{"key1":"value1"}]}}
salut2 eventadd {"tag":"ev1","name":"event-A"}
salut2 eventadd {"tag":"ev2","name":"event-B"}
salut2 eventpush {"tag":"ev1","data":[1,2,"salut2"]}
salut2 eventpush {"tag":"ev2","data":{"item":0}}
salut2 eventsub {"tag":"ev2"}
salut2 eventpush {"tag":"ev1","data":[1,2,"salut2"]}
salut2 eventpush {"tag":"ev2","data":{"item":0}}
salut2 eventsub {"tag":"ev1"}
salut2 subcall {"api":"salut2","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"salut2"]}}
salut2 subcall {"api":"salut2","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
salut2 subcallsync {"api":"salut2","verb":"eventpush","args":{"tag":"ev1","data":[1,2,"salut2"]}}
salut2 subcallsync {"api":"salut2","verb":"eventpush","args":{"tag":"ev2","data":{"item":0}}}
salut2 eventunsub {"tag":"ev2"}
salut2 eventpush {"tag":"ev1","data":[1,2,"salut2"]}
salut2 eventpush {"tag":"ev2","data":{"item":0}}
salut2 eventdel {"tag":"ev1"}
salut2 eventpush {"tag":"ev1","data":[1,2,"salut2"]}
salut2 eventpush {"tag":"ev2","data":{"item":0}}
salut2 eventdel {"tag":"ev2"}
EOC

