# Release acceptance checklist

Use this checklist on the exact installers that will be published. Record the
commit, installer SHA-256 values, operating-system versions, and pass/fail
result in the release notes.

## Build and provenance

- The release commit is tagged with an annotated Git tag.
- Windows x64 and macOS Apple Silicon installers are produced by CI from that
  tag.
- `SHA256SUMS.txt` is published beside every installer.
- The source archive for the tagged commit is available.
- Production installers are signed with the documented Windows and Apple
  signing identities.

## Connection regression

- Connect with TLS enabled and mutual certificate authentication.
- Reconnect both ways after restarting either application.
- Verify an invalid or untrusted certificate is rejected.
- Verify normal pointer switching on every configured edge.
- Verify keyboard shortcuts, modifier keys, clipboard text, and clipboard
  images.

## Drag-and-drop matrix

Run every row from macOS to Windows and from Windows to macOS:

- One file, including SHA-256 comparison.
- Several selected files.
- A folder containing nested folders and empty files.
- A browser image from Safari, Chrome, and Edge.
- An HTTP and HTTPS link.
- A repeated filename, verifying that the original is not overwritten.
- A filename containing spaces and non-ASCII characters.
- A transfer larger than 1 GiB.
- A rejected `javascript:` and `file:` URL.

## Transfer manager

- Progress, item name, byte count, speed, and direction update during transfer.
- Pause and resume continue without corrupting the destination.
- Cancel removes partial data and notifies the sender.
- Retry restarts a cancelled or failed outgoing transfer.
- Disconnect Wi-Fi during a large transfer, reconnect, and verify the transfer
  continues from the previously received byte offset.
- Completed history survives an application restart.
- **Show in folder** opens the actual committed destination.
- Completion and failure notifications appear once.

## Platform behavior

- A received item follows the native drag pointer into Finder on macOS.
- A received item follows the native drag pointer into Explorer or the target
  application on Windows.
- Accessibility and Input Monitoring denial produces an actionable message on
  macOS.
- Installing over a running Windows version requests that the application be
  closed instead of failing with an unexplained access-denied error.
