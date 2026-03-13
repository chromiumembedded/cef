# CEF Installer Enterprise Policy

This document is the normative reference for CEF shared-installer enterprise
policy. `README.md` provides an overview and `SECURITY.md` defines trust
boundaries; registry deployment and operational procedures are owned here.

## Registry schema and architecture

Configure machine-wide values under `HKLM\SOFTWARE\Policies\CEF`:

| Value | Type | Default | Meaning |
|-------|------|---------|---------|
| `AllowSharedUserStore` | `REG_DWORD` (`0` or `1`) | `1` | `0` excludes the standard `%LocalAppData%\CEF` store from reads and writes. |
| `CdnUrls` | `REG_MULTI_SZ` | absent | One through three HTTPS base URLs, tried in order, each at most 2048 bytes. |
| `DownloadPath` | `REG_SZ` | absent | Absolute local or UNC root containing the CDN mirror layout. |
| `DisableDownloads` | `REG_DWORD` (`0` or `1`) | `0` | `1` disables every network and mirror fetch. |

`CdnUrls`, `DownloadPath`, and `DisableDownloads=1` are mutually exclusive.
Wrong types, invalid values, unreadable known values, and conflicts return
119/`POLICY_ERROR`; they are never silently ignored. Unknown names are ignored
for forward compatibility.

On 64-bit Windows, policy and `HKLM\SOFTWARE\CEF\InstallLocation` are read from
the 64-bit machine registry view by x86, x64, and ARM64 processes. On 32-bit
Windows the native view is used. There is no redirected-view fallback or
migration. Deployment tools must explicitly target the shared view, for example:

`InstallLocation` is not a value under the policy key and does not mandate an
exclusive policy directory. It supplies the highest-priority standard
directory candidate; normal source-derived role, read, and mutation rules
still apply.

```powershell
reg.exe add "HKLM\SOFTWARE\Policies\CEF" /v AllowSharedUserStore `
  /t REG_DWORD /d 0 /f /reg:64
reg.exe add "HKLM\SOFTWARE\Policies\CEF" /v DisableDownloads `
  /t REG_DWORD /d 1 /f /reg:64
```

To select ordered CDN URLs instead, use a separate, non-disabled source
configuration:

```powershell
$policy = 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Policies\CEF'
New-Item -Path $policy -Force | Out-Null
New-ItemProperty -Path $policy -Name DisableDownloads -PropertyType DWord `
  -Value 0 -Force
New-ItemProperty -Path $policy -Name CdnUrls -PropertyType MultiString `
  -Value @('https://primary.example/cef/', 'https://backup.example/cef/') `
  -Force
```

To switch that configuration to a mirror, first remove `CdnUrls` and keep
downloads enabled:

```powershell
# CdnUrls must be removed before selecting the mutually exclusive mirror.
Remove-ItemProperty -Path $policy -Name CdnUrls
Set-ItemProperty -Path $policy -Name DisableDownloads -Value 0
New-ItemProperty -Path $policy -Name DownloadPath -PropertyType String `
  -Value '\\fileserver\cef-mirror' -Force
```

Each operation loads policy once into an immutable snapshot. Concurrent
multi-value registry edits are not transactionally atomic; deployment tooling
should stage changes so it never relies on an intermediate combination being
valid.

## Behavior and precedence

An effective policy download source has highest precedence and replaces rather
than merges with operation and application sources. With no download-source
policy, precedence is the operation-local mirror, the operation `cdn_urls`,
the selected application config's `cdn_urls`, and finally the hardcoded CEF
CDN.

