# CEF Installer Library

The CEF Installer Library provides automatic CEF version management for
applications using the bootstrap architecture. It handles downloading,
installing, updating, and uninstalling CEF from a shared installation
directory.

## Contents

- [Client Integration](#client-integration)
- [Command-Line Options](#command-line-options)
- [Background Update API](#background-update-api)
- [How It Works](#how-it-works)
  - [Automatic Installation](#automatic-installation)
  - [Uninstall execution and outcomes](#uninstall-execution-and-outcomes)
  - [Enterprise Policy](#enterprise-policy)
  - [Download Source Precedence and Robustness](#download-source-precedence-and-robustness)
  - [Shared Installation Directory](#shared-installation-directory)
  - [Version Selection](#version-selection)
  - [Unchecked CEF Path](#unchecked-cef-path)
- [Progress Notifications](#progress-notifications)
  - [Cancellation via Parent Window](#cancellation-via-parent-window)
  - [Parent Window Handle](#parent-window-handle-cross-process-security)
- [Exit Codes](#exit-codes)
- [Concurrent Access](#concurrent-access)
  - [Protecting a version that is in use](#protecting-a-version-that-is-in-use)
  - [Publishing changes safely](#publishing-changes-safely)
  - [Index recovery and existing destinations](#index-recovery-and-existing-destinations)
  - [Lock waits and failure behavior](#lock-waits-and-failure-behavior)
- [Version Pruning](#version-pruning)
- [Explicit Registration Retention](#explicit-registration-retention)
  - [Recommended workflow](#recommended-workflow)
  - [Eligible stores](#eligible-stores)
  - [Age and launch evidence](#age-and-launch-evidence)
  - [How retention runs](#how-retention-runs)
  - [Availability warning](#availability-warning)
- [Launch Health Tracking](#launch-health-tracking)
  - [Launch-state files](#launch-state-files)
  - [Mode Classification](#mode-classification)
  - [Rollback Behavior](#rollback-behavior)
  - [Cleanup Strategy](#cleanup-strategy)
  - [Stale File GC](#stale-file-gc)
  - [Pruning Protection](#pruning-protection)
  - [Post-Exit Pruning](#post-exit-pruning)
- [Security](#security)
  - [Code Signing](#code-signing)
  - [Config Security](#config-security)
  - [Enterprise Provisioning](#enterprise-provisioning)
  - [Standalone Mode Security](#standalone-mode-security)
  - [Sandbox Compatibility](#sandbox-compatibility)
- [Build Integration](#build-integration)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)

## Client Integration

### Prerequisites

The installer downloads and verifies signed Release builds of CEF. For
loading to succeed, your application **must**:

1. **Use a Release build of `bootstrap.exe`** — the installer downloads (or your
   application bundles) signed Release CEF binaries, and sandbox initialization
   is not compatible between Debug and Release binaries.
2. **Code sign `chrome_elf.dll`** (which ships next to the exe) with the **same
   certificate** as the exe. The bootstrap verifies that both binaries share the
   same primary thumbprint at startup.

### 1. Embed Installer Configuration

Add a `CEF_INSTALLER_CONFIG` resource to your client DLL's `.rc` file (or to the
bootstrap `.exe`'s `.rc` file — as a fallback when the client DLL has no config,
or for standalone mode; see [Standalone Mode
Security](#standalone-mode-security)):

```rc
CEF_INSTALLER_CONFIG RCDATA "installer_config.json"
```

Create `installer_config.json` with your application's requirements:

```json
{
  "appid": "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",
  "vmin": "151.1",
  "vmax": "",
  "abi_hash": "1234567890ABCDEF",
  "launch_health": "explicit"
}
```

For production builds, generate this resource from a template so `vmin` and
`abi_hash` stay synchronized with the bootstrap. See
[Build Integration](#build-integration).

### 2. Configuration Fields

| Field | Required | Description |
|-------|----------|-------------|
| `appid` | Yes | Unique UUID for your application. Never changes once assigned. |
| `vmin` | Yes | Minimum CEF version (e.g., `"151.1"` or `"151.2.1"`). |
| `vmax` | No | Maximum CEF version. Empty string means no upper bound. |
| `abi_hash` | No | Sandbox compatibility hash (16 hex chars). Must match `CEF_SANDBOX_COMPAT_HASH` from `cef_version.h`. Required for sandbox support. |
| `channel` | No | Manifest channel used for downloads: `""` (stable, default) or `"beta"`. Installed versions are channel-agnostic; see [Version Selection](#version-selection). |
| `launch_health` | No | Local launch-health mode: `"off"` (default), `"explicit"` (recommended), or `"exit_code"` (zero-code-change opt-in). |
| `enable_explicit_modes` | No | Enable explicit installer commands (`/cef-update`, `/cef-uninstall`, and `/cef-retention-*`) and standalone auto-install. Default `false`. Only read from the bootstrap `.exe`'s embedded resource. See [Standalone Mode Security](#standalone-mode-security). |
| `unchecked_cef_path` | No | Path to a directory containing `libcef.dll`, used as-is without version, ABI, or signature checks. Relative paths are resolved against the client DLL directory. Only read from the client DLL's embedded resource; ignored in the bootstrap resource. See [Unchecked CEF Path](#unchecked-cef-path). |
| `bundled_cef_path` | No | Path to a pre-extracted CEF distribution directory (must contain `cef_version.json`, `catalog.cat`, and `Release/libcef.dll`). Participates in version selection (newer wins, installed wins ties, revocation demotion). Relative paths are resolved against the client DLL directory. Only read from the client DLL's embedded resource and `RunInstaller` JSON; ignored in the bootstrap resource. See [Version Selection](#version-selection). |
| `install_path` | No | Exclusive installer-state namespace. Relative values are resolved against the client DLL directory. Only read from the client DLL's embedded resource; bootstrap resources cannot set it. `RunInstaller` JSON has an independent operation-specific field. |
| `cdn_urls` | No | One through three ordered HTTPS base URLs for the selected application config. Accepted from client and bootstrap resources; the selected source replaces rather than merges with fallback sources. When omitted, source resolution falls through. See [Download Source Precedence and Robustness](#download-source-precedence-and-robustness). |

Unrecognized configuration keys emit one warning per key and are otherwise
ignored. They never make an otherwise valid configuration fail, preserving
forward compatibility when one config is consumed by different bootstrap
versions.

**Version Clamping:** The `vmin` value is automatically clamped to the
bootstrap's compile-time API version. This prevents the installer from
downloading a CEF version older than what the bootstrap was built against,
which could cause compatibility issues. For example, if you specify
`vmin: "150.0"` but the bootstrap was built with CEF API version 15101, the
effective minimum becomes `"151.1"`. This is particularly relevant for
CMake/Bazel builds where `vmin` may be manually configured.

If clamping makes the range empty, configuration fails immediately with exit
code 100. For example, `vmin: "150.0", vmax: "151.0"` with an API 15101
bootstrap reports the configured minimum, effective minimum (`151.1`),
bootstrap API version, and configured maximum. It is not reported as the
transient no-matching-version error 103.

**Note:** CEF automatically loads resources (.pak files) and locales from
the `Release/` subdirectory where `libcef.dll` resides. No `CefSettings`
configuration is required when using the installer.

#### Maximum Version Guidance

Production configurations should generally cap `vmax` at the newest Chromium
milestone the application has tested. Major Chromium upgrades can require
application-specific compatibility work. For example, `"151.99"` accepts
versions through the literal, inclusive 151.99 bound while excluding 151.100
and Chromium 152 or later. Raise the ceiling deliberately after validating the
next milestone.

## Command-Line Options

When the bootstrap executable is run with these flags, it enters installer mode:

| Flag | Description |
|------|-------------|
| `/cef-update` | Check for and install CEF updates. |
| `/cef-uninstall` | Unregister application and prune unused CEF versions. |
| `/cef-retention-dry-run` | Report stale registrations and affected versions without modifying installer state. |
| `/cef-retention-apply` | Recompute and apply registration retention under the writer lock. |
| `/cef-max-age-days=<90..3650>` | Retention-only age threshold. Default: 180 days. |
| `/cef-forcecheck` | Force CDN check even if cache is fresh (use with `/cef-update`). |
| `/cef-background` | Run with below-normal process priority, no UI, and low-impact single-threaded extraction. For uninstall with a writable target, opt into asynchronous temp-child relaunch; the parent returns 109 after the child starts. |
| `/cef-headless` | Disable progress UI without changing extraction behavior. |
| `/cef-parent=<hwnd>` | Pumped parent window for progress notifications and supported asynchronous-uninstall lifecycle results. |

Examples:
```
MyApp.exe /cef-update                    # Check for updates
MyApp.exe /cef-update /cef-forcecheck    # Force update check
MyApp.exe /cef-update /cef-background    # Background update, no UI
MyApp.exe /cef-uninstall                 # Uninstall
MyApp.exe /cef-uninstall /cef-headless   # Silent uninstall
MyApp.exe /cef-uninstall /cef-background # Async; 109 means child started
```

In official builds, `/cef-update`, `/cef-uninstall`, and both
`/cef-retention-*` commands require `enable_explicit_modes: true` in the
bootstrap executable's embedded configuration. This requirement also applies
when a client DLL is present. See
[Standalone Mode Security](#standalone-mode-security) for configuration-source
and build-type behavior.

## Background Update API

Client DLLs can trigger background updates using the `RunInstaller` export:

```cpp
#include <windows.h>
#include "include/wrapper/cef_util_win.h"

// Function pointer type
typedef const char* (*RunInstallerFunc)(const char* command,
                                        const char* config_json);

void CheckForUpdates() {
  // Get export from bootstrap.exe
  HMODULE bootstrap = GetModuleHandle(nullptr);
  auto run_installer = reinterpret_cast<RunInstallerFunc>(
      GetProcAddress(bootstrap, "RunInstaller"));
  if (!run_installer) {
    return;  // Not running under bootstrap
  }

  // Read config from client DLL resource
  std::string config_json;
  if (!cef_util::ReadResourceData(GetCodeModuleHandle(),
                                  L"CEF_INSTALLER_CONFIG",
                                  &config_json)) {
    return;
  }

  // Run update check (returns JSON result)
  const char* result = run_installer("update", config_json.c_str());

  // Parse result JSON for success/error status
  // ...
}
```

Clients can also confirm launch health once CEF is up and rendering. This
decouples CEF health from the process exit code — an app-level crash *after*
confirmation does not penalize the CEF version. The call is lightweight (no
lock, no database) and needs no config: the sentinel path is a process-global
the bootstrap set before `RunWinMain`.

```cpp
void ConfirmLaunchHealth() {
  HMODULE bootstrap = GetModuleHandle(nullptr);
  auto run_installer = reinterpret_cast<RunInstallerFunc>(
      GetProcAddress(bootstrap, "RunInstaller"));
  if (!run_installer) {
    return;  // Not running under bootstrap
  }

  // No config needed — launch_success resolves the active sentinel itself.
  run_installer("launch_success", nullptr);
}
```

Call this after the app reaches a healthy steady state (e.g., a delay after
first render, or when playback/first-data-frame succeeds). See the cefclient
reference `StartLaunchHealthConfirmation` in `client_update_win.cc`.

### RunInstaller Commands

| Command | Description |
|---------|-------------|
| `"install"` | Install CEF if not present. |
| `"update"` | Check for and install updates. |
| `"uninstall"` | Unregister app and prune unused versions. |
| `"query"` | Query installed versions (no modifications). |
| `"retention_dry_run"` | Return a store-wide retention report without modifying installer state. |
| `"retention_apply"` | Recompute and apply store-wide retention. |
| `"launch_success"` | Confirm launch health — the current CEF version is working. Lightweight (no lock, no database). |

### Extended Configuration

When calling `RunInstaller`, you can include extended configuration fields in
the JSON:

```json
{
  "appid": "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",
  "vmin": "151.1",
  "vmax": "",
  "abi_hash": "1234567890ABCDEF",

  "cdn_urls": ["https://cef-builds.spotifycdn.com/"],
  "install_path": "",
  "bundled_cef_path": "",
  "certificate_thumbprint": "",
  "force_check": false,
  "show_progress_ui": true,
  "parent_window": "0",
  "local_download_path": ""
}
```

| Field | Default | Description |
|-------|---------|-------------|
| `cdn_urls` | *(omitted)* | One through three ordered HTTPS base URLs for this operation. When omitted, resolution falls through to the selected application config and then the hardcoded default. See [CDN file format](../../../tools/cefbuilds/README.md). |
| `install_path` | `""` | Exclusive custom CEF installation path. A safe existing read-only directory may be used by query/automatic startup; mutations require write access and never fall back. Empty uses the standard search. |
| `bundled_cef_path` | `""` | Path to pre-extracted bundled CEF directory. Used in-place (not copied to shared install dir). Also accepted in the client DLL's embedded `CEF_INSTALLER_CONFIG` resource (see [Configuration Fields](#2-configuration-fields)). See [Version Selection](#version-selection). |
| `certificate_thumbprint` | CEF certificate | Non-empty expected catalog-signer thumbprint. A trusted caller may select another Windows-valid signing certificate for legitimately re-signed distributions; use a distinct `install_path` and matching client-side load policy. |
| `force_check` | `false` | Force CDN version check even if cache is fresh. |
| `show_progress_ui` | `true` | Show progress dialog (false for silent operation). |
| `parent_window` | `"0"` | HWND as a base-10 unsigned string. A JSON number is accepted only when finite, non-negative, integral, exactly represented, and within `uintptr_t`. |
| `local_download_path` | `""` | Operation-local directory or UNC root with the CDN mirror layout. Normal hash, catalog, signature, metadata, and size validation still applies; this source does not persist revocation deltas. |

`cdn_urls` entries are at most 2048 bytes each. They must have an HTTPS scheme
and non-empty host, cannot contain userinfo, a query, fragment, newline, or
backslash, and are normalized to a trailing-slash path prefix. Supplied order
and duplicates are preserved. An empty array, more than three entries, a
wrong type, or an invalid URL is config error 100.

`RunInstaller` JSON applies only to that API call. Automatic startup resolves
CEF from an embedded configuration before `RunWinMain`; a later API call does
not retroactively change that selection. To rely entirely on `RunInstaller`, do
not embed `CEF_INSTALLER_CONFIG` in either the client DLL or bootstrap `.exe`.
See [Configuration Fields](#2-configuration-fields) for where individual fields
are accepted and [Config Security](#config-security) for application-config
source priority and fallback behavior.

### Result JSON Format

```json
{
  "success": true,
  "outcome": "committed",
  "libcef_path": "C:\\ProgramData\\CEF\\Versions\\151.1.0\\windows64\\Release\\libcef.dll",
  "installed_version": "151.1.0"
}
```

Or on error:
```json
{
  "success": false,
  "outcome": "failed",
  "error_code": 103,
  "error_name": "NO_MATCHING_VERSION",
  "error_message": "No compatible CEF version found"
}
```

`outcome` is always `committed`, `cleanup_deferred`, or `failed`.
`cleanup_deferred` is successful and includes a `warnings` string array naming
physical or auxiliary cleanup left for a later pass. Depending on the command,
this may include trash or quarantine reclamation, retention evidence cleanup,
deferred version removal, or retry-state cleanup. Check `retry_required` to
determine whether the same operation should be run again. Database and
version-index publication are hard commit requirements and therefore return
`failed`, never deferred cleanup. Failed results always contain numeric
`error_code`, symbolic `error_name`, and a diagnostic string. Successful
results omit all error fields. The returned pointer is thread-local and
remains valid until the next call on the same thread or until that thread
exits. Copy the JSON into caller-owned storage before another same-thread call
or before the calling thread exits if it must be retained.

There is intentionally no `RunInstaller("last_error")`: a process-global
query would race concurrent calls and duplicate the synchronous per-call and
startup results.

## How It Works

### Automatic Installation

The bootstrap's startup behavior depends on whether a client DLL is present.

**When a client DLL is present:**

1. Bootstrap loads client DLL with `DONT_RESOLVE_DLL_REFERENCES` (no code
   execution) and verifies its code signature
2. Reads config from client DLL resource (falls back to bootstrap resource only
   when the client resource is missing)
3. If no config found: installer is skipped, app launches normally
4. If `/cef-uninstall`: runs uninstall and exits
5. Resolves CEF — searches for a compatible version in the shared directory,
   downloads from CDN if needed (same logic as standalone auto-install). If
   `/cef-update` was specified, a CDN check is forced.
6. Populates the size-gated `cef_version_info_t` startup result
7. Loads the client DLL and calls `RunWinMain`
8. If CEF resolution failed: `libcef_path` is null and
   `installer_error_code`/`installer_error_message` describe the failure. The
   client tries adjacent fallback; cancellation (108) should exit gracefully
   with "restart to retry," while no fallback is a fatal error.

The appended startup fields require
`CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR`. Clients must check `size`
before reading them. Resolved success has a non-null path and code 0; installer
not configured has a null path, code 0, and null message; recoverable failure
has a null path, nonzero code, and UTF-8 message. All pointers are
bootstrap-owned process-lifetime storage and must not be freed. Crashpad keys
`bs-install-error`, `bs-install-msg`, `bs-install-vmin`, `bs-install-vmax`,
`bs-install-policy-denied`, and `bs-install-cancelled` supplement this
synchronous contract. For example, the installer-specific annotations for error
103 in representative cefclient Crashpad metadata look like this:

```json
{
  "bs-install-error": "103",
  "bs-install-msg": "No CDN version matches requirements (platform = windows64, version >= 151.1, ABI hash = 1671cc913eeb4ecf); no validated CDN manifest candidates were available",
  "bs-install-vmax": "unbounded",
  "bs-install-vmin": "151.1"
}
```

If installer resolution fails and the adjacent `libcef.dll` fallback is also
unavailable, cefclient additionally records a `LOG_FATAL` message. That field
is generated by the client and is not part of the bootstrap annotation contract.

**Standalone mode (no client DLL):**

1. Bootstrap reads config from its own embedded resource.
2. If no config found: installer is skipped, bootstrap fails (no client DLL to
   fall back to)
3. If `/cef-uninstall`: runs uninstall and exits
4. Resolves CEF — searches for a compatible version in the shared directory,
   downloads from CDN if needed. If `/cef-update`, forces a CDN check.
5. On success: exits (there is no client DLL to hand off to)

In official builds, `enable_explicit_modes` gates explicit commands and
standalone auto-install, but not normal startup resolution with a client DLL;
see [Standalone Mode Security](#standalone-mode-security).

### Uninstall execution and outcomes

**Requirement:** In official builds, the `/cef-uninstall` command requires
`enable_explicit_modes: true` in the bootstrap executable's embedded
configuration, whether or not a client DLL is present. See
[Standalone Mode Security](#standalone-mode-security). The
`RunInstaller("uninstall")` API remains available without this flag.

**Normal behavior:** When the executable is outside the selected writable
install directory, `/cef-uninstall` waits for the uninstall to finish. Exit 0
means the application registration and required version-index update were
committed. Some files may still be awaiting safe deletion; in that case the
operation still returns 0 and logs `cleanup_deferred` warnings. Database or
required index failures return the corresponding nonzero error instead.

`RunInstaller("uninstall")` always runs in the current process and reports the
same committed, deferred-cleanup, or failed distinction in JSON. See
[Result JSON Format](#result-json-format) for the general outcome contract.

**When uninstall continues in another process:** The CLI starts a temporary
child and returns 109 only when:

- `/cef-background` requests an asynchronous uninstall; or
- the running executable is inside the selected install directory and must exit
  before self-removal can continue.

Exit 109 means the child started and uninstall is still pending; it does not
mean uninstall completed. A caller that supplies a valid, continuously pumped
`/cef-parent` window receives a `relaunch_started` handoff and a correlated
`operation_result` after the child's result and progress UI are finalized. The
terminal result does not mean the child process has exited. Callers without a
supported window receive no asynchronous completion contract. Do not
automatically rerun `/cef-uninstall` after 109 or missing delivery; a later
invocation is a new operation, not a status query. Use non-background external
uninstall or `RunInstaller("uninstall")` when synchronous completion is
required.

The temporary file set contains the executable, required runtime DLLs,
optional `crash_reporter.cfg`, and validated `cef_uninstall_state.json`. It
also contains the already-authenticated client DLL under its actual basename
only when that DLL's resource supplied the selected application config. A
bootstrap-resource selection copies no client DLL.

**Failures and retries:** Before deciding whether a child is necessary, the
bootstrap safely compares the executable and target directory identities. If it
cannot determine their relationship, or if a required child cannot be prepared
or started, it returns 105 without uninstalling in the current process.

If no writable target is available, uninstall returns configuration error 100
without starting a child. The diagnostic explains whether no valid install
directory was found, the directory is not writable, or the configured path is
not a directory. This includes an exclusive custom or administrator store that
is readable but cannot be modified.

Registrations never expire automatically. After registration removal and the
required index update commit, later writer operations retry deferred `.trash/`
and orphan cleanup.

### Enterprise Policy

Managed deployments can disable the standard shared user store, select an
ordered HTTPS source list or trusted local/UNC mirror, or disable external
downloads completely. Policy is machine-wide, takes precedence over
operation-specific download sources, preserves offline provisioned/bundled
selection, and reports valid enforcement separately from malformed policy.
See [ADMIN_POLICY.md](ADMIN_POLICY.md) for the normative registry schema,
architecture view, provisioning procedures, revocation curation, and
administrator troubleshooting.

### Download Source Precedence and Robustness

For each operation, the installer selects one effective download source and
keeps that selection fixed. A source is a policy decision, local mirror, or
ordered URL list. An origin is one URL within an ordered list.

#### Source selection

| Priority | Source | Behavior |
|----------|--------|----------|
| 1 | Enterprise policy | Policy can select the hardcoded default CDN, an ordered HTTPS list, a local/UNC mirror, or disabled downloads; lower priorities are ignored. |
| 2 | Operation-local mirror | `/cef-download-path` or the equivalent operation setting uses local files without HTTP. |
| 3 | Operation URL list | `cdn_urls` supplied to `RunInstaller` provides one through three ordered HTTPS origins. |
| 4 | Selected application URL list | Uses `cdn_urls` from the application config selected by [normal config precedence](#config-security). |
| 5 | Hardcoded CEF CDN | Used when no higher-priority source is configured. |

Client-resource and bootstrap-resource configs are alternative application
sources. Normal config precedence selects one; their URL lists are never
combined.

#### Origin failover and version fallback

For a URL source, the revocation list and manifest independently try each
origin in order. After selecting a version, the installer keeps that version's
`<archive>.sha256` sidecar and archive on the same configured origin. If the
sidecar is missing, it verifies the archive with the SHA1 from the validated
manifest. Every origin is tried for that version before version fallback is
considered.

The installer selects at most one next-best compatible, non-revoked version
from the same validated manifest when:

- every URL origin reports that the selected archive is missing;
- the effective local or policy mirror lacks the archive; or
- candidate-specific hash, extraction, signature, catalog, or metadata
  verification fails after allowed recovery.

It does not change versions after cancellation, an outage across all origins,
policy denial, or a local write, permission, staging, or publication failure.
Those failures are terminal for the operation.

#### Resuming interrupted HTTP downloads

Only archive downloads are resumable. A partial is reusable only while both
its configured origin and its final origin after redirects still match. A safe
non-empty partial sends `Range: bytes=N-`. The installer appends only after a
valid `206` response whose `Content-Range` begins at N. If the server returns
`200`, the installer safely restarts from zero using that response without an
extra request.

A configured- or final-origin change deletes the old partial before new
response bytes are written. Each version candidate permits at most one
additional full request without `Range`, shared across all origins. That
request may recover from a final-origin change that needs another response,
invalid Range protocol including `416`, a completed-response length
inconsistency, or a hash mismatch.

An interruption, timeout, or truncated transport may retain a matching safe
partial. Moving to another configured origin deletes it first. Cancellation
ends the current operation but may preserve a safe partial for a later attempt.
Local and UNC mirrors always copy from zero and never use HTTP Range.

#### Cache and cleanup

Each complete or partial archive is limited to 500 MB. A complete archive is
published to the cache only after length and SHA256/SHA1 validation. It is
removed after the checked version index and required application registration
have been published successfully.

Extraction, signature, metadata, and publication failures before commit retain
the archive for retry. Failure to delete it after commit reports successful
`cleanup_deferred`. Installer-owned complete archives and structurally valid
origin-bound partials are pruned after seven days; unrelated files are not
matched. There is no aggregate byte or LRU quota.

### Shared Installation Directory

CEF uses a shared directory for installed versions. The installer resolves
directories using a single priority-ordered search that collects **readable**
directories and stops at the first **writable** one.

**Candidate locations** (searched in priority order):
1. `HKLM\SOFTWARE\CEF\InstallLocation` registry value (requires admin to set)
2. `%ProgramFiles%\CEF\`
3. `%LocalAppData%\CEF\`

An `install_path` embedded in the client DLL is an explicit per-application
override; the operation-specific `RunInstaller` field has the same directory
semantics. Either takes precedence over the standard search, and only that
directory is used. An existing safe readable but non-writable directory
remains the sole read-only store. A writable directory is the sole read/write
store. Invalid, unsafe, inaccessible, or missing read-only paths fail without
default-search fallback. Query never creates or write-probes the custom
directory.

`HKLM\SOFTWARE\CEF\InstallLocation` is a machine-wide default candidate, not a
mandatory enterprise-policy location. It participates in the same ordered
search and source-derived role rules as Program Files and LocalAppData.
Mandatory policy values live under `HKLM\SOFTWARE\Policies\CEF`; see
[ADMIN_POLICY.md](ADMIN_POLICY.md).

`install_path` must be distinct from every resolved standard candidate. An
exact, normalized, or safe same-directory alias of the HKLM, Program Files, or
LocalAppData candidate is configuration error 100 and never falls through.
Omit `install_path` when consuming a standard store. On 64-bit Windows the
HKLM provisioning value uses the shared 64-bit registry view for every process
architecture; see [ADMIN_POLICY.md](ADMIN_POLICY.md).

**The algorithm:** For each candidate in priority order, the installer checks
if the directory is readable (add to scan list) and writable (use for
installs). The search **stops at the first writable directory**. This means
readable directories never include locations with lower security priority
than the writable directory.

| Scenario | Readable dirs (in search order) | Writable dir | Notes |
|----------|---------------------------------|--------------|-------|
| **Admin**, no registry | ProgramFiles | ProgramFiles | LocalAppData never checked |
| **Standard user**, ProgramFiles exists | ProgramFiles, LocalAppData | LocalAppData | Sees admin-installed versions |
| **Admin**, HKLM registry set | Registry path | Registry path | Exclusive — naturally falls out |
| **Standard user**, HKLM registry set | Registry, ProgramFiles, LocalAppData | LocalAppData | Reads from all admin locations |
| **Official elevated automatic startup, `enable_explicit_modes: false`, no registry** | ProgramFiles | none | ProgramFiles is read-only; LocalAppData is never checked |
| **Official elevated automatic startup, `enable_explicit_modes: false`, HKLM registry set** | Registry path, ProgramFiles | none | Both locations are read-only; LocalAppData is never checked |
| **Official elevated automatic startup, `enable_explicit_modes: true`, no registry** | ProgramFiles | ProgramFiles | Stops at ProgramFiles |
| **Official elevated automatic startup, `enable_explicit_modes: true`, HKLM registry set** | Registry path | Registry path | Stops at the first writable location |
| **`install_path` writable** | install_path | install_path | Exclusive custom role, regardless of path spelling |
| **`install_path` read-only** | install_path | none | Exclusive selection; unregistered and untracked |

**Security:** Directory roles come from candidate source, never ACLs or path
prefixes. Elevated operations do not search `%LocalAppData%`, which prevents a
privileged process from trusting content in a user-writable directory. In the
official automatic-startup rows, `enable_explicit_modes` is a trusted bootstrap
setting, not enterprise policy or a directory permission. This restriction is
specific to automatic startup: an elevated client may still use
`RunInstaller("install")` or `RunInstaller("update")` to write to the first
writable administrator location when the setting is false. Non-official builds
also allow administrator mutation for development and testing. See
[Background Update API](#background-update-api).

If no writable directory is available but a compatible version already exists
in a readable directory, the installer returns that version instead of failing.
This covers the common case where an admin installed CEF to `%ProgramFiles%`
and a non-elevated app simply needs to use it.

**Directory permissions:** The installer does not set or modify ACLs on the
install directory. It relies on inherited permissions from the parent directory,
which are already correct for the standard locations (`%ProgramFiles%` restricts
write access to administrators; `%LocalAppData%` restricts access to the owning
user). When using `HKLM\...\InstallLocation` or `install_path` to configure a
custom location, the administrator or app-specific installer with admin
permissions is responsible for setting appropriate permissions on that
directory before use. An `install_path` store is application-owned by
contract regardless of path spelling, ACLs, writability, or elevation, and is
eligible for explicit registration retention. Durable administrator
provisioning uses the source-derived HKLM role (which may point anywhere) or
Program Files; those roles are never eligible for user-liveness retention.

Directory structure:
```
CEF/
├── installer.json              # Database of registered apps
├── versions.json               # Version index for lock-free reads
├── revocation_cache.json       # Cached CDN revocation delta
├── cef_installer.log           # Installer log file
├── .cache/                     # Manifests (1-hour TTL); validated/partial archives (7-day TTL)
├── .launch/                    # Per-app launch health sentinels (survive pruning)
├── .staging/                   # Temp extraction (same-volume for atomic rename)
├── .trash/                     # Pending deletions from in-use versions
└── Versions/
    ├── 151.1.0/
    │   └── windows64/          # Platform: windows64, windows32, or windowsarm64
    │       ├── cef_version.json    # Version metadata (from build, signed in catalog)
    │       ├── catalog.cat         # Code signing catalog
    │       ├── CREDITS.html
    │       ├── LICENSE.txt
    │       ├── README.txt
    │       └── Release/            # Binaries and resources
    │           ├── libcef.dll
    │           ├── chrome_elf.dll
    │           ├── chrome_100_percent.pak
    │           ├── resources.pak
    │           ├── icudtl.dat
    │           ├── locales/
    │           └── ...
    └── 151.2.0/
        └── windows64/
            └── ...
```

The platform subdirectory (`windows64`, `windows32`, or `windowsarm64`) allows
different architectures to coexist within the same version directory.

### Version Selection

The installer considers two sources of CEF versions: **installed versions**
from the [selected installer-managed directory](#shared-installation-directory),
and an optional **bundled version** shipped with the application via
`bundled_cef_path`.

`unchecked_cef_path` is resolved before version selection and bypasses this
process entirely; see [Unchecked CEF Path](#unchecked-cef-path).

#### Compatibility Filtering

Both installed and bundled versions must pass these checks:
- Version >= `vmin` and <= `vmax` (if specified)
- Matching `abi_hash` (if specified, required for sandbox)
- Matching platform (e.g., `windows64`)

Installed versions are signature-verified at download time (catalog +
certificate thumbprint). For bundled versions, startup verifies only that
`catalog.cat` exists as a regular non-reparse file. That is a
distribution-layout check: the catalog signature and member hashes are **not**
verified at runtime, because hashing a typical distribution (~236 files, ~370
MB) would add seconds to every app startup. Bundled integrity rests on the app
developer's signed installer/package.

`channel` selects what the installer downloads, not what it runs. It chooses
the stable or beta manifest URL for a network or mirror check, but channel is
not recorded in installed metadata and does not filter installed candidates.
Version range, ABI hash, platform, revocation, and launch-health state define
runtime eligibility; consequently a stable-channel app may use a compatible
version previously installed through a beta-channel request. To avoid adopting
a beta version from a newer milestone in a shared store, follow the
[maximum-version guidance](#maximum-version-guidance) and cap `vmax` at the
approved stable milestone. This limits eligible versions but does not identify
their source: strict stable/beta isolation requires separate `install_path`
stores.

#### Selection Decision Tree

This is the single source of truth for resolution order. The first matching
branch wins; later sections describe the individual mechanisms in more detail.

```text
unchecked_cef_path set and libcef.dll present there?
├─ yes → use it as-is (no version / abi / signature / revocation checks). Done.
└─ no  → continue

Build candidate sets, each filtered by vmin/vmax, abi_hash, and platform:
  • installed: shared-dir versions, EXCLUDING revoked AND crash-disqualified
  • bundled:   the bundled_cef_path version, if configured

A. Primary selection — does a qualifying candidate exist?
   ├─ installed and (non-revoked) bundled both qualify
   │     → newer wins; on a tie the installed version wins (it passed the
   │       installer's download-time catalog verification). Done.
   ├─ exactly one qualifies → use it. Done.
   └─ none qualify          → go to B

B. CDN download (skips installed + crash-disqualified versions)
   ├─ best compatible, non-revoked build installs → Done.
   ├─ candidate-specific absence/content failure
   │     → try one next-best entry from the same manifest; success → Done.
   └─ no match / revoked / terminal failure       → go to C

C. Last-resort fallback (a demoted version beats a broken app)
   ├─ a revoked bundled version exists                → use it. Done.
   ├─ else newest non-revoked crash-disqualified
   │       installed version exists                   → use it. Done.
   └─ else                                            → fail (no version)
```

The mechanisms used by the decision tree are detailed below:
[Bundled Version Behavior](#bundled-version-behavior),
[Revocation](#revocation), [Unchecked CEF Path](#unchecked-cef-path), and
[Launch Health Tracking](#launch-health-tracking) (crash-disqualification).

#### Bundled Version Behavior

When `bundled_cef_path` is specified (via the client DLL's embedded
`CEF_INSTALLER_CONFIG` resource or `RunInstaller` JSON):

- The bundled directory must contain `Release/libcef.dll`, `cef_version.json`,
  and the package's signed `catalog.cat`. Runtime checks only that the catalog
  is present as a regular non-reparse file; signature/member verification is the
  packaging pipeline's responsibility.
- The bundled version is **used in-place** — it is never copied to the shared
  install directory. This is intentional: CDN-installed versions have an
  additional installer-enforced catalog-verification step.
- Bundled distributions use the revocation behavior summarized below. The
  [decision tree](#selection-decision-tree) defines their exact ordering
  relative to installed and downloaded versions.

#### Revocation

The CDN publishes a revocation list (`revoked.json`) of versions with known
security issues. Revoked versions are handled differently depending on source:

| Source | Revocation behavior |
|--------|-------------------|
| **Installed** | Always excluded — never selected on any branch of the [decision tree](#selection-decision-tree), including the last resort. Pruned on next install/update. |
| **CDN** | Blocked from download even if CDN still lists them (caching delay). |
| **Bundled** | Demoted, not excluded — drops out of primary selection but survives to the last resort (branch C). |

#### Query Command

The `query` command uses the same selection logic (installed vs. bundled, newer
wins, installed wins ties) and does not contact the CDN. It reads all readable
directories' valid `versions.json` indexes and does not require a writable
directory. A missing or corrupt index is not replaced by a directory scan on
the query path; query remains index-only and recovery belongs to automatic
startup or the next writer-locked mutation. Revocation is checked using the
compiled-in baseline and any disk-cached CDN delta (no network required).
Revoked bundled versions are demoted, matching install/update behavior. It is a
lightweight, offline-capable operation.

#### Install, Update, and Read-only Behavior

| Operation | Local behavior | Version-manifest check | Possible writes |
|-----------|----------------|------------------------|-----------------|
| **Automatic startup** | Uses the best compatible candidate | Only after a local miss | Install, register, and prune when writable |
| **`install`** | Uses the best compatible candidate | Only after a local miss | Install, register, and prune |
| **`update`** | Keeps the best candidate as a fallback | Always; cached or remote as permitted | Install, register, and prune |
| **`query` and read-only resolution** | Reports the best local candidate | Never | None |

For `update`, a manifest entry that is the same, older, unavailable, or revoked
leaves the local result unchanged and returns a successful no-op. `force_check`
bypasses cache freshness but does not force a download.

**What all operations validate:** Every operation uses the same side-effect-free
offline selector. It receives installed candidates in directory-priority order,
optional bundled metadata, compatibility requirements, effective revocations,
and optional launch-health disqualifications. For duplicate `(version,
platform)` entries, the first valid candidate from the highest-priority
directory wins.

Before selection, each indexed installed candidate is checked against its
per-version metadata, required catalog and `libcef.dll` files, and reparse-safe
canonical path. These checks add filesystem work for each indexed candidate but
do not hash the distribution or reverify its signature. Launch-health
disqualification applies only to installed candidates; the app-shipped bundled
candidate remains eligible as a fallback. Results retain whether the selected
version was installed or bundled and whether it represents a rollback.

**Read-only guarantees:** Query and read-only resolution do not repair indexes,
reconcile state, register applications, prune versions, reclaim trash, or write
caches. Bundled CEF remains eligible when no writable directory exists.

**Startup concurrency:** Automatic startup performs offline selection before
acquiring the writer mutex. When it selects an installed version, it holds a
no-delete handle on `Release/libcef.dll` through `RunWinMain`, preventing a
concurrent prune from moving that version. If removal wins the race, startup
rereads the indexes and selects again.

**Emergency startup recovery:** If normal automatic startup finds no usable
indexed or bundled candidate, it may scan roots whose index is confirmed missing
or corrupt. A valid index—including a valid empty index—remains authoritative
and is never supplemented by scanning. An integrity failure does not delete the
damaged index; an inconclusive I/O failure fails closed without scanning.

The recovery scan examines at most four roots and 128 version directories, with
a soft 500 ms elapsed-time budget because synchronous Windows filesystem calls
cannot be interrupted. Every discovered candidate passes the normal metadata,
distribution, path, platform, ABI, range, revocation, launch-health, and lease
checks. Recovery does not acquire the writer mutex, access the network, modify
state, or repair the index. `query` never uses this recovery path.

Permanent repair occurs the next time a writable `install`, `update`, or
`uninstall` operation acquires the writer mutex—possibly during the same
automatic startup if the emergency scan finds no usable candidate. The writer
rescans and validates the on-disk version directories, then automatically
rebuilds, atomically writes, and rereads `versions.json`. Invalid version
directories are omitted. If the replacement index cannot be written,
atomically published, reread, or validated, the writer operation stops with a
recovery error.

**Revocation refresh:** Effective revocations combine the compiled baseline with
every valid cache in the readable-directory set. A successful additive refresh
is cached only in the selected writable directory. A recent writable-store cache
avoids HTTP only when both its integrity footer and JSON payload validate.

When that cache is stale or invalid and startup has a usable local candidate,
startup makes one synchronous refresh attempt across the effective ordered
`cdn_urls` list. It tries each origin until one returns a valid `revoked.json`
or the shared 5-second wall-clock deadline expires; a slow earlier origin can
therefore consume the budget before a later origin is reached. Each request has
3-second connect/resolve and 2-second receive/response limits. Internally, the
launch-sensitive request uses asynchronous WinHTTP, tries one address per
origin, and cancels at the shared deadline so DNS, connection, headers, and body
do not multiply the budget. If no origin succeeds, startup uses the compiled
baseline plus valid cached revocations and records an integrity-protected,
source-specific 15-minute backoff.

#### Example Scenarios

**CDN builds nightly, app ships on a 2-week cycle:**

| Installed | Bundled | Bundled revoked? | CDN available? | Result |
|-----------|---------|-----------------|----------------|--------|
| v151.1 | v151.2 | No | — | Bundled v151.2 (newer) |
| v151.2 | v151.1 | No | — | Installed v151.2 (newer) |
| v151.2 | v151.2 | No | — | Installed v151.2 (tie → installed wins) |
| v151.1 | v151.2 | Yes | — | Installed v151.1 (revoked bundled demoted) |
| — | v151.2 | No | — | Bundled v151.2 |
| — | v151.2 | Yes | v151.3 ✓ | CDN v151.3 |
| — | v151.2 | Yes | fails | Bundled v151.2 (last resort) |
| — | — | — | v151.3 ✓ | CDN v151.3 |
| — | — | — | fails | Error |

### Unchecked CEF Path

The `unchecked_cef_path` config field provides a fast path for applications that
bundle `libcef.dll` alongside their own binaries and want to skip the installer
entirely.

When set, the installer checks for `libcef.dll` directly in the specified
directory **before** any other resolution (shared directory scan, CDN download,
bundled version comparison). No version range, ABI hash, platform, signature,
or revocation checks are performed — the application is responsible for the
integrity of the DLL at this path.

```json
{
  "appid": "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",
  "vmin": "151.1",
  "abi_hash": "1234567890ABCDEF",
  "unchecked_cef_path": "."
}
```

**Caution with `"."`:** This setting places an unchecked `libcef.dll` beside the
client DLL and makes it the intentional first choice, ahead of installer-managed
or bundled versions. That adjacent DLL can be dangerous for a client that does
not strictly control load ordering. The client must explicitly load the exact
`libcef_path` returned by the bootstrap, as the cefclient reference
implementation does, **before calling any CEF function**. Because `libcef.dll`
is delay-loaded, a premature CEF call can implicitly load the adjacent DLL by
name. Without an adjacent DLL, the same bug would normally fail visibly instead
of silently loading an unintended copy. Use `"."` only when this override is
intentional, its directory is controlled, and the client enforces that ordering.

**Key behaviors:**

- **Relative paths** are resolved against the directory containing the client
  DLL that provided the config. `"."` means "libcef.dll is next to the client
  DLL."
- **Client DLL only:** The field is only read from the client DLL's embedded
  `CEF_INSTALLER_CONFIG` resource. It is ignored in the bootstrap `.exe`'s
  resource. A key supplied in `RunInstaller` JSON is ignored and does not
  enable the bootstrap startup fast path.
- **Fallback:** If `libcef.dll` is not found at the specified path, the
  installer falls through to the normal resolution flow (shared directory, CDN
  download).
- **Uninstall:** The field is ignored for `/cef-uninstall` — uninstall always
  proceeds through the normal database and pruning flow.

**Contrast with `bundled_cef_path`:** `bundled_cef_path` (accepted from the
client DLL's embedded `CEF_INSTALLER_CONFIG` resource and `RunInstaller` JSON)
points to a full CEF distribution with `cef_version.json`, `catalog.cat`, and a
`Release/` subdirectory. It participates in version selection (newer wins,
installed wins ties) and revocation demotion. `unchecked_cef_path` is simpler —
it points directly at the directory containing `libcef.dll` with no metadata
requirements and no version comparison.

**Contrast with no `CEF_INSTALLER_CONFIG` resource:** When neither the client
DLL nor bootstrap executable provides an installer config, the bootstrap skips
CEF resolution and validation entirely and reports “installer not configured”
to the client (null path, error 0). The client then decides how to locate CEF;
for example, cefclient falls back to an adjacent `libcef.dll`.
`unchecked_cef_path` is different: it explicitly asks the bootstrap to return a
particular `libcef.dll`, and if that file is absent, the still-configured
installer continues with normal installed, bundled, or CDN resolution.

## Progress Notifications

When `parent_window` is set in the extended config, progress updates are sent
via `WM_COPYDATA`:

```cpp
// COPYDATASTRUCT.dwData = 0x43454649 ('CEFI')
// COPYDATASTRUCT.lpData = UTF-8 JSON string:
{
  "step_name": "downloading",  // Also "checking", "extracting", "verifying", "installing", "committing", "cleaning"
  "step": 4,
  "total_steps": 9,
  "bytes_done": 1048576,
  "bytes_total": 10485760,
  "message": "Downloading..."
}
```

`step` is zero-based, and `total_steps` is the final step index (`cleaning`,
currently 9), not a step count. A notification with `step == total_steps` means
that the operation reached the cleaning phase; it does not indicate success or
guarantee that no later error occurred. Read-only operations and other
early-return paths report only the work they actually perform. The `CEFI`
stream has no terminal notification. Determine synchronous outcomes from the
`RunInstaller` result or process exit; asynchronous uninstall uses the separate
`CEFR` protocol below. This allows client applications to display custom
progress UI instead of the built-in dialog.

### Asynchronous uninstall lifecycle

For a relaunched `/cef-uninstall` with a valid `/cef-parent`, `WM_COPYDATA`
uses `dwData = 0x43454652` (`CEFR`) for two UTF-8 JSON events:

```json
{"protocol_version":1,"event":"relaunch_started","operation_id":"0123456789abcdef0123456789abcdef","child_pid":1234}
{"protocol_version":1,"event":"operation_result","operation_id":"0123456789abcdef0123456789abcdef","command":"uninstall","success":true,"outcome":"committed","exit_code":0}
```

The public operation ID is 32 lowercase hexadecimal characters. It correlates
events but does not authenticate either process. `operation_result` extends the
normal result JSON: success outcomes are `committed` or `cleanup_deferred`;
failures include matching `error_code`, `error_name`, and `error_message`.
Success uses exit code 0. Failure codes are restricted to the named stable
installer errors 100-108, 110-119, and 199. Exit 109 is reserved for the
original process and is never a child terminal; arbitrary integers paired with
`UNKNOWN_ERROR` are malformed. Warnings are optional. Retention-only result
fields are not emitted for uninstall.

Each payload is at most 32 KiB including exactly one trailing NUL. Failure text
is at most 4096 UTF-8 bytes; at most 32 warnings of 1024 bytes each are sent.
`diagnostics_truncated:true` reports any reduction. Dispatch by `dwData` before
parsing JSON, copy the bytes, return promptly, and parse outside the window
procedure. Lifecycle return values—including 2—are ignored and cannot cancel
work. Each send is one attempt bounded by 500 ms with no retry.

Events from separate operations may interleave, and a terminal can arrive
before its handoff. Buffer terminal-first events by operation ID, attribute
them only after the matching handoff, and keep only the first valid correlated
terminal. Bound and expire correlation state because the channel is not
authenticated, prefer evicting orphan terminals, and consume or remove entries
when the caller no longer needs them. An orphan terminal remains indeterminate.
`child_pid` is diagnostic metadata, not a durable process handle or wait
contract.

The terminal is sent after progress UI closure, and no later progress is
emitted, but it reports logical result/UI finalization rather than child exit.
`cleaning` progress is not completion. Delivery timeout/error, parent window
destruction or reuse, UIPI rejection, process initialization failure, invalid
trusted state, crash, forced termination, and power loss can leave completion
indeterminate. A higher-integrity receiver must explicitly allow `WM_COPYDATA`
with `ChangeWindowMessageFilterEx`; the installer does not change its filter.

### Cancellation via Parent Window

The parent window can request cancellation by returning
`kWmCopyDataResultCancel` (2) from its `WM_COPYDATA` handler:

```cpp
// 'CEFI' in little-endian — identifies CEF installer progress messages.
constexpr ULONG_PTR kCefInstallerProgressId = 0x43454649;
// Return this from WM_COPYDATA to request cancellation.
constexpr LRESULT kCefInstallerCancel = 2;

case WM_COPYDATA: {
  COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
  if (cds->dwData == kCefInstallerProgressId) {
    // Process progress JSON...

    // Return kCefInstallerCancel to cancel the installation.
    if (user_wants_cancel) {
      return kCefInstallerCancel;
    }
  }
  return TRUE;  // Message handled, continue installation
}
```

Only the specific sentinel value (2) triggers cancellation. Standard
`WM_COPYDATA` return values (`TRUE` = handled, `FALSE` = not handled), timeouts,
and send errors are all treated as "continue". This means a well-behaved parent
following MSDN conventions will never accidentally cancel the install.

### Parent Window Handle (Cross-Process Security)

The `parent_window` HWND is treated as untrusted input from an external
process. The following table describes how the installer's progress dialog
interacts with that window:

| Aspect | Progress dialog behavior |
|--------|--------------------------|
| **Ownership** | No owner relationship established (`CreateDialogParamW` receives `NULL`) |
| **Window state** | Never modified (`EnableWindow`, `SetForegroundWindow` not called) |
| **Positioning** | Read-only via `GetWindowRect` for centering the progress dialog |
| **Validation** | `IsWindow()`, `IsWindowVisible()`, `IsIconic()`, `MonitorFromRect()` checks |
| **Dialog lifetime** | Independent of parent; the dialog survives if the parent is destroyed |
| **Z-order** | Always topmost for visibility regardless of parent |

The HWND is only a positioning hint and progress-notification destination. Its
window procedure may request cancellation, but cannot control another process
or window through the installer. During retention apply, cancellation after
the `committing` state begins is deferred until the next safe checkpoint before
physical cleanup. See [Config & Input Parsing](SECURITY.md#5-config--input-parsing)
for the cross-process trust boundary and security invariants.

## Exit Codes

When running in standalone installer mode (`/cef-update`, `/cef-uninstall`,
`/cef-retention-dry-run`, or `/cef-retention-apply`), the process exit code
indicates success, pending work, or failure.

Installer-specific exit codes use the 100-199 range to avoid conflicts with
`cef_resultcode_t` values (Chrome codes: 0-40+, sandbox codes: 7006+).

The authoritative numeric values and their meanings are defined alongside the
constants in [installer_constants.h](installer_constants.h#L160). For enterprise
policy outcomes and remediation, see [ADMIN_POLICY.md](ADMIN_POLICY.md).

## Concurrent Access

Multiple processes can read or use the same CEF store at once. The installer
keeps those reads responsive while allowing only one process at a time to
change a particular writable store. See
[Install, Update, and Read-only Behavior](#install-update-and-read-only-behavior)
for when `query` and automatic startup remain read-only or enter a writer path.

### Protecting a version that is in use

When automatic startup selects an installed version, it keeps
`Release/libcef.dll` open without permission to delete it. Other processes may
use the same version at the same time. If a writer later removes that version,
it first removes the version from `versions.json` so new readers cannot select
it. The writer then tries to move the version directory to `.trash/`. A running
application's open file prevents that move, so physical cleanup is deferred
until after `RunWinMain` returns; the running application is not disrupted.

### Publishing changes safely

A named mutex serializes changes to each writable install directory. Writers
publish durable state in this order:

| Change | Publication order |
|--------|-------------------|
| Add a version | Validate and publish the complete version directory, atomically publish and reread the expanded `versions.json`, then publish the app registration. |
| Remove a version | Publish registration removal when required, atomically publish and reread the reduced `versions.json`, move the directory to `.trash/`, then delete it. Pruning starts with the index update. |

If a durable step fails, the installer stops before starting the next step; it
does not try to roll back work already published. This ordering ensures that
`versions.json` never points to a directory that removal has already moved.
Failure to move or delete files after the required database and index changes
is a successful `cleanup_deferred` outcome, not a failed logical operation. A
running application and a temporary access-denied error are common reasons for
deferred cleanup. See [Result JSON Format](#result-json-format) for how to handle
that outcome.

### Index recovery and existing destinations

Every `versions.json` update is written atomically, reread with the production
parser, and compared with the intended contents. A writer-locked install,
update, or uninstall automatically rebuilds a missing or damaged index as
described in
[Install, Update, and Read-only Behavior](#install-update-and-read-only-behavior).

When the existing index is valid, it remains authoritative. A version directory
that is not listed in the index is treated as an orphan and quarantined rather
than silently added back. Recovery never invents an app registration. Invalid
directory names and version-level reparse points are skipped so they do not
block unrelated changes, but they produce deferred-cleanup warnings. A
persistent artifact keeps later write results at `cleanup_deferred` until it is
removed.

Once an install has downloaded, extracted, and catalog-verified a staging
distribution, any safe regular-file or directory target already present at the
canonical destination is moved opaquely to the reparse-safe `.trash/` directory.
The installer does not read or classify the existing target's contents. It then
renames the current operation's verified staging directory into place before
publishing the expanded index, so existing bytes never win this collision.

A quarantine failure is a hard error that preserves both the target and verified
source at their original paths. A replacement-rename failure is also hard: the
old target remains under `.trash/`, verified staging remains at its source, and
the canonical destination is absent. The old target is never rolled back.
Physical reclamation of a successfully quarantined target is best-effort; if
logical directory and index publication otherwise complete, remaining trash is
reported as `cleanup_deferred` and retried by a later writer.

Staging is owned by the current controller operation and is cleaned up
best-effort on return. It is never persisted as trusted state. A later retry may
reuse verified archive-cache bytes, but it re-extracts the archive and repeats
catalog verification before attempting publication again.

### Lock waits and failure behavior

- The mutex name is derived from the writable install directory, so independent
  stores do not block one another.
- Explicit write commands wait up to 30 seconds by default. If the current
  writer does not finish in time, the waiting command returns
  `kExitCodeLockTimeout`.
- Automatic startup waits at most five seconds when it needs the writer lock.
  On timeout, it returns `kExitCodeLockTimeout` (107). A client DLL receives the
  code and diagnostic through `cef_version_info_t`; standalone mode exits 107.
  If enabled, the progress UI shows localizable “busy” guidance and the error
  code instead of the diagnostic.
- A local-hit startup refreshes its app registration only when the entry is
  missing or changed. It makes a zero-timeout lock attempt and skips the refresh
  if another writer is busy, so application launch is not delayed.
- If a writer exits while holding the mutex, Windows gives ownership to one
  waiter. That process records the abandoned lock and reconciles installer
  state before continuing.
- A version required by an app registered in the same install directory is
  never deleted.

## Version Pruning

Unused CEF versions are automatically cleaned up in the writable directory:
- Each app's required CEF version range is registered by the installer in
  `installer.json`
- Apps are registered by `(uuid, platform)` - the same app on different
  architectures are separate entries
- On uninstall, the app is unregistered and its now-unused version directories
  are pruned immediately. Valid crash-history records for that app remain for
  up to 90 days so a reinstall does not immediately retry known-bad builds.
- Versions not required by any app registered in that install directory are
  deleted
- Revoked installed versions are prunable even when a registration or confirmed
  fallback would otherwise protect them; a usable non-revoked fallback is
  retained when available
- Pruning covers every platform subdirectory; see
  [Pruning Protection](#pruning-protection) for the cross-platform rules
- Pruning happens during uninstall, after a successful install or update, and
  after eligible clean client exits as described in
  [Post-Exit Pruning](#post-exit-pruning)

## Explicit Registration Retention

Registration expiry is disabled by default. Normal commands never remove a
registration because it is old. This includes automatic startup, install,
update, uninstall, query, post-exit pruning, and ordinary `prune`.

Retention runs only when explicitly requested through one of these commands:

- CLI: `/cef-retention-dry-run` or `/cef-retention-apply`
- API: `RunInstaller("retention_dry_run", ...)` or
  `RunInstaller("retention_apply", ...)`

### Recommended workflow

An application-owned maintenance UI or trusted updater should:

1. Run `retention_dry_run`.
2. Show the affected registrations and versions.
3. Warn that removed registrations may require a future download and may break
   offline or download-restricted applications.
4. Run `retention_apply` only after the user or operator confirms.
5. Retry when `retry_required` is true. Treat `cleanup_deferred` as a successful
   logical change with physical cleanup still outstanding.

The CLI is intended for trusted support tools and managed scripts running as
the affected user. The bootstrap must enable explicit commands through its
embedded `enable_explicit_modes` setting.

Do not run retention automatically at startup, after exit, from a system-wide
elevated task, or in response to disk pressure. Those uses require a separate
policy because they have different availability risks.

### Eligible stores

Retention is based on store ownership, not on whether the current process is
elevated or can write to a path.

| Store | Retention allowed? |
|---|---|
| Per-user default store | Yes |
| Application-owned `install_path` store | Yes |
| HKLM-configured store | No |
| Program Files provisioning store | No |

Provisioning stores are rejected before write probing, file logging, or lock
acquisition. The same rules apply to the API and CLI.

### Age and launch evidence

The default age threshold is 180 days. A request may override it with
`max_age_days` in API JSON or `/cef-max-age-days` on the CLI. The accepted range
is 90 through 3650 days, inclusive. Evidence is stale when its age is equal to
or greater than the threshold.

Evidence is evaluated separately for each `(appid, platform)`. The newest
canonical, integrity-valid timestamp wins: either a health sentinel's
`pid_start_time` or a version-less liveness record's `last_launch`.

| Evidence | Result |
|---|---|
| Valid; age is below the threshold | Registration is retained |
| Valid; age meets or exceeds the threshold | Registration is a removal candidate |
| Missing or zero | Age is unknown; registration is retained |
| Malformed, integrity-failed, mismatched, or noncanonical | Age is unknown; registration is retained |
| Future-dated | Age is unknown; registration is retained |

Unknown evidence always protects the registration.

### How retention runs

1. **Preview the current plan with dry-run.** Dry-run takes the writer lock and
   is a read-only operation that produces a report without changing installer
   state. Each evidence file is read atomically, but the reads do not form one
   simultaneous global snapshot.

2. **Recompute and validate the plan during apply.** Apply takes the writer lock
   independently and does not use the earlier dry-run report as input. It first
   loads any interrupted cleanup recorded in installer-owned
   `retention_pending.json` into its working state, then builds a preliminary
   plan. After a progress checkpoint, it reads the launch evidence again and
   builds the final plan.

   The writer lock prevents concurrent installer mutations, but application
   launches can update launch-success or liveness evidence without taking that
   lock, including while the progress callback is running. Apply compares the
   preliminary and final registration-removal and expected version-removal
   sets. If either changed, it makes no changes and returns
   `kExitCodeRetentionSnapshotChanged`, `retry_required:true`, and the updated
   report for review.

3. **Order overlapping launches and enter the commit phase.** There is no
   single instant at which every evidence file is observed. A launch published
   before the direct final read of its canonical liveness file can protect that
   registration. A launch published afterward is ordered after this retention
   operation, even if other evidence files are still being scanned. Legacy
   health files are ordered by their own verified reads. Apply then offers one
   final cancellation checkpoint. Once accepted, the progress state changes to
   `committing`, and database and index publication are non-cancellable. Apply
   does not repeatedly replan or roll back a published database because newer
   evidence appeared after its final read.

4. **Publish the logical changes.** Before changing the database, apply records
   the exact version-cleanup scope in `retention_pending.json`. It saves all
   registration removals in one checked database update, then publishes the
   reduced version index before moving any version directories. If the database
   update fails, the index, versions, and evidence remain unchanged, and apply
   attempts to restore the previous pending scope. If that restore fails, the
   result requires a retry and includes a warning. If index publication fails
   after the database update, the result reports `success:false`,
   `outcome:"failed"`, `registrations_committed:true`,
   `versions_pruned:false`, and `retry_required:true`.

5. **Finish physical cleanup, or preserve it for retry.** Retention removes only
   versions made newly unreferenced by the registration removals. Pre-existing
   unreferenced indexed versions and unrelated revoked versions appear in the
   report but are not removed. Ordinary pruning may remove them during
   uninstall, after a qualifying install or update, or after an eligible clean
   client exit; see [Version Pruning](#version-pruning).

   Evidence is deleted only after logical publication. Apply verifies the exact
   payload, integrity status, and handle-resolved `.launch` parent against its
   final observation, using the same opened file for the checks and deletion.
   New or replaced evidence is preserved, as is any target reached by
   redirecting an intermediate directory through a junction or symlink.

   Live leases, failed directory moves, deferred cancellation, or evidence
   deletion failures may return successful `cleanup_deferred` with warnings. A
   cancellation request made during `committing` is remembered, disables the
   Cancel button, and stops physical cleanup at the next safe checkpoint after
   logical publication. `retention_pending.json` lets a later apply finish the
   exact remaining scope even if the registrations are gone or the index has
   already been reduced. The retry scans on-disk metadata for pending versions
   missing from the index and clears the pending record after cleanup succeeds.

6. **Return a deterministic report.** Reports sort registrations by
   appid/platform and versions by version/platform. Each registration includes
   its full appid, range, ABI, evidence kind, timestamp, age, decision, and
   reason. Each version includes its requirement state before and after
   retention, its compatibility or protection reason, and whether removal is
   expected.

   `reason` is a stable symbolic value suitable for use as a localization key.
   Registration reasons are `provisioning_store_ineligible`,
   `database_pruning_blocked`, `invalid_retention_options`, `invalid_evidence`,
   `missing_evidence`, `future_evidence`, `stale_evidence`, and
   `fresh_evidence`. Version reasons are `revoked`,
   `required_by_remaining_registration`, `confirmed_launch_protection`,
   `newly_unreferenced`, and `already_unreferenced`.

   Optional `diagnostic` text is for troubleshooting only; do not use it for
   localization or program logic. Headless CLI and API calls return JSON.
   Normal CLI use returns deterministic text. For retention, `RunInstaller`
   accepts only `install_path`, `max_age_days`, and `log_level`. Its result
   pointer remains valid until the next call on the same thread or until that
   thread exits.

### Availability warning

Retention trades application availability for recovered disk space. It is not
proof that an application was uninstalled. Applying it may force a dormant
application to download CEF again, and may permanently break an application
that is offline or prohibited from downloading. Use dry-run first.

## Launch Health Tracking

Launch-health tracking is an optional local safeguard against a CEF version
that repeatedly fails during application startup. For opted-in apps, it records
per-version launch outcomes in sentinel files and, after repeated failures,
removes the failing version from normal selection so a later launch can choose
a compatible fallback. Confirmed versions are also protected from pruning.
This per-device recovery mechanism complements, but does not replace,
fleet-wide revocation of a known-bad build.

Launch-health rollback is opt-in per app. The `launch_health` field accepts:

- `"off"` (default): no health-state read for selection, no health sentinel, and
  no local crash rollback.
- `"explicit"` (recommended): only `RunInstaller("launch_success")` confirms.
  Every ordinary `RunWinMain` return is neutral; only a dead process without
  confirmation increments the failure count.
- `"exit_code"`: exit 0 confirms, neutral exit codes preserve history, other
  exits or process death count as failure, and `launch_success` may confirm
  early.

Opt in only when the app is effectively single-instance for a user-data
directory. Apps with conventional exit codes can use `exit_code`; apps with
custom nonzero codes should confirm reliably and use `explicit`. Partial
explicit adoption is unsafe: unconfirmed ordinary exits never reset earlier
failures, so failures can accumulate across otherwise healthy runs.

The default is off because local crash attribution is heuristic; fleet-wide
bad builds belong on the revocation list. With tracking off, a startup-crashing
installed version can remain selected indefinitely. Recovery must come from a
launch-time revocation, another app installing a newer compatible version, or
an app update that raises `vmin` or ships a newer bundle. An in-app
`RunInstaller("update")` cannot help when startup never reaches that call.
Opted-in apps should keep their own confirmed fallback and assume deeper
rollback may require a CDN download or bundled fallback.

For opted-in apps, health evaluation runs during every install or update
check, including a background `RunInstaller("update")` call, so background
updates also keep versions disqualified by repeated launch failures out of
normal selection.

When automatic startup resolves exclusively from a read-only store and no
writable installer state root exists, launch-health tracking is omitted for
that launch.

An app's `launch_health` value controls health tracking and version selection
for that app; it does not hide `.launch/` files belonging to other registered
apps in the same store. Shared maintenance still honors those files regardless
of the current app's mode: confirmed health sentinels can protect versions from
pruning, and retention evaluates each registration's health or liveness
evidence independently. See [Pruning Protection](#pruning-protection) and
[Explicit Registration Retention](#explicit-registration-retention).

### Launch-state files

The `.launch/` directory contains two file formats because it supports two
independent policies:

- A **versioned health sentinel** records whether a particular CEF version
  started successfully. Opted-in launch-health rollback uses it to disqualify
  repeatedly failing versions and protect confirmed versions from pruning.
- A **versionless liveness record** records when the app last launched.
  [Registration retention](#explicit-registration-retention) uses it to decide
  whether a registration may be stale, even when launch-health rollback is
  off.

Keeping them separate lets liveness be published and refreshed independently
of launch-health mode, while preserving crash and confirmation history for each
CEF version across app launches and version changes.

**Versioned health sentinel.** This per-app, per-version state file is written
to the `.launch/` directory under the install directory immediately before
`RunWinMain`:

```
<install_dir>/.launch/<appid_hash>_<version>_<platform>
```

`appid_hash` is the first 16 hex chars of SHA-1(appid). The version and
platform are embedded in both the filename and the JSON body. The file uses
a CRC32 integrity footer (same format as other installer files):

```json
{
  "appid": "A3B9C4D5-E6F7-4A8B-9C0D-E1F2A3B4C5D6",
  "version": "151.1.0",
  "platform": "windows64",
  "pid": 12345,
  "pid_start_time": "133612345678901234",
  "consecutive_failures": 2,
  "running": true,
  "confirmed": false,
  "last_update": "133612345678901999"
}
```

The `pid` and `pid_start_time` fields prevent stale updates. After
`RunWinMain`, the bootstrap updates the launch state only if the PID in the
file still matches the current process. If a newer instance has already
overwritten the file, the exiting process skips the update.
`last_update` is a required, nonzero decimal Windows-epoch FILETIME used only
for launch-history garbage collection. The common writer replaces any caller
value with current time immediately before every pre-launch, explicit
launch-success, normal-exit, or neutral-exit publication. Failure to obtain a
nonzero time fails that publication without replacing prior state. A clean
exit after explicit launch success remains a no-op and does not publish twice.

Because files live in a single `.launch/` directory (not inside version
directories), crash history survives version pruning. This prevents a doom
loop where a crashed version is pruned, re-downloaded from the CDN, and
crashes again.

Writes use a same-directory temporary file and atomic replacement. If the
pre-launch write fails, the error is logged and the client launches untracked
for that run. CRC detects corruption but is not authentication. Both launch
record formats require a valid integrity footer; footerless files and health
sentinels missing `last_update` are invalid old-schema state. Read-only paths
ignore and preserve invalid records, while a writer-locked repair pass may
remove them.

**Versionless liveness record.** All non-bundled main-process launches write
this retention-liveness record immediately before the health sentinel (when
enabled) and before `RunWinMain`:

```
<install_dir>/.launch/<appid_hash>_<platform>
```

Its bounded, integrity-protected body contains only `appid`, `platform`, and a
decimal Windows-epoch `last_launch` timestamp. The version-less two-segment
filename cannot be parsed as a health sentinel or protect/disqualify a version.
Liveness publication and health-sentinel publication have independent success
semantics; liveness success never substitutes for a failed health sentinel.
The bootstrap rewrites it when missing, malformed, identity-mismatched,
future-dated, or at least 30 days old; the content timestamp, not mtime, is
authoritative. This records liveness for later retention policy without
deleting registrations based on age.

### Mode Classification

This applies only to the opt-in modes (`explicit` and `exit_code`).

After `RunWinMain` returns, the bootstrap classifies the exit code:

| Event | Mode | Effect |
|----------|--------|--------|
| **Explicit confirmation** (`RunInstaller("launch_success")`) | Both opted-in modes | Write `running=false`, `confirmed=true`, `failures=0`. |
| **Ordinary return** | `explicit` | Write `running=false`, `confirmed=false`; preserve failures regardless of exit code. |
| **Exit 0** | `exit_code` | Confirm and reset failures. |
| **Neutral exit** (for example 21, profile in use) | `exit_code` | Write unconfirmed and preserve failures. |
| **Other exit** | `exit_code` | Leave `running=true`; count it on the next launch. |

Clients using either opted-in mode can call
`RunInstaller("launch_success", nullptr)` during `RunWinMain`; no config is
needed because the bootstrap records the active sentinel before calling the
client. In `explicit` mode this call is the only way to confirm health. In
`exit_code` mode it can confirm health before process exit. Either way, an
app-level crash after confirmation does not penalize the CEF version, while a
crash before confirmation still can. See the mode table above for behavior when
the call is omitted and [RunInstaller Commands](#runinstaller-commands) for the
API contract.

### Rollback Behavior

This applies only to the opt-in modes (`explicit` and `exit_code`).

On each launch, the installer scans `.launch/` for the app's files, covering
both installed and previously-pruned versions, to build the
**crash-disqualified** set:

1. If a sentinel shows `running=true` and the process is dead, the version is
   treated as having crashed one more time. A `pid_start_time` before the
   estimated current Windows boot boundary is indeterminate and never counts as
   a crash. If boot evidence is unavailable or inconsistent, classification also
   fails open to indeterminate.
2. If three or more consecutive crashes are projected, the version is
   **crash-disqualified**.

Disqualified versions then feed into the
[Selection Decision Tree](#selection-decision-tree): they are removed from
primary selection (so the next-best version is chosen instead) and from the CDN
skip list, but a non-revoked disqualified version remains available as a last
resort if nothing else qualifies. See the decision tree for the exact ordering
relative to revoked and bundled versions.

### Cleanup Strategy

This applies only to the opt-in modes (`explicit` and `exit_code`). Shared
cleanup performed by pruning is described separately in
[Stale File GC](#stale-file-gc).

On successful exit (version confirmed), the bootstrap deletes the `.launch/`
*files* for older versions that were previously confirmed (qualifying those
versions for pruning). It only deletes confirmed-state files; `.launch/` files
recording crash history (`running=true`, or `failures>0`) are kept.
This confirmed-file cleanup is separate from the 90-day age grace applied to
orphaned and below-`vmin` crash history.

This is about the small `.launch/` sentinel files, not the version directories.
A crashed version directory is **not** protected from pruning — only *confirmed*
versions are (see [Pruning Protection](#pruning-protection)) — so a known-bad
version's bits can be reclaimed. Its crash-history file lives separately in
`.launch/` and survives that pruning, which is what keeps the version
disqualified and off the CDN re-download list, preventing a crash-loop.

Cleanup only affects the current app's files. If apps A and B both have
confirmed files for v1, app A confirming v2 deletes app A's v1 file but
leaves app B's v1 file intact.

### Stale File GC

During ordinary pruning, valid orphaned records (no app registered for their
`(appid, platform)`) and records below the minimum `vmin` of all registered
apps on their platform become eligible for age cleanup. They are retained
while their integrity-verified content timestamp is less than 90 days old and
deleted when its age is 90 days or more. Health records use `last_update`;
liveness records use `last_launch`. Filesystem timestamps are ignored.
Consequently, a record that is already old may be deleted the first time it
becomes eligible. A future timestamp or unavailable clock preserves the file.

Writer-locked prune also repairs a reparse point at `.launch/`, removes
final-component reparse launch files without following their targets, and
deletes schema-invalid or noncanonical files, including abandoned atomic-write
temporaries. Query and launch-health evaluation remain read-only and fail
closed on those paths. Turning launch health off for the mutating app removes
its launch records without the age grace. Explicit registration-retention apply
uses its separate retention policy and does not run ordinary launch-record age
GC.

### Pruning Protection

Versions with a confirmed launch state (`running=false`, `confirmed=true`,
`failures=0`) in `.launch/` are protected from pruning. The check spans all
apps — if *any* app has a confirmed file for a version, that version is kept.
This ensures a shared version is not deleted while any app registered in the
same install directory considers it good. Registrations and protection are
intentionally not aggregated across stores.

Protection applies only to a registered app's record at or above that
platform's global `vmin`. An orphaned or below-`vmin` record never protects
version binaries, even while its recent history is retained, its timestamp is
future-dated, or the clock is unavailable. Conversely, an active registered
confirmed record at or above `vmin` protects its version regardless of record
age.

Protection is independent of the pruning app's mode: an `off` app does not
ignore another registered app's confirmed rollback target. Revocation remains
authoritative, so a revoked version is prunable despite confirmation.

**Cross-platform:** Pruning, and therefore launch-state handling, is not gated
to the running installer's platform. Version pruning scans all platform
subdirectories (a 64-bit installer can prune orphaned 32-bit versions and vice
versa), so protection, orphan deletion, and stale GC are evaluated for every
platform's `.launch/` files — each against its own `(appid, platform)`
registration and that platform's `vmin`. Without this, a prune on one platform
could delete a version another platform's launch health is keeping as a
rollback target. The shared `installer.json` database provides registration
context for all platforms.

When a newer version is confirmed, the older version's launch file is
cleaned up as described in [Cleanup Strategy](#cleanup-strategy). Once all
apps have moved on, the version loses its protection and becomes eligible for
pruning.

### Post-Exit Pruning

After `RunWinMain` returns with a **clean exit (code 0)** and the launch state
is updated, the bootstrap runs a prune pass with a minimal lock timeout (1 ms)
so it never blocks exit. This cleans up versions that are no longer needed. If
the lock is unavailable (another writer is running), pruning is skipped
silently.

Pruning (and the older-confirmed-file cleanup) runs **only** on exit code 0 —
not on neutral exits, and not on failures. A neutral exit such as "profile in
use" implies another instance is concurrently running with `libcef.dll`
loaded (or that an immediate relaunch is coming), which violates the "no
in-use file conflicts" premise that makes post-exit pruning safe. The next
clean exit, or a standalone `/cef-update`, prunes instead.

When `RunInstaller("launch_success")` has already confirmed this launch, the
post-exit handler skips the sentinel write (the confirmation is already
durable). Cleanup and pruning still run, but — as above — only on exit code 0.

## Security

See [SECURITY.md](SECURITY.md) for the full security model, trust boundaries,
invariants, and developer checklists.

### Code Signing

Downloaded CEF distributions require a Windows-valid signed catalog and a
non-empty expected certificate thumbprint before publication. Applications
using a legitimate alternative signer must also use an isolated `install_path`
and a matching client-side load policy. See
[Signature Verification](SECURITY.md#1-signature-verification) for the
authoritative trust model and invariants.

### Config Security

Config is loaded with the following priority:

**When client DLL is present:**
1. Client DLL embedded resource
2. Bootstrap `.exe` embedded resource (fallback)

**Standalone mode (no client DLL):**
1. Bootstrap `.exe` embedded resource. This is also the **only** source for
   `enable_explicit_modes`.

The selected source's `cdn_urls` replaces, rather than merges with, every
fallback application-config source.

An embedded resource is trusted and authoritative when present: malformed
JSON, missing/invalid fields, or an empty post-clamp range is a hard config
error and never falls through to another embedded source. Only an absent client
resource may fall back to the bootstrap resource. A missing standalone
resource reports no config; malformed JSON or invalid fields produce config
error 100.

### Enterprise Provisioning

Admin/shared stores use explicitly managed, durable provisioning pins. Use a
dedicated installer bootstrap rather than elevating the application. See
[Provisioning and revocation curation](ADMIN_POLICY.md#provisioning-and-revocation-curation)
for exact-version pins, signer requirements, store isolation, scheduled
updates, and offline mirrors.

### Standalone Mode Security

The `enable_explicit_modes` flag (default `false`) controls whether a
bootstrap binary can be used as a standalone installer. This flag is **only
read from the bootstrap `.exe`'s own `CEF_INSTALLER_CONFIG` embedded
resource** — it is ignored in client DLL resources and `RunInstaller` JSON.
When `false` in official builds:

1. **Explicit commands blocked:** `/cef-update` and `/cef-uninstall` are
   rejected (both standalone and with a client DLL present).
2. **Standalone auto-install blocked:** Auto-install (no client DLL, no explicit
   command) is blocked.
3. **Automatic admin-store mutation blocked:** A launcher with a client DLL may
   read provisioned HKLM/ProgramFiles versions or use bundled CEF, but it does
   not create, probe, register, reconcile, prune, cache, or download into an
   admin-default store and an elevated process does not continue to
   `%LocalAppData%`.

Without `enable_explicit_modes`, the binary can only function as a launcher
when a client DLL is present. CEF resolution still requires a
`CEF_INSTALLER_CONFIG` resource in the client DLL or bootstrap `.exe` —
without one, the bootstrap loads the client DLL with no installer
involvement. The [`RunInstaller`](#background-update-api) export is always
available regardless of this flag. In non-official builds, all standalone
operations are allowed regardless of this flag (for development/testing).

| Bootstrap resource | Explicit commands | Standalone auto-install |
|--------------------|-------------------|-------------------------|
| `enable_explicit_modes: true` | Allowed | Allowed |
| No flag or `false` | **Blocked** | **Blocked** |
| Missing or invalid | **Blocked** | **Blocked** |

App builders who want standalone installer features must explicitly opt in
by embedding `"enable_explicit_modes": true` in the bootstrap `.exe`'s
`CEF_INSTALLER_CONFIG` resource. See
[Config & Input Parsing](SECURITY.md#5-config--input-parsing) for the security
rationale and official-build invariants behind this gate.

### Sandbox Compatibility

The `abi_hash` field ensures sandbox compatibility between client and CEF:
- Hash is computed from sandbox-related struct definitions
- Mismatched hashes prevent sandbox escape or crashes
- Use `CEF_SANDBOX_COMPAT_HASH` from `cef_version.h`

## Build Integration

The installer consumes two embedded Windows `RT_RCDATA` resources. Its
first-run UI can additionally be branded and localized with executable string-
table resources.

### Standard binary distribution CMake example

The standard Windows binary distribution documents an opt-in
`-DUSE_INSTALLER=On` CMake example in its `README.txt`. That Release-only,
bootstrap/sandbox build focuses on cefclient and embeds a generated managed
`CEF_INSTALLER_CONFIG` in the client DLL while staging only the bootstrap,
client DLL, and `chrome_elf.dll` runtime payload. It does not modify bootstrap
resources or enable bootstrap-only explicit command-line or standalone modes.
Replace the sample application identity and follow the signing and compatible
CEF distribution requirements in this document before production use.

### 1. `CEF_INSTALLER_CONFIG` (client DLL or bootstrap resource)

A JSON configuration specifying the application's CEF version requirements.
See [Config Security](#config-security) for loading priority and
[Standalone Mode Security](#standalone-mode-security) for the
`enable_explicit_modes` field. The default bootstrap binary does **not**
include this resource; apps that want bootstrap-driven CEF resolution must
embed the config in their client DLL or add it to the bootstrap `.exe` (at
build time via the `.rc` file, or post-build using a resource editor such
as ResourceHacker).

For build-time integration, add the corresponding `RCDATA` declaration to the
client DLL or renamed bootstrap executable's `.rc` file. To add or update the
resource after building, see
[Embedding Resources with ResourceHacker](TESTING.md#embedding-resources-with-resourcehacker).

### 2. Progress-dialog branding and localization

Applications ship a renamed bootstrap executable and may customize its icon,
version information, and installer strings alongside the embedded config.
See [installer_resources.rc](installer_resources.rc#L23) for the authoritative
dialog layout, string-table IDs, and default English text. The progress UI
loads these resources from the running executable, using the compiled English
text only when a lookup fails.

Override the values in the renamed executable's `.rc` file or with the same
resource editor used to embed `CEF_INSTALLER_CONFIG`. For localization, add
language-specific `STRINGTABLE` blocks with the same numeric IDs; `LoadStringW`
selects the resource for the thread UI language. The error dialog's standard
OK button is supplied and localized by Windows. Translate both
`IDS_INSTALLER_ERROR_MESSAGE_FORMAT` and the body resources so the error-code
label, punctuation, placeholder order, and guidance are all localized. In that
format, `$1` is the localized guidance body and `$2` is the invariant decimal
error code. Both placeholders are required; an invalid customized format falls
back to the complete English format so it cannot omit the numeric error code.

Installer `error_message` values in result JSON and startup reporting remain
unlocalized diagnostics for logs and support. The progress dialog instead
selects localized guidance from `error_code` and includes that numeric code for
correlation.

### 3. `CEF_REVOCATION_LIST` (bootstrap resource)

A compiled-in baseline revocation list embedded in the bootstrap binary.
Generated at build time by `tools/make_revocation_resource.py` from
`tools/cefbuilds/revoked.json` and `cef_api_versions.json`. This ensures the
installer can enforce revocation checks even without network access.

See the `installer_revocation_resource` target in `cef/BUILD.gn`.

## Testing

See [TESTING.md](TESTING.md) for unit, integration, E2E, manual, fuzz, and code
coverage workflows.

## Troubleshooting

### Installer is skipped
- Verify `CEF_INSTALLER_CONFIG` resource exists in client DLL
- Use Resource Hacker or similar to inspect the DLL

### Version not found
- Check `vmin`/`vmax` range matches available versions
- Verify `abi_hash` matches if sandbox is enabled
- Note that `vmin` is clamped to the bootstrap's API version (check logs for
  "clamped" messages)

### Resource loading fails
- Verify the `Release/` directory (alongside `libcef.dll`) contains `.pak` files
  and `locales/` subdirectory
- CEF automatically loads resources relative to `libcef.dll` location

### Reset launch-health history

When [launch-health tracking](#launch-health-tracking) is enabled, the bootstrap
records whether each CEF version reached a healthy state or repeatedly failed
to launch. It uses this history to avoid a crash-disqualified version and to
preserve a confirmed fallback. Reset the history only when those records no
longer reflect the application's actual health.

There is no timed disqualification expiry. Close the application and delete
its files under `<install_dir>\.launch\` to reset local health history.
Task termination, OOM/process kills, logoff, or failure to confirm before
those events can still look like a crash. Newer versions, a rising `vmin`, a
clean last-resort redemption, or this manual reset provide recovery.

### Logs
- Check `<install_dir>\cef_installer.log` for detailed logs (e.g.,
  `%ProgramFiles%\CEF\cef_installer.log` or
  `%LocalAppData%\CEF\cef_installer.log`)
- If no writable install directory is found (including when `install_path`
  points to a read-only directory), logs fall back to `%TEMP%\cef_installer.log`
