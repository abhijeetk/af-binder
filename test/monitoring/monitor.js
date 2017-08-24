
var afb;
var ws;

var t_api;
var t_verb;
var t_logmsg;
var t_traceevent;
var t_verbosity;
var t_trace;
var apis = {};
var events = [];
var inhibit = false;
var msgs = false;

var root_node;
var connected_node;
var trace_events_node;
var logmsgs_node;
var apis_node;
var all_node;

/* flags */
var show_perms = false;
var show_monitor_events = false;

_.templateSettings = { interpolate: /\{\{(.+?)\}\}/g };

function untrace_all() {
	do_call("monitor/trace", {drop: true});
	for_all_nodes(null, ".trace-item input[type=radio]", function(n){n.checked = n.value == "no";});
}

function disconnect() {
	untrace_all();
	apis = {};
	apis_node.innerHTML = "";
	root_node.className = "off";
	connected_node.innerHTML = "Connection Closed";
	connected_node.className = "ok";
	ws && ws.close();
	afb = null;
	ws = null;
}

function connect(args) {
	drop_all_trace_events();
	drop_all_logmsgs();
	ws && ws.close();
	afb = new AFB(args);
	ws = new afb.ws(onopen, onabort);
}

function on_connect(evt) {
	connect({
		host: at("param-host").value + ":" + at("param-port").value,
		token: at("param-token").value
	});
}

function init() {
	/* prepare the DOM templates */
	t_api = at("t-api").content.firstElementChild;
	t_verb = at("t-verb").content.firstElementChild;
	t_logmsg = at("t-logmsg").content.firstElementChild;
	t_traceevent = at("t-traceevent").content.firstElementChild;
	t_verbosity = at("t-verbosity").content.firstElementChild;
	t_trace = at("t-trace").content.firstElementChild;

	root_node = at("root");
	connected_node = at("connected");
	trace_events_node = at("trace-events");
	logmsgs_node = at("logmsgs");
	apis_node = at("apis");
	all_node = at("all");

	plug(t_api, ".verbosity", t_verbosity);
	plug(t_api, ".trace", t_trace);
	plug(all_node, ".trace", t_trace);
	plug(all_node, ".verbosity", t_verbosity);
	plug(at("common"), ".verbosity", t_verbosity);
	for_all_nodes(root_node, ".opclo", function(n){n.onclick = on_toggle_opclo});
	for_all_nodes(root_node, ".opclo ~ :not(.closedoff)", function(n){n.onclick = on_toggle_opclo});
	for_all_nodes(root_node, ".verbosity select", function(n){n.onchange = set_verbosity});
	for_all_nodes(root_node, ".trace-item input", function(n){n.onchange = on_trace_change});
	at("disconnect").onclick = disconnect;
	at("connect").onclick = on_connect;
	at("droptracevts").onclick = drop_all_trace_events;
	at("dropmsgs").onclick = drop_all_logmsgs;
	at("stopmsgs").onclick = toggle_logmsgs;
	start_logmsgs(false);
	trace_events_node.onclick = on_toggle_traceevent;

	connect();
}

function for_all_nodes(root, sel, fun) {
	(root ? root : document).querySelectorAll(sel).forEach(fun);
}

function get(sel,x) {
	if (!x)
		x = document;
	var r = x.querySelector(sel);
	return r;
}
function at(id) { return document.getElementById(id); }

function plug(target, sel, node) {
	var x = get(sel, target);
	var n = target.ownerDocument.importNode(node, true);
	x.parentNode.insertBefore(n, x);
	x.parentNode.removeChild(x);
}

function onopen() {
	root_node.className = "on";
	connected_node.innerHTML = "Connected " + ws.url;
	connected_node.className = "ok";
	ws.onevent("*", gotevent);
	ws.onclose = onabort;
	do_call("monitor/get", {apis:true,verbosity:true}, on_got_apis, on_error_apis);
}
function onabort() {
	root_node.className = "off";
	connected_node.innerHTML = "Connection Closed";
	connected_node.className = "error";
}

function start_logmsgs(val) {
	at("stopmsgs").textContent = (msgs = val) ? "Stop logs" : "Get logs";
}

function toggle_logmsgs() {
	start_logmsgs(!msgs);
}

function drop_all_logmsgs() {
	logmsgs_node.innerHTML = "";
}

function drop_all_trace_events() {
	trace_events_node.innerHTML = "";
}

