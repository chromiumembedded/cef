# Installer Testing Guide

All commands below run from the `chromium/src` directory.

## Building Tests

The installer unit tests are defined in `//cef/BUILD.gn` as the
`cef_installer_unittests` target. Replace `<build_dir>` with your build
directory (e.g., `out/Debug_GN_x64`).

```bash
# Unit + integration tests only
autoninja -C <build_dir> cef_installer_unittests

# Everything (unit tests + E2E infrastructure + bootstrap binary)
autoninja -C <build_dir> cef cef_installer_unittests
```

## Running Tests

```bash
# Run all installer tests
<build_dir>/cef_installer_unittests.exe

# Run a specific test suite
<build_dir>/cef_installer_unittests.exe --gtest_filter="InstallerConfig*"

# Run a specific test
<build_dir>/cef_installer_unittests.exe --gtest_filter="InstallerArchiveTest.ExtractValidArchive"

# List all available tests
<build_dir>/cef_installer_unittests.exe --gtest_list_tests
```

## Directory roles and provisioning

`InstallerPathsTest` uses source-tagged candidate overrides, injected physical
read/write state, and injected elevation so the HKLM, Program Files, per-user,
and custom matrix does not depend on the runner's token or host ACLs. The
controller tests additionally override the official admin-mutation decision to
prove gated startup takes no writer lock, database/registration path, or
per-user fallback. Focused filters are:

```bash
<build_dir>/cef_installer_unittests.exe \
  --gtest_filter="InstallerPathsTest.*Role*:InstallerPathsTest.*Admin*:InstallerPathsTest.*Elevated*:Controller*.*AdminDirectory*"

<build_dir>/cef_installer_unittests.exe \
  --gtest_filter="InstallerConfigTest.*InstallPath*:ConfigPrecedenceTest.*InstallPath*"
```

The resource fixture `cef_config_test_appid_a.dll` contains a relative
`install_path`; helper tests verify client-only acceptance, DLL-relative
resolution, and independent bootstrap provenance for `enable_explicit_modes`.
Real Program Files/HKLM permission coverage requires an appropriately elevated
Windows environment. Broad E2E role tests should use isolated roots and the
non-official override; cleanup must be limited to paths/registry values created
by that test.

Provisioning uses existing `/cef-update` and `/cef-uninstall` commands with a
dedicated appid and exact `vmin = vmax` pin. Multiple pins use multiple appids.
Inspect `installer.json`, `versions.json`, `.launch`, `.cache`, `.staging`, and
`.trash` under only the selected test root when asserting side effects.

## E2E Tests

End-to-end tests exercise the real `bootstrap.exe` binary as a subprocess,
covering command-line parsing, config loading, exit codes, launcher mode, and
process lifecycle.

### Building

E2E tests require the full `cef` target (includes `bootstrap.exe`, the mock
client DLL, the progress helper, and the CDN builder):

```bash
autoninja -C <build_dir> cef
```

### Prerequisites

- **`CEF_RESOURCE_HACKER_PATH`** (system environment variable) — Must point to
  `ResourceHacker.exe` for tests that add or replace embedded configuration
  resources, including E2E2 and the bootstrap/client-resource CDN failover
  tests. Those tests fail with a clear error if it is not set. Tests that do
  not modify embedded resources do not require ResourceHacker.
- **Windows certificate-store access** — The E2E CDN builder creates signed
  test distributions, and the suite launches helper binaries. Sandboxed runs
  may fail with certificate or key-storage errors. Rerun the E2E command with
  the required elevated permission instead of changing product code.
- **Loopback HTTPS assets** - The self-signed localhost certificate and its
  test-only private key are checked in under `testdata`; OpenSSL is not a test
  prerequisite. The private key is fixture data and must never be used outside
  the installer tests.
- **Enterprise policy fixture access** - `test_enterprise_policy.py` creates
  one unique subkey below
  `HKCU\\SOFTWARE\\CEF\\InstallerPolicyTests` by default, explicitly in the
  shared 64-bit view, so ordinary non-administrator runs exercise real
  registry I/O. Non-official binaries accept only that test-root prefix and
  the `HKCU`/`HKLM` test-hive values; official builds ignore both seams. Set
  `CEF_INSTALLER_E2E_POLICY_HKLM=1` in an elevated environment to run the same
  cases below `HKLM` instead. The test removes only its UUID-named key and
  never replaces or deletes the real `HKLM\\SOFTWARE\\Policies\\CEF` key.

### Running

```bash
# Run all E2E tests
python3 cef/libcef_dll/bootstrap/installer/e2e/run_e2e_tests.py \
  --build-dir=<build_dir>

# Run a single E2E test
python3 cef/libcef_dll/bootstrap/installer/e2e/run_e2e_tests.py \
  --build-dir=<build_dir> \
  -k test_install_flow.TestInstallFlow.test_fresh_install
```

### Uninstall routing coverage

The uninstall unit and E2E tests use exact exit codes and inspect final logical
state:

- An external executable with a writable target is synchronous (0 on success),
  including a client-resource relative custom path and the two-app prune case.
- Database/index failures return their exact nonzero controller code; physical
  cleanup failure returns 0 after logical commit and leaves cleanup deferred.
