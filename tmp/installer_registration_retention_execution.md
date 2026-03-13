# Execution Plan — Registration Retention Maintenance

**Coordinator plan:** Plan 10 in
`cef/tmp/installer_design_gaps_plans.md`

**Design source:** R5 and C8 in `cef/tmp/installer_design_gaps.md`, the Plan 10
coordinator section, and completed Plans 2 and 3.

> **Concurrency protocol supplement:**
> `cef/tmp/installer_registration_retention_cutoff_supplement.md` supersedes
> this plan's exact-snapshot, launch-state mutex, change-event, rescue-file,
> replan-loop, and post-database rollback requirements. Use the supplement's
> observable atomic-publication cutoff and compare-before-delete contract for
> implementation and review. All other requirements in this plan remain in
> force.

This checklist intentionally does not reinstate automatic 180-day expiry.
Retention is an explicit, dry-run-capable maintenance operation. Its command,
authority, age, role, evidence, report, and failure contracts are settled below.
Plan 10 owns the post-Plan-3 refinement that makes the preserved `kCustom` role
application-owned and retention-eligible; Plan 3 remains unchanged and complete.

## Plan metadata

| Field | Value |
|-------|-------|
| Status | Complete |
| Owner | Codex |
| Last updated | 2026-07-11 |
| Target build directory | `out/Debug_GN_x64` |
| Required predecessors | Plans 2 and 3 (complete) |
| Dependent plan | Plan 8 |

## Scope

### Goals

- Identify stale per-user registrations from durable registrations and valid
  launch/liveness evidence only after explicit operator invocation.
- Produce a dry-run report of registrations and versions that would be
  reclaimed, with liveness and compatibility reasons.
- Apply a recomputed plan under the writer lock using checked database/index
  publication and whole-directory removal.
- Keep ordinary startup, install, update, uninstall, and prune free of
  age-based registration expiry.

### Non-goals

- No implicit expiry, per-app retention config, executable-path heuristic,
  disk-pressure trigger, compatibility-floor heuristic, or profile enumeration.
- No expiry of HKLM/Program Files provisioning registrations.
- No ownership inference from path spelling, ACLs, writability, or elevation:
  custom is application-owned by source; HKLM/Program Files are provisioning.
- No retroactive change to Plan 3 implementation scope or completion record.
  The role-policy change, helper behavior, tests, and documentation land as
  Plan 10 work.
- No promise that an expired dormant app remains usable offline or under a
  no-download policy.

### Required invariants

- Dry-run performs no writes, cleanup, reconciliation, network, or cache work.
- Missing, malformed, noncanonical, future-dated, or pre-feature liveness is
  unknown/protected, never proof of abandonment.
- Evidence is keyed by `(appid, platform)`; the newest valid health-sentinel
  `pid_start_time` or liveness-only `last_launch` wins.
- Source-derived role gates eligibility. `kPerUserDefault` and `kCustom` are
  application/user managed and eligible. `kHklmDefault` and
  `kProgramFilesDefault` are provisioning roles and ineligible.
- Apply acquires the writer lock and recomputes from authoritative state; it
  never accepts a stale dry-run plan or caller-supplied candidate list.
- Database removal commits before reduced-index publication; checked index
  publication precedes whole-directory moves.
- Database save failure changes no index/version/evidence state. Index failure
  after database commit conservatively retains versions and is retryable.
- Physical deletion/evidence cleanup may be `cleanup_deferred`; database and
  index publication remain hard requirements.
- Revocation-aware pruning and live version leases retain existing semantics.
- Ordinary `prune` never invokes retention apply.

### Compatibility and on-disk state

- No `installer.json` timestamp or schema change is planned. Liveness remains
  in Plan 2's `.launch/` files.
- Old registrations without valid liveness remain protected until they first
  acquire evidence and later exceed the approved threshold.
- No custom-store migration exists or is required. Nothing has shipped and
  there are no protected custom stores. Plan 10 defines `install_path` as
  application-owned; administrator provisioning uses HKLM `InstallLocation` or
  Program Files.
- Older binaries reject the new command; newer binaries do not reinterpret old
  prune commands.
- Retrying after interruption reloads database/index/evidence and converges
  without inventing registrations.

## Predecessor gates

- [x] Reconfirm Plan 2 liveness formats, lock-free startup/query, conservative
      index, leases, and index-before-directory removal.
