# GPT Instructions for CEF Updates

Use these guides when asking a coding agent to update CEF patches or fix build
errors during a Chromium update. The GPT guides give Sol a short overview of
what to do and how to verify the result. They link to the existing Claude
guides for detailed commands, examples, and troubleshooting.

## Choosing a model

| Model | Starting effort | Guides to use |
| --- | --- | --- |
| GPT-6.1 Sol | Low | The GPT guides in this directory |
| GPT-5.6 Terra | Choose for your task | The Claude guides in `cef/tools/claude/` |

For Sol, start with low effort and use the Sol examples below. The agent can
consult the linked Claude guides as it works. For Terra, use the Terra examples,
which point directly to the detailed Claude guides. Those guides work with
other models despite their filenames.

Select the model and effort in your coding agent before sending the prompt.
The instruction files describe the work; model selection is a separate setting.

These recommendations are a starting point for trying the workflows on real
updates. Adjust the GPT model and reasoning effort to balance speed and cost
while meeting the same completion checks. Check whether the agent covers every
failed file, preserves CEF behavior, and saves source fixes into the patches.
If Sol gets stuck on a particular issue or cannot explain why a behavioral
change is correct, try that issue again at medium effort. When comparing speed
and cost, include any follow-up work needed to correct the result.

## Getting started

Before launching the agent, use the existing Chromium update guide to prepare
your checkout and the analysis for the phase you're working on:

1. Review the [complete update workflow](../claude/CHROMIUM_UPDATE.md#complete-update-workflow)
   for the order of checkout updates, patch repairs, builds, and testing.
2. Check the [prerequisites](../claude/CHROMIUM_UPDATE.md#prerequisites) and
   [setup](../claude/CHROMIUM_UPDATE.md#setup): you'll need a Chromium checkout
   with CEF integration, Python 3, and the appropriate build configuration.
   The `setup_claude.py` step installs context for Claude Code; it is not a
   prerequisite for using the GPT prompts.
3. For patch repairs, follow [Phase 1: Fixing Patches](../claude/CHROMIUM_UPDATE.md#phase-1-fixing-patches)
   to capture `patch_updater.py` output and generate `patch_analysis.txt` with
   your old and new Chromium versions. Then use the patch prompt below.
4. Once the patches apply cleanly, follow [Phase 2: Fixing Build Errors](../claude/CHROMIUM_UPDATE.md#phase-2-fixing-build-errors)
   to capture a build of your chosen target and generate `build_analysis.txt`.
   Then use the build prompt below.

Start your coding agent from the `chromium/src` checkout root. Copy one of the
complete prompts below, then replace the Chromium versions and analysis path
with yours. For a build task, also set your output directory and build target.
All paths in the prompts are relative to `chromium/src`.

Use the analysis files produced by the existing tools in `cef/tools/claude/`:

- `patch_analysis.txt` lists failed patches and files.
- `build_analysis.txt` lists build errors and points to the relevant sections
  of `build_output.txt`.

The build examples include permission to fix Chromium files outside `cef/`
when custom CEF patches cause their build failures. Keep that instruction if
it applies to your update: it tells the agent to save those fixes into the
associated CEF patches and overrides the detailed build guide's usual scope.

## Example prompts for Sol at low effort

### Update patches

```text
Work from the chromium/src checkout root; all paths below are relative to it.

Please update patches from 154.0.8037.0 to 156.0.8078.0
Read and follow the instructions in:
cef/tools/gpt/GPT_PATCH_INSTRUCTIONS.md
When a patch has multiple failed files you MUST check all of the files.

Here's the patch output analysis: cef/tools/claude/patch_analysis.txt
```

### Fix build errors

```text
Work from the chromium/src checkout root; all paths below are relative to it.

Please fix build errors for the 154.0.8037.0 to 156.0.8078.0 update
Read and follow the instructions in:
cef/tools/gpt/GPT_BUILD_INSTRUCTIONS.md

Build output directory: out/Debug_GN_arm64
Build target: cef

IMPORTANT: Some .cc files outside of cef may also have build errors after
the Chromium update, due to the custom CEF patches applied. Fix those build
errors and re-save the associated CEF patch files using
cef/tools/gpt/GPT_PATCH_INSTRUCTIONS.md.

Here's the build error analysis: cef/tools/claude/build_analysis.txt

IMPORTANT: Do not try to read all of cef/tools/claude/build_output.txt. Use
the positional markers from cef/tools/claude/build_analysis.txt to read
relevant sections ONLY.
```

## Example prompts for Terra

### Update patches

```text
Work from the chromium/src checkout root; all paths below are relative to it.

Please update patches from 154.0.8037.0 to 156.0.8078.0
Read and follow the instructions in:
cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md
When a patch has multiple failed files you MUST check all of the files.

Here's the patch output analysis: cef/tools/claude/patch_analysis.txt
```

### Fix build errors

```text
Work from the chromium/src checkout root; all paths below are relative to it.

Please fix build errors for the 154.0.8037.0 to 156.0.8078.0 update
Read and follow the instructions in:
cef/tools/claude/CLAUDE_BUILD_INSTRUCTIONS.md

Build output directory: out/Debug_GN_arm64
Build target: cef

IMPORTANT: Some .cc files outside of cef may also have build errors after
the Chromium update, due to the custom CEF patches applied. You should also
fix those build errors and re-save the associated CEF patch files using
cef/tools/claude/CLAUDE_PATCH_INSTRUCTIONS.md. This extends the build
guide's default restriction to files under cef.

Here's the build error analysis: cef/tools/claude/build_analysis.txt

IMPORTANT: Do not try to read all of cef/tools/claude/build_output.txt. Use
the positional markers from cef/tools/claude/build_analysis.txt to read
relevant sections ONLY.
```
