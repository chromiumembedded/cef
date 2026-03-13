# Supplemental Execution Plan - Retention Evidence Cutoff Protocol

**Supplements:** `cef/tmp/installer_registration_retention_execution.md`

**Applies to:** Plan 10 registration retention maintenance

**Status:** Complete

**Owner:** Codex

**Last updated:** 2026-07-12

**Target build directory:** `out/Debug_GN_x64`

**Implementation revisions:** CEF commits `ad52051104` and `3d2dbc6fc`.

**Validation:** At review revision `3d2dbc6fc`, `cef` and
`cef_installer_unittests` built and all 1,140 installer unit tests passed. The
intermediate-component follow-up `c5baf2774` built both targets and passed all
1,141 installer unit tests. The current UI follow-up builds both targets and
passes all 1,144 installer unit tests. Commit `ad52051104` previously passed all
93 installer E2E tests and 50 iterations of each core observation/cancellation
test.

## Purpose

This supplement replaces the cross-process launch-evidence coordination design
added while implementing Plan 10. It does not reopen the approved retention
command, authority, age, source-role, publication-order, reporting, or physical
cleanup contracts in the parent execution plan.

The replacement is necessary because the current design attempts to provide
all three of the following without defining an observable ordering point:

- launch-evidence writers never block application startup;
- retention eventually completes; and
- every writer that overlaps retention rescues its registration.

A writer may be paused before it publishes any observable state. No mutex,
resettable event, or rescue-file handshake can allow retention to distinguish
that paused writer from the absence of a writer while also guaranteeing both
nonblocking startup and eventual retention completion. The protocol must
instead define when a launch becomes observable and order retention relative to
that publication.

This plan establishes atomic canonical launch-intent publication and documented
per-registration final observations. It removes the launch-state mutex, named
change event, rescue-file side channel, replan loop, and post-database rollback
protocol.

## Relationship to the parent plan

The parent execution plan remains the historical implementation record for
Plan 10. This supplement supersedes only these parent-plan statements:

- dry-run or apply obtains a globally exact evidence snapshot;
- a distinct launch-evidence mutex coordinates retention with evidence writers;
- every vaguely concurrent evidence write rescues a registration;
- apply may repeatedly replan until a change event remains stable; and
- apply rolls back a committed database based on a post-save evidence signal.

All other parent-plan invariants remain required, including:

- retention is explicit and never runs from ordinary startup or prune;
- missing, malformed, noncanonical, future-dated, or pre-feature evidence is
  protected;
- source role, not path spelling or elevation, controls eligibility;
- dry-run performs no filesystem writes;
- apply recomputes under the installer writer lock;
- database publication precedes reduced-index publication;
- checked index publication precedes whole-directory moves;
- database failure changes no index, version, or evidence state;
- index failure after database commit is retryable;
- live version leases and revocation policy retain their existing semantics;
- `retention_pending.json` scopes crash recovery to versions made newly
  unreferenced by retention; and
- reports remain deterministic and distinguish logical commit from deferred
  physical cleanup.

## Settled concurrency contract

### Observable launch intent

A launch becomes retention-visible when its canonical per-`(appid, platform)`
liveness file has been atomically published with valid integrity and identity.
Function entry, process creation, an in-memory timestamp, or an in-progress
temporary file is not observable launch intent.

The bootstrap publishes or refreshes this canonical record at the earliest
safe point after the authoritative store and application identity are known and
before client code executes. This applies to every launch-health mode. Health
sentinels remain a separate mechanism with their existing success, rollback,
confirmation, and exit-state semantics.

### Final-validation observations

Apply has one bounded final-validation pass with an explicit observation point
for each registration:

1. Apply completes all potentially lengthy discovery and preliminary planning.
2. Apply offers a progress/cancellation checkpoint before final validation.
3. Apply performs one authoritative final collection of atomic per-file
   observations and recomputes the plan.
4. If candidate or version-removal scope changed, apply aborts without
   mutation and requires an explicit retry.
5. If the plan is unchanged, apply offers its final cancellation checkpoint.
6. If cancellation is declined, apply emits a non-cancellable `committing`
   progress state and immediately enters logical commit.
