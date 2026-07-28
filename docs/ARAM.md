# Host ARAM and ARQ compatibility

The host AR module currently provides a deterministic compatibility backend,
not cycle-accurate ARAM or complete ARQ emulation.

## Current behavior

- ARAM is represented by a bounded 16 MiB byte array.
- `ARStartDMA()` performs an immediate `memcpy()` on the calling thread.
- Both MRAM-to-ARAM and ARAM-to-MRAM transfers are supported.
- MRAM addresses may be strict console-memory addresses or live host-address
  tokens.
- The MRAM address, ARAM address, and length must all be 32-byte aligned.
- Strict console-memory and ARAM ranges are bounds-checked.
- DMA status is busy during the copy and idle before completion callbacks run.
- Registered AR callbacks and ARQ callbacks complete deterministically before
  the posting function returns.
- An ARQ callback receives a temporary generation-checked address token for its
  `ARQRequest`. The token is released immediately after the callback returns.
- Invalid transfers are rejected without copying, but completion is still
  delivered so compatibility builds do not deadlock.

Host-address tokens carry pointer identity but not allocation size. A live
token is validated and decoded, while the caller remains responsible for
ensuring that its allocation contains the requested transfer length. Strict
console-memory addresses are fully range-checked.

## Deliberate limitations

`ARQPostRequest()` is synchronous during this debugging phase. It records the
requested priority, but does not yet:

- defer work to a runtime service point;
- order high- and low-priority requests;
- split low-priority requests into chunks; or
- emulate DMA timing.

The next ARQ phase can add a deterministic queue serviced by the runtime poll
point. That preserves priority, chunking, and callback ordering without adding
an ARQ worker thread.