- [x] Reuse `ReadLivenessPath` and `ReadLaunchStatePath`; do not duplicate
      `.launch/` parsing.
- [x] Reconfirm Plan 3 source roles and per-directory database ownership.
- [x] Consume `InstallDirectories::writable_role` and
      `IsUserRetentionEligible`; never infer role from a path.
- [x] Reject admin roles before any maintenance mutation.
- [x] As Plan 10 work, extend `IsUserRetentionEligible` to accept custom stores,
      update its Plan-3 handoff comment, and reject only HKLM/Program Files.
- [x] Add path tests proving this new Plan 10 policy without modifying Plan 3's
      completed checklist or claiming Plan 3 implemented custom retention.
- [x] Preserve Plan 9 registration-removal-before-index ordering and normalized
      `committed`, `cleanup_deferred`, and `failed` outcomes.
- [x] Record `git -C cef status --short` and preserve unrelated changes.
- [x] Baseline build:
      `autoninja -C out\Debug_GN_x64 cef cef_installer_unittests`.
- [x] Baseline tests:
      `cef_installer_unittests.exe --gtest_filter=InstallerDatabaseTest.*:InstallerLaunchStateTest.*:InstallerPathsTest.*Retention*:Controller*Prune*`.

## Existing baseline

- `installer.json` schema 1 stores appid/platform/range/ABI, not last-seen.
- Health sentinels and version-less liveness records are integrity-protected;
  off-mode liveness refreshes at most every 30 days.
- Ordinary prune already scans `.launch/`, retains registered liveness-only
  records for Plan 10, and performs orphan/malformed cleanup.
- `Database::CanPrune()` guards newer schemas and corruption-recovery grace.
- `PruneUnusedVersions()` already computes required versions, checks index
  publication, then moves whole version directories.
- `IsUserRetentionEligible()` currently accepts only `kPerUserDefault`; Phase
  10 must add `kCustom` under the approved source-ownership contract.
- No retention CLI command or structured retention result exists.

## Current status

**Complete.** Product code, unit tests, E2E coverage, and documentation are
implemented. The final Debug GN x64 build, all 1,113 installer unit tests, all
93 installer E2E tests, repeated retention concurrency/recovery tests, and
`git diff --check` pass.

## Phase 0 — Encode approved contracts

### 0a. Command and authority

- [x] CLI commands are `/cef-retention-dry-run` and
      `/cef-retention-apply`; `RunInstaller` command names are
      `retention_dry_run` and `retention_apply`. Never overload prune.
- [x] Expose both CLI and API. The explicit command is authorization; do not
      expose retention through embedded config or automatic startup.
- [x] Apply trusted `enable_explicit_modes` gating to CLI entry points.
- [x] Source role, not elevation, defines ownership. Custom is
      application-owned; administrators provision through HKLM/Program Files.
- [x] Dry-run takes the writer lock for an exact snapshot but performs no
      directory creation, write probes, or file logging. Apply independently
      reloads and recomputes under the lock. Both modes hold the launch-evidence
      mutex across evidence and confirmed-protection collection.

### 0b. Eligibility and time

- [x] Default `max_age_days` is 180. Accept an operation-specific integer
      override from 90 through 3650 days. `age >= threshold` is stale.
- [x] Add a test-injectable FILETIME clock and checked age arithmetic.
- [x] Newest valid canonical evidence wins for exact appid/platform. A
      potentially matching malformed/integrity-failed record makes the entry
      unknown; unrelated malformed files do not protect every registration.
- [x] Future or missing evidence protects as unknown.
- [x] When `CanPrune()` is false, both modes report the store blocker and no
      actionable candidates; apply fails without mutation.
- [x] Custom stores are application-owned and retention-eligible. HKLM (which
      may point anywhere) and Program Files are provisioning and ineligible.
- [x] This is a Plan 10 policy refinement over Plan 3's deliberately neutral
      custom-role handoff, not a reopened or amended Plan 3 deliverable.

### 0c. Report and outcome

- [x] Report all registrations with appid/platform, range/ABI, evidence
      kind/time/age, decision, and reason.
- [x] Report version/platform, before/after requirement,
      compatibility/protection reason, and expected removal state.
- [x] Emit full appids to the owning caller and sort registrations by
      appid/platform and versions by version/platform.
