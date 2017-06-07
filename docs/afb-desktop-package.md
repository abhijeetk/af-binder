
Desktop packages for binder developement
========================================

It exists packages of the ***binder*** (afb-daemon)
for common desktop linux distributions.

 - Fedora
 - Ubuntu
 - Debian
 - Suse

Installing the developement package of the ***binder***
allows to write ***bindings*** that runs on the destop
computer of the developper.

It is very convenient to quickly write and debug a binding.

Retriving compiling option with pkg-config
==========================================

The ***binder*** afb-daemon provides a configuration
file for **pkg-config**.
Typing the command

	pkg-config --cflags afb-daemon

Print flags use for compilation:

	$ pkg-config --cflags afb-daemon
	-I/opt/local/include -I/usr/include/json-c

For linking, you should use

	$ pkg-config --libs afb-daemon
	-ljson-c

It automatically includes the dependency to json-c.
This is activated through **Requires** keyword in pkg-config.