function add_logmsg(tag, content, add) {
	if (!msgs) return;
	var x = document.importNode(t_logmsg, true);
	get(".tag", x).textContent = tag;
	get(".content", x).textContent = content;
	get(".close", x).onclick = function(evt){x.remove();};
	if (add)
		x.className = x.className + " " + add;
	logmsgs_node.prepend(x);
}

function add_error(tag, obj) {
	add_logmsg(tag, JSON.stringify(obj, null, 1), "error");
}

function on_error_apis(obj) {
	add_error("can't get apis", obj);
}

function do_call(api_verb, request, onsuccess, onerror) {
	var call = api_verb + "(" + JSON.stringify(request, null, 1) + ")";
	add_logmsg("send request", call, "call");
	ws.call(api_verb, request).then(
		function(obj){
			add_logmsg("receive success", call + " -> " + JSON.stringify(obj, null, 1), "retok");
			if (onsuccess)
				onsuccess(obj);
		},
		function(obj){
			add_logmsg("receive error", call + " -> ", JSON.stringify(obj, null, 1), "reterr");
			if (onerror)
				onerror(obj);
		});
}

/* show all verbosities */
function on_got_verbosities(obj) {
	inhibit = true;
	_.each(obj.response.verbosity, function(verbosity, api_name){
		if (api_name == "monitor") return;
		var node = api_name ? apis[api_name].node : at("common");
		if (node)
			get(".verbosity option[value='"+verbosity+"']", node).selected = true;
	});
	inhibit = false;
}

function set_verbosity(evt) {
	if (inhibit) return;
	inhibit = true;
	var obj = evt.target;
	var req = {verbosity:{}};
	var name = obj.API ? obj.API.name : obj === get(".select", all_node) ? "*" : "";
	if (name != "*") {
		req.verbosity[name] = obj.value;
	} else {
		req.verbosity = obj.value;
	}
	inhibit = false;
	do_call("monitor/set", req);
	do_call("monitor/get", {verbosity:true}, on_got_verbosities);
}

/* show all apis */
function on_got_apis(obj) {
	inhibit = true;
	_.each(obj.response.apis, function(api_desc, api_name){
		if (api_name == "monitor") return;
		var api = apis[api_name];
		if (!api) {
			api = {
				node: document.importNode(t_api, true),
				verbs: {},
				name: api_name
			};
			api.node.API = api;
			api.node.dataset.api = api_name;
			api.vnode = get(".verbs", api.node);
			apis[api_name] = api;
			get(".name", api.node).textContent = api_name;
			get(".desc", api.node).textContent = api_desc.info.description || "";
			for_all_nodes(api.node, ".opclo", function(n){n.onclick = on_toggle_opclo});
			for_all_nodes(api.node, ".opclo ~ :not(.closedoff)", function(n){n.onclick = on_toggle_opclo});
			for_all_nodes(api.node, ".trace-item input", function(n){n.onchange = on_trace_change});
			apis_node.append(api.node);
			_.each(api_desc.paths, function(verb_desc, path_name){
				var verb_name = path_name.substring(1);
				var verb = api.verbs[verb_name];
				if (!verb) {
					verb = {
						node: document.importNode(t_verb, true),
						name: verb_name,
						api: api
					};
					verb.node.VERB = verb;
					verb.node.dataset.verb = verb_name;
					api.verbs[verb_name] = verb;
					get(".name", verb.node).textContent = verb_name;
					var g = verb_desc.get ||{};
					var r = g["responses"] || {};
					var t = r["200"] || {};
					var d = t.description || "";
					get(".desc", verb.node).textContent = d;
					if (show_perms) {
						var p = g["x-permissions"] || "";
						get(".perm", verb.node).textContent = p ? JSON.stringify(p, null, 1) : "";
					}
					api.vnode.append(verb.node);
				}
			});
			var s = get(".verbosity select", api.node);
			s.API = api;
			s.onchange = set_verbosity;
		}
	});
	inhibit = false;
	on_got_verbosities(obj);
}

function on_toggle_opclo(evt) {
	toggle_opened_closed(evt.target.parentElement);
}