7. The direct read (or absence observation) of a registration's canonical
   liveness path is that registration's launch-intent ordering point.
8. Evidence atomically published before that observation participates in the
   final plan and can rescue the registration. Publication after it is ordered
   after retention even if other files are still being scanned.

There is no single store-wide linearization point across independently written
files. Canonical liveness paths are read directly once and skipped by the
subsequent directory enumeration. Legacy health files are ordered by their
individual verified reads. The final cancellation callback occurs after all of
these observations, so evidence published by or during the callback is
post-observation and is preserved by compare-before-delete cleanup. This rule
is deterministic and testable; it does not depend on scheduler timing terms
such as "concurrent."

### Post-observation evidence

Post-observation evidence must never be deleted accidentally. Apply records the
exact path and verified payload of each stale evidence file used by the final
plan. Evidence cleanup opens with delete sharing, compares the recorded
integrity status, exact payload, and handle-resolved parent on that handle, and
marks that same file object for deletion only on a match. A concurrent atomic
replacement names a different file object and therefore survives even when it
occurs after the comparison. Redirecting an intermediate directory component
also changes the resolved parent and preserves the target.

If a launch publishes newer evidence after its observation, cleanup preserves
it.
The active process remains protected by its version lease. Because its launch
is ordered after retention, the registration may already have been removed and
the next ordinary startup may need to register or resolve the application
again. This is part of the explicit retention availability contract, not a
reason to roll back a completed transaction.

### Snapshot terminology

Dry-run and preliminary apply planning consume a deterministic collection of
atomic per-file observations. They do not claim a globally simultaneous
snapshot across independently written files. Apply is authoritative because it
performs the single bounded final collection and plan comparison described
above, with per-file rather than store-wide ordering.

## Required invariants for the replacement

- Launch and health evidence writers never wait for retention-specific locks.
- Evidence writers perform no named-event, shared-counter, or rescue-file
  protocol.
- Both health-enabled and health-off main-process launches publish canonical
  retention liveness.
- A liveness publication result is never reported as a successful health-
  sentinel publication.
- Retention never invokes external callbacks inside a logical publication
  sequence.
- Apply has one documented final cancellation checkpoint. After the caller
  accepts that checkpoint, cancellation is deferred until the logical
  database/index publication sequence finishes.
- Final validation performs exactly one full-store collection and never loops.
  The final cancellation callback follows that collection. A non-cancellable
  committing notification follows acceptance, but no enumeration occurs before
  the first durable write.
- Apply never enters an unbounded replan loop. Any material evidence change
  between preliminary planning and final validation aborts without mutation
  and returns a retryable snapshot-changed result.
- Database/index rollback is not used to resolve evidence ordering.
- Evidence cleanup is compare-before-delete and cannot remove a newer atomic
  publication.
- No ordinary command performs age classification as a consequence of this
  change.

## Result contract refinement

Add a stable retryable result for a changed apply plan:

- `success: false`
- `outcome: "failed"`
- a dedicated symbolic/numeric error such as
  `kExitCodeRetentionSnapshotChanged`
- `registrations_committed: false`
- `versions_pruned: false`
- `retry_required: true`
- the final recomputed report, not the preliminary report
- a stable warning/reason indicating that launch evidence changed during final
  validation

No database, pending-state, index, evidence, version, cache, staging, or trash
state may change on this result. The caller can display the changed report and
explicitly retry apply. Do not silently loop until the plan stabilizes.

The final cancellation checkpoint is also settled:

- returning false from the checkpoint aborts without mutation;
- after returning true, one non-cancellable "committing" progress notification
  represents the transition to the built-in UI and an external parent;
- the built-in Cancel button remains visually enabled until a click or
  window-close request records deferred cancellation, then becomes disabled to
  prevent repeated requests;
- cancellation responses to that notification are recorded, no later external
  progress callback runs until logical publication completes, and the pending
  request is honored at the next safe checkpoint by deferring physical cleanup
  and retaining its retry scope; and
- physical cleanup may again report progress or honor cancellation only if it
  cannot invalidate the already published logical result.

