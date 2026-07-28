# Performance measurement

Input Leap can collect low-overhead latency histograms for its input pipeline.
Collection is disabled by default and does not alter the network protocol.

## Enabling metrics

When running the command-line server or client directly, add:

```text
--enable-perf-metrics
```

To measure backend processes started by the Qt GUI, set the environment variable
before launching the GUI:

```powershell
$env:INPUTLEAP_PERF_METRICS = "1"
.\input-leap.exe
```

Stop Input Leap normally after the measurement interval. Each backend writes a
summary at `NOTE` log level during shutdown. The summary includes sample count,
average, minimum, p50, p95, p99, and maximum durations in microseconds.

## Reported stages

- `input-capture`: time spent processing a Windows keyboard, mouse button,
  motion, or wheel callback.
- `capture-to-server-dispatch`: time between creating an input event and the
  server event loop dispatching it. This exposes local queue pressure.
- `server-dispatch`: time spent routing and serializing an input event for the
  active client.
- `client-protocol`: time spent parsing and forwarding an input protocol
  message. Compressed mouse messages are counted even when they do not
  immediately cause injection.
- `input-injection`: time spent passing an input operation to the platform
  backend.

These are per-process stage measurements. They are intentionally not presented
as cross-machine end-to-end latency because independent machine clocks are not
guaranteed to be synchronized precisely enough. The first optimization work
should compare stage distributions before and after a change.

## Baseline procedure

1. Install the same Release build on the server and client.
2. Enable metrics on both machines and log at `NOTE` level or more verbose.
3. Connect normally and wait 30 seconds for startup activity to settle.
4. Record 60 seconds each of mouse motion, clicking, typing, and scrolling.
5. Stop both processes normally and save their performance summary lines.
6. Repeat three times and compare p50, p95, p99, CPU usage, and sample counts.

Do not compare Debug-build timings with Release-build timings.

## Implemented optimizations

### Event-driven TLS handshakes

TLS connect and accept retries now wait for the exact socket readiness requested
by OpenSSL (`WANT_READ` or `WANT_WRITE`). The previous path slept for 10 ms on
every retry and continued watching writable sockets while waiting for input,
which added deterministic setup latency and could cause needless wakeups.

Fatal TLS accepts also return directly instead of sleeping for one second on the
shared socket-multiplexer thread. This prevents a failed or malicious handshake
from stalling unrelated connections.

This change affects connection setup only. It does not change protocol framing,
TLS policy, input-event ordering, or steady-state wire compatibility.

### Stack-backed input serialization

`ProtocolUtil` now serializes messages up to 256 bytes into stack storage.
Normal key, button, wheel, and pointer messages therefore avoid one heap
allocation per event. Clipboard and file messages larger than this threshold
continue to use dynamically sized storage.

The benchmark is built with the test targets and can be repeated with:

```powershell
.\scripts\build-windows.ps1 -Configuration Release
.\scripts\benchmark-protocol.ps1 -Configuration Release -Runs 7 -Iterations 5000000
```

On the development machine (AMD Ryzen 7 7800X3D, MSVC 19.44, Release), the
median time for formatting a representative `DMMV` pointer message changed
from 59.02 ns to 35.63 ns per operation, a 39.6% reduction. Microbenchmark
results are machine-specific; use the same build, power plan, iteration count,
and machine for comparisons.
