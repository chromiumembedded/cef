# CEF Bootstrap Contributor Guidance

The Windows bootstrap is a process-integration layer. Changes under
`installer/` must also follow [installer/AGENTS.md](installer/AGENTS.md) and
the authoritative documents referenced there.

## Directory ownership

- `bootstrap_win.cc` owns executable startup, process classification, sandbox
  integration, module validation and loading, installer dispatch, and the
  final handoff to the client DLL.
- `bootstrap_util_win.*` owns reusable module-name and module-path rules. It is
  compiled into both the bootstrap executables and `libcef`; changes to its
  switches, default names, or validation rules must remain compatible across
  those consumers.
- `win/*` owns the bootstrap executable's Windows resources.
- `installer/*` owns installer behavior and has additional contributor
  guidance in `installer/AGENTS.md`.

## Startup and security invariants

- Keep sandboxed subprocess startup separate from unsandboxed startup.
  Sandboxed processes may already be locked down and must not gain calls to
  Win32 APIs that are only safe before sandbox initialization.
- Preserve the client-DLL loading sequence: load without executing code,
  validate the resolved path and signatures, allow installer inspection of
  trusted resources, unload, and only then perform the normal executable load.
  Never execute client DLL code from the untrusted load.
- Preserve fail-closed module-path, executable, client-DLL, and
  `chrome_elf.dll` validation. Do not weaken these checks to support a new
  launch layout; extend the trusted layout rules explicitly.
- Keep browser-process validation authoritative for sandboxed subprocesses.
  Subprocesses consume the validated module name propagated by the browser
  process and must not independently reinterpret untrusted path input.

## Compatibility and lifetime

- `bootstrap.exe` and `bootstrapc.exe` share `bootstrap_win.cc`. Keep their
  behavior aligned except for intentional GUI/console entry-point differences.
- Changes to `bootstrap_util::switches::kModule` or the default bootstrap
  executable names require corresponding `libcef` compatibility and rebuild
  consideration.
- Keep installer startup state, version strings, launch-health data, and any
  installed-version lease alive through the complete `RunWinMain` call.
- Preserve size-gated compatibility for structures passed across the
  bootstrap/client DLL boundary. Update the corresponding version tests when
  fields or ownership rules change.

## Installer boundary

- `bootstrap_win.cc` owns executable startup, process integration, CEF launch
  orchestration, and calls through the public `cef_installer` interface.
- `bootstrap_win.cc` must not reference `cef_installer::internal` or
  `cef_installer::test`. If it needs installer behavior, add or extend a
  narrowly scoped public `cef_installer` operation that owns the complete
  installer-specific decision.
- Do not promote a low-level helper solely to bypass this boundary. Keep
  parsing, policy, validation, mutation, lifecycle, and testing-bypass
  decisions inside the installer library.
- Bootstrap code may apply a test-only behavior only through a guarded public
  installer operation. It must not manipulate an installer test seam directly.
- `bootstrap_win.cc` must not include an installer `*_test_support.h` header or
  call an installer `*ForTesting` API.

Before finishing a change to `bootstrap_win.cc`, this command must produce no
matches:

```powershell
rg -n "cef_installer::(internal|test)::|installer/.*_test_support\.h|cef_installer::.*ForTesting" `
  cef/libcef_dll/bootstrap/bootstrap_win.cc
```

## Documentation and verification

- [docs/sandbox_setup.md](../../docs/sandbox_setup.md) is the public reference
  for bootstrap/client integration. Update it when client requirements,
  exported entry points, module selection, or deployment layout changes.
- Build `cef`, `bootstrap`, and `bootstrapc` after changing shared bootstrap
  code. Let Ninja determine the affected objects.
- Run `VersionTest.*` after changing bootstrap/client version information or
  size-gated structures.
- Follow `installer/TESTING.md` for the required installer unit, integration,
  and E2E coverage when installer dispatch, resolution, lifecycle, or
  launch-health behavior changes.
