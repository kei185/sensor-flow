# Typed Transformation Pipelines

Design proposal, not the current implementation. Example stage types are
placeholders. Current queues and workers: [architecture.md](architecture.md).

## Stages

```mermaid
flowchart LR
    frames["Frame"] --> parser["LidarParser"]
    parser --> raw["LidarRaw"]
    raw --> converter["CoordinateConverter"]
    converter --> points["CartesianPoint"]
    points --> filter["NoiseFilter"]
    filter --> filtered["Filtered CartesianPoint"]
    filtered --> consumer["Plotter or recorder"]
```

Each stage has a declared input type and output type.

## Compile-time composition

The stage order is fixed at build time. Every pair of adjacent stages must pass
this check:

```mermaid
flowchart LR
    first["Stage A output type"] --> check{"Matches Stage B input type?"}
    check -- Yes --> connected["Connect the stages"]
    check -- No --> rejected["Reject at build time"]
```

## Execution models

```mermaid
flowchart TB
    subgraph combined["One worker per pipeline"]
        direction LR
        inOne["Input queue"] --> workerOne["Worker<br/>Stage 1 → Stage 2 → Stage 3"] --> outOne["Output queue"]
    end
    subgraph separate["One worker per stage"]
        direction LR
        inMany["Input queue"] --> first["Worker: Stage 1"]
        first --> middleOne["Queue"] --> second["Worker: Stage 2"]
        second --> middleTwo["Queue"] --> third["Worker: Stage 3"]
        third --> outMany["Output queue"]
    end
    combined ~~~ separate
```

| Model | Benefit | Cost |
| --- | --- | --- |
| One worker per pipeline | Fewer queue operations and synchronization points | A slow stage delays the whole pipeline |
| One worker per stage | Stages can run concurrently and buffer short bursts | More synchronization, buffer limits, and shutdown rules |

Start with one worker per pipeline. Add intermediate queues only when
measurement shows a need for independent stage execution.

## Integration with Worker

```mermaid
flowchart LR
    input["Frame queue"] --> parser
    subgraph worker["Worker owns Pipeline processor"]
        parser["LidarParser"] --> converter["CoordinateConverter"] --> filter["NoiseFilter"]
    end
    filter --> output["Typed output queue"] --> distributor["Distributor worker"]
```

## Runtime composition

Runtime composition allows stages to be added or removed while the application
runs. Connections must be checked at runtime, with extra data dispatch and memory
management costs. Use it only when runtime reconfiguration is required.
