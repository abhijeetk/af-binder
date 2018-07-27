#!/bin/sh

$R/bin/afb-client -s -e $WSURL <<EOC
hello ping true
HELLO PING false
hello pIngNull true
hello PingBug true
hello PiNgJsOn {"well":"formed","json":[1,2,3,4.5,true,false,null,"oups"]}
hello call {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
hello callsync {"api":"hello","verb":"pingjson","args":[{"key1":"value1"}]}
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
hello reftok true
hello has_loa-1
hello has_loa-2
hello has_loa-3
hello set-loa 1
hello has_loa-1
hello has_loa-2
hello has_loa-3
hello set-loa 3
hello has_loa-1
hello has_loa-2
hello has_loa-3
hello close true
hello has_loa-1
hello has_loa-2
hello has_loa-3
hello setctx "some-text-0"
hello setctxif "some-text-1"
hello getctx 
hello setctx "some-text-2"
hello getctx
hello info
hello settings
hello verbose {"level":2,"message":"hello"}
hello eventloop
hello dbus false
hello dbus true
hello reply-count 0
hello reply-count 2
hello get null
hello get {"name":"toto"}
hello get {"name":"toto","toto":5}
hello ref null
hello rootdir null
hello eventadd {"tag":"EVENT","name":"EVENT"}
hello locale {"file":"loc.txt","lang":"ru,de,jp-JP,fr"}
hello locale "loc.txt"
hello locale "i don't exist"
hello api {"action":"create","api":"_extra_"}
hello api {"action":"addverb","api":"_extra_","verb":"ping"}
hello api {"action":"seal","api":"_extra_"}
hello api {"action":"addverb","api":"_extra_","verb":"ping"}
hello api {"action":"destroy","api":"_extra_"}
_extra_ ping2 {"a":true}
_extra_ ping3 {"b":false}
_extra_ ping {"c":[1,2,3]}
hello api {"action":"create","api":"extra"}
extra api {"action":"addverb","verb":"blablabla"}
extra api {"action":"addverb","verb":"ping"}
extra api {"action":"addverb","verb":"ping2"}
extra api {"action":"addverb","verb":"ping3"}
extra api {"action":"addverb","verb":"q*"}
extra api {"action":"delverb","verb":"blablabla"}
extra api {"action":"addhandler","pattern":"*","closure":"*"}
extra api {"action":"addhandler","pattern":"hello/*","closure":"hello/*"}
extra call {"api":"hello","verb":"eventsub","args":{"tag":"EVENT"}}
hello eventpush {"tag":"EVENT","data":[1,2,"hello"]}
hello api {"action":"delhandler","api":"extra","pattern":"hello/*"}
hello eventpush {"tag":"EVENT","data":[1,2,"hello"]}
extra ping2 {"a":true}
extra ping3 {"b":false}
extra ping {"c":[1,2,3]}
extra query {"c":[1,2,3]}
extra blablabla {"c":[1,2,3]}
extra api {"action":"addverb","verb":"ping"}
extra call {"api":"hello","verb":"eventunsub","args":{"tag":"EVENT"}}
extra ping {"c":[1,2,3]}
hello api {"action":"destroy","api":"extra"}
extra ping {"c":[1,2,3]}
EOC

