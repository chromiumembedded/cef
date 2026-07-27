# CEF Installer

**Available starting with CEF version 151.1.**

The CEF Installer Library is part of the standard bootstrap binary and provides
automatic CEF version management for Windows applications using the
[bootstrap architecture](sandbox_setup.md). It downloads, installs, updates, and
uninstalls CEF from a shared installation directory so that multiple applications
can share a single managed copy of a CEF version (including libcef and all
associated DLLs and resources).

Installer behavior is optional. It activates only when a `CEF_INSTALLER_CONFIG`
resource is embedded in the client DLL or bootstrap executable. Without that
resource, the client DLL loads the libcef DLL in the usual way with no installer
involvement.

The authoritative reference is
[libcef_dll/bootstrap/installer/README.md](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md).
This document is a brief overview; follow the links below for full details.

## What It Does

- **Automatic resolution**: On startup, the bootstrap finds the best compatible
  CEF version already installed, or downloads and installs one from the CDN.
  Downloaded archives are verified against their hash and signed catalog before
  installation.
- **Shared installation**: Multiple apps share versioned CEF installations
  under a common directory (typically `%LocalAppData%\CEF\` for standard users
  or `%ProgramFiles%\CEF\` for admin installs).
- **Updates**: Apps can check for newer CEF versions in the background through
  the `RunInstaller` API, or manually via `/cef-update` (when enabled, see below).
- **Uninstall and cleanup**: Removing an app's registration automatically
  prunes CEF versions that no other app needs.
- **Version safety**: Revoked versions are excluded, and optional launch-health
  tracking rolls back versions that crash repeatedly.
- **Enterprise support**: Group Policy controls can restrict download sources,
  enforce offline mirrors, or disable external downloads entirely.

## Quick Start

### 1. Add an installer configuration resource

Embed a `CEF_INSTALLER_CONFIG` JSON resource in your client DLL (or bootstrap
`.exe`):

```json
{
  "appid": "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",
  "vmin": "151.1",
  "vmax": "",
  "abi_hash": "1234567890ABCDEF",
  "launch_health": "explicit"
}
```

Key fields:

| Field | Purpose |
|-------|---------|
| `appid` | Unique UUID for your application (never changes). |
| `vmin` | Minimum compatible CEF version. |
| `vmax` | Maximum compatible CEF version (empty = no upper bound). |
| `abi_hash` | Sandbox compatibility hash from `cef_version.h`. |
| `launch_health` | `"off"`, `"explicit"` (recommended), or `"exit_code"`. |

For the full field reference, see
[Configuration Fields](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#2-configuration-fields).

### 2. Build with the standard binary distribution (cefclient example)

The standard Windows binary distribution provides an opt-in CMake flag for
building cefclient with installer support:

```bash
cmake -G "Visual Studio 17" -A x64 -DUSE_INSTALLER=On ..
```

This Release-only configuration:
- Builds the bootstrap and sandbox libraries
- Embeds the `CEF_INSTALLER_CONFIG` resource in the client DLL
- Stages only the bootstrap `.exe`, client DLL, and `chrome_elf.dll`

Replace the sample application identity (`appid`) before production use. See
[Build Integration](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#build-integration)
for resource embedding details.

### 3. Sign your binaries

The bootstrap verifies that `chrome_elf.dll` and the `.exe` share the same
code-signing certificate. Both must be signed before deployment. CEF
installations are signed and verified separately using their own catalog and
certificate.

## Command-Line Usage

Run the bootstrap executable with installer flags:

```
MyApp.exe /cef-update                    # Check for and install updates
MyApp.exe /cef-update /cef-background    # Silent background update
MyApp.exe /cef-uninstall                 # Unregister and prune unused versions
```

These commands require `enable_explicit_modes: true` in the bootstrap's
embedded configuration. See
[Command-Line Options](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#command-line-options).

## Background Updates from Code

Client DLLs can trigger updates programmatically via the `RunInstaller` export:

```cpp
auto run_installer = reinterpret_cast<RunInstallerFunc>(
    GetProcAddress(GetModuleHandle(nullptr), "RunInstaller"));
const char* result = run_installer("update", config_json.c_str());
```

The result is a JSON string indicating success or failure. Available commands:
`install`, `update`, `uninstall`, `query`, and `launch_success`.

For the full API, result format, and extended configuration options, see
[Background Update API](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#background-update-api).

## Version Selection

The installer picks the best CEF version from multiple sources:

1. **Installed versions** in the shared directory (or an app-specific
   `install_path` if configured)
2. An optional **bundled version** shipped with the app (`bundled_cef_path`)
3. **CDN download** when no local version qualifies

Among qualifying candidates, newer wins; ties go to the installed copy. Revoked
versions are excluded from normal selection, and crash-disqualified versions
(when launch-health tracking is enabled) are deprioritized.

For the complete decision tree, see
[Version Selection](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#version-selection).

### Bundled CEF Modes

Applications can optionally ship CEF alongside their own binaries instead of
relying solely on the shared installation directory and CDN downloads.

- **`bundled_cef_path`**: Points to a full CEF distribution directory (with
  `cef_version.json`, `catalog.cat`, and `Release/libcef.dll`). The bundled
  version participates in normal version selection — it is compared against
  installed versions and the newer one wins. It is used in-place and never
  copied to the shared directory. See
  [Bundled Version Behavior](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#bundled-version-behavior).

- **`unchecked_cef_path`**: Points directly to a directory containing
  `libcef.dll`. When set, this path is checked first and bypasses all version,
  ABI, signature, and revocation checks — the application is fully responsible
  for the integrity of that DLL. If the DLL is absent, the installer falls
  through to normal resolution. See
  [Unchecked CEF Path](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#unchecked-cef-path).

## Further Reading

| Topic | Link |
|-------|------|
| Full installer reference | [README.md](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md) |
| Security model | [SECURITY.md](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/SECURITY.md) |
| Enterprise policy | [ADMIN_POLICY.md](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/ADMIN_POLICY.md) |
| Testing | [TESTING.md](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/TESTING.md) |
| Bootstrap/client integration | [sandbox_setup.md](sandbox_setup.md) |