- [x] No candidates is committed/no-op; ineligible or `CanPrune()`-blocked apply
      fails; physical cleanup may be deferred.
- [x] If database removal commits but index publication fails, return
      `success:false`, `outcome:failed`, `registrations_committed:true`,
      `versions_pruned:false`, and `retry_required:true`.
- [x] Apply reports its recomputed locked plan, applied entries, final version
      effects, and warnings.
- [x] Normal CLI output is deterministic text; headless CLI and API output are
      JSON.
- [x] Add serializer/parser tests before controller integration.

## Phase 1 — Pure planner

### 1a. Evidence collection

Files: `installer_launch_state.h/.cc`, `installer_launch_state_unittest.cc`

- [x] Add side-effect-free collection for registered appid/platform pairs.
- [x] Enumerate regular non-reparse files only; require canonical filename and
      matching body identity.
- [x] Return evidence kind/time and diagnostics without repair or deletion.
- [x] Test multiple health versions, mixed evidence types, platform isolation,
      malformed/integrity/noncanonical/future input, and missing directories.
- [x] Prove the collector performs no writes.
- [x] Tests pass:
      `cef_installer_unittests.exe --gtest_filter=InstallerLaunchStateTest.*Retention*`.

### 1b. Registration decisions and version impact

Files: preferably new `installer_retention.h/.cc/.unittest.cc`, plus database
and resolver helpers

- [x] Update `installer_paths.h/.cc` in Plan 10 so
      `IsUserRetentionEligible(kCustom)` is true, with focused tests for custom,
      per-user, HKLM, Program Files, and misleading filesystem locations.
- [x] Define internal `RetentionOptions`, `RetentionEvidence`, decision, plan,
      and version-impact types from Phase 0 contracts.
- [x] Make planning a pure function over role, database, evidence, installed
      metadata, revocations, threshold, and current time.
- [x] Classify unknown evidence as protected and preserve full appid/platform
      identity with deterministic ordering.
- [x] Reuse `GetRequiredVersionSet` for before/after simulation; do not create a
      divergent compatibility selector.
- [x] Distinguish newly reclaimable, already unreferenced, revoked, confirmed-
      protected, and physically-deferred versions.
- [x] Test empty/multi-platform databases, overlapping ranges, ABI differences,
      shared versions, revocation, threshold edges, and no compatible install.
- [x] Tests pass:
      `cef_installer_unittests.exe --gtest_filter=InstallerRetentionTest.*:InstallerDatabaseTest.*Retention*`.

## Phase 2 — Writer-locked apply and recovery

### 2a. Publish registration removals

Files: `installer_controller.h/.cc`, `installer_database.cc`, controller tests

- [x] Resolve directories and reject ineligible roles before mutation.
- [x] Acquire the writer lock using explicit-command timeout semantics.
- [x] Reload database/evidence and recompute under lock; concurrent fresh
      liveness or registration must rescue a candidate. Serialize launch-state
      writers with a distinct evidence mutex and revalidate after acquiring it
      so the final revalidation-to-commit window is closed. Never invoke an
      external callback while holding the mutex: release after the preliminary
      plan, run cancellation/progress, then reacquire and validate a per-store
      change event. Launch-critical writers never wait: contention publishes a
      separate integrity-protected rescue record and signals the event. Replan
      and offer cancellation again after a change; check again at database
      commit and roll back registration removal when necessary. Release the
      final barrier after logical publication and evidence cleanup, before
      physical version moves.
- [x] Honor too-new schema, integrity recovery, `CanPrune()`, and cancellation
      before commit.
- [x] Remove all approved candidates as one checked database save.
- [x] Database save failure leaves database/index/version/evidence unchanged.
- [x] Zero candidates is an idempotent committed/no-op result.
- [x] Test lock timeout, save failure, cancellation, concurrent refresh/
      registration, exact threshold, and retry.

### 2b. Prune newly unreferenced versions

Files: `installer_controller.cc`, `installer_file_ops.cc`, controller tests

- [x] After database commit, call shared revocation-aware prune using the
      committed registration set.
- [x] Preserve checked reduced-index publication before directory moves.
- [x] On index failure after database commit, report the partial logical state,
      retain indexed versions, and make retry converge using an integrity-
      protected pending-version set written before the database commit. Retry
      scans physical metadata for pending keys absent from a reduced index.
