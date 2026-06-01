# Contributing to CEF

CEF is a volunteer-driven open source project. Contributions of all kinds are
welcome — bug reports, feature requests, code changes, unit tests, and
documentation improvements. All contributions are made under the
[BSD license](LICENSE.txt).

Please use the [CEF Forum](http://www.magpcss.org/ceforum/) for general usage
questions, not the issue tracker.

## Reporting Issues

Search for [existing issues](https://github.com/chromiumembedded/cef/issues)
before creating new ones. Each issue should:

* Include the CEF revision or binary distribution version.
* Include your OS and compiler version.
* For bugs, provide detailed steps to reproduce the problem.
* For features, describe why the feature is beneficial.

Vote for issues that are important to you. This helps with development
prioritization.

Report security vulnerabilities privately via
[GitHub Security Advisories](https://github.com/chromiumembedded/cef/security).
Do not file security issues in the public issue tracker.

## Development Setup

See the [Branches and Building](https://chromiumembedded.github.io/cef/branches_and_building)
page for instructions on downloading and building CEF source code. The
[Master Build Quick-Start](https://chromiumembedded.github.io/cef/master_build_quick_start)
guide covers the fastest path to a working build.

## Submitting Changes

Create or find an appropriate issue for each distinct bug, feature, or change.
For large or architectural changes, discuss the approach in the issue before
starting work. Then submit a
[pull request](https://chromiumembedded.github.io/cef/contributing_with_git).

### Branch Targets

Submit changes against the current
[CEF master branch](https://github.com/chromiumembedded/cef/tree/master)
unless explicitly fixing a bug that only occurs in a release branch.

### Code Style

Follow the style of existing CEF source files. CEF uses the
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md).

### Commit Messages

Use the imperative present tense: "Fix crash in X" not "Fixed crash in X" or
"Fixes crash in X". Keep the first line under 72 characters. Add a blank line
before any extended description.

### Tests

Include new or modified unit tests as appropriate to the functionality. Bug
fixes should include a test that would have caught the bug.

### Keep Changes Focused

Each pull request should address a single concern. Do not bundle unrelated
refactoring, cosmetic changes, or feature work into the same submission.

## AI Policy

The use of AI and LLM tools is permitted under the conditions described in
[AI_POLICY.md](AI_POLICY.md).
