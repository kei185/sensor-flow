# Architecture

Current implementation. [Typed pipelines](typed-pipelines.md) are a proposal.
Host parsing: [frame.md](frame.md).
Wire format and payloads: [sensor-echo Frame Format](https://github.com/kei185/sensor-echo/blob/main/docs/frame.md).

## Data flow

```mermaid
flowchart LR
    device["Serial device"] --> receiver["Receiver<br/>Route by stream"]
    receiver --> raw["Raw frame queues"]

    raw -->|SYSTEM| systemParser["Parser<br/>SYSTEM"]
    raw -->|LiDAR| lidarParser["Parser<br/>LiDAR"]
    raw -->|IMU| imuParser["Parser<br/>IMU"]
    raw -->|Encoder| encoderParser["Parser<br/>Encoder"]

    systemParser --> systemQueue["systemMessage queue"] --> systemDistributor["Distributor<br/>SYSTEM"]
    lidarParser --> lidarQueue["LidarPoint queue"] --> lidarDistributor["Distributor<br/>LiDAR"]
    imuParser --> imuQueue["Imu queue"] --> imuDistributor["Distributor<br/>IMU"]
    encoderParser --> encoderQueue["Encoder queue"] --> encoderDistributor["Distributor<br/>Encoder"]

    directives["Motor command queue"] --> systemDistributor
    systemDistributor --> transmitter["Transmitter"]
    transmitter -->|Commands| device
```

SYSTEM groups device status messages and command acknowledgements.

| Distributor | Current behavior                               |
| ----------- | ---------------------------------------------- |
| SYSTEM      | Runs device startup and the motor command loop |
| LiDAR       | Drains the queue; plotting is disabled         |
| IMU         | Plots raw acceleration on three axes           |
| Encoder     | Drains the queue                               |

## Ownership

| Manager member   | Owned resource                | Used by                                 |
| ---------------- | ----------------------------- | --------------------------------------- |
| `port`           | One Port                      | Receiver reads; Transmitter writes      |
| `frameStreams`   | Map of raw frame queues       | Receiver and Parser                     |
| `dataStreams`    | Map of parsed data queues     | Parser and Distributor                  |
| `directiveQueue` | Queue of MotorCommand         | Command producer and SYSTEM Distributor |
| `transmitter`    | One Transmitter               | SYSTEM Distributor                      |
| `receiverWorker` | Receiver processor and thread | Serial RX                               |
| `parsers`        | Map of Parser workers         | One per stream                          |
| `distributors`   | Map of Distributor workers    | One per stream                          |

Parsed queues share a common base class. Each processor borrows a queue of its
concrete data type.

## Workers

```mermaid
classDiagram
    class Worker {
        +dispatch()
        +abort()
    }
    class Parser_T["Parser"]
    class DeviceController["Distributor: DeviceController"]
    class Plotter_T["Distributor: Plotter"]
    Manager *-- Worker : owns
    Worker *-- Processor : unique_ptr instance
    Worker *-- JThread : thread
    Processor <|-- Receiver
    Processor <|-- Parser_T
    Processor <|-- DeviceController
    Processor <|-- Plotter_T
    note for Worker "Non-template wrapper<br/>dispatch(): start thread<br/>abort(): request stop"
    note for Processor "run(std::stop_token)"
    note for JThread "std::jthread"
```

Starting or stopping a worker fails if its processor is missing.

## Queue synchronization

```mermaid
sequenceDiagram
    participant Parser
    participant Queue as Parsed data queue
    participant Distributor

    Distributor->>Queue: Wait for data, stop, or deadline
    Note over Queue: Check for data under mutex<br/>Wait only while empty
    Parser->>Queue: Enqueue samples under mutex
    Note right of Parser: Enqueueing does not notify
    Parser->>Queue: Notify one Distributor
    alt Data available
        Queue-->>Distributor: Data ready
        Distributor->>Queue: Take a sample under mutex
        Queue-->>Distributor: Sample
    else Stop requested or deadline reached
        Queue-->>Distributor: Stop waiting
    end
```

| Stage                                  | Empty-queue behavior                           |
| -------------------------------------- | ---------------------------------------------- |
| Parser: SYSTEM, IMU, Encoder           | Wait for Receiver notification or stop         |
| Parser: LiDAR                          | Poll; Receiver does not notify it              |
| Distributor: all streams               | Wait for Parser notification or stop           |
| SYSTEM Distributor: motor command loop | Wait for command-producer notification or stop |

## Initialization and execution

```mermaid
sequenceDiagram
    participant App as Application
    participant Init as Initialization
    participant Manager
    participant Receiver
    participant Parsers as Parser workers
    participant Distributors as Distributor workers

    App->>Init: Initialize with serial path
    Init->>Init: Create port, queues, transmitter, and processors
    Note right of Init: Port: 230400 bps, 8 data bits<br/>No parity, one stop bit
    Init->>Manager: Transfer ownership of resources and workers
    Init-->>App: Manager
    App->>Manager: Start processing
    Manager->>Receiver: Start worker
    Manager->>Parsers: Start each worker
    Manager->>Distributors: Start each worker
    Manager->>Receiver: Wait for thread to finish
```

## Host protocol

Frame exchanges and firmware recovery:
[sensor-echo Protocol](https://github.com/kei185/sensor-echo/blob/main/docs/protocol.md).

```mermaid
flowchart LR
    clear["SYSTEM Distributor<br/>Clear old system messages"] --> startup["Handshake, readiness,<br/>and time synchronization"]
    startup -->|Failure| clear
    startup -->|Success| enter["Wait for ENTER"]
    enter --> scan["Request scan start"]
    scan -->|Failure| clear
    scan -->|Success| drive["Wait for a motor command"]
    drive --> transmit["Send through Transmitter"]
    transmit --> drive
```

Startup retries continue until stopped. Command ACKs, READY, and TIME REPORT
each have a 3-second wait limit. Other system messages are consumed and logged;
STARTUP FAILED ends a READY or TIME REPORT wait with an error.

The motor command loop does not convert velocity, clamp values, or wait for a
motor ACK. The current main does not produce motor commands.

## Shutdown and limits

```mermaid
flowchart LR
    destructor["Manager destruction"] --> stop["Request stop on Receiver, Parser, and Distributor workers"]
    stop --> cancellable["Queue waits return on stop"]
    stop --> blocking["Serial read or ENTER wait<br/>May remain blocked"]
    cancellable --> returned["Processing ends"]
    blocking -->|After blocking call returns| returned
    returned --> join["Wait for worker threads before destruction completes"]
```

- Queues have no size limit or backpressure.
- Receiver I/O failures are logged and terminate the process.
- A SYSTEM Distributor startup retry does not reset the MCU or fully recover an active firmware session.
