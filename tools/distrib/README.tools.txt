CONTENTS
--------

Debug       Contains the Debug build of tools.

Release     Contains the Release build of tools.


IMPORTANT NOTE
--------------

CEF/Chromium builds are created using the following host architectures:

- Linux:   x86-64 (Intel/AMD)
- Windows: x86-64 (Intel/AMD)
- MacOS:   ARM64 (Apple Silicon)

Binaries in this tools package must be run on the supported host OS/architecture
even in cases where the output targets a different architecture.

For example, files targeting a MacOS 64-bit (Intel) application must be created
on a MacOS ARM64 (Apple Silicon) host system using the MacOS 64-bit (Intel)
tools distribution.


USAGE
-----

Start with the required host system and the tools distribution that matches your
application's target OS/architecture and CEF version.

Custom V8 Snapshots

Custom startup snapshots [https://v8.dev/blog/custom-startup-snapshots] can be
used to speed up V8/JavaScript load time in the renderer process. With CEF this
works as follows:

1. Generate a single JavaScript file that contains custom startup data. For
   example, using https://github.com/atom/electron-link.

2. Execute the `run_mksnapshot` script to create a `v8_context_snapshot.bin`
   file containing the custom data in addition to the default V8 data.

   Example:
   % run_mksnapshot Release /path/to/snapshot.js

   Note that bin file names include an architecture component on MacOS
   (e.g. `v8_context_snapshot.[arm64|x86_64].bin`)

3. Replace the existing `v8_context_snapshot.bin` file in the installation
   folder or app bundle.

4. Run the application and verify in DevTools that the custom startup data
   exists. For example, electron-link adds a global `snapshotResult` object.

Please visit the CEF Website for additional usage information.

https://bitbucket.org/chromiumembedded/cef/
