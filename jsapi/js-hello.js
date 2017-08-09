var counter = 0;

exports.ping = function(req, args) {

	afb_req_success(req, args, String(++counter));
};

exports.fail = function(req, args) {

	afb_req_fail(req, "fail", String(++counter));
};

exports.subcall = function(req, args) {

	if (!args.api || !args.verb)
		afb_req_fail(req, "bad-args", String(++counter));
	else {
		var x = afb_req_subcall_sync(req, args.api, args.verb, args.args);
		if (!x)
			afb_req_fail(req, "null-answer", String(++counter));
		else if (x.request.status == "success")
			afb_req_success(req, x.response, String(++counter));
		else
			afb_req_fail(req, x.request.status, String(++counter));
	}
};


