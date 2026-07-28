# Fork security model

Input Leap transports keyboard, mouse, clipboard, and optional file-transfer
data. A peer accepted by the application can therefore observe sensitive input
and inject input on the controlled machine. Security defaults and release
packaging must treat network authentication as a primary boundary.

## Implemented controls

- TLS 1.2 is the minimum accepted protocol version. TLS 1.2 is restricted to
  forward-secret AEAD cipher suites; TLS 1.3 uses the OpenSSL defaults.
- TLS compression and renegotiation are disabled.
- New GUI profiles require mutual certificate authentication. Existing
  profiles retain their explicit setting to avoid silently changing deployed
  configurations.
- Certificate fingerprints are checked against the trusted peer databases.
- TLS retries are driven by OpenSSL read/write readiness without blocking the
  shared socket multiplexer.
- The server permits at most 64 simultaneous pre-authentication TLS handshakes.
  A secure handshake that does not complete within 10 seconds is closed.
- Packet frames with a zero length or a payload larger than 4 MiB are rejected.
  Once a framing error occurs, the filter stops consuming attacker-controlled
  bytes until the connection is closed.
- Transfer-v2 rejects unsafe or conflicting relative paths and symbolic links,
  stages received data below a random per-transfer directory, verifies every
  file with SHA-256, and never overwrites an existing destination. Manifests,
  chunks, entry counts, paths, and aggregate bytes are bounded. Protocol 1.6
  peers use the legacy single-file path capped at 256 MiB.
- Protocol strings are limited to 1 MiB and protocol integer lists to
  1,048,576 elements by the existing parser.

## Trust assumptions

- The first certificate fingerprint exchange must occur over a trusted
  out-of-band channel. Accepting an unverified first-use fingerprint permits a
  machine-in-the-middle attack.
- Anyone with access to a trusted peer certificate and its private key is
  treated as that peer.
- The local operating-system account, configuration directory, certificate
  files, and process memory are trusted.
- Clipboard and file synchronization deliberately transfer user data after
  authentication; TLS protects it in transit but not at either endpoint.

## Distribution requirements

- Ship supported OpenSSL runtime libraries with security updates applied.
- Do not weaken mutual authentication or TLS policy in branded Release builds.
- Publish the exact source corresponding to distributed GPLv2 binaries,
  including build scripts and local modifications.
- Document the package checksum and signing identity for every public release.

## Remaining hardening roadmap

- Replace manual trust-on-first-use with a guided pairing flow and a short
  authentication code displayed on both machines.
- Store private keys with OS credential facilities where available.
- Add fuzzing targets for packet framing, protocol parsing, clipboard
  unmarshalling, transfer manifests/frames, and configuration parsing.
- Add signed, reproducible installers and an authenticated update mechanism.
- Run dependency and static-analysis scans in CI on Windows, Linux, and macOS.
