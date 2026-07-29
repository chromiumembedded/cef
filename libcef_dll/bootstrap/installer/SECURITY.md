# CEF Installer Security Model

This document defines the security invariants, trust boundaries, and defensive
controls for the CEF installer library. It is the authoritative reference for
developers modifying this code.

---

## How to Use This Document

### When writing implementation plans

Reference this document in the plan and include a security section that maps
planned changes to affected invariants. For each new code path, check every
"If you change" callout in the relevant invariant sections and add
corresponding exit criteria. Example from the parallel XZ extraction plan:

> **Security (S1):** `ExtractTarFromBuffer()` enforces `kMaxFileSize`,
> `kMaxTotalExtractionSize`, `kMaxEntryCount`, and skips symlinks
> *(from Invariant 3: Archive Extraction)*

### When reviewing plans or code

Use the invariant tables as a checklist. For each file touched by the change,
find the matching invariant section and verify every row still holds. Pay
special attention to:
- New data paths that bypass existing validation
- New testing bypasses that need `OFFICIAL_BUILD && NDEBUG` guards
- New check-then-use patterns that should be added to the TOCTOU table

### When updating this document

Update invariant tables when:
- A new security control is added (new row)
- An existing control moves to a different file or function (update location)
- A new TOCTOU race or testing bypass is introduced (add to relevant table)

Do NOT add review progress, findings, or task-specific notes here. Those
belong in review plans or issue trackers.

---

## Trust Boundaries

| Boundary | Trusted | Untrusted |
|----------|---------|-----------|
| CDN HTTPS | Installer process | CDN server responses (manifests, archives, revocation list) |
| Disk (admin dirs) | Administrator-owned ACL boundary | Standard users and lower-integrity processes |
| Disk (`%LocalAppData%`/user custom dirs) | User-integrity applications share the user's authority | Network input and accidental corruption; not arbitrary native code running as that user |
| Application CDN selection | Client and bootstrap resources execute at the owning user's integrity | Not an administrator boundary; another native process running as that user has equivalent authority |
| IPC (WM_COPYDATA) | Installer mutation/result state and nonce-bound relaunch context | Parent window and all messages/return values; public lifecycle IDs provide correlation, not authentication |
| Config sources | Client DLL and bootstrap embedded resources (authoritative when selected) | Command-line args; `RunInstaller` JSON is caller-controlled operation input |
| Archive contents | N/A (always untrusted) | tar.xz entries: filenames, sizes, types, compressed data |
| Revocation cache | Compiled baseline (immutable RT_RCDATA) | Disk-cached CDN delta (writable by install-dir owner) |

---

## Assumptions

- HTTPS/TLS termination is sound (system cert store is trusted).
- The code signing certificate private key is not compromised.
- The compiled-in revocation baseline cannot be tampered with at runtime.
- Chromium `base::` primitives (file I/O, crypto, threading) are sound.

---

## Security Invariants

### 1. Signature Verification

Every downloaded CEF distribution MUST be verified against a pinned
certificate thumbprint before publication. The default user-writable store is
not an ongoing authenticity boundary against arbitrary native code running as
that user. Client-side runtime signer checks are defense in depth and do not
reauthenticate every catalog-covered dependency/resource.

Bundled CEF is a different trust path. `CheckBundledCef()` requires
`catalog.cat` to exist as a regular non-reparse file, but that is only a layout
check; startup does not verify its signature or catalog members. Bundled
integrity rests on the application's signed package.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| All installed files are verified against a signed catalog (`catalog.cat`) | `VerifyWithCatalog()` | `installer_signature.cc` |
| Catalog signature is verified BEFORE file hashes are checked | `VerifyWithCatalog()` call order | `installer_signature.cc` |
| Certificate thumbprint is always pinned (never empty) in production paths | `kCefCertificateThumbprint` default, traced from `DownloadAndInstall()` | `installer_constants.h`, `installer_controller.cc` |
| Bundled `catalog.cat` presence is a non-reparse layout check, not runtime authentication | `CheckBundledCef()` | `installer_controller.cc` |
| `SetSignatureTestingMode()` is a no-op in official builds | `#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))` guard | `installer_signature.cc` |
| Authenticode revocation checking is disabled (no network dependency) | `WTD_REVOKE_NONE` flag | `installer_signature.cc` |
| `GetSignatureThumbprint()` only returns thumbprints from valid signatures | Return-empty-on-failure pattern | `installer_signature.cc` |

A trusted `RunInstaller` caller may replace the default CEF thumbprint with
another non-empty expected thumbprint. This still requires a Windows-valid
catalog signature matching that certificate; it does not permit unsigned
publication in official builds. Alternative-signer applications should use
distinct `install_path` namespaces to prevent compatibility collisions. This
separation is not a boundary against the user who owns a user-writable store.
The cefclient reference additionally pins CDN-installed `libcef.dll` at load
time as defense in depth, but does not reauthenticate every catalog member.

**If you change:** A new downloaded-install publication path MUST call
`VerifyWithCatalog()` with a non-empty `expected_thumbprint` before
publication. A new bundled/load path must document and test its source-
appropriate trust behavior instead of implying ongoing catalog authentication.

### 2. Download Security