- Resolver/preflight unit tests cover read-only exclusive custom/admin targets:
  they return the normal no-writable error without a lower-priority fallback.
  A containment unit test applies a scoped empty DACL to a temporary file, and
  the CLI E2E applies a scoped write-deny DACL to an owned custom store. The E2E
  verifies the normal error, unchanged state, and no temporary child.
- Physical containment uses handle-resolved `FilePath` equality/parent checks.
  A real case-sensitive directory fixture verifies that sibling `CEF` and
  `cef` components remain distinct.
- With a writable target, the missing-executable ordinary-uninstall preflight
  test verifies an exact 105 decision before mutation or relaunch. A separate
  real ACL-inaccessible fixture verifies that the containment helper returns
  `kIndeterminate`.
- A non-official, accepted-value-only directory-role E2E seam models an
  elevated admin store whose mutation gate is closed. The subprocess test
  verifies no per-user fallback, mutation, or temporary child.
- An executable staged beneath the writable target, or an external executable
  using `/cef-background`, returns exactly 109. With a pumped `/cef-parent`,
  lifecycle E2E tests correlate the handoff and normalized terminal result.
  Filesystem polling remains only legacy test cleanup and is not a supported
  caller completion contract.
- Prepared-controller tests verify that snapshots bound to relaunch or reject
  decisions cannot be passed to the direct controller entry point.
- Forced relaunch failure returns 105 with state unchanged. A marker without
  valid trusted state, or relaunch transport on a non-uninstall command,
  returns 100 without mutation.

Use these focused commands from `//src`:

```powershell
out\Debug_GN_x64\cef_installer_unittests.exe `
  --gtest_filter='BootstrapHelpersTest.*Uninstall*:InstallerPathsTest.*Contain*:InstallerRelaunchTest.*:Controller*.*Uninstall*:InstallerOutcomeTest.*'

python cef\libcef_dll\bootstrap\installer\e2e\run_e2e_tests.py `
  --build-dir out\Debug_GN_x64 `
  -k 'test_uninstall or exit_code_relaunched or install_path_relative_in_client_dll_resource or trash_reclamation_reports_cleanup_deferred' `
  --verbose

out\Debug_GN_x64\cef_installer_unittests.exe `
  --gtest_filter='InstallerLifecycleTest.*:ProgressUIIntegrationTest.*:InstallerRelaunchTest.*'

python cef\libcef_dll\bootstrap\installer\e2e\run_e2e_tests.py `
  --build-dir out\Debug_GN_x64 `
  -k 'lifecycle or progress or uninstall' `
  --verbose
```

Run lifecycle race/correlation filters at least 20 times when changing the
transport. Unit and E2E binaries require Windows certificate-store access;
rerun outside a restricted sandbox when certificate/key-storage APIs are
denied. Elevation-specific UIPI coverage is environment-dependent: a
higher-integrity receiver must opt in to `WM_COPYDATA`; same-integrity delivery
is always covered.

`/cef-uninstall-relaunched` is internal transport/test state, not a supported
caller bypass. It is accepted only for explicit uninstall with the nonce-bound
trusted state file created beside the temporary executable. The non-official
`CEF_INSTALLER_TEST_RELAUNCH_FAILURE=1` environment seam deterministically
exercises failure before temp publication or child creation.

The admin-gate E2E pairs
`CEF_INSTALLER_TEST_DIRECTORY_SCENARIO=admin_mutation_denied` with an absolute
`CEF_INSTALLER_TEST_DIRECTORY_ROOT` containing existing `Admin` and `PerUser`
directories. It is accepted only for an original uninstall in non-official
builds; missing, unknown, relative, or incomplete values return configuration
error, and official release builds reject requested test state.

### E2E test infrastructure

- **`e2e_test_base.py`** — Base class with helpers for running the installer,
  building local CDN directories, reading the database, embedding config
  resources via ResourceHacker, and launch state assertions. Each test
  unconditionally restores its fixed-name executable from unmodified
  `bootstrap.exe` before resource edits; launcher helpers likewise restore the
  executable and client DLL they own. Resource-editing tests require
  `CEF_RESOURCE_HACKER_PATH`, while the missing-resource test launches the
  freshly restored executable without invoking Resource Hacker.
- **`cef_e2e_mock_client.dll`** — Mock client DLL for launcher mode tests.
  Supports single-threaded mode (env: `CEF_E2E_CONFIG_JSON`) and multi-threaded
  mode (env: `CEF_E2E_THREADING_MODE=multi`, `CEF_E2E_CONFIG_JSON_A/B`).
  When `CEF_E2E_EXIT_CODE` is set, `RunWinMain` writes the marker file and
  returns the specified exit code without calling `RunInstaller` — this lets
  launch health tests cover default/missing `off`, `explicit`, and
  `exit_code` modes, including clean exits, crashes, neutral exits, prior-boot
  records, explicit confirmation, and atomic write failure behavior.
  Controller unit tests cover invalid-file GC and writer-locked reparse-point
  repair.
  When `CEF_E2E_LAUNCH_SUCCESS` is also set, the mock client calls
  `RunInstaller("launch_success", ...)` to confirm launch health *before*
  writing the marker file and returning the exit code. This lets launch health
  tests simulate explicit confirmation before clean exits or crashes.
