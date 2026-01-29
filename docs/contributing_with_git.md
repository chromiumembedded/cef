This page provides information about using Git to contribute code changes to CEF.

**Contents**

- [Overview](#overview)
- [Initial Setup](#initial-setup)
- [Working With Private Changes](#working-with-private-changes)
  - [Creating a Branch](#creating-a-branch)
  - [Creating a Commit](#creating-a-commit)
  - [Modifying a Commit](#modifying-a-commit)
  - [Rebasing on Upstream Changes](#rebasing-on-upstream-changes)
  - [Deleting a Branch](#deleting-a-branch)
  - [Cleaning a Checkout](#cleaning-a-checkout)
- [Working With Pull Requests](#working-with-pull-requests)
  - [Coding Style](#coding-style)
  - [Creating a Pull Request](#creating-a-pull-request)
  - [Reviewing a Pull Request](#reviewing-a-pull-request)
- [Contributing to the Wiki](#contributing-to-the-wiki)

---

# Overview

The CEF project uses the Git source code management system hosted via GitHub. The easiest way to contribute changes to CEF is by creating your own personal fork of the CEF Git repository and submitting pull requests with your proposed modifications. This document is intended as a quick-start guide for developers who already have some familiarity with Git. If you are completely new to Git you might want to review the series of [Git tutorials](https://docs.github.com/en/get-started/getting-started-with-git) provided by GitHub.

# Initial Setup

Git can maintain your changes both locally and on a remote server. To work with Git efficiently the remote server locations must be configured properly.

1\. Log into GitHub and create a public forked version of the cef.git repository using the Fork button at https://github.com/chromiumembedded/cef. This public linked fork will be used to create and submit pull requests.

2\. Optionally, create a private unlinked fork using git commands directly. This private fork can be used for day-to-day development without exposing your work publicly.

3\. Check out CEF/Chromium source code as described on the [Master Build Quick Start](master_build_quick_start.md) or [Branches And Building](branches_and_building.md) page. Note that if you use the automate-git.py tool you will want to back up your customized CEF checkout before changing branches or performing a clean build.

4\. Change the remote origin of the CEF checkout so that it points to your private forked repository. This is the remote server location that the `git push` and `git pull` commands will operate on by default.

```sh
cd /path/to/chromium/src/cef

# Replace <UserName> with your GitHub user name.
# If you don't have a private fork, point origin to your public fork instead.
git remote set-url origin https://<UserName>@github.com/<UserName>/cef_private.git
```

5\. Add the remote public location pointing to your public forked repository. This is used to push branches for pull request submission.

```sh
# Replace <UserName> with your GitHub user name.
git remote add public https://<UserName>@github.com/<UserName>/cef.git
```

6\. Set the remote upstream of the CEF checkout so that you can merge changes from the main CEF repository.

```sh
git remote add upstream https://github.com/chromiumembedded/cef.git
```

7\. Verify that the remotes are configured correctly.

```sh
git remote -v
```

You should see output like the following:

```cpp
origin    https://<UserName>@github.com/<UserName>/cef_private.git (fetch)
origin    https://<UserName>@github.com/<UserName>/cef_private.git (push)
public    https://<UserName>@github.com/<UserName>/cef.git (fetch)
public    https://<UserName>@github.com/<UserName>/cef.git (push)
upstream    https://github.com/chromiumembedded/cef.git (fetch)
upstream    https://github.com/chromiumembedded/cef.git (push)
```

8\. Configure your name and email address.

```sh
git config user.name "User Name"
git config user.email user@example.com
```

9\. Configure the correct handling of line endings in the repository.

```sh
# Use this value on all platforms. Files will be unchanged in the working
# directory and converted to LF line endings in the object database.
git config core.autocrlf input

# Cause Git to abort actions on files with mixed line endings if the change is
# not reversible (e.g. changes to binary files that are accidentally classified
# as text).
git config core.safecrlf true
```

# Working With Private Changes

You can now commit changes to your personal repository and merge upstream changes from the main CEF repository. To facilitate creation of a pull request or the sharing of your code changes with other developers you should make your changes in a branch. Use `git push origin` to push changes to your private fork for day-to-day development, and `git push public` when you're ready to submit a pull request.

## Creating a Branch

Create a new personal branch for your changes.

```sh
# Start with the branch that your changes will be based upon.
git checkout master

# Create a new personal branch for your changes.
# Replace <BranchName> with your new branch name.
git checkout -b <BranchName>
```

## Creating a Commit

After making local modifications you can commit them to your personal branch.

```sh
# For example, add a specified file by path.
git add path/to/file.txt

# For example, add all existing files that have been modified or deleted.
git add -u

# Commit the modifications locally.
git commit -m "A good description of the fix (issue #1234)"

# Push the modifications to your personal remote repository.
git push origin <BranchName>
```

## Modifying a Commit

You can also modify an existing commit if you need to make additional changes.

```sh
# For example, add all existing files that have been modified or deleted.
git add -u

# Update the current HEAD commit with the changes.
git commit --amend

# Push the modifications to your personal remote repository.
# Using the `--force` argument is not recommended if multiple people are sharing the
# same branch.
git push origin <BranchName> --force
```

## Rebasing on Upstream Changes

The main CEF repository will receive additional commits over time. You will want to include these changes in your personal repository. To keep Git history correct (showing upstream CEF commits on the CEF branch instead of on your personal branch) you will need to rebase the local CEF branch before rebasing your local personal branch.

```sh
# Fetch changes from the main CEF repository. This does not apply them to any
# particular branch.
git fetch upstream

# Check out the local CEF branch that tracks the upstream CEF branch.
# Replace "master" with a different branch name as appropriate (e.g. "2171", "2272", etc).
git checkout master

# Rebase your local CEF branch on top of the upstream CEF branch.
# After this command your local CEF branch should be identical to the upstream CEF branch.
git rebase upstream/master

# Check out the personal branch that you want to update with changes from the CEF branch.
# Replace <BranchName> with the name of your branch.
git checkout <BranchName>

# Rebase your personal branch on top of the local CEF branch.
# After this command your local commits will come after all CEF commits on the same branch.
git rebase master

# Push the modifications to your personal remote repository.
git push origin <BranchName>
```

You may get merge conflicts if your personal changes conflict with changes made to the main CEF respository. For instructions on resolving merge conflicts see [this articicle](https://help.github.com/articles/resolving-merge-conflicts-after-a-git-rebase/).

For more information on using the rebase command go [here](https://www.atlassian.com/git/tutorials/merging-vs-rebasing).

## Deleting a Branch

Once you no longer need a branch you can delete it both locally and remotely. Do not delete branches that are associated with open pull requests.

```sh
# Delete the branch locally.
git branch -D <BranchName>

# Delete the branch remotely.
git push origin --delete <BranchName>
```

## Cleaning a Checkout

You can remove all local changes from your checkout using the below commands.

```sh
# Check the current state of the repository before deleting anything.
git status

# Remove all non-committed files and directories from the local checkout.
# If you run this command with JCEF it will also remove all third_party directories and you will
# need to re-run the `gclient runhooks` command.
git clean -dffx

# Remove all local commits from the current branch and reset branch state to match
# origin/master. Replace "origin/master" with a different remote branch name as appropriate.
git reset --hard origin/master
```

# Working With Pull Requests

Once your personal changes are complete you can request that they be merged into the main CEF (or JCEF) repository. This is done using a pull request. Before submitting a pull request you should:

* Rebase your changes on the upstream CEF (or JCEF) branch (see "Rebasing on Upstream Changes").
* Fix any coding style issues (see "Coding Style").
* Make sure your changes follow [API versioning guidelines](api_versioning.md#usage-in-pull-requests).
* Find or create an appropriate issue in the [CEF issue tracker](https://github.com/chromiumembedded/cef/issues) (or [JCEF issue tracker](https://github.com/chromiumembedded/java-cef/issues) if the change targets that project). Make sure the issue number is referenced in your commit description.
* Include new or modified unit tests as appropriate to the functionality.
* Remove unnecessary or unrelated changes.

## Coding Style

CEF uses the [Chromium coding style](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md). All C/C++, ObjC, Java and Python code must be formatted using the fix_style tool. For example, to fix the style of the unstaged files in your CEF Git checkout:

```sh
cd /path/to/chromium/src/cef
./tools/fix_style.sh unstaged
```

The fix_style tool supports file paths and git hashes as input. Run `tools/fix_style.[bat|sh]` without arguments for complete usage information.

Changes to other types of files should match the existing style in that file.

## Creating a Pull Request

Pull requests can only be created from a public repository that was [forked](https://docs.github.com/articles/fork-a-repo) using the GitHub interface. Push your branch to your public fork and then create the pull request as described [here](https://docs.github.com/articles/creating-a-pull-request-from-a-fork). Pull requests will only be accepted if they meet the requirements described above.

```sh
# Push your branch to your public fork for PR submission.
git push public <BranchName>
```

## Reviewing a Pull Request

Your pull request will be reviewed by one or more CEF developers. Please address any comments and update your pull request. The easiest way to update a pull request is by pushing new commits to the same branch -- those new commits will be automatically reflected in the pull request. Once your changes are deemed acceptable they will be squashed and merged into the main CEF repository.

The contents of a pull request can also be downloaded as a patch file and applied to your local Git checkout:

```sh
# Download the patch file (replace {pull_no} with the PR number).
curl -L https://github.com/chromiumembedded/cef/pull/{pull_no}.diff -o name.patch

# Apply the patch file to your local Git checkout.
git apply name.patch
```

# Contributing to the Wiki

To contribute to the wiki, clone it, apply your changes and upload it in a public location before submitting it for review in the forum or issue tacker.