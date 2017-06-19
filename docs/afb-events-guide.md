Guide for developing with events
================================

Signaling agents are services that send events to any clients that
subscribed for receiving it. The sent events carry any data.

To have a good understanding of how to write a signaling agent, the
actions of subscribing, unsubscribing, producing, sending and receiving
events must be described and explained.

Overview of events
------------------

The basis of a signaling agent is shown in the following figure:

![scenario of using events](pictures/signaling-basis.svg)

This figure shows the main role of the signaling framework for the events
propagation.

For people not familiar with the framework, a signaling agent and
a “binding” are similar.

### Subscribing and unsubscribing

Subscribing is the action that makes a client able to receive data from a
signaling agent. Subscription must create resources for generating the data, and
for delivering the data to the client. These two aspects are not handled by the
same piece of software. Generating the data is the responsibility of the
developer of the signaling agent while delivering the data is handled by the
framework.

When a client subscribes for data, the agent must:

1.  check that the subscription request is correct;
2.  establish the computation chain of the required data, if not already
    done;
3.  create a named event for the computed data, if not already done;
4.  ask the framework to establish the subscription to the event for the
    request;
5.  optionally give indications about the event in the reply to
    the client.

The first two steps are not involving the framework. They are linked to
the business logic of the binding. The request can be any description of
the requested data and the computing stream can be of any nature, this
is specific to the binding.

As said before, the framework uses and integrates **libsystemd** and its event
loop. Within the framework, **libsystemd** is the standard API/library for
bindings expecting to setup and handle I/O, timer or signal events.

Steps 3 and 4 are bound to the framework.

The agent must create an object for handling the propagation of produced
data to its clients. That object is called “event” in the framework. An
event has a name that allows clients to distinguish it from other
events.

Events are created using the ***afb\_daemon\_make\_event*** function
that takes the name of the event. Example:

```C
	event = afb_daemon_make_event(name);
```

Once created, the event can be used either to push data to its
subscribers or to broadcast data to any listener.

The event must be used to establish the subscription for the requesting
client. This is done using the ***afb\_req\_subscribe*** function
that takes the current request object and event and associates them
together. Example:

```C
	rc = afb_req_subscribe(req, event);
```

When successful, this function make the connection between the event and
the client that emitted the request. The client becomes a subscriber of
the event until it unsubscribes or disconnects. The
***afb\_req\_subscribe*** function will fail if the client
connection is weak: if the request comes from a HTTP link. To receive
signals, the client must be connected. The AGL framework allows connections
using WebSocket.

The name of the event is either a well known name or an ad hoc name
forged for the use case.

Let's see a basic example: client A expects to receive the speed in km/h
every second while client B expects the speed in mph twice a second. In
that case, there are two different events because it is not the same
unit and it is not the same frequency. Having two different events
allows to associate clients to the correct event. But this doesn't tell
any word about the name of these events. The designer of the signaling
agent has two options for naming:

