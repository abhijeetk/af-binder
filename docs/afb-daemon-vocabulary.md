
Vocabulary for AFB-DAEMON
=========================

## Binding

A shared library object intended to add a functionality to an afb-daemon
instance. It implements an API and may provide a service.

Binding made for services can have specific entry point called after
initialization and before serving.

## Event

Message with data propagated from the services to the client and not expecting
any reply.

The current implementation allows to widely broadcast events to all clients.

## Level of assurance (LOA)

This level that can be from 0 to 3 represent the level of
assurance that the services can expect from the session.

The exact definition of the meaning of these levels and how to use it remains to
be achieved.

## Plugin

Old name for binding, see binding.

## Request

A request is an invocation by a client to a binding method using a message
transferred through some protocol: HTTP, WebSocket, DBUS... and served by
***afb-daemon***

## Reply/Response

This is a message sent to client as the result of the request.

## Service

Service are made of bindings running by their side on their binder.
It can serve many client. Each one attached to one session.

The framework establishes connection between the services and
the clients. Using DBus currently but other protocols are considered.

## Session

A session is meant to be the unique instance context of a client,
which identify that instance across requests.

Each session has an identifier. Session identifier generated by afb-daemon are
UUIDs.

Internally, afb-daemon offers a mechanism to attach data to sessions.
When the session is closed or disappears, the data attached to that session
are freed.

## Token

The token is an identifier that the client must give to be authenticated.

At start, afb-daemon get an initial token. This initial token must be presented
by incoming client to be authenticated.

A token is valid only for a period.

The token must be renewed periodically. When the token is renewed, afb-daemon
sends the new token to the client.

Tokens generated by afb-daemon are UUIDs.

## UUID

It stand for Universal Unique IDentifier.

It is designed to create identifier in a way that avoid has much as possible
conflicts. It means that if two different instances create an UUID, the
probability that they create the same UUID is very low, near to zero.

## x-afb-reqid

Argument name that can be used with HTTP request. When this argument is given,
it is automatically added to the "request" object of the answer.

## x-afb-token

Argument name meant to give the token without ambiguity.
You can also use the name **token** but it may conflicts with others arguments.

## x-afb-uuid

Argument name for giving explicitly the session identifier without ambiguity.
You can also use the name **uuid** but it may conflicts with others arguments.
