# Transfer-v2 protocol

Transfer-v2 is this fork's bounded, streaming file-transfer extension. It is
negotiated as Input Leap protocol 1.7 and falls back to the existing 1.6 file
messages when the other peer does not support it.

## Wire flow

Each `DTR2` protocol message carries one `ILF2` frame:

1. `Manifest` describes every file and directory, its safe relative path, byte
   size, type, and SHA-256 digest.
2. `Chunk` carries up to 256 KiB for one entry at an exact byte offset.
3. `EntryComplete` causes the receiver to check the size and SHA-256 digest.
4. `TransferComplete` commits verified top-level items to the drop directory.
5. `Cancel` removes only that transfer's staging directory.

Peers negotiate live resume as Input Leap protocol 1.8. After every manifest,
the receiver returns a `ResumeState` containing the exact
persisted offset for each entry. The sender waits for that state before sending
chunks. If the connection is interrupted, the outgoing plan and transfer ID are
kept in memory; after reconnection the same manifest is offered again and the
sender continues from the receiver-confirmed offsets.

## Bounds and validation

- Transfer ID: 128-bit random value encoded as 32 lowercase hexadecimal digits.
- Manifest: at most 900 KiB and 2,048 entries.
- Relative path: at most 1,024 bytes; absolute paths, traversal components,
  alternate separators, control characters, duplicate paths, and file/parent
  conflicts are rejected.
- Chunk payload: at most 256 KiB and accepted only at the receiver's next exact
  offset.
- Aggregate payload: at most 64 GiB.
- Files: SHA-256 verified before commit.
- Symbolic links: rejected by the sender catalog and in receiver staging paths.
- Destination conflicts: never overwritten; a numeric suffix is added.

The transport remains inside Input Leap's mutually authenticated TLS channel.
The manifest hashes provide corruption detection and bind file contents to the
manifest, but do not replace TLS peer authentication.

## Implementation map

- `TransferManifest` owns validation and `ILT2` serialization.
- `TransferCatalog` recursively builds a manifest and source-file plan.
- `TransferFrame` validates and serializes `ILF2` frames.
- `TransferSender` streams a plan and publishes progress snapshots.
- `TransferReceiver` stages, resumes, verifies, and commits received entries.
- `Client`, `Server`, `ServerProxy`, and `ClientProxy1_6` connect `DTR2` frames
  to the existing event and socket layers.

## Current beta status

Working and covered by Windows unit/integration tests:

- protocol 1.7 negotiation with protocol 1.6 fallback;
- bidirectional client/server streaming;
- recursive folder manifests and materialization;
- cancellation and per-transfer cleanup;
- persistent partial data and engine-level resume offsets;
- SHA-256 verification and safe, non-overwriting commit.
- multi-item file and folder capture on macOS and Windows;
- Windows OLE capture for images, links, text, and virtual files;
- native macOS promised-file destination selection.
- native Windows OLE destination handoff at the pointer;
- live reconnect/resume negotiation;
- GUI progress, pause, cancel, retry, history, and completion notifications.

Still required for a finished cross-platform product:

- signed Windows and macOS release artifacts and platform acceptance testing.
