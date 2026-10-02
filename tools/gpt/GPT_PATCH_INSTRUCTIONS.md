# CEF Patch Updates

Update CEF patches for the requested Chromium version while preserving their
intended behavior. Use the supplied old/new versions and patch analysis as the
starting point.

## Detailed reference

Use the [full patch guide](../claude/CLAUDE_PATCH_INSTRUCTIONS.md) for command
recipes, examples, and troubleshooting. Read the relevant sections before
performing an unfamiliar operation; this overview defines the active workflow.
The user's task instructions take precedence over both documents.

- [Investigation and repair](../claude/CLAUDE_PATCH_INSTRUCTIONS.md#step-3-fix-each-patch)
- [Resaving and verification](../claude/CLAUDE_PATCH_INSTRUCTIONS.md#d-resave-and-verify-the-patch)
- [Moved files and other troubleshooting](../claude/CLAUDE_PATCH_INSTRUCTIONS.md#troubleshooting)
- [Final verification](../claude/CLAUDE_PATCH_INSTRUCTIONS.md#step-5-final-verification)

## Workflow

1. Establish the working directory and inspect existing changes. Preserve
   unrelated work. Read the supplied `patch_analysis.txt`; if only raw output is
   available, generate the analysis with `analyze_patch_output.py`.
2. Track every failed patch, every failed file within it, and every rejected
   hunk. Work through patches one at a time. A patch with multiple failed files
   is incomplete until all of those files have been accounted for.
3. Read each complete `.rej` file and the surrounding current source. Trace
   upstream changes between the supplied versions to understand refactoring,
   API changes, and moved or deleted code. Adapt the CEF behavior to the current
   implementation rather than mechanically copying old code.
4. Apply fixes to the Chromium source files associated with the patch. Format
   only the files changed for the task and inspect any formatter changes.
5. Resave each repaired patch with `patch_updater.py --resave --patch NAME`.
   For moved or renamed files, follow the detailed guide's manual regeneration
   procedure and include the complete patch file list at its current paths.
6. Run `verify_patch.py` for file coverage, then inspect the saved patch for
   content correctness. Account for every rejected change by preserving its
   behavior or documenting why the new Chromium implementation supersedes it.
   File coverage alone does not establish content correctness.
7. Once all listed failures are resolved, rerun `patch_updater.py`, regenerate
   the analysis, and resolve any remaining failures. Inspect the final diff for
   unrelated changes and run `git diff --check` in the affected repositories.

## Essential constraints

- Use the `apply_patch` tool when editing files so the user can see your changes.
- Never run `git checkout` or the CEF `patch_updater.py` reset/reapply cycle
  in the filesystem sandbox.
- Treat examples as illustrations. Verify replacement APIs and their semantics
  against the current source before using them.
- When investigating a new behavioral regression or crash, first identify and
  understand the change that introduced the defect. Review the relevant diff,
  surrounding code, and affected callers to establish the context before
  choosing a fix.
- Avoid type casts unless existing CEF or Chromium code supports that pattern
  for the same use case. Look for current APIs that provide properly typed
  access before introducing a cast to resolve a build error.
- For adaptations that change control flow, ownership, threading, or API
  behavior, record the affected callers and the evidence that their contract
  remains satisfied. Identify unresolved assumptions explicitly.
- Never restore `.cefbak` files: they contain code from before the Chromium
  update. Use rejects, upstream history, and the current source for repairs.
- Persist Chromium source fixes in their associated CEF patch files. Normally
  use the resave tool; use manual regeneration only where the detailed guide
  calls for it.
- Keep edits limited to the requested patch update and preserve CEF intent,
  including lifecycle, threading, and API behavior.
- Use the actual checkout paths and build configuration. Treat example
  versions, filenames, and output directories as placeholders.
- Continue resolving issues when source evidence supports the next step.
  Ask for input when an unresolved design choice or missing information
  prevents a correct fix; report the evidence and specific blocker.

## Completion

Reconcile every original analysis item with its disposition: fixed and
verified, already resolved, superseded with an explanation, or blocked.
Completion requires all patches to apply cleanly, no remaining rejected hunks,
and saved patches that contain the intended source changes. Report significant
adaptations, verification performed, and any remaining blockers. Distinguish
patch application checks from build or runtime validation; proceed to those
phases when requested.
