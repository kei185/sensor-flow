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
        -Worker receiverWorker
        -map~Type, Worker~ parsers
        -map~Type, Worker~ distributors
        +run() expected~void, Error~
    }

    class Worker {
        +unique_ptr~Processor~ instance
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

    class Processor {
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
    Manager *-- Worker : owns workers
    Manager *-- Transmitter : owns
    Worker *-- Processor : owns instance
    Processor <|-- Receiver
    Processor <|-- Parser_T
    Processor <|-- DeviceController
    Processor <|-- Plotter_T
    Receiver --> Manager : borrows port and frame queues
    Parser_T --> Manager : borrows typed queues
    DeviceController --> Transmitter : borrows
    DeviceController --> DataStreams : borrows system queue
    Plotter_T --> DataStreams : borrows data queue
    Transmitter --> Manager : borrows port
```

`worker::Worker` owns one `processor::Processor` instance and its `std::jthread`. Receivers, parsers, and distributors inherit from `Processor` and implement `run(std::stop_token)`. The constructor accepts a `std::unique_ptr<Processor>`; `dispatch()` and `abort()` return `WORKER_DISPATCH_FAILED` if the instance is missing.

```cpp
worker::Worker receiver(std::make_unique<receiver::Receiver>(*port, *frameStreams));
std::map<frame::Type, worker::Worker> parsers;
std::map<frame::Type, worker::Worker> distributors;
```

## Data Flow

```mermaid
flowchart LR
    source[Serial device]
    io[(Manager::port)]
    receiver[Worker<br/>Receiver]
    frameType{Valid frame type}
    systemFrames[(SYSTEM Frame queue)]
    lidarFrames[(LIDAR Frame queue)]
    systemParser[Worker<br/>Parser of systemMessage]
    lidarParser[Worker<br/>Parser of LidarPoint]
    systemData[(systemMessage queue)]
    lidarData[(LidarPoint queue)]
    controller[Worker<br/>DeviceController]
    plotter[Worker<br/>Plotter of LidarPoint]
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
    participant Init as application::init
    participant ReceiverStage as Worker (Receiver)
    participant ParserStages as Workers (Parser)
    participant DistributorStages as Workers (Distributor)
    participant Manager

    App->>Init: init(path)
    Init->>Init: create Port, Transmitter, and queues
    Init->>ReceiverStage: Worker(make_unique of Receiver)
    Init->>ParserStages: worker(type, frame queue, data queue)
    Init->>DistributorStages: worker(type, data queue, ...)
    Init->>Manager: inject resources and workers
    Init-->>App: unique_ptr of Manager
    App->>Manager: run()
    Manager->>ReceiverStage: dispatch()
    Manager->>ParserStages: dispatch() each worker
    Manager->>DistributorStages: dispatch() each worker
```

`application::init()` constructs processors and workers, then passes the resources and workers into `Manager` through its constructor. Threads are started later by `Manager::run()` through `Worker::dispatch()`.

## Shutdown

`Worker::abort()` requests cancellation through the worker's `std::jthread`. The `jthread` destructor subsequently joins the thread. Every component must check its `std::stop_token` regularly and return after cancellation is requested.

## Current Limitations

- The queues are accessed from multiple threads, but `std::queue` is not thread-safe. A synchronized queue abstraction is still required.
- Empty queues are polled continuously, creating busy-wait loops. A condition variable or blocking queue would avoid unnecessary CPU use.
- `frameStreams` and the queues inside `DataStreams` are heap-allocated even though `Manager` has sole ownership. They could be stored directly unless stable indirection is required.
- Initialization is listed explicitly for every frame type. This is verbose, but keeps the mapping between frame type, parsed data type, and distributor visible.
