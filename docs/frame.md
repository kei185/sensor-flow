# Frame Payload Parsing

`Receiver` decodes the serial header's eight-byte little-endian timestamp into
`Frame.timestamp`. LiDAR, IMU, and Encoder parsers preserve this value in their
parsed samples. System messages retain the host Unix-millisecond receive time
used by time synchronization.

## Ill-formed Payloads

Ill-formed sensor payloads that cannot contain a complete sample are ignored.
`parsePayload()` returns an empty vector, publishes no samples, and lets the
parser worker continue with the next frame. A malformed payload does not abort
the worker or produce a partially initialized sample.

| Parser | Payload layout | Handling |
|---|---|---|
| LiDAR | Four bytes per point: distance, then Q6 angle; both 16-bit little-endian | Fewer than four bytes produce no scan. Otherwise, parse complete points and ignore an incomplete trailing record. |
| IMU | Twelve bytes: six signed 16-bit little-endian axes | Fewer than twelve bytes produce no sample. |
| Encoder | Four bytes: left wheel, then right wheel; both signed 16-bit little-endian Q6 values | Fewer than four bytes produce no sample. |

IMU and Encoder decode one sample from the first twelve and four bytes,
respectively; any additional payload bytes are ignored. LiDAR retains complete
points even when one to three trailing bytes cannot form another point.

These rules apply to sensor payload parsing. At the receiver, an invalid header
CRC or an unrecognized frame type is skipped; an I/O read failure remains an
error.

## Encoder Decoding

The Encoder parser emits one `frame::Encoder` per complete payload. Bytes 0–1
populate `left`, and bytes 2–3 populate `right`. Decode each field as a signed
16-bit two's-complement Q6 value before dividing by `64.0f` to obtain wheel
rotations, and copy the frame's 64-bit timestamp.

For example, payload `20 03 D0 01` yields `left = 12.5` and `right = 7.25`.
Payload `E0 FC D0 01` yields `left = -12.5` and `right = 7.25`. The signed Q6
range is `-512` through `511.984375` rotations.
Encoder frames use wire type `0x03` and are routed to the existing Encoder frame
queue before parsing.
