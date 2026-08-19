# macOS and Windows drag and drop

This fork extends the existing Input Leap drag-transfer path in both directions
between macOS and Windows.

## Supported payloads

- One or several files keep their original contents and names.
- Folders are recursively transferred with their directory structure.
- Names that differ only by case or collapse to the same Windows-safe name are
  kept as separate items with a numeric suffix before the file extension.
- Images published directly by an application are materialized as an image
  file. macOS accepts PNG and TIFF; Windows accepts PNG and DIB/BMP.
- HTTP or HTTPS links are materialized as `.url` shortcuts for Windows or
  portable HTML link files for macOS.
- Windows Unicode text drags are materialized as UTF-8 text files.
- Windows virtual-file drags, including attachments and generated files, are
  materialized from the OLE stream before transfer.

Image bytes take precedence over a source URL when an application publishes
both. Unsupported URL schemes, embedded line breaks, and materialized image
payloads larger than 64 MiB are rejected.

When both peers negotiate protocol 1.7, files and folders use the transfer-v2
streaming protocol. It limits each data frame to 256 KiB, verifies every file
with SHA-256 before making it visible, permits at most 2,048 manifest entries,
and caps one transfer at 64 GiB. A peer using protocol 1.6 falls back to the
legacy in-memory path, which is limited to one file and 256 MiB.

The receiver first stages and verifies each item, then hands the materialized
items to the native destination drag system at the pointer on macOS and
Windows. The configured drop directory is used as a safe fallback when a
destination application does not accept the native drop. Existing files are
not overwritten; Input Leap adds ` (1)`, ` (2)`, and so on.

## User setup

1. Install the matching fork build on both machines.
2. In **Configure Server > Advanced server settings**, enable
   **Enable drag and drop for files, images, and links**.
3. Start the server and client normally with TLS enabled.
4. Click and hold one or several supported items on either computer.
5. Drag the pointer across the configured edge onto the other screen.
6. Release the mouse button and wait for the completion message in the log.
7. Find the resulting item on the Windows desktop or configured drop directory.

Both the macOS and Windows builds must contain this fork's changes. Protocol
1.6 peers still receive one ordinary file through the legacy transfer path.

## macOS build

The repository's existing `mac-build` GitHub Actions job produces x86_64,
Apple Silicon, and universal artifacts after the fork is pushed to GitHub.
For a local build, install Xcode, CMake, Ninja, Qt 6.6, and OpenSSL, then use
the macOS configuration in `.github/workflows/builds.yml`.

The application needs the normal macOS Accessibility and Input Monitoring
permissions to control input and observe the cross-screen drag.

## Acceptance checklist

- Drag a Finder file and verify its SHA-256 hash matches on Windows.
- Drag several Finder files together and verify every item arrives.
- Drag Windows files, a folder, and several selected items to macOS.
- Drag a PNG from Safari and Chrome and verify it opens correctly.
- Drag a link in each direction and verify the HTML file opens the expected
  HTTPS address.
- Drag Windows text and a virtual attachment and verify their contents.
- Repeat the same filename twice and verify the first file is not overwritten.
- Try a `javascript:` or `file:` URL and verify no shortcut is transferred.
- Verify a normal pointer switch without the left button held does not initiate
  a transfer.