All network downloads MUST use HTTPS and enforce size limits.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Only HTTPS URLs accepted in production | `IsValidDownloadUrl()` | `installer_download.cc` |
| `allow_http_for_testing` compiled out in official builds | `#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))` | `installer_download.cc` |
| Downloads capped at `kDefaultMaxDownloadSize` (500 MB); revocation list capped at 1 MB | `DownloadOptions::max_download_size` check after download | `installer_download.cc`, `installer_controller.cc` |
| File deleted on size-limit excess | Post-download check in `DownloadFile()` | `installer_download.cc` |
| SHA256 hashing uses streaming (no full-file memory load) | `ComputeFileSha256()` via `crypto::SecureHash` | `installer_download.cc` |
| Hash mismatch causes file deletion | Error path in `DownloadFile()` | `installer_download.cc` |
| Generic downloads clean temporary files; resumable archives retain only safe origin-bound partials after interruption/cancellation and prune them after seven days | `DownloadFile()`, `PruneCacheDirectory()` | `installer_download.cc` |
| HKLM policy URL lists require HTTPS and forbid userinfo, query, and fragment components | `ValidateEnterprisePolicySnapshot()` | `installer_policy.cc` |
| Policy mirrors retain mandatory hash/catalog/signature validation | Effective source selects transport only | `installer_policy.cc`, `installer_controller.cc` |
| Application `cdn_urls` are one-to-three bounded HTTPS prefixes without userinfo/query/fragment | Shared validation before source resolution | `installer_config.cc`, `installer_policy.cc`, `installer_controller.cc` |
| Resumable archive state is bound to configured and post-redirect final-origin digests | Deterministic partial naming and pre-body origin comparison | `installer_download.cc` |
| A partial appends only after a strict single-range `206`; `200` restarts from zero | Range state machine and `Content-Range` parser | `installer_download.cc` |
| Complete cache publication requires final length and SHA256/SHA1 validation | Hash-before-move promotion | `installer_download.cc` |

HKLM enterprise policy is administrator authority over shared-installer
behavior. Policy URL and mirror sources override application/operation
transport inputs, but do not weaken post-download verification. A policy
mirror may persist revocation deltas so read-only managed users consume the
administrator-curated view; command-line and API local paths cannot. See
[ADMIN_POLICY.md](ADMIN_POLICY.md) for registry and operational procedures.

`DisableDownloads` accepts the availability tradeoff that revocation freshness
becomes administrator-curated. It stops external refreshes but never disables
compiled or integrity-valid cached revocation enforcement. Policy does not
restrict bundled or unchecked CEF shipped by the application itself; the
shared installer is not a security boundary against same-user application
code.

Application source configuration in a client DLL or bootstrap resource
likewise operates at user integrity. HTTPS,
transport hashes, catalog/signature verification, the compiled revocation
baseline, the vmin clamp, platform checks, and ABI matching protect source
authenticity, corruption detection, and compatibility. They do not create a
security boundary against arbitrary native code already running as that user.

A stale or misconfigured application/policy mirror can reduce availability,
serve an older still-valid build, omit a newer revocation delta, or consume
cache space with failed downloads. Per-download limits, the additive compiled
revocation baseline, mandatory signature/hash checks, and seven-day complete/
partial archive pruning bound those effects; there is intentionally no
aggregate cache quota.

Completed archives and installed distributions do not retain origin
provenance because their hashes and signed catalogs establish content
identity. Transient partials are different: their filenames carry only bounded
digests of the normalized configured and post-redirect final origins. A change
to either identity discards the partial before response bytes can be appended,
so one archive body is never assembled across origins. Raw URLs, paths,
queries, fragments, and credentials are not stored in partial names or
metadata.

**If you change:** Adding a new download path? It MUST go through
`DownloadFile()` or `DownloadToString()` — never call WinHTTP functions
directly.

### 3. Archive Extraction

All archive extraction paths MUST enforce resource limits and path safety.
These limits exist in `installer_archive.cc` and MUST be replicated in any
new extraction path (e.g., parallel extraction, buffer-based extraction).

| Limit | Constant | Value | Purpose |
|-------|----------|-------|---------|
| Max decompressed buffer | `kMaxDecompressedSize` | 512 MB | Prevents decompression bomb in streaming buffer |
| Max file size | `kMaxFileSize` | 4 GB | Per-file extraction limit |
| Max total extraction | `kMaxTotalExtractionSize` | 10 GB | Aggregate across all entries |
| Max path length | `kMaxPathLength` | 4096 | Prevents excessive path construction |
| Max entry count | `kMaxEntryCount` | 100,000 | Prevents entry-count DoS |

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Every entry path passes through `IsPathSafe()` | `TarReader::IsPathSafe()` (both string and FilePath overloads) | `installer_archive.cc` |
| `IsPathSafe()` rejects: `../`, absolute paths, UNC paths, Windows reserved names, null bytes, oversized paths | Explicit checks in `IsPathSafe()` | `installer_archive.cc` |
| PAX path overrides pass through `IsPathSafe()` | Check after `ParsePaxPath()` | `installer_archive.cc` |
| GNU long name paths pass through `IsPathSafe()` | Check after long-name parsing | `installer_archive.cc` |
| Symlinks are skipped (never extracted) | Type check in extraction loop | `installer_archive.cc` |
| Hard links, device nodes, FIFOs are rejected | Type check → `kOther` | `installer_archive.cc` |
| Size arithmetic uses `base::CheckedNumeric` | Checked accumulation in extraction loop | `installer_archive.cc` |
| Tar header checksum is validated | `ValidateChecksum()` | `installer_archive.cc` |

