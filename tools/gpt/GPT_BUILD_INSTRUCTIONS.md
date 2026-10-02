# CEF Build Error Fixes

Fix build failures for the requested Chromium update while preserving CEF
behavior and API compatibility. Use the supplied versions, build analysis,
output directory, and target.

## Detailed reference

Use the [full build guide](../claude/CLAUDE_BUILD_INSTRUCTIONS.md) for command
recipes, error patterns, and troubleshooting. Read the relevant sections before
performing an unfamiliar operation; this overview defines the active workflow
and scope. The user's task instructions take precedence over both documents.

- [Error analysis](../claude/CLAUDE_BUILD_INSTRUCTIONS.md#step-3-analyze-build-errors)
- [Investigation and fix-or-deprecate decisions](../claude/CLAUDE_BUILD_INSTRUCTIONS.md#step-4-fix-each-error)
- [Common error patterns](../claude/CLAUDE_BUILD_INSTRUCTIONS.md#common-build-error-patterns)
- [Final build verification](../claude/CLAUDE_BUILD_INSTRUCTIONS.md#step-7-final-verification)
- [API versioning](../claude/CLAUDE_LIBCEF_INSTRUCTIONS.md#cef-api-versioning)
- [Patch persistence](GPT_PATCH_INSTRUCTIONS.md)

## Scope

The default scope is CEF source and build files. When the task also authorizes
fixing failures in Chromium files affected by custom CEF patches, repair those
files and resave their associated CEF patches using the patch guide. That
authorization overrides the full build guide's restriction to `cef/` files.
Preserve unrelated working changes and keep repairs limited to the update.

## Workflow

1. Establish the working directory, inspect existing changes, and identify the
   actual output directory and requested target. Use an existing supplied build
   analysis; generate one with `analyze_build_output.py` if necessary.
2. Read the error index in `build_analysis.txt`. Use its positional markers to
   read only relevant sections of `build_output.txt`; do not read the entire
   raw build log. Track every listed failing file and error group.
3. Investigate current declarations, Chromium callers, upstream changes, and
   CEF consumers and tests. Group errors with a common cause. Preserve caller
   expectations: a compiling replacement must also implement the required
   behavior. If Chromium removed the underlying capability, follow the existing
   CEF deprecation and versioning patterns instead of inventing a substitute.
4. Apply focused fixes. Keep declarations and definitions consistent. Fix the
   source or generator inputs rather than editing generated output directly.
   For public API changes, read the API versioning reference, preserve stable
   ABI contracts, and regenerate the required wrappers and metadata.
5. Format only changed files and inspect the resulting diff. Rebuild affected
   individual compilation targets using the targets from the analysis. Update
   the error inventory as root causes and cascading failures disappear.
6. For authorized Chromium source repairs, resave the associated CEF patches
   and verify both file coverage and saved content. Successful compilation does
   not prove that a repair was persisted in the patch.
7. Once affected files compile, build the full requested target. Capture the
   actual build exit status when logging through a pipeline; do not mistake
   `tee` success or a final log message for build success. If new compile or
   linker failures appear, regenerate the analysis and continue from its index.
8. Inspect the final source and patch diffs for unrelated changes and run
   `git diff --check` in the affected repositories.

## Essential constraints

- Use the `apply_patch` tool when editing files so the user can see your changes.
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
- Use the actual platform and output directory, not example paths. Regenerate
  build files when GN inputs require it using the existing CEF configuration.
- Preserve incremental build state. Do not run `gn clean`, delete object
  directories, or clear caches as a routine response to build errors.
- Maintain behavior, threading, ownership, and API compatibility rather than
  merely silencing diagnostics or weakening tests.
- Continue resolving issues when source evidence supports the next step.
  Ask for input when an unresolved design choice or missing information
  prevents a correct fix; report the evidence and specific blocker.

## Completion

Reconcile every original analysis item with its disposition: fixed and
verified, already resolved, superseded with an explanation, or blocked.
Completion requires the full requested target to build with exit code zero
and any Chromium source fixes to be captured in the associated CEF patches.
Report significant adaptations, new warnings, verification performed, and any
remaining blockers. Identify the configuration and target validated; build
success does not establish runtime behavior or other platform results.