## On-disk state

### Retained

- `installer.json`
- `versions.json`
- canonical `.launch/` health sentinels
- canonical `.launch/` per-application liveness files
- `retention_pending.json`

### Removed from the design

- `*_rescue_*` liveness files
- the launch-state mutex
- the launch-state changed event
- any resettable notification used as evidence transaction state

Nothing using the rescue/event protocol has shipped, so no product migration
is required. Tests and local fixtures may delete their own rescue artifacts.
Product code must treat an unexpected rescue-named file conservatively as
noncanonical/unknown evidence while it exists; it must not reinterpret that
file as canonical liveness.

## Implementation phases

### Phase 0 - Freeze the protocol and add failing race tests

Files:

- `installer_controller_unittest.cc`
- `installer_launch_state_unittest.cc`
- `installer_integration_test.cc`

- [x] Stop modifying the event/rescue protocol except to remove it.
- [x] Encode the cutoff contract in test names and comments before product
      changes.
- [x] Add a deterministic hook immediately before the authoritative final
      evidence collection.
- [x] Add a deterministic hook immediately after final validation and before
      the first durable transaction write.
- [x] Add a deterministic hook between evidence cleanup comparison and delete.
- [x] Prove evidence published before final read rescues the registration.
- [x] Prove evidence published after final read does not roll back the logical
      transaction.
- [x] Prove post-observation evidence survives compare-before-delete cleanup.
- [x] Prove there is no wait, event, retry loop, or scheduler-dependent test.

Gate: the new tests must fail for the expected reasons before the replacement
implementation lands.

### Phase 1 - Remove retention-specific evidence synchronization

Files:

- `installer_lock.h/.cc`
- `installer_lock_unittest.cc`
- `installer_launch_state.h/.cc`
- `installer_controller.h/.cc`

- [x] Remove `SingletonLock::AcquireLaunchState` and its mutex-name helper.
- [x] Remove `LaunchStateChangeTracker`, `SignalLaunchStateChange`, and event-
      name helpers.
- [x] Remove `GetInstallDirRetentionRescuePath` and `WriteRetentionRescue`.
- [x] Remove rescue-file acceptance from retention collection, pruning, and
      evidence cleanup.
- [x] Remove evidence-change retry loops and post-database evidence rollback.
- [x] Remove test-only event/rescue/rollback fault seams.
- [x] Verify the ordinary installer writer lock is unchanged.
- [x] Verify launch-evidence reads and writes remain atomic and integrity-
      protected through the existing file helpers.

Gate: no production reference to launch-state mutexes, changed events, rescue
paths, or rescue rollback remains.

### Phase 2 - Publish canonical launch intent for every mode

Files:

- `bootstrap_win.cc`
- `installer_launch_state.h/.cc`
- bootstrap and launch-state unit tests

- [x] Identify the earliest common main-process point where authoritative
      `install_dir`, `appid`, and `platform` are available.
- [x] Refresh the canonical liveness record at that point for health-off,
      health-on, and health-required modes.
- [x] Retain the existing refresh interval so ordinary startup does not rewrite
      a fresh record on every launch.
- [x] Force a refresh when existing evidence is future-dated, malformed,
      mismatched, or old enough to require refresh.
- [x] Keep liveness publication best-effort and nonblocking with clear logging
      on actual filesystem failure.
- [x] Write the health sentinel independently after the retention-liveness
      step.
- [x] Set the active health-sentinel path only when the canonical health
      sentinel was actually written.
- [x] Preserve `launch_success`, consecutive-failure, confirmed-version,
      neutral-exit, and post-exit behavior unchanged.
- [x] Ensure subprocesses do not publish main-process launch intent.

Gate: health-mode tests prove that liveness failure, liveness success, sentinel
failure, and sentinel success are four distinct observable cases.

### Phase 3 - Represent final evidence observations explicitly

Files:

- `installer_launch_state.h/.cc`
- `installer_retention.h/.cc`
- associated unit tests

- [x] Add an internal final-observation type containing registration key,
      canonical path, integrity status, and the exact verified payload read.