1.  names can be the same (“speed” for example) with sent data
    self describing itself or having a specific tag (requiring from
    clients awareness about requesting both kinds of speed isn't safe).
2.  names of the event include the variations (by example:
    “speed-km/h-1Hz” and “speed-mph-2Hz”) and, in that case, sent data
    can self describe itself or not.

In both cases, the signaling agent might have to send the name of the
event and/or an associated tag to its client in the reply of the
subscription. This is part of the step 5 above.

The framework only uses the event (not its name) for subscription,
un-subscription and pushing.

When the requested data is already generated and the event used for
pushing it already exists, the signaling agent must not instantiate a
new processing chain and must not create a new event object for pushing
data. The signaling agent must reuse the existing chain and event.

Unsubscribing is made by the signaling agent on a request of its client.
The ***afb\_req\_unsubscribe*** function tells the framework to
remove the requesting client from the event's list of subscribers.
Example:

```C
	afb_req_unsubscribe(req, event);
```

Subscription count does not matter to the framework: subscribing the
same client several times has the same effect that subscribing only one
time. Thus, when unsubscribing is invoked, it becomes immediately
effective.

#### More on naming events

Within the AGL framework, a signaling agent is a binding that has an API
prefix. This prefix is meant to be unique and to identify the binding
API. The names of the events that this signaling agent creates are
automatically prefixed by the framework, using the API prefix of the
binding.

Thus, if a signaling agent of API prefix ***api*** creates an event
of name ***event*** and pushes data to that event, the subscribers
will receive an event of name ***api/event***.

### Generating and pushing signals and data

This of the responsibility of the designer of the signaling agent to
establish the processing chain for generating events. In many cases,
this can be achieved using I/O or timer or signal events inserted in the
main loop. For this case, the AGL framework uses **libsystemd** and
provide a way to integrates to the main loop of this library using
afb\_daemon\_get\_event\_loop. Example:

```C
	sdev = afb_daemon_get_event_loop();
	rc = sd_event_add_io(sdev, &source, fd, EPOLLIN, myfunction, NULL);
```

In some other cases, the events are coming from D-Bus. In that case, the
framework also uses **libsystemd** internally to access D-Bus. It provides
two methods to get the available D-Bus objects, already existing and
bound to the main**libsystemd**event loop. Use either
***afb\_daemon\_get\_system\_bus*** or
***afb\_daemon\_get\_user\_bus*** to get the required instance. Then
use functions of **libsystemd** to handle D-Bus.

In some rare cases, the generation of the data requires to start a new
thread.

When a data is generated and ready to be pushed, the signaling agent
should call the function ***afb\_event\_push***. Example:

```C
	rc = afb_event_push(event, JSON);
	if (rc == 0) {
		stop_generating(event);
		afb_event_drop(event);
	}
```

The function ***afb\_event\_push*** pushes json data to all the
subscribers. It then returns the count of subscribers. When the count is
zero, there is no subscriber listening for the event. The example above
shows that in that case, the signaling agent stops to generate data for
the event and delete the event using afb\_event\_drop. This is one
possible option. Other valuable options are: do nothing and continue to
generate and push the event or just stop to generate and push the data
but keep the event existing.

### Receiving the signals

Understanding what a client expects when it receives signals, events or
data shall be the most important topic of the designer of a signaling
agent. The good point here is that because JSON[^1] is the exchange
format, structured data can be sent in a flexible way.

The good design is to allow as much as possible the client to describe
what is needed with the goal to optimize the processing to the
requirements only.

### The exceptional case of wide broadcast

Some data or events have so much importance that they can be widely
broadcasted to alert any listening client. Examples of such an alert
are:

-   system is entering/leaving “power safe” mode
-   system is shutting down
-   the car starts/stops moving
-   ...

An event can be broadcasted using one of the two following methods:
***afb\_daemon\_broadcast\_event*** or
***afb\_event\_broadcast***.

Example 1:

```C
	afb_daemon_broadcast_event(name, json);
```

Example 2:

```C
	event = afb_daemon_make_event(name);
	. . . .
	afb_event_broadcast(event, json);
```

As for other events, the name of events broadcasted using
***afb\_daemon\_broadcast\_event*** are automatically prefixed by
the framework with API prefix of the binding (signaling agent).

Reference of functions
----------------------

### Function afb\_event afb\_daemon\_make\_event

The function ***afb\_daemon\_make\_event*** that is defined as below:

```C
/*
 * Creates an event of 'name' and returns it.
 */
struct afb_event afb_daemon_make_event(const char *name);
```

The correct way to create the event at initialization is to call the function
***afb\_daemon\_make\_event*** within the initialization
function referenced by the field ***init*** of the structure ***afbBindingV2***.

### Function afb\_event\_push

The function ***afb\_event\_push*** is defined as below:

```C
/*
 * Pushes the 'event' with the data 'object' to its observers.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
int afb_event_push(struct afb_event event, struct json_object *object);
```

As the function ***afb\_event\_push*** returns 0 when there is no
more subscriber, a binding can remove such unexpected event using the
function ***afb\_event\_drop***.

### Function afb\_event\_drop

The function ***afb\_event\_drop*** is defined as below:

```C
/*
 * Drops the data associated to the event
 * After calling this function, the event
 * MUST NOT BE USED ANYMORE.
 */
void afb_event_drop(struct afb_event event);
```

### Function afb\_req\_subscribe

The function ***afb\_req\_subscribe*** is defined as below:

```C
/*
 * Establishes for the client link identified by 'req' a subscription
 * to the 'event'.
 * Returns 0 in case of successful subscription or -1 in case of error.
 */
int afb_req_subscribe(struct afb_req req, struct afb_event event);
```

The subscription adds the client of the request to the list of subscribers
to the event.

### Function afb\_req\_unsubscribe

The function ***afb\_req\_unsubscribe*** is defined as
below:

```C
/*
 * Revokes the subscription established to the 'event' for the client
 * link identified by 'req'.
 * Returns 0 in case of successful un-subscription or -1 in case of error.
 */
int afb_req_unsubscribe(struct afb_req req, struct afb_event event);
```

The un-subscription removes the client of the request of the list of subscribers
to the event.
When the list of subscribers to the event becomes empty,
the function ***afb\_event\_push*** will return zero.

### Function afb\_event\_broadcast

The function ***afb\_event\_broadcast*** is defined as below:

```C
/*
 * Broadcasts widely the 'event' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
int afb_event_broadcast(struct afb_event event, struct json_object *object);
```

This uses an existing event (created with ***afb\_daemon\_make\_event***)
for broadcasting an event having its name.


### Function afb\_daemon\_broadcast\_event

The function ***afb\_daemon\_broadcast\_event*** is defined as below:

```C
/*
 * Broadcasts widely the event of 'name' with the data 'object'.
 * 'object' can be NULL.
 *
 * For convenience, the function calls 'json_object_put' for 'object'.
 * Thus, in the case where 'object' should remain available after
 * the function returns, the function 'json_object_get' shall be used.
 *
 * Returns the count of clients that received the event.
 */
int afb_daemon_broadcast_event(const char *name, struct json_object *object);
```

The name is given here explicitly. The name is automatically prefixed
with the name of the binding. For example, a binding of prefix "xxx"
would broadcast the event "xxx/name".

### Function onevent (field of afbBindingV2)

Binding can designate an event handling function using the field **onevent**
of the structure **afbBindingV2**. This function is called when an event is
broadcasted or when an event the binding subscribed to is pushed.
That allow a service to react to an event and do what it is to do if this is
relevant for it (ie: car back camera detects imminent collision and broadcast
it, then appropriate service enable parking brake.).
