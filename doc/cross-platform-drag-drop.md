# macOS to Windows drag and drop

This fork extends the existing Input Leap drag-transfer path for drags that
start on macOS and cross onto a Windows screen.

## Supported payloads

- A Finder file is transferred with its original contents.
- A Finder folder is recursively transferred with its directory structure.
- PNG data published by an application is transferred as `Dragged Image.png`.
- TIFF image data is transferred as `Dragged Image.tiff`.
- An HTTP or HTTPS link is transferred as a Windows Internet Shortcut (`.url`).

Image bytes take precedence over a source URL when an application publishes
both. Unsupported URL schemes, embedded line breaks, and materialized image
payloads larger than 64 MiB are rejected.

When both peers negotiate protocol 1.7, files and folders use the transfer-v2
streaming protocol. It limits each data frame to 256 KiB, verifies every file
with SHA-256 before making it visible, permits at most 2,048 manifest entries,
and caps one transfer at 64 GiB. A peer using protocol 1.6 falls back to the
legacy in-memory path, which is limited to one file and 256 MiB.

The Windows receiver writes the item to the configured drop directory. If no
drop directory is configured, the Windows desktop is used. Existing files are
not overwritten; Input Leap adds ` (1)`, ` (2)`, and so on.

This implementation materializes a file on Windows. It does not yet synthesize
a native Windows OLE drag object for dropping directly into an arbitrary
application.

## User setup

1. Install the matching fork build on both machines.
2. In **Configure Server > Advanced server settings**, enable
   **Enable drag and drop for files, images, and links**.
3. Start the server and client normally with TLS enabled.
4. On macOS, click and hold a Finder file, browser image, or browser link.
5. Drag the pointer across the configured edge onto the Windows screen.
6. Release the mouse button and wait for the completion message in the log.
7. Find the resulting item on the Windows desktop or configured drop directory.

Both the macOS and Windows builds must contain this fork's changes. An older
macOS build only recognizes local Finder files.

Selecting several Finder items at once is not yet exposed by the macOS capture
adapter. Drag the containing folder to transfer several files in this beta.

## macOS build

The repository's existing `mac-build` GitHub Actions job produces x86_64,
Apple Silicon, and universal artifacts after the fork is pushed to GitHub.
For a local build, install Xcode, CMake, Ninja, Qt 6.6, and OpenSSL, then use
the macOS configuration in `.github/workflows/builds.yml`.

The application needs the normal macOS Accessibility and Input Monitoring
permissions to control input and observe the cross-screen drag.

## Acceptance checklist

- Drag a Finder file and verify its SHA-256 hash matches on Windows.
- Drag a PNG from Safari and Chrome and verify it opens correctly.
- Drag a link and verify the `.url` shortcut opens the expected HTTPS address.
- Repeat the same filename twice and verify the first file is not overwritten.
- Try a `javascript:` or `file:` URL and verify no shortcut is transferred.
- Verify a normal pointer switch without the left button held does not initiate
  a transfer.
