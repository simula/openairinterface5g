# OAI gNB LTTng Tracing Setup Guide

## Overview

This guide will walk you through setting up tracing for an OpenAirInterface (OAI) gNB (gNodeB) using LTTng (Linux Trace Toolkit Next Generation) and Babeltrace.
### What is LTTng and Why Use It?

LTTng, or Linux Trace Toolkit Next Generation, is a powerful logging framework designed for Linux systems. It provides low-overhead tracing capabilities, allowing developers to monitor and analyze system behavior in real-time without significant performance impact. LTTng offers several advantages:

- **Low Overhead**: LTTng introduces minimal overhead to the system, making it suitable for use in production environments without affecting system performance.

- **Customizable**: LTTng allows users to define custom tracepoints in their applications, providing fine-grained control over what events to trace and collect.

- **Scalability**: It can scale to large distributed systems, making it suitable for tracing complex software stacks across multiple nodes.

## Prerequisites

- Ubuntu system

Note: only LTTng 2.3.8 is supported.

## Building OAI gNB

1. **Clean and Build OAI gNB with LTTng:**

    1.1  Install Dependencies

    ```bash
    ./build_oai --ninja -I --clean --enable-LTTNG
    
    ```
    1.2  Build gNB and nrUE
    ```
    ./build_oai --ninja --gNB --nrUE -w SIMU --enable-LTTNG
    ```
## Setting up LTTng

1. **Start LTTng Session and Relay:**

    ```bash
    sudo lttng-sessiond -d
    sudo lttng-relayd -d
    ```

2. **Create Live LTTng Session:**

    ```bash
    sudo lttng create my-session --live --set-url=net://127.0.0.1
    ```

3. **Enable gNB Trace Events:**

    ```bash
    sudo lttng enable-event --userspace OAI:gNB
    ```

4. **Start LTTng Tracing:**

    ```bash
    sudo lttng start
    ```

## Running the gNB

1. **Run gNB:**

    ```bash
    ./$binary_path -O $configuration_file PARALLEL_SINGLE_THREAD --rfsimulator.serveraddr server --rfsim -E
    ```

## Verifying Tracepoints

1. **List Active Tracepoints:**

    ```bash
    sudo lttng list -u
    ```

    *Possible Output:*

    ```
    UST events:
    -------------
    
    PID: 1154722 - Name: /home/firecell/Desktop/FirecellRepos/firecellrd-oai5g-ran/cmake_targets/ran_build/build/nr-softmodem
          OAI:gNB (loglevel: TRACE_DEBUG_FUNCTION (12)) (type: tracepoint)
    ```

## Analyzing Traces

1. **Install Babeltrace:**

    ```bash
    sudo apt-get install babeltrace
    ```


2. **Capture Trace Logs Live:**

    - To learn the full path of the trace session:
    
        ```bash
        babeltrace --input-format=lttng-live net://localhost
        ```

    - Trace logs using the full path:
    
        ```bash
        babeltrace --input-format=lttng-live net://localhost/host/firecell-XPS-15-9530/my-session
        ```

    *Possible Output:*

    ```
    [19:35:32.181608002] (+2.664882127) firecell-XPS-15-9530 OAI:gNB: { cpu_id = 10 }, { MODNAME = "OAI-NR_MAC info", EVENTID = -1, SFN = -1, SLOT = -1, FUNCTION = "gNB_dlsch_ulsch_scheduler", LINE = 246, MSG = "Frame.Slot 0.0\n" }
    ```
3. **Capture Trace Logs Offline:**

    - Create an offline trace session with a specified output directory:

        ```bash
        sudo lttng create offline_session --output=/home/trace_offline/
        ```

    - Enable gNB trace events:

        ```bash
        sudo lttng enable-event --userspace OAI:gNB
        ```

    - Start capturing trace logs:

        ```bash
        sudo lttng start
        ```

    - Stop the trace capture:

        ```bash
        sudo lttng stop
        ```

    - Destroy the trace session:

        ```bash
        sudo lttng destroy
        ```

    - Use Babeltrace to analyze the captured trace logs:

        ```bash
        sudo babeltrace /home/trace_offline/
        ```