- [x] Preserve confirmed protection for remaining registrations and live lease
      cleanup deferral.
- [x] Do not download, refresh, reconcile unrelated state, or repair versions.
- [x] Restrict normal apply and retry to versions newly unreferenced by this
      retention transaction; never fold pre-existing orphan/revoked pruning
      into retention.
- [x] Test index failure, live lease, trash retry, revocation, confirmed
      protection, cross-platform impact, and idempotency.
- [x] Tests pass:
      `cef_installer_unittests.exe --gtest_filter=Controller*Retention*Prune*:Controller*Retention*Recovery*`.

### 2c. Evidence cleanup

- [x] Delete removed registrations' canonical `.launch/` evidence best-effort
      only after database/index logical commit.
- [x] Treat evidence deletion failure as deferred auxiliary cleanup.
- [x] Preserve evidence after failed database publication.
- [x] Keep dry-run outside existing prune-time orphan cleanup.
- [x] Inject failures at database save, index save, directory move, and evidence
      cleanup boundaries.

## Phase 3 — Controller, API, and bootstrap

### 3a. Controller integration

Files: `installer_controller.h/.cc`, `installer_controller_unittest.cc`

- [x] Add approved command enum value(s), parsers, and exhaustive switch cases.
- [x] Skip appid/vmin validation for store-wide maintenance while validating
      operation-specific options.
- [x] Ensure maintenance never registers the invoking app.
- [x] Ensure dry-run calls no reconciliation, cache/orphan prune, save, index
      publication, or removal path.
- [x] Ensure automatic startup, query, install, update, uninstall, and ordinary
      prune never classify registration age.
- [x] Load effective cached revocations without network for impact simulation.
- [x] Populate the approved report and normalized outcome.
- [x] Tests pass:
      `cef_installer_unittests.exe --gtest_filter=Controller*Retention*`.

### 3b. `RunInstaller` integration

- [x] Parse only approved operation-specific options; do not add embedded app
      configuration fields.
- [x] Reject malformed thresholds/modes, contradictory dry-run/apply options,
      and candidate lists supplied by callers.
- [x] Serialize deterministic registration/version arrays and reason names.
- [x] Preserve existing numeric error, symbolic name, outcome, and thread-local
      result lifetime contracts.
- [x] Test no-op, ineligible store, malformed input, commit failure, partial
      commit, and cleanup deferred JSON.

### 3c. Bootstrap CLI

Files: `installer_bootstrap_helpers.h/.cc`, `bootstrap_win.cc`, unit tests

- [x] Add approved CLI switches without overloading update/uninstall/prune.
- [x] Gate official CLI use with trusted `enable_explicit_modes`.
- [x] Exit after maintenance; never continue into client DLL loading or
      automatic installation.
- [x] Define headless/progress/report output and stable exit behavior.
- [x] Test standalone/launcher/client-DLL, standard/elevated, disabled gate,
      and conflicting switches.
- [x] Tests pass:
      `cef_installer_unittests.exe --gtest_filter=InstallerBootstrapHelpersTest.*Retention*:Bootstrap*Retention*`.

## Phase 4 — Integration and regression tests

### 4a. Eligibility and report accuracy

- [x] Per-user fresh/exact-threshold/old evidence follows approved policy.
- [x] Missing/corrupt/malformed/noncanonical/mismatched/future evidence remains
      protected.
- [x] HKLM and Program Files are ineligible even when writable in test seams.
- [x] Custom paths under misleading locations remain application-owned and
      eligible; HKLM paths outside Program Files remain provisioning/ineligible.
- [x] Platform identities remain independent.
- [x] Dry-run output is deterministic and database/index/launch/cache/staging/
      trash/version trees are byte-for-byte unchanged.
- [x] Apply recomputation reflects changes between earlier dry-run and apply.

### 4b. Failure and concurrency

- [x] Lock timeout/abandonment never applies an unlocked plan.
- [x] Database failure changes no state; index failure after database commit is
      reported precisely and retry converges.
- [x] Live leases yield index reduction plus deferred physical cleanup.
- [x] Fresh liveness/registration before locked recomputation rescues entries.
- [x] Cancellation before commit is clean; no cancellation point splits a
      logically coupled publication sequence unsafely.
- [x] Stress focused concurrency/recovery tests repeatedly.

### 4c. Excluded-command regression

- [x] Automatic startup and install/update post-prune never expire entries,
      including evidence older than 180 days.
