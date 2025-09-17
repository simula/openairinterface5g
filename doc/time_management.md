# Time management module

[[_TOC_]]

## Introduction

Several parts of the 4G/5G protocol stack keep track of time, for example
for timeout events (in RRC, PDCP, or RLC for example).

The obvious simple way to deal with time is to use the computer's realtime
clock. This works well when we run the stack in realtime, with real
radio equipment. But in simulation the system may run faster or slower
than realtime, so we need another way to deal with time. A good solution
is then to base time on the current IQ data processed by the system. So time
becomes "IQ samples" based, not realtime.

The system may be decomposed into various components (CU, DU, RU, various
UEs) running either in realtime or simulation. Time may be distributed
to all these components from only one time source for accurate execution
of the scenario, or each component may have its own time source, either
based on realtime or IQ samples.

## Overall structure

The time management module is made of three main parts. The first one
is the time source. It can either be realtime or "IQ samples" based.
In one running scenario of the system, there can be one or more time
sources (imagine one for the gNB, and one per each UE). There can also
be only one time source, shared between various users of the time
manager.

The second part is the server. One server is attached to one time source
and receives a tick every millisecond (real or simulated) from the time
source. It distributes the tick to the various connected clients.

And finally the last part is the client, which connects to a single server
and receives ticks from it. The client does not have a time source attached
to it. It gets the time from the server.

It is possible for a program (monolithic gNB for example) to have only
a time source, without server or client.

## Configuration

To configure the time management module, add the following section in
the configuration file.

```
time_management = {
  #valid time sources: realtime, iq_samples
  time_source = realtime

  #valid modes: standalone, server, client
  mode = standalone

  #set ip/port of server (for server mode, this is the address/port to bind to;
  #for client mode, this is the address/port to connect to)
  server_ip = "127.0.0.1"
  server_port = 7374
}
```

If not set in the configuration file, some default is applied, depending
on the program run. (See the function `time_manager_start()` for details.)

It is also possible to pass configuration parameters on the command line.
For example add `--time_manager.time_source iq_samples` to the command line
to set the time source to `iq_samples`. The same syntax is applicable to the
other parameters.

## Examples

Here come some examples of configuration for typical use cases of OAI.

### Monolithic gNB realtime

This is a very simple example. The gNB runs in `standalone` mode with
a `realtime` time source.

```
time_management = {
  time_source = realtime
  mode = standalone
}
```
### Monolithic gNB "IQ samples time"

For this configuration, it's the same as the previous example, but the
time source is now `iq_samples`.

```
time_management = {
  time_source = iq_samples
  mode = standalone
}
```
### RF simulator CU/DU "IQ samples time"

Let's suppose the CU runs on a machine with IP address 1.2.3.4 and the DU
runs on a machine with IP address 10.11.12.13.

The DU has the time source and so is the server. The CU acts as a client.

Here comes the configuration for the CU.

```
time_management = {
  mode = client
  server_ip = "10.11.12.13"
  server_port = 7374
}
```
And here is the configuration for the DU.

```
time_management = {
  time_source = iq_samples
  mode = server
  server_ip = "10.11.12.13"
  server_port = 7374
}
```

### RF simulator CU/DU "realtime"

Let's suppose the CU runs on a machine with IP address 1.2.3.4 and the DU
runs on a machine with IP address 10.11.12.13.

CU and DU can both run in standalone mode, with no exchange between them.
Any one of them can also acts as a server and the other one as a client.

Let's suppose for this example that the CU and the DU run in client/server
mode. Let's set the CU as the time source and so as the server. The DU acts
as a client.

Here comes the configuration for the CU.

```
time_management = {
  time_source = realtime
  mode = server
  server_ip = "1.2.3.4"
  server_port = 7374
}
```
And here is the configuration for the DU.

```
time_management = {
  mode = client
  server_ip = "1.2.3.4"
  server_port = 7374
}
```

Note that the time management module is flexible. It's possible to
configure more complex setups. For example the time source can be a gNB
and several UEs can be connected as client.

One could also write a simple program acting as a time source and distributing
time to everyone (one or several gNB, several UEs).

## Programming API

Here comes the internal API of the time manager module. The (simpler) OAI
API is described afterwards.

### Time source

- opaque type: `time_source_t`
- create a time source:
  ```
    time_source_t *new_time_source(time_source_type_t type)
  ```
  where `type` is one of:
  * `TIME_SOURCE_REALTIME`
  * `TIME_SOURCE_IQ_SAMPLES`
- delete a time source:
  ```
    void free_time_source(time_source_t *time_source)
  ```
- set millisecond callback:
  ```
    void time_source_set_callback(time_source_t *time_source,
                                  void (*callback)(void *),
                                  void *callback_data)
  ```
  where `callback` is a function that will be called for each millisecond using
  the provided `callback_data`
- for IQ samples time management, the user of the time source must call
  this function when it processes IQ samples:
  ```
    void time_source_iq_add(time_source_t *time_source,
                            uint64_t iq_samples_count,
                            uint64_t iq_samples_per_second)
  ```
  then the time source will generate a tick for each block for IQ samples
  lasting one millisecond. (If you pass `iq_samples_count` lasting several
  milliseconds then the time source will generate several ticks in a row,
  one for each millisecond.)

### Server

- opaque type: `time_server_t`
- create a server:
  ```
    time_server_t *new_time_server(const char *ip,
                                   int port,
                                   void (*callback)(void *),
                                   void *callback_data)
  ```
  where ip/port is the IP adress and port the server will listen to
  and `callback` is a function that will be called for each millisecond using
  the provided `callback_data`
- delete a server:
  ```
    void free_time_server(time_server_t *time_server)
  ```
- attach a time source:
  ```
    void time_server_attach_time_source(time_server_t *time_server,
                                        time_source_t *time_source)
  ```

### Client

- opaque type: `time_client_t`
- create a client:
  ```
    time_client_t *new_time_client(const char *server_ip,
                                   int server_port,
                                   void (*callback)(void *),
                                   void *callback_data)
  ```
  the client will connect on given server ip/port
  and use the `callback` with `callback_data` for each tick received
- delete a client:
  ```
    void free_time_client(time_client_t *time_client)
  ```

### Threading

Each component (time source, server, client) runs its own thread which will
be the one calling the configured callback. Be careful of using proper
synchronization techniques in your callback to have correct behavior with
the other threads of the program.

## OAI API

OAI uses the time manager through a simplified API. All the code is contained
in `time_manager.c`, together with some global variables (so that there is no
need to pass objects around, to limit risks of misuse).

Each program (for example: gnb, ue, cu, du) calls the `time_manager_start()`
function, passing the callback functions to be called when a tick is generated
(several callbacks can be passed) and a default time source. Based on this,
`time_manager_start()` initializes what is needed.

The function `time_manager_iq_samples()` is to be called by programs that
read IQ samples. It is called unconditionally. It may do nothing if the
configuration is to use realtime ticks and not IQ samples ticks.

When the program exits, it calls `timer_manager_finish()` which in turns stops
the various threads created by `time_manager_start()` and releases all the
allocated data.

Here comes the API.

- init the time manager:
  ```
    void time_manager_start(time_manager_tick_function_t *tick_functions,
                            int tick_functions_count,
                            time_source_type_t time_source);
  ```
  (for valid values of `client_type` and `running_mode`  see in
  `time_manager.h`)
- update IQ samples' based time manager:
  ```
    void time_manager_iq_samples(uint64_t iq_samples_count,
                                 uint64_t iq_samples_per_second)
  ```
- terminate the time manager:
  ```
    void time_manager_finish(void)
  ```
