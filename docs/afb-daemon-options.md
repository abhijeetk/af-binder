
Launching options of afb-daemon
---------------------

The launch options for binder **afb-daemon** are:

	  --help

		Prints help with available options

	  --version

		Display version and copyright

	  --verbose

		Increases the verbosity, can be repeated

	  --quiet

		Decreases the verbosity, can be repeated

	  --port=xxxx

		HTTP listening TCP port  [default 1234]

	  --workdir=xxxx

		Directory where the daemon must run [default: $PWD if defined
		or the current working directory]

	  --uploaddir=xxxx

		Directory where uploaded files are temporarily stored [default: workdir]

	  --rootdir=xxxx

		Root directory of the application to serve [default: workdir]

	  --roothttp=xxxx

		Directory of HTTP served files. If not set, files are not served
		but apis are still accessible.

	  --rootbase=xxxx

		Angular Base Root URL [default /opa]

		This is used for any application of kind OPA (one page application).
		When set, any missing document whose url has the form /opa/zzz
		is translated to /opa/#!zzz

	  --rootapi=xxxx

		HTML Root API URL [default /api]

		The bindings are available within that url.

	  --alias=xxxx

		Maps a path located anywhere in the file system to the
		a subdirectory. The syntax for mapping a PATH to the
		subdirectory NAME is: --alias=/NAME:PATH.

		Example: --alias=/icons:/usr/share/icons maps the
		content of /usr/share/icons within the subpath /icons.

		This option can be repeated.

	  --apitimeout=xxxx

		binding API timeout in seconds [default 20]

		Defines how many seconds maximum a method is allowed to run.
		0 means no limit.

	  --cntxtimeout=xxxx

		Client Session Timeout in seconds [default 3600]

	  --cache-eol=xxxx

		Client cache end of live [default 100000 that is 27,7 hours]

	  --session-max=xxxx

		Maximum count of simultaneous sessions [default 10]

	  --ldpaths=xxxx

		Load bindings from given paths separated by colons
		as for dir1:dir2:binding1.so:... [default = $libdir/afb]

		You can mix path to directories and to bindings.
		The sub-directories of the given directories are searched
		recursively.

		The bindings are the files terminated by '.so' (the extension
		so denotes shared object) that contain the public entry symbol.

	  --binding=xxxx

		Load the binding of given path.

	  --token=xxxx

		Initial Secret token to authenticate.

		If not set, no client can authenticate.

		If set to the empty string, then any initial token is accepted.

	  --random-token

		Generate a random starting token. See option --exec.

	  --mode=xxxx

		Set the mode: either local, remote or global.

		The mode indicate if the application is run locally on the host
		or remotely through network.

	  --dbus-client=xxxx

		Transparent binding to a binder afb-daemon service through dbus.

		It creates an API of name xxxx that is implemented remotely
		and queried via DBUS.

	  --dbus-server=xxxx

		Provides a binder afb-daemon service through dbus.

		The name xxxx must be the name of an API defined by a binding.
		This API is exported through DBUS.

	  --ws-client=xxxx

		Transparent binding to a binder afb-daemon service through a WebSocket.

		The value of xxxx is either a unix naming socket, of the form "unix:path/api",
		or an internet socket, of the form "host:port/api".

	  --ws-server=xxxx

		Provides a binder afb-daemon service through WebSocket.

		The value of xxxx is either a unix naming socket, of the form "unix:path/api",
		or an internet socket, of the form "host:port/api".

	  --foreground

		Get all in foreground mode (default)

	  --daemon

		Get all in background mode

	  --no-httpd

		Forbids HTTP serve

	  --exec

		Must be the last option for afb-daemon. The remaining
		arguments define a command that afb-daemon will launch.
		The sequences @p, @t and @@ of the arguments are replaced
		with the port, the token and @.

	  --tracereq=xxxx

		Trace the processing of requests in the log file.

		Valid values are 'no' (default), 'common', 'extra' or 'all'.

	  --traceditf=xxxx

		Trace the accesses to functions of class daemon.

		Valid values are 'no' (default), 'common', 'extra' or 'all'.

	  --tracesvc=xxxx

		Trace the accesses to functions of class service.

		Valid values are 'no' (default) or 'all'.

	  --traceevt=xxxx

		Trace the accesses to functions of class event.

		Valid values are 'no' (default), 'common', 'extra' or 'all'.

    --call=xxx

		Call a binding at start (can be be repeated).
		The values are given in the form API/VERB:json-args.

		Example: --call 'monitor/set:{"verbosity":{"api":"debug"}}'