function on_trace_change(evt) {
	var obj = evt.target;
	var tra = obj.parentElement;
	while (tra && !tra.dataset.trace)
		tra = tra.parentElement;
	var api = tra;
	while (api && !api.dataset.api)
		api = api.parentElement;
	var tag = api.dataset.api + "/" + tra.dataset.trace;
	if (tra) {
		var drop = false;
		for_all_nodes(tra, "input", function(n){
			if (n.checked) {
				n.checked = false;
				if (n != obj && n.value != "no")
					drop = true;
			}
		});
		if (drop)
			do_call("monitor/trace", {drop: {tag: tag}});
		obj.checked = true;
		if (obj.value != "no") {
			var spec = {tag: tag, name: "trace"};
			spec[tra.dataset.trace] = obj.value;
			if (api.dataset.api != "*")
				spec.api = api.dataset.api;
			do_call("monitor/trace", {add: spec});
		}
	}
}

function makecontent(node, deep, val) {
	if (--deep > 0) {
		if (_.isObject(val)) {
			node.append(makeobj(val, deep));
			return;
		}
		if (_.isArray(val)) {
			node.append(makearr(val, deep));
			return;
		}
	}
	node.innerHTML = obj2html(val);
}

function makearritem(tbl, deep, val) {
	var tr = document.createElement("tr");
	var td = document.createElement("td");
	tr.append(td);
	tbl.append(tr);
	makecontent(td, deep, val);
}

function makearr(arr, deep) {
	var node = document.createElement("table");
	node.className = "array";
	_.each(arr, function(v) { makearritem(node, deep, v);});
	return node;
}

function makeobjitem(tbl, deep, key, val) {
	var tr = document.createElement("tr");
	var td1 = document.createElement("td");
	var td2 = document.createElement("td");
	tr.className = key;
	tr.append(td1);
	td1.textContent = key;
	tr.append(td2);
	tbl.append(tr);
	makecontent(td2, deep, val);
}

function makeobj(obj, deep, ekey, eobj) {
	var node = document.createElement("table");
	node.className = "object";
	_.each(_.keys(obj).sort(), function(k) { makeobjitem(node, deep, k, obj[k]);});
	if (ekey)
		makeobjitem(node, deep, ekey, eobj);
	return node;
}

function gotevent(obj) {
	if (obj.event != "monitor/trace")
		add_logmsg("unexpected event!", JSON.stringify(obj, null, 1), "event");
	else {
		add_logmsg("trace event", JSON.stringify(obj, null, 1), "trace");
		gottraceevent(obj);
	}
}

function gottraceevent(obj) {
	var data = obj.data;
	var type = _.find(["request", "service", "daemon", "event"],function(x){return x in data;});
	var desc = data[type];
	if (!show_monitor_events) {
		if (type == "event" ? desc.name.startsWith("monitor/") : desc.api == "monitor")
			return;
	}
	var x = document.importNode(t_traceevent, true);
	x.dataset.event = obj;
	get(".close", x).onclick = function(evt){x.remove();};
	x.className = x.className + " " + type;
	get(".time", x).textContent = data.time;
	get(".tag", x).textContent = ({
		request: function(r) { return r.api + "/" + r.verb + "  [" + r.index + "] " + r.action; },
		service: function(r) { return r.api + "@" + r.action; },
		daemon: function(r) { return r.api + ":" + r.action; },
		event: function(r) { return r.name + "!" + r.action; },
		})[type](desc);
	var tab = makeobj(desc, 4);
	if ("data" in data)
		makeobjitem(tab, 2, "data", data.data);
	get(".content", x).append(tab);
	trace_events_node.append(x);
}

function toggle_opened_closed(node, defval) {
	var matched = false;
	var cs = node.className.split(" ").map(
		function(x){
			if (!matched) {
				switch(x) {
				case "closed": matched = true; return "opened";
				case "opened": matched = true; return "closed";
				}
			}
			return x;
		}).join(" ");
	if (!matched)
		cs = cs + " " + (defval || "closed");
	node.className = cs;
}

function on_toggle_traceevent(evt) {
	if (getSelection() != "") return;
	var node = evt.target;
	while(node && node.parentElement != trace_events_node)
		node = node.parentElement;
	node && toggle_opened_closed(node);
}

function obj2html(json) {
	json = JSON.stringify(json, undefined, 2);
	json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
	return json.replace(
		/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g,
		function (match) {
			var cls = 'number';
			if (/^"/.test(match)) {
				if (/:$/.test(match)) {
					cls = 'key';
				} else {
					cls = 'string';
				}
			} else if (/true|false/.test(match)) {
				cls = 'boolean';
			} else if (/null/.test(match)) {
				cls = 'null';
			}
			return '<span class="json ' + cls + '">' + match + '</span>';
		});
}

