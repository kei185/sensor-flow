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
| Encoder | Eight bytes: left wheel, then right wheel; both signed 32-bit little-endian raw encoder counts | Fewer than eight bytes produce no sample. |

IMU and Encoder decode one sample from the first twelve and eight bytes,
respectively; any additional payload bytes are ignored. LiDAR retains complete
points even when one to three trailing bytes cannot form another point.

These rules apply to sensor payload parsing. At the receiver, an invalid header
CRC or an unrecognized frame type is skipped; an I/O read failure remains an
error.

## Encoder Decoding

The Encoder parser emits one `frame::Encoder` per complete payload. Bytes 0–3
contain the left count, and bytes 4–7 contain the right count. Decode each field
as a signed 32-bit two's-complement integer, then divide by `2800.0` to obtain
the wheel rotation change. Store the results in `double` members `left` and
`right`, and copy the frame's 64-bit timestamp. Positive rotations indicate
forward wheel motion; negative rotations indicate reverse wheel motion.

For example, payload `3E 01 00 00 D5 FF FF FF` contains counts `318` and `-43`,
yielding `left = 318 / 2800.0` (about `0.113571429` rev) and
`right = -43 / 2800.0` (about `-0.015357143` rev).
The wire counts range from `-2147483648` through `2147483647` per sample.
The current firmware uses 2800 counts per wheel revolution and targets a
100 ms sampling period. Parsed values are rotations, not radians or rotations
per second. Consumers apply the wheel circumference and sampling period to
these rotation changes when velocity is needed.

The wire type is unchanged, but the payload size and units differ from the
former 4-byte Q6 wheel rotations. Update the firmware, parser, and consumers
together. Old 4-byte Encoder payloads are ignored as incomplete samples.
Encoder frames use wire type `0x03` and are routed to the existing Encoder frame
queue before parsing.
