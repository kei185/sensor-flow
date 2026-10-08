# Architecture

This document describes the current implementation, including ownership, worker lifecycle, and data flow.

## Class Diagram

```mermaid
%%{init: {"theme": "base", "themeVariables": {"darkMode": true, "background": "#1F2230", "primaryColor": "#303648", "primaryTextColor": "#E5E7EB", "primaryBorderColor": "#9CA3AF", "lineColor": "#E5E7EB", "textColor": "#E5E7EB"}}}%%
classDiagram
    direction LR

    class Manager {
        -io::Port port
        -map~Type, queue_Frame~ frameStreams
        -DataStreams dataStreams
        -unique_ptr~Transmitter~ transmitter
        -ReceiverWorker receiverWorker
        -map~Type, ParserWorker~ parsers
        -map~Type, DistributorWorker~ distributors
        +run() expected~void, Error~
        -initParsers(...)
        -initDistributors(...)
    }

    class Worker_T {
        <<template Component>>
        +unique_ptr~Component~ component
        +jthread thread
        +dispatch() expected~void, Error~
        +abort() expected~void, Error~
    }

    class DataStreams {
        +unique_ptr~queue_systemMessage~ system
        +unique_ptr~queue_LidarPoint~ lidar
    }

    class Receiver {
        +io::Port& port
        +map~Type, queue_Frame~& outQueues
        +run(stop_token)
    }

    class ParserBase {
        <<abstract>>
        +run(stop_token)*
    }

    class Parser_T {
        <<template T>>
        +Type type
        +queue_Frame& inQueue
        +queue_T& outQueue
        +run(stop_token)
        +parsePayload(Frame&) vector_T
    }

    class Distributor {
        <<abstract>>
        +run(stop_token)*
    }

    class DeviceController {
        +Type type
        +Transmitter& transmitter
        +queue_systemMessage& inQueue
        +run(stop_token)
    }

    class Plotter_T {
        <<template T>>
        +Type type
        +queue_T& inQueue
        +run(stop_token)
        +distributeStuff(T)
    }

    class Transmitter {
        +io::Port& port
        +transmit(OperationType) expected~void, Error~
        +request(stop_token, OperationType, systemQueue) expected~void, Error~
    }

    Manager *-- DataStreams : owns
    Manager *-- Worker_T : owns workers
    Manager *-- Transmitter : owns
    Worker_T *-- Receiver : ReceiverWorker
    Worker_T *-- ParserBase : ParserWorker
    Worker_T *-- Distributor : DistributorWorker
    ParserBase <|-- Parser_T
    Distributor <|-- DeviceController
    Distributor <|-- Plotter_T
    Receiver --> Manager : borrows port and frame queues
    Parser_T --> Manager : borrows typed queues
    DeviceController --> Transmitter : borrows
    DeviceController --> DataStreams : borrows system queue
    Plotter_T --> DataStreams : borrows data queue
    Transmitter --> Manager : borrows port
```

`Worker<Component, InitError>` owns one runnable component and its `std::jthread`. Every runnable component exposes `run(std::stop_token)`, allowing the worker to start any component without component-specific dispatch logic. `InitError` selects the error returned if the component is missing.

```cpp
using ReceiverWorker    = Worker<receiver::Receiver, Error::RECEIVER_INIT_FAILED>;
using ParserWorker      = Worker<parser::ParserBase, Error::PARSER_INIT_FAILED>;
using DistributorWorker = Worker<distributor::Distributor, Error::DISTRIBUTOR_INIT_FAILED>;
```

## Data Flow

```mermaid
flowchart LR
    source[Serial device]
    io[(Manager::port)]
    receiver[ReceiverWorker]
    frameType{Valid frame type}
    systemFrames[(SYSTEM Frame queue)]
    lidarFrames[(LIDAR Frame queue)]
    systemParser[ParserWorker<br/>Parser of systemMessage]
    lidarParser[ParserWorker<br/>Parser of LidarPoint]
    systemData[(systemMessage queue)]
    lidarData[(LidarPoint queue)]
    controller[DistributorWorker<br/>DeviceController]
    plotter[DistributorWorker<br/>Plotter of LidarPoint]
    transmitter[Transmitter]
    output[Console or plot output]

    source -->|incoming bytes| io
    io --> receiver
    receiver --> frameType
    frameType -->|Non-sensor messages: 0x04 to 0x08| systemFrames
    frameType -->|Sensor data: LIDAR 0x01| lidarFrames
    systemFrames --> systemParser --> systemData --> controller
    lidarFrames --> lidarParser --> lidarData --> plotter --> output
    controller -->|operation request| transmitter
    transmitter -->|command bytes| io

    linkStyle default stroke:#E5E7EB,stroke-width:2px;
```

The intended routing sends valid sensor data to its sensor-specific Frame queue and all non-sensor messages to the SYSTEM Frame queue. The currently defined non-sensor wire types are INITIALIZING (`0x04`), DEVICE_INFO (`0x05`), HEALTH_STATUS (`0x06`), READY (`0x07`), and STARTUP_FAILED (`0x08`). LIDAR (`0x01`) is the only sensor path with a queue and parser today; IMU (`0x02`) and Encoder (`0x03`) would need their own sensor paths when supported.

The SYSTEM routing shown above is not implemented yet. `Receiver` currently uses the wire type directly as the `frameStreams` key, so types `0x04` through `0x08` create queues without a parser instead of reaching the SYSTEM queue. This remains part of [issue #1](https://github.com/kei185/my-plot/issues/1).

`Manager` owns one `io::Port`. `Receiver` reads incoming bytes from its POSIX file descriptor, while `Transmitter` writes commands through the same port.

## Initialization and Execution

```mermaid
%%{init: {"theme": "base", "themeVariables": {"darkMode": true, "background": "#1F2230", "primaryColor": "#303648", "primaryTextColor": "#E5E7EB", "primaryBorderColor": "#9CA3AF", "lineColor": "#E5E7EB", "textColor": "#E5E7EB", "actorLineColor": "#E5E7EB", "signalColor": "#E5E7EB", "signalTextColor": "#E5E7EB"}}}%%
sequenceDiagram
    participant App
    participant Manager
    participant ReceiverWorker
    participant ParserWorkers
    participant DistributorWorkers

    App->>Manager: Manager(file)
    Manager->>Manager: create Transmitter
    Manager->>Manager: create one Frame queue per Type
    Manager->>ReceiverWorker: assign Receiver component
    Manager->>ParserWorkers: initParsers()
    Manager->>DistributorWorkers: initDistributors()
    App->>Manager: run()
    Manager->>ReceiverWorker: dispatch()
    Manager->>ParserWorkers: dispatch() each worker
    Manager->>DistributorWorkers: dispatch() each worker
```

`initParsers()` and `initDistributors()` construct components and assign them to workers. Threads are started later by `Manager::run()` through `Worker::dispatch()`.

## Shutdown

`Worker::abort()` requests cancellation through the worker's `std::jthread`. The `jthread` destructor subsequently joins the thread. Every component must check its `std::stop_token` regularly and return after cancellation is requested.

## Current Limitations

- The queues are accessed from multiple threads, but `std::queue` is not thread-safe. A synchronized queue abstraction is still required.
- Empty queues are polled continuously, creating busy-wait loops. A condition variable or blocking queue would avoid unnecessary CPU use.
- `frameStreams` and the queues inside `DataStreams` are heap-allocated even though `Manager` has sole ownership. They could be stored directly unless stable indirection is required.
- Initialization is listed explicitly for every frame type. This is verbose, but keeps the mapping between frame type, parsed data type, and distributor visible.
