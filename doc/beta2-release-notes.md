# Secure fork beta 2

This is a Windows x64 test build, not a production-signed release.

## Highlights

- TLS 1.2+ hardening, mutual certificate authentication for new profiles,
  bounded pre-authentication handshakes, and stricter packet parsing.
- macOS capture support for Finder items, PNG/TIFF image data, and HTTP(S)
  links, with safe Windows materialization.
- Transfer-v2 negotiation between matching fork peers with protocol 1.6
  fallback.
- Streaming 256 KiB chunks, recursive folder transfer, SHA-256 verification,
  per-transfer staging, cancellation cleanup, and non-overwriting commit.
- Performance metrics and a reproducible protocol serializer benchmark.

## Test status

The Windows Debug and Release builds pass 158 core unit tests, 26 integration
tests, and 60 Qt GUI tests. Live integration coverage includes 10 MiB
client-to-server and server-to-client transfer-v2 streams.

## Known limitations

- This package is not code-signed.
- The macOS side must be built from the matching source package and tested on
  macOS; a Windows build cannot produce or validate Apple binaries.
- Several selected Finder items are not captured together yet. Drag their
  containing folder.
- A network disconnect restarts a transfer. Persistent staging and resume
  offsets are implemented, but live reconnect negotiation is not.
- Windows receives the item in the Desktop or configured drop directory; it
  does not yet create a native OLE drop inside the application under the
  pointer.
- Progress/pause/resume/cancel controls are not exposed in the Qt GUI.