- **`cef_e2e_progress_helper.exe`** — Creates a message-only window. Its
  legacy invocation writes the existing progress JSON list. `--lifecycle`
  writes separate raw `progress` and `lifecycle` arrays plus a rejected-message
  count. `--lifecycle-return=N`, `--non-pumping`,
  `--close-after-handoff`, and `--allow-copydata` cover receiver return,
  timeout, destruction, and UIPI opt-in behavior. The window procedure only
  bounds and copies bytes; parsing occurs afterward.
- **`lifecycle_receiver.py`** — Strict version-1 reference parser and
  operation-ID correlator. It buffers terminal-first delivery, leaves missing
  handoffs unattributed, rejects non-terminal/unknown exit codes, ignores
  malformed/future events, and preserves the first valid correlated terminal.
  State is capped at 256 operations, expires after 30 idle minutes, and can
  be consumed or explicitly removed.
- **`cef_e2e_build_test_cdn.exe`** — Builds signed test distributions in CDN
  layout.

Marker JSON from `cef_e2e_mock_client.dll` records size-gated
`installer_error_code` and `installer_error_message`; a null message remains
JSON `null`.

### Test-only flags

See [Testing Mode Guards](SECURITY.md#testing-mode-guards) for the
official-build requirements that prevent test seams from becoming production
bypasses.

All `CEF_INSTALLER_TEST_*` environment variables are parsed centrally into
typed `InstallerE2EConfig` state. The owning directory, relaunch, config,
database, index, or file-operation component applies the requested fault;
bootstrap orchestration does not interpret test scenarios.

- **`/cef-install-path=<path>`** — Override the CEF installation directory. Each
  E2E test uses its own temp directory for isolation. Available only in
  non-official builds.
- **`/cef-log-level=<level>`** — Set minimum log level for `cef_installer.log`.
  Accepted values: `info`, `warning`, `error` (case-insensitive). Default:
  `warning`. Use `/cef-log-level=info` for verbose output when debugging.
  Available in all builds.
- **`CEF_INSTALLER_TEST_CHILD_CONFIG_FAILURE=1`** — Fails only a marked child
  after trusted state is accepted at the config-loading boundary, producing a
  controlled config terminal.
- **`CEF_INSTALLER_TEST_CHILD_STATE_BARRIER=<absolute path>`** — Writes a
  fixture-owned ready marker after state acceptance and waits for up to 30
  seconds while it exists, allowing deterministic forced termination.
- **`certificate_thumbprint`** (embedded test resource, non-official builds
  only) — Override the certificate thumbprint used for archive verification.
  When set to the test cert thumbprint, the bootstrap auto-enables signature
  testing mode so self-signed archives pass verification.

Lifecycle teardown terminates only the exact helper/child processes started by
the fixture, waits for them, and removes only fixture-owned markers and
verified new relaunch directories. Missing terminal delivery is indeterminate;
tests do not infer completion from database, index, or temporary-directory
internals.

### Writing E2E tests — common pitfalls

**`CEF_E2E_LAUNCH_SUCCESS` and `CEF_E2E_EXIT_CODE` interact.** Setting both
simulates an app that confirms CEF health then exits with the given code. The
sentinel is confirmed (`running=false, failures=0`) during `RunWinMain`, so a
subsequent crash exit does *not* count as a CEF failure (the post-exit handler
doesn't run for a failure exit, and the confirmation is already durable).
Without `CEF_E2E_LAUNCH_SUCCESS`, a non-zero (failure) exit leaves the sentinel
as `running=true`, which is detected as a crash on the next launch. Use the
`launch_success=True` parameter on `_run_launcher_with_exit_code` to set it.
Note that confirming on a *clean* exit still triggers post-exit cleanup/prune,
which can delete an older confirmed version's `.launch/` file (and the version)
— confirm on a crash exit if you need an older fallback version to survive.

**`_standalone_install` has side effects.** Each standalone install runs
`ComputeAndInstall` → `DownloadAndInstall` → `PruneUnusedVersions`. The
prune age-checks valid orphaned and below-global-`vmin` `.launch/` files.
Records less than 90 days old survive; records exactly 90 days old or older
may be deleted. E2E fixtures simulate age with integrity-protected
`last_update` (health) or `last_launch` (liveness) content. Do not use
filesystem mtime to age a fixture.

**Global vmin is the minimum across ALL registered apps.** If you need a
`.launch/` file to survive a standalone install that raises one app's vmin,
register a second app with a lower vmin first (e.g., `appid2` with
`vmin=v1`). This keeps the global minimum low.

**`_standalone_install(version, vmin=X)` won't install `version` if a
version ≥ X is already installed.** The installer selects the best existing
version that satisfies vmin. To force a specific version, use `vmin=version`
(or `vmin=version, vmax=version` to pin).

**`_embed_launcher_config` writes to the exe, not the DLL.** The bootstrap
reads config from the client DLL first; the exe resource is a fallback. In
launcher mode with the mock client DLL (which has no embedded config), the
exe resource IS used. But if you've embedded config into the DLL (e.g., for
bundled path tests), the DLL config takes priority and the exe config is
ignored.

**Cleanup vs GC — test the right mechanism.** On successful exit, the
bootstrap deletes `.launch/` files via `cleanup_paths` (only confirmed older
files for the current app). Separately, `PruneUnusedVersions` runs stale
file GC for orphaned and below-global-vmin records after the 90-day content-age
grace. If your test asserts a file was deleted, make sure it was deleted by the
mechanism you intend to test, not the other one. Use the second-app trick above
to neutralize GC when testing cleanup, or keep vmin constant.

**Launch GC has a dedicated unit-test clock.** Tests that call
`SetLaunchStateGcTimeForTesting()` must reset it to `std::nullopt` in fixture
teardown. Keep health `last_update` independent from retention's
`pid_start_time`; liveness uses `last_launch` for both policies. Useful focused
checks are:

```powershell
out\Debug_GN_x64\cef_installer_unittests.exe `
  --gtest_filter="LaunchStateTest.*:ControllerTempDirTest.Pruning_*"
python cef\libcef_dll\bootstrap\installer\e2e\run_e2e_tests.py `
  --build-dir out\Debug_GN_x64 -k launch_health --verbose
```

**Pruning requires a version to not be the best match.** A version is only
prunable if no registered app's best match points to it. Installing a newer
version doesn't make the older one prunable if the older one is still the
best match (e.g., because vmin hasn't changed). Unregistering the app or
installing a strictly better version makes the old one prunable.

## Security

See [SECURITY.md](SECURITY.md) for the security invariants that tests must
cover. When adding a new code path (e.g., a new extraction method), check the
"If you change" sections in SECURITY.md for required test coverage.

## Test Conventions

Each production source file has a corresponding `*_unittest.cc` (e.g.,
`installer_archive.cc` -> `installer_archive_unittest.cc`). Integration tests
that require a mock CDN server use the `InstallerIntegrationTest` fixture in
`installer_integration_test.cc` (full `Controller::Run` with embedded HTTP
server). Config precedence unit tests (`TryLoadInstallerConfig`) use test DLLs
with embedded resources (`cef_config_test_appid_a.dll`, etc.) loaded via
`LoadLibraryEx(LOAD_LIBRARY_AS_DATAFILE)`.

The `cef_config_test_malformed.dll` and `cef_config_test_invalid.dll`
fixtures verify that present-but-bad trusted resources do not fall back.
`embed_raw_config_resource()` provides the equivalent E2E path.

The CDN-list resource fixtures also include valid client/bootstrap lists and
invalid HTTP, wrong-shape, empty, over-count, and over-length lists. Keep these
fixtures in the public config-loader matrix: application `cdn_urls` use normal
client/bootstrap source selection and must never merge.

Failure and API contract focused validation:

```bash
<build_dir>/cef_installer_unittests.exe --gtest_filter="InstallerConfigTest.*:ConfigPrecedenceTest.*:InstallerResultTest.*:InstallerExitCodeTest.*:InstallerOutcomeTest.*:ExtendedConfigTest.*:RunInstallerTest.*:InstallerStartupResultTest.*"
python3 cef/libcef_dll/bootstrap/installer/e2e/run_e2e_tests.py \
  --build-dir=<build_dir> \
  -k "test_run_installer or test_launcher_mode or test_exit_codes"
```

Resolver and transactional-correctness focused validation:

```bash
<build_dir>/cef_installer_unittests.exe --gtest_filter="InstallerVersionResolverTest.*:VersionIndexTest.*:InstallerRevocationTest.*:InstallerFileOpsTest.*Repair*:ControllerTempDirTest.*Index*:ControllerTempDirTest.*Rebuild*:ControllerTempDirTest.*Orphan*:InstallerIntegrationTest.*Update*:InstallerIntegrationTest.*Repair*"
```

Registration-retention focused validation:

```bash
<build_dir>/cef_installer_unittests.exe \
  --gtest_filter="InstallerRetentionTest.*:LaunchStateTest.*Retention*:ControllerTempDirTest.Retention*:BootstrapHelpersTest.*Retention*:RetentionOutputTest.*:InstallerFileIntegrityTest.*Conditional*:ProgressDialogTest.*Cancel*"

python3 cef/libcef_dll/bootstrap/installer/e2e/run_e2e_tests.py \
  --build-dir=<build_dir> \
  -k test_retention
```

CDN robustness focused validation:

```powershell
out\Debug_GN_x64\cef_installer_unittests.exe `
  --gtest_filter='InstallerConfigTest.*Cdn*:ConfigPrecedenceTest.*Cdn*:ExtendedConfigTest.*Cdn*:CombinedConfigTest.*Cdn*:ExtendedConfigSecurityTest.*Cdn*:InstallerPolicyTest.*Source*:InstallerPolicyTest.*Url*:InstallerPolicyTest.*Identity*:InstallerDownloadTest.*ContentRange*:InstallerDownloadTest.*Partial*:InstallerDownloadTest.*Prune*Archive*:InstallerDownloadServerTest.*Range*:InstallerDownloadServerTest.*Partial*:InstallerDownloadServerTest.*Origin*:InstallerDownloadServerTest.*Promotion*:InstallerControllerServerTest.*CdnFailover*:InstallerControllerServerTest.NextBest*:InstallerControllerClassificationTest.NextBest*:InstallerControllerTest.NextBest*:InstallerIntegrationTest.*PolicyFailover*:InstallerIntegrationTest.*NextBest*:InstallerIntegrationTest.*ArchiveCleanup*:InstallerIntegrationTest.Background*:BootstrapHelpersTest.*Background*:InstallerParallelXzTest.*Background*:DetermineThreadCountTest.*Background*'

python cef\libcef_dll\bootstrap\installer\e2e\run_e2e_tests.py `
  --build-dir out\Debug_GN_x64 `
  --filter 'next_best_when_newest_archive_missing or explicit_background_update or authorized_application_cdn_https_failover or bootstrap_resource_cdn_https_failover or client_resource_cdn_https_failover'
```

Range tests use deterministic HTTP handlers that inspect exact request headers
and script `200`, `206`, `416`, duplicate/malformed `Content-Range`, length
disagreement, short/overlong bodies, redirects, and request counts. They must
assert that rejected responses are never promoted, interruption/cancellation
retains only the matching origin-bound partial, progress begins at the retained
size, and the protocol/final-origin/hash conditions share one clean-request
budget. Small manifest, revocation, and sidecar downloads remain non-resumable.

The signed integration tests cover one next-best selection without manifest
refetch, ordinary commit cleanup, archive retention after checked-index or
registration failure, post-commit `cleanup_deferred`, and propagation-lag
fallback. The local-CDN E2E builder can be called twice to advertise two
compatible versions; deleting only the newer archive exercises the same
fallback in a real bootstrap process.

Retention tests should verify the report as well as the durable state. Check
`installer.json`, `versions.json`, canonical files under `.launch`, and
`retention_pending.json`. Failure and cancellation cases must preserve the
documented database-before-index ordering, retain an exact pending version
scope when retry is required, and converge on an explicit retry.

`SetVersionIndexFaultForTesting()` deterministically injects checked index
write, replace, reread, and intended-set validation failures. Recovery tests
use explicit missing/corrupt/valid index snapshots plus staging, trash, and
unindexed directories; assertions must cover the durable index, registration,
directory membership, `outcome`, numeric `error_code`, symbolic `error_name`,
and `warnings` for deferred physical cleanup. Run crash/fault filters more than
once when changing publication order.

`SetFileOpsFaultForTesting()` covers quarantine/replacement moves and trash move
and reclamation. A quarantine-move failure preserves the existing target and
verified staging at their original paths. A replacement-move failure preserves
verified staging, leaves the old target under `.trash/`, and leaves the
canonical destination absent. Recovery must report post-index orphan cleanup
failures as successful `cleanup_deferred`; collision reclamation is likewise
`cleanup_deferred` only after directory and checked index publication succeed.

- Use `base::ScopedTempDir` for filesystem operations.
- Use `Version::FromApiVersion(CEF_API_VERSION_LAST)` for version values to
  avoid vmin clamping.
- Use `SetTestingMode(true)` in non-official tests to permit an already-parsed,
  internally injected HTTP source and suppress error dialogs. Public resource
  and `RunInstaller` `cdn_urls` parsing remains HTTPS-only.
- `VersionMetadata::IsValid()` requires non-empty `version`, `abi_hash`, and
  `platform`.

## Manual Testing

There are three ways to manually test the bootstrap + installer, each
exercising a different CEF loading path. All three require a Release build
(see [Prerequisites](README.md#prerequisites)).

In the examples below, `<build_dir>` is your build output directory
(e.g., `out/Release_GN_x64`) and `cefclient` is the client name.
The bootstrap binary is always renamed to match the client DLL (e.g.,
`bootstrap.exe` → `cefclient.exe`, which loads `cefclient.dll`).

**`enable_explicit_modes`:** Explicit commands (`/cef-update`,
`/cef-uninstall`) and standalone auto-install require
`"enable_explicit_modes": true` in the bootstrap exe's embedded
`CEF_INSTALLER_CONFIG` resource (official builds only — non-official builds
allow these unconditionally). See
[Embedding Resources with ResourceHacker](#embedding-resources-with-resourcehacker)
for how to add this post-build.

### Mode 1: Unchecked CEF Path (all binaries co-located)

The simplest setup — all binaries live in one directory. This is the default
when `cefclient.dll` has an `unchecked_cef_path` config (typically `"."`) or
no `CEF_INSTALLER_CONFIG` resource at all.

**Setup:** No extra steps needed. The build output already has `cefclient.exe`,
`cefclient.dll`, `chrome_elf.dll`, `libcef.dll`, and all CEF resources in the
same directory.

```
<build_dir>/
  cefclient.exe         # bootstrap.exe, renamed
  cefclient.dll
  chrome_elf.dll        # Must match exe signing (same cert, or both unsigned)
  libcef.dll
  *.pak, locales/, ...  # CEF resources
```

**Run:**
```
<build_dir>/cefclient.exe
```

The bootstrap finds `libcef.dll` adjacent to the client DLL via
`unchecked_cef_path` and skips the installer entirely — no version checks,
no signature verification, no CDN access.

### Mode 2: Bundled CEF Path (CEF in a subdirectory)

Tests the bundled version selection path. The bootstrap, client DLL, and
`chrome_elf.dll` are in one directory; the CEF distribution is pre-extracted
into a subdirectory.

**Setup:** Create a directory with the bootstrap files, and extract a signed
CEF distribution into a subdirectory (e.g., `cef/`):

```
test_dir/
  cefclient.exe         # Copy of bootstrap.exe, renamed
  cefclient.dll         # Client DLL with bundled_cef_path config
  chrome_elf.dll        # Must match exe signing (same cert, or both unsigned)
  cef/                  # Extracted CEF distribution
    cef_version.json
    catalog.cat
    Release/
      libcef.dll
      chrome_elf.dll
      *.pak, locales/, ...
```

The client DLL's `CEF_INSTALLER_CONFIG` resource should include:
```json
{
  "appid": "...",
  "vmin": "...",
  "abi_hash": "...",
  "bundled_cef_path": "cef"
}
```

The `bundled_cef_path` is resolved relative to the client DLL's directory.

**Run:**
```
test_dir/cefclient.exe
```

The bootstrap finds the bundled version via `bundled_cef_path`, validates it
against version/ABI constraints, and loads it in-place without any CDN access.

### Mode 3: Local Download Path (install from archive)

Tests the full CDN download + install flow using a local directory instead of
the network. The `/cef-download-path` switch redirects all CDN requests to
local file reads. All post-download validation (hash verification, signature
checking, archive extraction, version filtering) runs normally.

**Setup:** Create a local directory simulating the CDN layout. The installer
derives filenames from the URLs it would normally fetch:

```
cdn_dir/
  {abi_hash}_{platform}.json                        # Manifest (e.g., 1671cc913eeb4ecf_windows64.json)
  cef_binary_151.1.0_windows64_signed.tar.xz        # Signed archive
  cef_binary_151.1.0_windows64_signed.tar.xz.sha256 # SHA256 hash (hex string, no filename)
```

**Manifest filename:** When `abi_hash` is set in the config, the manifest
filename is `{abi_hash}_{platform}.json`. Without `abi_hash`, it is
`{milestone}_{platform}.json` (e.g., `151_windows64.json`).

The manifest is a JSON array of build entries. The example uses the current
CEF 151.1 sandbox compatibility hash; replace the archive digests and timestamp
with values from the actual test artifact.
```json
[
  {
    "version": "151.1.0",
    "file": "cef_binary_151.1.0_windows64_signed.tar.xz",
    "sha1": "70e10aff7a7b346ed24c3eb24c04b503b3f61f29",
    "last_modified": "2026-07-15T00:00:00Z",
    "abi_hash": "1671cc913eeb4ecf"
  }
]
```

The `.sha256` file contains just the hex hash (no filename):
```
2a4b43a1936e0d56511b98cf1ae497e8439710a2412e57379560561ab3f86124
```

**Run:**
```bash
# With a client DLL (installer resolves CEF before loading the client)
cefclient.exe /cef-download-path=C:\tmp\cdn_dir

# Standalone mode (no client DLL, requires a bootstrap exe resource)
cefclient.exe /cef-download-path=C:\tmp\cdn_dir

# With verbose logging (default log level is warning)
cefclient.exe /cef-download-path=C:\tmp\cdn_dir /cef-log-level=info
```

### Embedding Resources with ResourceHacker

You can add or update `CEF_INSTALLER_CONFIG` resources post-build using
[ResourceHacker](http://www.angusj.com/resourcehacker/):

1. Create `config.json`. Use the same `appid`, `vmin`, and `abi_hash` as the
   client DLL's config so the exe registers/unregisters the same application.
   `appid` and `vmin` are required — the config parse fails without them.
   ```json
   {
     "appid": "...",
     "vmin": "...",
     "abi_hash": "...",
     "enable_explicit_modes": true
   }
   ```

2. Create `config.rc`:
   ```rc
   CEF_INSTALLER_CONFIG RCDATA "config.json"
   ```

3. Compile and embed (run from the directory containing both files):
   ```bash
   ResourceHacker.exe -open config.rc -save config.res -action compile
   ResourceHacker.exe -open cefclient.exe -save cefclient.exe -action addoverwrite -res config.res -mask RCDATA,CEF_INSTALLER_CONFIG,
   ```

The `CEF_RESOURCE_HACKER_PATH` environment variable should point to
`ResourceHacker.exe` (also used by E2E tests).

### Testing Explicit Commands

```bash
# Install CEF from a local directory first (see Mode 3 above)
cefclient.exe /cef-download-path=C:\tmp\cdn_dir

# Uninstall (unregisters the app and prunes unused versions)
cefclient.exe /cef-uninstall

# Explicit update check (checks even when a local version qualifies, and
# downloads only a newer compatible non-revoked version)
cefclient.exe /cef-update /cef-download-path=C:\tmp\cdn_dir
```

Registration retention is a separate, explicit maintenance operation. Start
with dry-run, review the reported registrations and version impact, and apply
only after confirming that dormant applications may need to download CEF again:

```powershell
# Deterministic text report; makes no installer-state changes.
cefclient.exe /cef-retention-dry-run /cef-max-age-days=180

# JSON report for scripts and automated verification.
cefclient.exe /cef-retention-dry-run /cef-max-age-days=180 /cef-headless

# Recomputes under the writer lock before committing removals.
cefclient.exe /cef-retention-apply /cef-max-age-days=180
```

Retention commands require `enable_explicit_modes` in the bootstrap's embedded
configuration. Verify that dry-run leaves `installer.json`, `versions.json`,
`.launch`, version directories, and `retention_pending.json` unchanged. After
apply, inspect `retry_required`, `registrations_committed`, `versions_pruned`,
and `warnings`; rerun apply when `retry_required` is true.

To test revocation, add a `revoked.json` to the local CDN directory:
```json
{
  "revoked_versions": [
    { "version": "151.1.0", "reason": "testing revocation" }
  ]
}
```

Then uninstall any previously installed version and attempt a fresh install —
it should fail with "CDN version is revoked":
```bash
cefclient.exe /cef-uninstall
cefclient.exe /cef-download-path=C:\tmp\cdn_dir
```

Note: revocation does not block bundled installs (Mode 2). A revoked bundled
version is demoted but still used as a last resort when no other version is
available. See [Version Selection](README.md#version-selection) for details.

Check `cef_installer.log` in the install directory (e.g.,
`%LocalAppData%\CEF\cef_installer.log`) for detailed output. The install
directory also contains `installer.json` (registered apps) and
`versions.json` (installed versions) which can be inspected directly.

### Lock-free launch and concurrency

Focused unit coverage:

```powershell
out\Debug_GN_x64\cef_installer_unittests.exe `
  --gtest_filter='InstallerLockTest.*:InstallerFileOpsTest.*VersionLease*:ControllerTempDirTest.*Query*:ControllerTempDirTest.AutomaticStartup*:InstallerControllerServerTest.AutomaticStartup*:LaunchStateTest.*Liveness*:InstallerRevocationTest.*Backoff*'
```

The tests use deterministic events rather than sleeps to pause a startup at the
writer transition. They cover query/local startup while the mutex is held,
post-lock index re-resolution with zero duplicate requests, contention timeout,
one abandoned handoff across three waiters, lease-blocked directory move,
fresh-cache zero requests, corrupt-cache refresh, stale refresh affecting the
current selection, post-writer refresh gating, strict failure-backoff
integrity, asynchronous cancellation while response headers are withheld,
asynchronous cancellation while a response body remains open, and liveness
refresh/clock-skew rules. Mutex coverage includes consecutive abandonment and
wrong-thread release rejection. The lease rename behavior is Windows-specific
and must be rerun for every locally available supported architecture; absence
of a 32-bit output directory is a validation limitation, not 32-bit coverage.

Emergency recovery unit tests use missing, corrupt, and valid-empty index
fixtures. They assert the selected lease, unchanged index state, query
exclusion, bad-CRC preservation, generic-read-error fail-closed behavior,
post-eligibility duplicate handling, held-mutex behavior, candidate rejection,
and each scan bound. `SetEmergencyRecoveryScanLimitsForTesting()` reduces
individual limits to zero for deterministic stopping tests. The launcher E2E
case `test_bootstrap_recovers_missing_and_corrupt_version_index` exercises both
damaged-index states in a real bootstrap/client process. See
[Version Selection](README.md#version-selection) for the production contract
and limits.

## Fuzz Testing

Archive extraction code processes untrusted data from the network. Three
libFuzzer targets provide continuous coverage-guided testing of the parsing
and decompression layers.

| Fuzzer | Target code | Attack surface |
|--------|-------------|----------------|
| `installer_xz_index_fuzzer` | `ParseXzIndex()`, `ReadMultiByteInt()` | XZ footer/index binary parsing, integer overflow, offset arithmetic |
| `installer_tar_fuzzer` | `ExtractTarFromBuffer()`, `TarReader::ParseHeader()`, `ParsePaxPath()` | Path traversal, PAX/GNU extensions, octal parsing, file writes |
| `installer_xz_block_fuzzer` | `DecompressXzBlock()` | LZMA2 block header parsing, decompression, CRC verification |

### Building Fuzzers

Create a fuzzer build directory by copying the existing debug config:

```bash
mkdir out/libfuzzer
cp out/Debug_GN_x64/args.gn out/libfuzzer/args.gn
```

Edit `out/libfuzzer/args.gn` and add or modify these settings:

```gn
# Changed from Debug_GN_x64:
is_component_build = false  # was true; libfuzzer requires static linking
is_debug = false            # was true; release build for fuzzer performance

# Added for fuzzing:
use_libfuzzer = true        # enable libFuzzer instrumentation
is_asan = true              # enable AddressSanitizer (catches buffer overruns, use-after-free)
is_ubsan_security = true    # enable UBSan security checks (integer overflow, shift UB)
is_ubsan_vptr = false       # vptr check unsupported on Windows
```

Then generate the build files:

```bash
gn gen out/libfuzzer
```

Keep all other settings from `Debug_GN_x64/args.gn` unchanged, such as
`target_cpu`, CDM keys, and codec flags. Only the six values above need to
change. UBSan's `vptr` check requires RTTI, which is disabled on Windows
(`/GR-`), so `is_ubsan_vptr` must be explicitly false.

Build the fuzzer targets:

```bash
autoninja -C out/libfuzzer installer_xz_index_fuzzer installer_tar_fuzzer installer_xz_block_fuzzer
```

### Running Fuzzers

```bash
# Run with seed corpus (recommended for the first run):
out/libfuzzer/installer_xz_index_fuzzer.exe cef/libcef_dll/bootstrap/installer/testdata/

# Run from an empty corpus:
out/libfuzzer/installer_tar_fuzzer.exe

# Run for a fixed duration:
out/libfuzzer/installer_xz_block_fuzzer.exe -max_total_time=300

# Reproduce a crash:
out/libfuzzer/installer_tar_fuzzer.exe crash-abc123...
```

Output lines containing `NEW` indicate that the fuzzer discovered inputs
covering previously unseen code edges. A crash writes the input to
`crash-<hash>` in the current directory.

### Design Notes

- The XZ index fuzzer neuters the CRC table, following the pattern in
  `third_party/lzma_sdk/google/seven_zip_reader_fuzzer.cc`, so mutations are
  not rejected at the CRC check before reaching deeper parsing logic.
- The tar fuzzer does not neuter checksums. Tar header checksums are simple sums
  that the fuzzer discovers organically, and `ValidateChecksum()` is itself
  worth fuzzing.
- The tar fuzzer caps input at 64 KB (`max_len=65536`) to keep iterations fast
  because `ExtractTarFromBuffer` performs file I/O.

When adding a code path that parses untrusted archive data, add a corresponding
fuzzer target or extend an existing one to cover the new parsing logic. The
authoritative extraction invariants are in
[Archive Extraction](SECURITY.md#3-archive-extraction).

## Code Coverage

### Prerequisites

See [Coverage Setup Instructions](../../../tools/claude/CLAUDE_COVERAGE_INSTRUCTIONS.md)
for one-time setup (enabling coverage tools in `.gclient`, creating a coverage
build directory).

### Generating a Coverage Report

Coverage requires a separate build directory (`out/coverage`) with coverage
instrumentation enabled. Build the tests in that directory first:

```bash
autoninja -C out/coverage cef_installer_unittests
```

Then generate the report:

```bash
python3 tools/code_coverage/coverage.py \
    cef_installer_unittests \
    -b out/coverage \
    -o out/report \
    -c out/coverage/cef_installer_unittests.exe \
    -f cef/libcef_dll/bootstrap/installer \
    --coverage-tools-dir=third_party/llvm-build/Release+Asserts/bin \
    --no-component-view
```

This produces:
- `out/report/win/summary.json` -- machine-readable summary
- `out/report/win/coverage.profdata` -- raw profile data

### Analyzing Coverage

The analysis commands below use the `analyze_coverage_output.py` script. The
`--profdata`, `--binary`, and `--llvm-cov` flags are needed for detailed
analysis (uncovered function names and line ranges); omit them for a quick
summary.

```bash
# Quick summary (filtered to installer files)
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --filter cef/libcef_dll/bootstrap/installer

# Detailed analysis (includes uncovered function names)
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --filter cef/libcef_dll/bootstrap/installer \
    --profdata out/report/win/coverage.profdata \
    --binary out/coverage/cef_installer_unittests.exe \
    --llvm-cov third_party/llvm-build/Release+Asserts/bin/llvm-cov.exe

# Show uncovered line ranges for a specific file
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --profdata out/report/win/coverage.profdata \
    --binary out/coverage/cef_installer_unittests.exe \
    --llvm-cov third_party/llvm-build/Release+Asserts/bin/llvm-cov.exe \
    --show-uncovered-lines cef/libcef_dll/bootstrap/installer/installer_controller.cc

# Show uncovered lines for all installer source files
python3 cef/tools/claude/analyze_coverage_output.py \
    out/report/win/summary.json \
    --profdata out/report/win/coverage.profdata \
    --binary out/coverage/cef_installer_unittests.exe \
    --llvm-cov third_party/llvm-build/Release+Asserts/bin/llvm-cov.exe \
    --show-uncovered-lines "cef/libcef_dll/bootstrap/installer/*.cc"
```

For details on how `--show-uncovered-lines` computes coverage from LLVM regions,
see [How `--show-uncovered-lines` Works](../../../tools/claude/CLAUDE_COVERAGE_INSTRUCTIONS.md#how---show-uncovered-lines-works).

### Coverage Expectations

**Function coverage:** All production files should have 100% function coverage.

**Line coverage:** Overall line coverage should stay at or above 90%. Files that
currently have 100% line coverage (per `llvm-cov report`) must not regress. When
adding new code, add corresponding tests to maintain coverage.

**Acceptable exceptions from coverage:**

- **Singleton destructors** (e.g., `Logger::~Logger`) -- intentionally never
  called due to `base::NoDestructor`.
- **WinHTTP internals** (e.g., `ScopedWinHttpHandle` default constructor,
  `DownloadSink::OnError` base class method) -- trivial internal plumbing
  exercised implicitly through download calls.
- **Interactive message-box backend** (`MessageBoxExW`) -- requires user
  interaction. `ShowErrorDialog` and `ShowErrorMessageBox` are covered through
  the test message-box runner.
- **Process creation** (e.g., `CopyFileIfExists` in installer_relaunch.cc)
  -- requires real process spawning that cannot be mocked in unit tests.
- **DLL export function** (`RunInstaller`) -- thin C ABI wrapper around
  `Controller::Run()`; tested indirectly through the controller.
- **Registry path validation** (`IsRegistryPathValid` in installer_paths.cc)
  -- requires HKLM registry entries that unit tests cannot create without
  admin privileges.
- **`TryLoadInstallerConfig` official-build branch** -- the
  `enable_explicit_modes` command gate in official builds is unreachable in
  non-official test builds. Resource provenance and bootstrap-only policy
  parsing are covered by `ConfigPrecedenceTest`.
- **Unreachable switch fallthroughs** -- default cases in exhaustive switch
  statements that exist for compiler warnings but can never execute.