For policy `CdnUrls`, revocation and manifest requests independently try each
origin in order. After selecting a version, the installer requests its hash
sidecar and archive from the same configured origin and tries every origin for
that version before considering version fallback. If the selected archive is
missing from every origin or from the effective mirror, or candidate-specific
validation fails after allowed recovery, the installer may try at most one
next-best compatible version from the same manifest. Cancellation, an outage
across all origins, policy denial, and local write or publication failures are
terminal for the operation. See
[Download Source Precedence and Robustness](README.md#download-source-precedence-and-robustness)
for the complete behavior.

Validated revocation, manifest, and complete archive cache entries are
source-neutral. Resumable archive partials are bound to their configured and
final origins and are discarded when either origin changes.
Installer-owned complete archives and valid partials expire after seven days.
Use `force_check` (`/cef-forcecheck`) when a newly selected source must be
contacted immediately.

`AllowSharedUserStore=0` immediately hides existing shared LocalAppData
versions. HKLM and Program Files provisioning stores remain readable, and a
dedicated explicit-mode executable may mutate an admin store when elevated.
Bundled and unchecked app-shipped CEF remain eligible because the shared
installer is not a security boundary against the application itself.

A distinct `install_path` is application-owned and remains fully functional,
including normal install, update, registration, repair, and pruning. This is
operationally equivalent to app-shipped content. An `install_path` that is
textually or canonically equal to the resolved HKLM, Program Files, or
LocalAppData standard candidate is always configuration error 100, whether or
not policy exists. It never selects that standard store or falls through;
vendors must omit `install_path` to consume a standard location.

`DisableDownloads=1` permits offline selection of provisioned, installed, or
bundled content, but prevents all network and mirror reads. An explicit
`update` still requires its version check and therefore returns policy denial
113. Elevation is not a bypass. Compiled and integrity-valid cached revocations
remain enforced; external `revoked.json` refresh and refresh/cache/backoff
writes are disabled.

Managed applications should ship a bundled fallback. If the effective minimum
version exceeds every provisioned version and no bundled fallback exists,
startup is intentionally broken until an administrator provisions a matching
version or changes policy.

## Provisioning and revocation curation

Use a dedicated installer binary whose trusted resource sets
`enable_explicit_modes: true`; never elevate the application. A scheduled
elevated `/cef-update` can maintain an HKLM or Program Files store while
`AllowSharedUserStore=0`. Do not set `DisableDownloads=1` while that updater is
expected to fetch.

Registrations in an administrator-owned store are durable provisioning pins,
not a census of standard-user consumers. To provision exact version `X`, give
the dedicated bootstrap a unique provisioning `appid` and set `vmin = vmax =
X`, then run `/cef-update`. Give each independent pin its own appid. Run
`/cef-uninstall` with the same config to remove that pin; pruning reclaims only
versions not required by another pin. Revocation remains authoritative over
every pin.

Embed the provisioning config in the dedicated bootstrap for a one-file
artifact. Both an unsigned bootstrap and unsigned `chrome_elf.dll`, or both
binaries signed by the same certificate, are supported. A signed executable
paired with a mismatched `chrome_elf.dll` is rejected. No separate provisioning
command is required.

Standard-user registrations in `%LocalAppData%` never protect an admin-store
version, and admin pins never protect a per-user-store version. Admin-role pins
are not eligible for user-liveness retention. Use distinct custom paths when
applications require incompatible signer policies. This provides operational
isolation, not a security boundary against the owner of a user-writable store.

For an offline fleet, maintain a complete trusted mirror including curated
`revoked.json`:

1. Temporarily clear or set `DisableDownloads` to `0`.
2. Set the mutually exclusive policy `DownloadPath` to the mirror.
3. Run the dedicated updater with `/cef-update /cef-forcecheck` (elevated only
   when the target is an admin store).
4. Remove `DownloadPath` and restore `DisableDownloads=1`.

The forced check prevents a fresh source-neutral cache or matching refresh
backoff from suppressing mirror ingestion. The run may also install versions;
there is no revocation-only ingestion command. Command-line
`/cef-download-path` and API `local_download_path` deliberately do not persist
revocations. Never hand-edit integrity-protected `revocation_cache.json`.

## Troubleshooting

| Code | Name | Meaning and remediation |
|------|------|-------------------------|
| 100 | `CONFIG_ERROR` | Application configuration is invalid, including a standard-location `install_path` alias. Fix the vendor configuration. |
| 113 | `POLICY_DENIED` | Valid policy blocked required shared-store mutation or downloading. Review effective vmin and provisioned versions, then contact the administrator. |
| 119 | `POLICY_ERROR` | A known HKLM policy value is unreadable, malformed, wrong-type, or conflicts with another source state. Correct the registry deployment. |

Policy denial is available through structured startup/API results, crash
annotations, normal logs, and standalone output. CEF does not register or
produce Windows Event Log events for these failures.
