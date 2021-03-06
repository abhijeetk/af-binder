
Overview of AFB-DAEMON
======================

Roles of afb-daemon
-------------------

The name **afb-daemon** stands for *Application
Framework Binder Daemon*. That is why afb-daemon
is also named ***the binder***.

**Afb-daemon** is in charge to bind one instance of
an application to the AGL framework and AGL system.

On the following figure, you can use a typical use
of afb-daemon:

<h4><a id="binder-fig-basis">Figure: binder afb-daemon, basis</a></h4>

![binder-basis][binder-basis]

The application and its companion binder run in secured and isolated
environment set for them. Applications are intended to access to AGL
system through the binder.

The binder afb-daemon serves multiple purposes:

1. It acts as a gateway for the application to access the system;

2. It acts as an HTTP server for serving files to HTML5 applications;

3. It allows HTML5 applications to have native extensions subject
to security enforcement for accessing hardware resources or
for speeding parts of algorithm.

Use cases of the binder afb-daemon
----------------------------------

This section tries to give a better understanding of the binder
usage through several use cases.

### Remotely running application

One of the most interesting aspect of using the binder afb-daemon
is the ability to run applications remotely. This feature is
possible because the binder afb-daemon implements native web
protocols.

So the [figure binder, basis](#binder-fig-basis) would become
when the application is run remotely:

<h4><a id="binder-fig-remote">Figure: binder afb-daemon and remotely running application</a></h4>


### Adding native features to HTML5/QML applications

Applications can provide with their packaged delivery a binding.
That binding will be instantiated for each application instance.
The methods of the binding will be accessible by applications and
will be executed within the security context.

### Offering services to the system

It is possible to run the binder afb-daemon as a daemon that provides the
API of its bindings.

This will be used for:

1. offering common APIs

2. provide application's services (services provided as application)

In that case, the figure showing the whole aspects is

<h4><a id="binder-fig-remote">Figure: binder afb-daemon for services</a></h4>

![afb-for-services][afb-for-services]

For this case, the binder afb-daemon takes care to attribute one single session
context to each client instance. It allows bindings to store and retrieve data
associated to each of its client.

The bindings of the binder afb-daemon
------------------------------------

The binder can instantiate bindings. The primary use of bindings
is to add native methods that can be accessed by applications
written with any language through web technologies ala JSON RPC.

This simple idea is declined to serves multiple purposes:

1. add native feature to applications

2. add common API available by any applications

3. provide customers services

A specific document explains how to write an afb-daemon binder binding:
[HOWTO WRITE a BINDING for AFB-DAEMON](afb-bindings-writing.html)


Launching the binder afb-daemon
-------------------------------

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
		but apis are still accessibles.

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

	  --no-httpd

		Tells to not start the HTTP server.

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

	  --readyfd=xxxx

		Set the #fd to signal when ready

		If set, the binder afb-daemon will write "READY=1\n" on the file
		descriptor whose number if given (/proc/self/fd/xxx).

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





Future development of afb-daemon
--------------------------------

- The binder afb-daemon would launch the applications directly.

- The current setting of mode (local/remote/global) might be reworked to a
mechanism for querying configuration variables.

- Implements "one-shot" initial token. It means that after its first
authenticated use, the initial token is removed and no client can connect
anymore.

- Creates some intrinsic APIs.

- Make the service connection using WebSocket not DBUS.

- Management of targeted events.

- Securing LOA.

- Integration of the protocol JSON-RPC for the websockets.

[binder-basis]: pictures/AFB_overview.svg
[afb-for-services]: pictures/AFB_for_services.svg