**If you change:** Adding a new extraction path (e.g., `ExtractTarFromBuffer`,
parallel block extraction)? It MUST enforce ALL of the above limits. Copy the
security checks from the existing streaming extraction loop, and add or extend
a fuzz target as described in [Fuzz Testing](TESTING.md#fuzz-testing).

### 4. File System & Path Security

All file operations on install directories MUST check for reparse points
(symlinks/junctions) and validate paths.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Readable dirs never include lower-priority locations than the writable dir | `ResolveInstallDirectories()` stops at first writable | `installer_paths.cc` |
| Install/uninstall source dirs checked for reparse points | `IsDirectorySafe()` → `IsReparsePoint()` | `installer_file_ops.cc` |
| All discovered install paths checked for reparse points | `IsReadableDirectory()` → `IsReparsePoint()` | `installer_paths.cc` |
| `Versions/` parent directory checked for reparse points | `IsReparsePoint(versions_dir)` before enumerating or installing | `installer_file_ops.cc`, `installer_paths.cc`, `installer_version_metadata.cc` |
| Version directories checked during scanning | `ScanInstalledVersions()` reparse check | `installer_paths.cc` |
| Pre-operation reparse point cleanup (staging, cache, archive paths) | `VerifySafeFilePath()` / `VerifySafeDirectoryPath()` — removes reparse point then optionally deletes | `installer_file_ops.cc`, `installer_controller.cc` |
| Cache directory checked before read/write/enumerate | `IsReparsePoint()` in `DownloadWithCache()`, `PruneCacheDirectory()` | `installer_download.cc` |
| Cache files checked before read (via `IsCacheValid()`) and before write | `IsReparsePoint()` in `IsCacheValid()`, `DownloadWithCache()` | `installer_download.cc` |
| Cache, revocation, and database files have CRC32 integrity footer | `WriteFileWithIntegrity()` / `ReadFileWithIntegrity()` — doom on mismatch | `installer_file_integrity.cc` |
| Database uses atomic write (temp file + rename) | `Database::Save()` via `base::ReplaceFile()` | `installer_database.cc` |
| Database corruption suspends pruning for 7-day grace period | `SuspendPruning()` / `CanPrune()` with persisted `pruning_suspended_until` timestamp | `installer_database.cc`, `installer_controller.cc` |
| Revocation cache file checked before read and write | `IsReparsePoint()` on dir and file in both `WriteRevocationCache()` and `LoadRevocationCache()` | `installer_revocation.cc` |
| Temp directory checked after creation (relaunch) | `IsReparsePoint()` after `CreateDirectory()` | `installer_relaunch.cc` |
| Registry paths come from HKLM only (not HKCU) | `HKEY_LOCAL_MACHINE` in `ResolveRegistryPath()` | `installer_paths.cc` |
| Registry paths validated as absolute, no parent refs | `IsAbsolute()`, `!ReferencesParent()` | `installer_paths.cc` |
| Custom `install_path` is exclusive (fail hard, no fallback) | Early return in `ResolveInstallDirectories()` | `installer_paths.cc` |
| Custom `install_path` remains readable when safe but non-writable; query does not create or probe it | operation-aware custom resolution | `installer_paths.cc` |
| Directory roles come from candidate source, not ACLs/path prefixes | role-tagged candidate construction and stable deduplication | `installer_paths.cc` |
| Official automatic startup cannot mutate admin-default roles without the trusted bootstrap gate | `IsAdminMutationAllowed()` before candidate write probing | `installer_paths.cc`, `installer_controller.cc` |
| All returned libcef.dll paths validated for reparse points before use | `MakeValidatedLibcefResult()` → `IsPathSafeForLoading()` | `installer_controller.cc`, `installer_file_ops.cc` |
| Launch state read/write/scan check `.launch/` dir for reparse points | `IsReparsePoint()` guard in `ReadLaunchStatePath()`, `WriteLaunchStatePath()`, and `ScanLaunchStates()`; writer-locked controller paths repair unsafe directories | `installer_launch_state.cc`, `installer_controller.cc` |
| Launch records require a valid CRC footer and bounded current-schema JSON; health `last_update` and liveness `last_launch` must be nonzero | `ReadJsonWithIntegrity()`, `ParseLaunchStateDict()`, `ParseLivenessDict()` | `installer_launch_state.cc` |
| Launch-record GC trusts only CRC-verified content time, ignores filesystem timestamps, preserves future timestamps and unavailable time, and expires at the exact 90-day boundary | `ClassifyLaunchStateGcAge()`, `PruneUnusedVersions()` | `installer_launch_state.cc`, `installer_controller.cc` |
| GC observes one bounded no-follow publication with its resolved parent and conditionally deletes only the identical content at that location; invalid repair uses exact raw bytes, an opened-object oversized check, or same-object reparse removal | `ReadLaunchRecordSnapshot()`, `DeleteFileWithIntegrityIfMatching()`, `DeleteFileRawIfMatching()`, `DeleteFileIfOversized()`, `DeleteFileIfReparsePoint()` | `installer_launch_state.cc`, `installer_controller.cc`, `installer_file_integrity.cc` |
| Read-only launch-state paths preserve corrupt, footerless, old-schema, noncanonical, and indeterminate files; writer-locked prune conditionally repairs only conclusive observations | preserve-on-mismatch readers and `PruneUnusedVersions()` repair snapshots | `installer_launch_state.cc`, `installer_controller.cc` |
| Orphaned and below-vmin launch records never protect executable versions, including while age-retained or when time is future/unavailable | GC eligibility before confirmed protection in `PruneUnusedVersions()` | `installer_controller.cc` |
| Empty install roots never produce relative `.launch` paths | Launch-health evaluation returns early and launch-state result paths require a non-empty install directory | `installer_launch_state.cc`, `installer_controller.cc` |
| Atomic rename via `MoveFileExW` with `MOVEFILE_WRITE_THROUGH` | `InstallVersion()` | `installer_file_ops.cc` |
| Staging under install dir guarantees same-volume atomic rename | `<install_dir>/.staging/` in `DownloadAndInstall()` | `installer_controller.cc` |
| Trash directory uses cryptographic random suffix | `base::RandUint64()` in `GenerateTrashPath()` | `installer_file_ops.cc` |
| Once verified staging exists, every safe regular-file or directory publication target is moved whole to reparse-safe `.trash/` without reading its contents; verified staging is then renamed into place, and reclamation deletes nested reparse points without traversing them | `InstallVersion()` | `installer_file_ops.cc` |
| Mandatory target replacement is limited to verified install/update publication under the writer lock; missing/damaged-index recovery, query/read-only selection, prune, and retention scans remain unchanged | Controller call graph | `installer_controller.cc` |

Launch health is heuristic local compatibility data, not authentication,
security policy, or fleet-wide revocation. It is disabled by default and keyed
by appid, platform, and version. Opted-in classification uses a boot boundary
and PID creation time to reduce stale-process attribution, but task termination,
OOM, and logoff remain ambiguous. Integrity writes use same-directory atomic
replacement; CRC detects corruption but does not authenticate content.
Launch-record age is availability-oriented cleanup metadata, not a trust
boundary: writers own health `last_update`, zero is invalid, future values and
an unavailable clock preserve state, and filesystem times are never evidence.

**Priority search and privilege separation:** `ResolveInstallDirectories()`
walks candidates in priority order (registry → ProgramFiles → LocalAppData),
collecting readable directories and stopping at the first permitted writable
one. Roles are source-derived. Elevated processes never continue to
LocalAppData. In official automatic startup, a closed trusted explicit-mode
gate makes HKLM and ProgramFiles read-only even if their ACLs permit writes.
Custom paths are deployment-owned and require ACLs appropriate to the
application's execution level.

**If you change:** Adding a new path discovery source or file operation? For
read-only checks (enumeration, validation), use `IsReparsePoint()` or
`IsReadableDirectory()`. For paths that need reparse-point removal before use
(e.g., staging or cache directories the installer is about to create or write
to), use `VerifySafeFilePath()` or `VerifySafeDirectoryPath()`. Be aware of
TOCTOU — minimize the window between the check and the operation.

### 5. Config & Input Parsing

All parsed data from untrusted sources MUST be length-bounded and validated.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Client resources win when valid; only absence permits bootstrap-resource fallback; standalone mode reads only the bootstrap resource | `TryLoadInstallerConfig()` tri-state result and source provenance | `installer_bootstrap_helpers.cc` |
| Malformed or invalid selected resources fail with config error 100 and never fall through | `TryLoadInstallerConfig()` tri-state result | `installer_bootstrap_helpers.cc` |
| `install_path` is accepted from the client DLL resource only, resolved relative to that DLL, and ignored in bootstrap resources | parse option plus `ResolveClientInstallPath()` | `installer_config.cc`, `installer_bootstrap_helpers.cc` |
| Unrecognized application-config keys warn individually but remain non-fatal for forward compatibility | recognized-key audit in `ParseConfigFromJson()` | `installer_config.cc` |
| Application `cdn_urls` accepted from the selected client/bootstrap resource; operation `cdn_urls` accepted from `RunInstaller` JSON | Shared strict one-to-three URL-list parser; selected application lists never merge | `installer_config.cc`, `installer_controller.cc`, `installer_policy.cc` |
| HTTPS enforced for every parsed `cdn_urls` entry | `ValidateAndNormalizeCdnUrls()` before source resolution and `IsValidDownloadUrl()` before requests | `installer_policy.cc`, `installer_download.cc` |
| CDN manifest filenames validated | `IsValidArchiveFilename()` before URL construction | `installer_cdn_manifest.cc` |
| JSON string fields bounded by `kMax*Length` constants in CDN manifest, database, and metadata parsers | `ParseBuildEntry()`, `Database::Load()`, `ReadVersionMetadata()` | `installer_cdn_manifest.cc`, `installer_database.cc`, `installer_version_metadata.cc` |
| Log messages sanitized against injection | `SanitizeForLog()` escapes control characters | `installer_logger.cc` |
| Parent HWND treated as untrusted (no ownership, only positioning) | `GetWindowRect()` only, with `IsWindow()` check | `installer_progress_dialog.cc` |
| `parent_window` parses a decimal string or exactly represented numeric value with `uintptr_t` bounds; truncation and rounding are rejected | `ParseExtendedConfigFromJson()`, `ParseExtendedConfigFromCommandLine()` | `installer_controller.cc`, `installer_bootstrap_helpers.cc` |
| Official builds read `enable_explicit_modes` only from the bootstrap's embedded resource; a missing or false gate blocks explicit commands and standalone auto-install | trusted bootstrap provenance and explicit-mode checks | `installer_bootstrap_helpers.cc`, `bootstrap_win.cc` |
| Structured installer error messages are diagnostic data only and are never interpreted as commands or paths | Startup result and JSON serialization only | `installer_bootstrap_helpers.cc`, `installer_controller.cc` |
| Known HKLM policy values are exact-type validated in one immutable operation snapshot; malformed/conflicting policy returns 119 | `LoadEnterprisePolicy()`, `ValidateEnterprisePolicySnapshot()` | `installer_policy.cc`, `installer_registry.cc` |
| `WM_COPYDATA` progress and lifecycle messages contain no credentials, nonce, or filesystem paths | Bounded progress fields and normalized path-free lifecycle results | `installer_controller.cc`, `installer_lifecycle.cc` |

See [Config Security](README.md#config-security) for the integrator-facing
configuration source priority and fallback behavior.

The explicit-mode gate prevents a trusted, code-signed bootstrap from being
repurposed through renaming or copying. Signature verification prevents
arbitrary downloaded code, but without this gate the bootstrap could still be
abused for network access, disk consumption, or installer-state poisoning. See
[Standalone Mode Security](README.md#standalone-mode-security) for the
integrator-facing behavior matrix.

The parent HWND may belong to a lower-integrity process while the installer is
elevated. The installer therefore never establishes window ownership, changes
the parent window's state, or grants it control over another process. It uses
the HWND only for positioning and bounded progress/cancellation messages. See
[Parent Window Handle](README.md#parent-window-handle-cross-process-security)
for the integration contract.

**Field length limits** (from `installer_constants.h`):

| Constant | Limit | Used for |
|----------|-------|----------|
| `kMaxVersionLength` | 64 | Version strings |
| `kMaxUuidLength` | 256 | Application UUIDs |
| `kMaxAbiHashLength` | 256 | ABI compatibility hashes |
| `kMaxFilenameLength` | 256 | Archive filenames |
| `kMaxSha1Length` | 40 | SHA1 hex strings |
| `kMaxSha256Length` | 64 | SHA256 hex strings |
| `kMaxTimestampLength` | 64 | ISO 8601 timestamps |
| `kMaxVersionFullLength` | 256 | Full version strings |
| `kMaxPlatformLength` | 32 | Platform strings |
| `kMaxReasonLength` | 1024 | Revocation reason strings |
| `kMaxRevocationDownloadSize` | 1 MB | CDN revocation list download cap |
| `kMaxRevocationCacheFileSize` | 2 MB | On-disk revocation cache file size cap |
| `kMaxRevocationEntryCount` | 10,000 | Revocation list entry count cap (prevents CPU exhaustion during parse/merge) |

**If you change:** Adding a new JSON parser or data source? It MUST enforce
length limits from `installer_constants.h` on all string fields. If you need
a new limit, add it to that file with a comment explaining the rationale.

### 6. Version Revocation

Revoked versions MUST be excluded from installation and use.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Compiled baseline loaded from immutable `RT_RCDATA` resource | `LoadCompiledRevocationList()` | `installer_revocation.cc` |
| `MergeRevocationLists()` is additive-only (CDN cannot shrink baseline) | Union merge logic | `installer_revocation.cc` |
| Revoked installed versions excluded from selection | `FindBestVersion()` filters against revocation list | `installer_version_resolver.cc` |
| Revoked CDN versions blocked from download | `IsVersionRevoked()` check before download | `installer_controller.cc` |
| Revoked bundled versions demoted | Priority adjustment | `installer_controller.cc` |
| Effective revocations merge the compiled baseline with valid caches in every readable directory | `LoadEffectiveRevocationList()` seed passed through selection and prune | `installer_revocation.cc`, `installer_controller.cc` |
| Revocation is authoritative over registration and confirmed prune protection | `GetRequiredVersionSet()`, `GetPrunableVersions()` | `installer_version_resolver.cc` |

**Accepted deviation:** The CDN revocation list is not cryptographically
signed. Residual risk is mitigated by the compiled baseline (cannot be
removed) and by the fact that adding fake entries only causes DoS (revokes
legitimate versions), not code execution.

**If you change:** Adding a new version source (e.g., local cache, peer
discovery)? It MUST be filtered against the merged revocation list before use.

### 7. Concurrency & Locking

Only one installer instance may modify a given install directory at a time.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Global named mutex with `Global\` prefix | `SingletonLock::Acquire()` | `installer_lock.cc` |
| Lock scoped per install directory (hash-based name) | Name derivation in `SingletonLock` | `installer_lock.cc` |
| Kernel abandonment grants exactly one waiter ownership; same-thread recursion is rejected and release asserts owner thread | `WAIT_ABANDONED` handling and owner tracking | `installer_lock.cc` |
| Historical semaphore/new mutex object-type collision fails closed | `CreateMutexW()` failure on existing semaphore name | `installer_lock.cc` |
| RAII release (destructor releases mutex) | `~SingletonLock()` | `installer_lock.cc` |
| Authoritative database writes occur under the mutex; startup registration is zero-timeout best effort | `Controller::Run()`, `TryRegisterStartup()` | `installer_controller.cc` |
| Registration, index publication, reconciliation, and prune ownership are scoped to one selected writable directory; cross-store registrations are not aggregated | controller passes only `install_dir` to authoritative operations | `installer_controller.cc` |
| Query and automatic-startup offline selection do not acquire the writer mutex | pre-lock dispatch and `ResolveStartupOffline()` | `installer_controller.cc` |
| Automatic-startup emergency scanning is limited to confirmed missing/corrupt indexes; non-dooming reads fail closed on generic I/O errors and valid indexes remain authoritative | per-root index status plus bounded scan in `ResolveStartupOffline()` | `installer_controller.cc`, `installer_version_metadata.cc` |
| Installed startup result holds a no-delete `libcef.dll` lease through client execution | `AcquireVersionLease()`, `InstallerStartupState` | `installer_file_ops.cc`, `bootstrap_win.cc` |
| Checked add publication is directory -> expanded index -> registration | `DownloadAndInstall()`, `PublishRegistration()` | `installer_controller.cc` |
| Checked removal publication is registration -> reduced index -> whole-directory trash move | `UpdateDatabase()`, `PruneUnusedVersions()` | `installer_controller.cc` |
| Locked prune orphan classification requires a valid index reread and never rebuilds an invalid index; ordinary prune retains its existing scan/rebuild behavior after index-read failure | `QuarantineUnindexedVersions()`, `kPrune` dispatch, `PruneUnusedVersions()` | `installer_controller.cc` |
| Query/read-only selection never invokes reconciliation, repair, prune, network, or cache mutation | command dispatch and `LoadQueryRevocations()` | `installer_controller.cc` |
| Only an integrity-valid, parseable fresh revocation cache means zero HTTP; stale/invalid launch refresh uses a single-attempt asynchronous request canceled at an absolute deadline and integrity-protected failure backoff | cache freshness/backoff gate and asynchronous request cancellation | `installer_revocation.cc`, `installer_download.cc`, `installer_controller.cc` |

**If you change:** Adding a new operation that modifies the install directory
or database? It MUST run while `SingletonLock` is held.

### 8. Process Launch (Uninstall Relaunch)

Self-copy-and-relaunch for uninstall MUST not introduce privilege escalation
or path injection vectors. Routing MUST select a mutation-permitted target
before authorizing mutation and MUST fail closed when physical containment
cannot be established. Once relaunch is required, failure to prepare or start
the child MUST NOT fall back to direct mutation.

| Invariant | Enforced by | File |
|-----------|-------------|------|
| Original uninstall resolves enterprise policy and one mutation-permitted writable target before routing; no writable target cannot trigger relaunch | `PrepareUninstall()`, `ResolveInstallDirectories()` | `installer_bootstrap_helpers.cc`, `installer_paths.cc`, `installer_policy.cc` |
| Bootstrap CLI direct dispatch requires a prepared snapshot bound to uninstall, configuration, install path, and the `kInProcess` decision; `RunInstaller("uninstall")` resolves its own state through `Controller::Run()` | `MaybeRunInstaller()`, `Controller::RunPreparedUninstall()` validation | `bootstrap_win.cc`, `installer_controller.cc` |
| Physical containment is tri-state over handle-resolved identities; an indeterminate ordinary-uninstall result returns 105 before mutation or relaunch | `GetPhysicalPathContainment()`, `DecideUninstallExecution()` | `installer_paths.cc`, `installer_bootstrap_helpers.cc`, `bootstrap_win.cc` |
| The internal relaunch marker and state are accepted only as a complete pair for explicit uninstall; the child executable must be physically contained by the installer temp directory and state must pass reparse, size, JSON, nonce, and absolute-path validation | `ResolveUninstallInvocationContext()`, `IsRunningFromTempDirectory()`, `GetPhysicalPathContainment()`, `ReadUninstallRelaunchState()` | `installer_bootstrap_helpers.cc`, `installer_paths.cc`, `installer_relaunch.cc` |
| Temp directory uses 64-bit cryptographic random suffix | `base::RandUint64()` | `installer_relaunch.cc` |
| Temp directory checked for reparse points after creation | `IsReparsePoint()` post-`CreateDirectory()` | `installer_relaunch.cc` |
| `CreateProcessW` uses full path (no PATH search) | Explicit path construction | `installer_relaunch.cc` |
| Exe path is quoted in command line | Quoting in command-line construction | `installer_relaunch.cc` |
| Relaunched process inherits same privilege level | No elevation request | `installer_relaunch.cc` |
| Failure to prepare or start a required relaunch returns 105 and never falls back to direct mutation | terminal relaunch branch in `MaybeRunInstaller()` | `bootstrap_win.cc` |
| Optional lifecycle state binds one public operation ID and canonical decimal parent HWND to the private nonce state; partial, malformed, overflowed, or command-mismatched values fail closed | `WriteUninstallRelaunchState()`, `ReadUninstallRelaunchState()` | `installer_relaunch.cc` |
| Lifecycle handoff occurs only after successful child creation; terminal results occur only after valid state acceptance and controlled child finalization | `CopySelfToTempAndRelaunch()`, `FinalizeUninstallLifecycle()` | `installer_relaunch.cc`, `bootstrap_win.cc`, `installer_lifecycle.cc` |
| Lifecycle sends are one-shot and bounded to 500 ms; receiver results cannot cancel or alter commit/exit status | `SendInstallerLifecycleMessage()` | `installer_lifecycle.cc` |
| Lifecycle payloads are valid UTF-8 with one NUL, 32 KiB total, bounded diagnostics, and path-free normalized messages | serializers/reference parsers and result producers | `installer_lifecycle.cc`, `installer_lifecycle_test_support.cc`, `installer_result_json.cc`, `installer_controller.cc` |
| Child terminal failures accept only explicit stable installer error codes; exit 109 and arbitrary `UNKNOWN_ERROR` integers are rejected | lifecycle serializer and reference parsers | `installer_lifecycle.cc`, `installer_lifecycle_test_support.cc`, `e2e/lifecycle_receiver.py` |
| Reference correlation state is capped at 256 operations, expires after 30 idle minutes, prioritizes orphan-terminal eviction, and exposes consume/remove operations | `InstallerLifecycleCorrelator`, `Correlator` | `installer_lifecycle_test_support.cc`, `e2e/lifecycle_receiver.py` |

The private relaunch nonce authorizes trusted-state consumption. The public
128-bit operation ID only correlates a handoff with a terminal result; it does
not authenticate the sender or receiver. Another process at the same user and
integrity level may spoof window messages, and HWND destruction/reuse can
redirect a later best-effort send. No receiver response carries a command or
can change installer work.

Windows UIPI may block delivery from a lower-integrity child to a
higher-integrity receiver. Such a receiver controls any opt-in by calling
`ChangeWindowMessageFilterEx(WM_COPYDATA, MSGFLT_ALLOW)` on its own window. The
installer does not weaken message filters. A blocked, timed-out, destroyed, or
hung destination is an indeterminate transport outcome, never a reason to
retry or roll back the uninstall.

---

## Registration Retention Availability Tradeoff

Registration retention is explicit maintenance, not an uninstall detector or
a security boundary. It uses durable registration identity and canonical,
integrity-valid launch evidence; it never infers ownership or abandonment from
executable paths, ACLs, writability, or elevation. Missing, malformed,
noncanonical, mismatched, zero, or future-dated evidence fails closed as
unknown/protected.

Only source-derived per-user-default and custom `install_path` stores
are eligible. HKLM and Program Files roles are provisioning-owned and
ineligible even when their paths or permissions look user-writable. Apply
recomputes under the writer lock and performs one authoritative final evidence
collection. Each registration's direct canonical-liveness observation, rather
than completion of the sequential store scan, is its launch-intent cutoff.
Apply then offers a final cancellation checkpoint and enters the `committing`
state. Logical database/index publication cannot be cancelled after that
transition, but the built-in UI and parent-window protocol continue to accept
one cancellation request and defer it to the next safe checkpoint before
physical version cleanup. Registration removal is committed before checked
reduced-index publication, and partial logical commits are reported for retry.
Startup evidence writers never wait for retention-specific locks. Evidence
published after its observation is
preserved by exact, same-file-object compare-before-delete cleanup. Cleanup also
requires the opened file's handle-resolved parent to match the observed
`.launch` directory, so a later intermediate junction or symlink cannot
redirect deletion. Later evidence does not roll back an already published
database.
Operators accept the availability risk that reclaimed dormant applications may
later require a download or fail permanently under offline/no-download policy.

## Known TOCTOU Races

These check-then-use patterns have inherent race windows. They are mitigated
but not eliminated. Document any new ones here.

| Check | Use | Location | Mitigation |
|-------|-----|----------|------------|
| `IsReparsePoint()` | `MoveFileExW()` (install) | `installer_file_ops.cc` `InstallVersion()` | Same-volume atomic rename minimizes window |
| `IsReparsePoint()` | `MoveFileExW()` (uninstall) | `installer_file_ops.cc` `UninstallVersion()` | Same-volume atomic rename |
| `IsReparsePoint()` | `base::CopyFile()` (relaunch) | `installer_relaunch.cc` | 64-bit random makes prediction impractical |
| `PathExists(catalog)` | `VerifyCatalogSignature()` | `installer_signature.cc` `VerifyWithCatalog()` | Catalog from just-extracted temp dir under installer control |
| `GetFileSize()` | `VerifyFileHash()` | `installer_download.cc` `DownloadFile()` | Low risk: file just written by this process in temp dir |
| `TryUseDirectory()` write test | Actual file writes | `installer_paths.cc` | Write test only checks capability; real writes are under lock. Explicit uninstall captures the selected mutation target in its prepared snapshot. |
| `GetPhysicalPathContainment()` | Choose direct mutation or temporary relaunch | `installer_paths.cc`, `installer_bootstrap_helpers.cc`, `bootstrap_win.cc` | Both identities are handle-resolved and indeterminate results fail closed. The immutable preflight binds the selected target; downstream reparse checks and the writer lock limit later mutation races. |
| `IsReparsePoint(cache_dir)` | `WriteFile()` / `ReadFileToString()` | `installer_download.cc` `DownloadWithCache()` | Best effort — attacker already has user-level access; code signing is the hard boundary |
| `IsReparsePoint(cache_path)` | `WriteFile()` | `installer_download.cc` `DownloadWithCache()` | Content is CDN-sourced (HTTPS), not attacker-controlled |
| `IsReparsePoint(path)` | `WriteFile()` / `ReadFileToString()` | `installer_revocation.cc` `WriteRevocationCache()`, `LoadRevocationCache()` | Additive-only merge means injected entries cause DoS, not code execution |
| `IsReparsePoint(versions_dir)` | `FileEnumerator` / `MoveFileExW` | `installer_file_ops.cc`, `installer_paths.cc`, `installer_version_metadata.cc` | Same-privilege; attacker must replace Versions/ dir between check and use |
| `IsDirectorySafe()` | `MoveFileExW()` | `installer_file_ops.cc` `InstallVersion()` | Lock prevents concurrent modification by other installer instances |
| `VerifySafeDirectoryPath()` | `CreateDirectory()` (staging, cache) | `installer_controller.cc` `DownloadAndInstall()` | Same-privilege; lock prevents concurrent installer instances |
| `VerifySafeFilePath()` | `DownloadFile()` (archive) | `installer_controller.cc` `DownloadAndInstall()` | Content is verified post-download (signature + hash) |
| `IsPathSafeForLoading()` | `LoadLibrary()` by client | `installer_file_ops.cc` / `installer_controller.cc` `MakeValidatedLibcefResult()` | Accepted at the owning user's trust level for user-writable stores. The cefclient reference loader's optional load-time `libcef.dll` signer check narrows this race as defense-in-depth; arbitrary clients must opt in, and catalog-covered dependencies/resources remain inside the same-user boundary. |

---

## Testing Mode Guards

These features exist for testing but MUST be compiled out of official builds
or reject requested test state in official builds:

| Feature | Effect | Guard | File |
|---------|--------|-------|------|
| `SetTestingMode()` / `IsTestingMode()` | Master toggle; enables HTTP downloads, ignores TLS certificate errors, suppresses error dialogs | `OFFICIAL_BUILD && NDEBUG` | `installer_controller.cc` |
| `SetStringResourceLoaderForTesting()` / `SetMessageBoxRunnerForTesting()` | Replaces localized UI strings or error-dialog display | `OFFICIAL_BUILD && NDEBUG` (callbacks and setters are compiled out) | `installer_progress_dialog.cc` |
| `SetUninstallRelaunchCallbacksForTesting()` | Replaces process launch, lifecycle delivery, and authenticated client-module copying for deterministic relaunch tests | `OFFICIAL_BUILD && NDEBUG` (callbacks and setter are compiled out) | `installer_relaunch.cc` |
| `SetEmergencyRecoveryScanLimitsForTesting()` | Overrides bounded automatic-startup recovery root, entry, and time limits | `OFFICIAL_BUILD && NDEBUG` (override state and method are compiled out) | `installer_controller.cc` |
| `SetSignatureTestingMode()` | Accepts self-signed catalogs, skips thumbprint matching | `OFFICIAL_BUILD && NDEBUG` | `installer_signature.cc` |
| `allow_http_for_testing` in `DownloadOptions` | Allows HTTP URLs (set by `IsTestingMode()`) | `OFFICIAL_BUILD && NDEBUG` | `installer_download.cc` |
| `ignore_certificate_errors_for_testing` in `DownloadOptions` / `CEF_INSTALLER_IGNORE_CERTIFICATE_ERRORS_FOR_TESTING` | Ignores TLS certificate errors; set automatically by `IsTestingMode()` or explicitly by the environment variable for loopback process tests | `OFFICIAL_BUILD && NDEBUG` | `installer_controller.cc`, `installer_download.cc` |
| `OverrideInstallDirectoriesForTesting()` | Overrides readable/writable directory discovery | `OFFICIAL_BUILD && NDEBUG` | `installer_paths.cc` |
| `ResolveInstallDirectories()` override path | Returns test dirs instead of registry/filesystem | `OFFICIAL_BUILD && NDEBUG` | `installer_paths.cc` |
| `InstallerE2EConfig` | Parses all `CEF_INSTALLER_TEST_*` subprocess inputs once into typed, process-wide settings; product components never read those environment variables directly | `OFFICIAL_BUILD && NDEBUG` (fault settings are inert; requested directory overrides are rejected) | `installer_e2e_config.cc` |
| `CEF_INSTALLER_TEST_DIRECTORY_SCENARIO=admin_mutation_denied` with `CEF_INSTALLER_TEST_DIRECTORY_ROOT` | For original-uninstall E2E coverage, replaces directory candidates, reports elevation, and closes the admin mutation gate during uninstall preflight | `OFFICIAL_BUILD && NDEBUG` (requested test state is rejected) | `installer_e2e_config.cc`, `installer_bootstrap_helpers.cc`, `installer_paths.cc` |
| `CEF_INSTALLER_TEST_RELAUNCH_FAILURE=1` | Forces required uninstall relaunch to fail before publication or child creation | `OFFICIAL_BUILD && NDEBUG` (setting is inert) | `installer_e2e_config.cc`, `installer_relaunch.cc` |
| `CEF_INSTALLER_TEST_CHILD_CONFIG_FAILURE=1` | Fails a marked child after trusted state acceptance at the config-loading boundary | `OFFICIAL_BUILD && NDEBUG` (setting is inert) | `installer_e2e_config.cc`, `installer_bootstrap_helpers.cc` |
| `CEF_INSTALLER_TEST_CHILD_STATE_BARRIER=<path>` | Signals a fixture-owned marker immediately after trusted-state acceptance and waits up to 30 seconds for deterministic termination testing | `OFFICIAL_BUILD && NDEBUG` (setting is inert) | `installer_e2e_config.cc`, `installer_bootstrap_helpers.cc` |
| `CEF_INSTALLER_TEST_DATABASE_SAVE_FAILURE`, `CEF_INSTALLER_TEST_INDEX_FAULT`, `CEF_INSTALLER_TEST_FILE_OPS_FAULT` | Injects typed persistence and mutation-boundary failures in subprocess/E2E tests | `OFFICIAL_BUILD && NDEBUG` (settings are inert) | `installer_e2e_config.cc`, `installer_database.cc`, `installer_version_metadata.cc`, `installer_file_ops.cc` |
| `certificate_thumbprint` in an embedded test resource / `Config` | Overrides the certificate thumbprint for archive verification; auto-enables `SetSignatureTestingMode` when the thumbprint differs from production default | `OFFICIAL_BUILD && NDEBUG` | `installer_config.cc`, `bootstrap_win.cc` |
| `/cef-install-path` CLI flag | Overrides the CEF installation directory; could redirect installs to attacker-controlled paths | `OFFICIAL_BUILD && NDEBUG` | `installer_bootstrap_helpers.cc` |
| `/cef-download-path` CLI flag, `local_download_path` in `DownloadOptions` and `ExtendedConfig` | Reads manifests and archives from a local directory instead of CDN; bypasses network security (HTTPS, certificate pinning) for the transport layer. All post-download validation (hash, signature, size limits) still applies | **No guard (allowed in official builds)** — signature verification is the security boundary, not HTTPS transport | `installer_bootstrap_helpers.cc`, `installer_download.cc`, `installer_controller.h` |

`Logger::ResetForTesting()` and `ProgressDialog::FlushForTesting()` have no
`OFFICIAL_BUILD` guard but are not security-sensitive (they only reset internal
logging state or drain pending tasks on the internal dialog thread,
respectively).

`ScopedThreadPool` intentionally uses `ThreadPoolInstance::JoinForTesting()`
and `FeatureList::ResetEarlyFeatureAccessTrackerForTesting()` in official
builds. The installer creates a temporary thread pool only when the embedding
process has not initialized one, and must tear it down before that process may
later initialize Chromium. These are lifecycle operations, not configurable
testing bypasses.

If adding a new testing bypass, it MUST use the same `OFFICIAL_BUILD && NDEBUG`
guard pattern and MUST be documented in this table.