- [x] Keep the public report independent of filesystem paths and digests.
- [x] Collect canonical liveness and legacy health sentinels in one
      deterministic final pass; a new health launch is also covered by its
      required canonical liveness publication.
- [x] Treat a disappeared, malformed, mismatched, integrity-failed,
      noncanonical, or future-dated final observation as protected.
- [x] Select the newest valid evidence using the same policy as preliminary
      planning.
- [x] Make final observations deterministically ordered by appid/platform and
      then path.
- [x] Preserve exact final payloads for a side-effect-free cleanup comparison
      that distinguishes unchanged, changed, missing, and unsafe files.

Gate: pure tests cover threshold edges, canonical replacement, timestamp ties,
platform separation, integrity mismatch, and changed-content detection.

### Phase 4 - Implement bounded final validation

Files:

- `installer_controller.h/.cc`
- `installer_controller_unittest.cc`

- [x] Keep database/index/revocation/pending-state discovery before final
      validation.
- [x] Build the preliminary report under the ordinary installer writer lock.
- [x] Invoke a progress/cancellation checkpoint before the authoritative final
      collection, with no retention-specific evidence lock held.
- [x] Perform exactly one authoritative final evidence collection and
      recompute the plan.
- [x] Recompute registration and version effects from those final observations.
- [x] If any candidate decision or version-removal scope changes, return
      `kExitCodeRetentionSnapshotChanged` with the final report and no writes.
- [x] Do not loop or automatically retry.
- [x] Invoke the final cancellation checkpoint after validation; on
      cancellation, return without creating or modifying
      `retention_pending.json`.
- [x] After final acceptance, emit the documented non-cancellable committing
      notification, then enter logical commit with no intervening enumeration.
- [x] If the plan is unchanged, write pending state and publish the database.
- [x] Preserve existing database-save failure rollback of pending state.
- [x] Preserve database-before-index and index-before-directory ordering.
- [x] Never roll back a successfully published database because of evidence
      written after its final observation.

Gate: there is exactly one path from final cancellation acceptance through the
non-cancellable committing notification to the first durable transaction
write, with no scan or cancellable callback in between.

### Phase 5 - Make evidence cleanup compare-before-delete

Files:

- `installer_controller.cc`
- `installer_launch_state.cc`
- controller and launch-state tests

- [x] Carry final stale evidence observations through logical publication.
- [x] After database and checked index publication, reread each cleanup target
      without repair.
- [x] Delete only when integrity result and exact payload match the final
      observation on the same opened file object.
- [x] Preserve changed, replaced, newly created, unsafe, or unreadable files.
- [x] Report deletion failure as `cleanup_deferred`.
- [x] Do not report a changed/newer file as a cleanup failure; preserving it is
      the required race outcome.
- [x] Release all logical-transaction state before physical version moves.
- [x] Keep per-version deferred cleanup reporting exact.

Gate: a deterministic post-observation writer always survives cleanup and no
stale observation can delete its replacement.

### Phase 6 - Documentation and compatibility cleanup

Files:

- `README.md`
- `SECURITY.md`
- this supplement
- coordinator Plan 10 references, if present

- [x] Replace all "exact global snapshot" language with atomic per-file and
      per-registration final-observation language.
- [x] Document the observable per-registration launch-intent ordering point.
- [x] Document that post-observation launches are ordered after retention and
      do not roll back a committed transaction.
- [x] Document exact-match evidence cleanup.
- [x] Document the final cancellation/committing transition.
- [x] Remove mutex, event, rescue-file, and rollback descriptions.
- [x] Document `kExitCodeRetentionSnapshotChanged` and retry behavior.
- [x] Record that no shipped on-disk migration is needed.
- [x] Mark this supplement complete only after all validation gates pass.

### Phase 7 - Regression and E2E validation

- [x] Build:

  ```powershell
  autoninja -C out\Debug_GN_x64 cef cef_installer_unittests
  ```

- [x] Run focused unit tests for retention, launch state, controller,
      bootstrap helpers, database, paths, lock removal, and version pruning.
