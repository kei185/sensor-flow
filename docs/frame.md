# Frame Payload Parsing

Wire format, payload fields, and units:
[sensor-echo Frame Format](https://github.com/kei185/sensor-echo/blob/main/docs/frame.md).
This document covers host parsing. Worker flow: [architecture.md](architecture.md).

## Receiver validation

```mermaid
flowchart LR
    sof["Find frame start"] --> header["Read header"]
    header --> crc{"Valid CRC?"}
    crc -- No --> sof
    crc -- Yes --> payload["Read payload"]
    payload --> known{"Known stream?"}
    known -- No --> discard["Discard frame"]
    discard --> sof
    known -- Yes --> queue["Route to Parser queue"]
    queue --> sof
    sof -. I/O failure .-> error["Log and exit process"]
    header -. I/O failure .-> error
    payload -. I/O failure .-> error
```

## Parser behavior

| Parser | Payload handling |
| --- | --- |
| SYSTEM | Keep the message text and type; use the host parsing time |
| LiDAR | Decode every complete point; ignore trailing bytes of an incomplete point |
| IMU | Decode the first sample; ignore extra bytes |
| Encoder | Decode the first sample; ignore extra bytes |

A sensor payload too short for one point or sample produces no parsed data.
The Parser continues with the next frame.

### Timestamps

```mermaid
flowchart LR
    device["Device frame timestamp"] --> sensor["LiDAR / IMU / Encoder timestamp<br/>Copy unchanged"]
    now["Host clock when SYSTEM Parser runs"] --> system["System message timestamp<br/>Host ACK time for time sync"]
```

The SYSTEM timestamp is not captured directly by the Receiver's serial read.
Queue delay can affect it.

### LiDAR

```mermaid
flowchart LR
    packet["Complete points in one payload"] --> distances["All distances"] --> points["LidarPoint.point<br/>double values"]
    packet --> angles["First and last angle"] --> bounds["angleStart / angleEnd<br/>Degrees"]
    bounds --> span["span = end - start<br/>Add 360 if negative"]
    span --> step["angleDiff = span / (N - 1)<br/>N = 1: angleDiff = 0"]
```

Intermediate encoded angles are not retained. Reconstructing them from the
start angle and angle step, collecting a full rotation, and converting to Cartesian
coordinates are left to the Distributor or application.

### IMU

```mermaid
flowchart LR
    gyro["Gyroscope X / Y / Z"] --> rot["Imu.rot<br/>Raw angular velocity"]
    accel["Accelerometer X / Y / Z"] --> trans["Imu.trans<br/>Raw acceleration"]
```

The Parser does not apply sensitivity or integrate the readings.
The field names do not imply orientation or position.

### Encoder

```mermaid
flowchart LR
    left["Left count change"] --> leftScale["Convert to wheel turns"] --> leftOut["Encoder.left<br/>double"]
    right["Right count change"] --> rightScale["Convert to wheel turns"] --> rightOut["Encoder.right<br/>double"]
```

The Distributor or application can calculate speed from successive timestamps:

```text
elapsed_seconds = (timestamp_now - timestamp_previous) / 1000.0
wheel_rev_per_second = wheel_rotation_change / elapsed_seconds
wheel_metres_per_second = wheel_rev_per_second * wheel_circumference
```

The elapsed time must be positive.
