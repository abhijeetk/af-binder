Vocabulary for AFB-DAEMON
=========================
    version: 1
    Date:    27 mai 2016
    Author:  José Bollo

TABLE-OF-CONTENT-HERE

## Event

Message with data propagated from the services to the client and not expecting
any reply.

The current implementation allows to widely broadcast events to all clients.

## Level of assurance (LOA)

This level that can be from 0 to 3 represent the level of
assurance that the services can expect from the session.

The exact definition of the meaning of this levels and of
how to use it remains to be achived.

## Plugin

A shared library object intended to be plug to an afb-daemon instance
to implement an API.

## Request

A request is an invocation by a client to a method of a plugin using a message
transfered through some protocol: HTTP, WebSocket, DBUS... served by afb-daemon

## Reply/Response

This is a message sent to client as the result of the request.

## Service

Service are made of plugins runnning by their side on their binder.
It can serve many client. Each one being attached to one session.

The framework establishes the connection between the services and
the clients. Using DBus currently.

## Session

A session is meant to be the unic context of an instance of client, 
identifying that instance across requests.

Each session has an identifier. Session identifier generated by afb-daemon are UUIDs.

Internally, afb-daemon offers a mechanism to attach data to sessions.
When the session is closed or disappears, the data attached to that session
are freed.

## Token

The token is an identifier that the the client must give to be authentificated.

At start, afb-daemon get an initial token. This initial token must be presented
incoming client to be authentificated.

A token is valid only for a period.

The token must be renewed periodically. When the token is renewed, afb-daemon
sends the new token to the client.

Tokens generated by afb-daemon are UUIDs.

## UUID

It stand for Universal Unic IDentifier.

Its is designed to create identifier in a way that avoid has much as possible conflicts.
It means that if two differents instance create a UUID, the probability that they create the same UUID is very low, near to zero.

## x-afb-reqid

Argument name that can be used with HTTP request.
When this argument is given, it is automatically added to the "request" object of the
answer.

## x-afb-token

Argument name for giving the token without ambiguity.
You can also use the name **token** but it may conflicts with other arguments.

## x-afb-uuid

Argument name for giving explicitely the session identifier without ambiguity.
You can also use the name **uuid** but it may conflicts with other arguments.