- [x] Run all installer unit tests.
- [x] Run installer E2E tests with the required certificate-store permissions.
- [x] Repeat cutoff race tests under stress.
- [x] Run `git -C cef diff --check`.
- [x] Format changed C/C++ files only; do not format `cef/BUILD.gn`.
- [x] Confirm no unrelated or pre-existing untracked files are staged.

## Required deterministic test matrix

| Scenario | Required result |
|---|---|
| Canonical intent published before its direct final observation | Registration protected |
| Canonical intent published after its direct final observation | Ordered after retention; newer evidence preserved |
| Canonical intent published after database commit | No rollback; evidence preserved |
| Evidence changes between preliminary and final plan | No mutation; snapshot-changed retry result |
| Final evidence becomes malformed or unreadable | Registration protected |
| Final evidence disappears | Registration protected |
| Health sentinel succeeds | Active health path published and health behavior unchanged |
| Health sentinel fails after liveness succeeds | Retention evidence preserved; health path not published |
| Liveness write fails | Launch remains nonblocking; failure logged; no false health success |
| Cancellation at final checkpoint | No pending/database/index/evidence/version change |
| Cancellation after commit phase begins | Deferred until logical publication completes |
| Database save fails | Pending state restored; index/evidence/version unchanged |
| Index save fails after database commit | Retry required; pending version scope preserved |
| Crash after reduced index publication | Pending disk scan converges on retry |
| Evidence replacement before cleanup | Replacement survives |
| Evidence delete fails without replacement | `cleanup_deferred` with deterministic warning |
| Live version lease | Index reduced; exact version cleanup marked deferred |
| Provisioning role | Rejected before write probe, file logging, or lock acquisition |
| Dry-run | No filesystem mutation and no global-snapshot claim |

## Review checklist

- [x] Can every cross-process ordering claim be tied to an atomic file
      publication or checked database/index publication?
- [x] Is there any resettable event, best-effort notification, or in-memory
      flag whose loss could change retention eligibility? There must not be.
- [x] Can an application startup block on retention? It must not.
- [x] Can retention spin indefinitely because applications keep launching? It
      must not.
- [x] Can a liveness-only write be mistaken for a health sentinel? It must not.
- [x] Can evidence published after its observation be deleted from an earlier
      observation? It must not.
- [x] Is there any cancellable callback or scan between the committing
      notification and the first durable transaction write? There must not be.
- [x] Does any retry broaden version pruning beyond the durable pending scope?
      It must not.
- [x] Does ordinary prune perform age classification? It must not.

## Risks and mitigations

- **Post-observation active launch:** The launch is ordered after retention. Publish
  intent as early as possible, preserve its evidence, and rely on the active
  version lease for the running process.
- **Large evidence store:** Final validation performs one linear directory
  pass. It completes before the final callback and aborts rather than looping
  when the plan changes.
- **Filesystem publication failure:** A launch cannot create durable evidence
  when the filesystem refuses the atomic write. Keep startup nonblocking, log
  the failure, and preserve unknown/malformed evidence conservatively.
- **Operator surprise after changed plan:** Return the final changed report and
  require explicit retry rather than silently applying a different plan.
- **Legacy health-only evidence:** Continue to honor it conservatively. New
  health launches publish canonical liveness first, eliminating discovery races
  for newly arriving health evidence.
- **Transaction interruption:** Retain the checked pending-state/database/index
  recovery protocol already implemented and tested.

## Exit criteria

This supplement is complete only when:

- retention-specific launch mutexes, events, rescue files, and rollback logic
  are removed;
- every main-process health mode publishes canonical retention liveness without
  blocking startup;
- per-registration observation and later-publication cleanup semantics are
  documented and enforced;
- changed plans abort once with a retryable result instead of looping;
- final cancellation behavior is explicit and tested;
- crash recovery, version scope, live leases, role eligibility, dry-run purity,
  and deterministic reporting retain their parent-plan guarantees;
- all focused, complete unit, and E2E validation passes; and
- this document and the Plan 10 coordinator status are updated with the final
  implementation revision (and commit only when separately requested) and any
  approved deviation.