- [x] Uninstall removes only the named app; ordinary prune never age-expires.
- [x] Query remains lock-free and side-effect-free.
- [x] All Plan 1/2/3 launch, database, paths, prune, and controller tests pass.

## Phase 5 — E2E tests

Files: `e2e/e2e_test_base.py`, new `e2e/test_retention.py`

- [x] Add approved switch constants and helpers for integrity-valid database,
      index, health, and liveness files with controlled FILETIME values.
- [x] Dry-run reports an old evidenced registration/version without mutation.
- [x] Apply removes stale registrations and only newly unreferenced versions.
- [x] Preserve a shared version and every registration with unknown evidence.
- [x] Reject admin/provisioning stores without mutation.
- [x] Test a custom path beneath Program Files as eligible and an HKLM path
      outside Program Files as ineligible.
- [x] Test a fresh launch between dry-run/apply rescuing the registration.
- [x] Test explicit-mode gate and existing E2E suite without regressions.

## Phase 6 — Documentation

- [x] README: state expiry is off by default and absent from ordinary commands.
- [x] README: establish the retention contract: `install_path` is
      application-owned and retention-eligible; durable admin provisioning uses
      HKLM `InstallLocation` or Program Files. Do not describe a migration.
- [x] README: document syntax, threshold/evidence/eligibility, report, exit
      behavior, idempotency, and custom/admin rules.
- [x] Warn that apply may require redownload or permanently break dormant
      offline/policy-restricted apps.
- [x] Explain unknown evidence protection and logical commit versus deferred
      physical cleanup.
- [x] SECURITY.md: describe this as an operator availability/disk tradeoff, not
      proof of uninstall or a security boundary.
- [x] Keep the completed Plan 3 execution document historical; record the
      custom-role policy refinement in Plan 10 and downstream Plan 4/8 docs.
- [x] Document the `RunInstaller` schema/lifetime, test fixtures/filters, and
      update coordinator status/link.

## Phase 7 — Final validation

- [x] `autoninja -C out\Debug_GN_x64 cef cef_installer_unittests` passes.
- [x] All focused and complete installer unit tests pass.
- [x] All installer E2E tests pass with required Windows permissions.
- [x] Concurrency/recovery tests pass repeatedly.
- [x] Format changed C/C++ only; do not format BUILD.gn.
- [x] `git -C cef diff --check` passes and the diff has no unrelated changes.
- [x] Re-read R5/C8 and predecessor invariants; record deviations.
- [x] Confirm no new open question invalidates an approved contract and update
      plan/coordinator status.

## Open questions

None. The command, invocation authority, source-role ownership, age, report,
locking, evidence, safety-gate, partial-commit, cleanup, and console contracts
are approved in Phase 0.

## Validation notes

- No 32-bit output directory was available or required; compilation and tests
  used the plan's `out/Debug_GN_x64` target.
- Provisioning-role rejection and misleading-path ownership were validated
  through deterministic source-role unit seams. The E2E suite did not write
  host-global HKLM configuration or create state in the real Program Files
  directory.
- The trusted `enable_explicit_modes` gate reuses the existing
  official-build explicit-command gate and is covered by gate/config unit
  tests. CLI retention behavior and output were additionally exercised in the
  non-official E2E build.

## Risks

- **Availability:** mitigate with default dry-run, explicit apply, conservative
  unknown handling, impact reports, and offline/policy warnings.
- **Clock/pre-feature false positives:** invalid, future, and missing evidence is
  always protected; use checked FILETIME arithmetic.
- **Provisioning deletion:** gate by source role: HKLM/Program Files are
  ineligible; custom is application-owned. Document that administrators must
  use HKLM rather than `install_path` for durable provisioning.
- **Dry-run/apply race:** recompute under writer lock and reject candidate lists.
- **Split transaction:** report database/index state precisely and test retry at
  every injected publication boundary.
- **Regression:** cover command parsing, controller switches, launch scanning,
  database save, index/prune, directory roles, and bootstrap gating.

## Exit criteria

Complete only when all Plan 10 behavior is implemented without implicit expiry;
all invariants and failure/concurrency paths are tested; approved command, age,
evidence, report, outcome, and source-role contracts are documented; unit/E2E
tests and `diff --check` pass; no blocking question remains; and coordinator
status/link is current.
