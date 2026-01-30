This page provides information about CEF branches and instructions for downloading, building and packaging source code.

**Contents**

- [Background](#background)
- [Development](#development)
- [Release Branches](#release-branches)
  - [Version Number Format](#version-number-format)
  - [Current Release Branches (Supported)](#current-release-branches-supported)
  - [Legacy Release Branches (Unsupported)](#legacy-release-branches-unsupported)
- [Building from Source](#building-from-source)
  - [Automated Method](#automated-method)
  - [Manual Downloading](#manual-downloading)
  - [Manual Building](#manual-building)
  - [Manual Packaging](#manual-packaging)
  - [Build Notes](#build-notes)

---

# Background

The CEF project is an extension of the Chromium project hosted at [chromium.org](https://www.chromium.org). CEF maintains development and release branches that track Chromium branches. CEF source code can be built manually or with automated tools.

# Development

Ongoing development of CEF occurs in the [CEF master branch](https://github.com/chromiumembedded/cef/tree/master). This location tracks the current [Chromium master branch](https://chromium.googlesource.com/chromium/src.git) and is not recommended for production use.

Current CEF master branch build requirements are as follows. See the [Master Build Quick Start](master_build_quick_start.md) page for a development build quick-start guide.

Windows Build Requirements | macOS Build Requirements | Linux Build Requirements |
|:---------------------------|:----------------------------|:-------------------------|
Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.6+ build system w/ 26.0 base SDK (Xcode 26.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |

The following URLs should be used for downloading development versions of CEF.

  * CEF3 - <https://github.com/chromiumembedded/cef/tree/master>

CEF1 is no longer actively developed or supported. See the [CEF1 Retirement Plan](http://magpcss.org/ceforum/viewtopic.php?f=10&t=10647) for details.

# Release Branches

CEF branches are created to track each Chromium release milesone (MXX) branch. Users developing applications for production environments are encouraged to use release branches for the following reasons:

  * Binary CEF builds are tied to specific Chromium releases.
  * Release versions of CEF/Chromium are better tested and more appropriate for release applications.
  * Within a release branch the CEF API is "frozen" and generally only security/bug fixes are applied.
  * CEF release branches can include patches to Chromium/Blink source if necessary.
  * CEF master development won't interfere with consumer release schedules.

CEF release branches and associated platform build requirements are described below.

## Version Number Format

The CEF version number format was changed in 2019 to include more relevant information and to provide compatibility with the [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html) standard.

Newer CEF release version numbers have the format "X.Y.Z+gHHHHHHH+chromium-A.B.C.D" where:

* "X" is the CEF (and Chromium) major version (e.g. 73).
* "Y" is the CEF minor version. It's an incremental number that starts at 0 when a release branch is created and changes only when the CEF C/C++ API changes (see [Api Versioning](api_versioning.md) for details).
* "Z" is the CEF bugfix version. It's an incremental number that starts at 0 when a release branch is created and changes on each commit, with reset to 0 when "Y" changes.
* "gHHHHHHH" is the 7-character abbreviation for the Git commit hash. This facilitates lookup of the relevant commit history in Git.
* "A.B.C.D" is the Chromium version (e.g. 73.0.3683.75).

Older CEF release version numbers have the format X.YYYY.A.gHHHHHHH where:

  * "X" is the CEF major version (e.g. 3).
  * "YYYY" is the Chromium branch.
  * "A" is an incremental number representing the number of commits in the current branch. This is roughly equivalent to the SVN revision number but on a per-branch basis and assists people in quickly determining the order of builds in the same branch (for bug reports, etc).
  * "gHHHHHHH" is the 7-character abbreviation for the Git commit hash. This facilitates lookup of the relevant commit history in Git.

Detailed Chromium and CEF version information is available in the include/cef\_version.h header file which is created during the build process or by loading the "about:version" URL in a CEF-derived application.

## Current Release Branches (Supported)

Support for newer branches begins when they enter the Chromium beta channel. Support for older branches ends when they exit the Chromium stable channel. Every sixth branch (starting with M138) proceeds through the long-term support candidate (LTC) and long-term support (LTS) channels after exiting stable. The LTC/LTS channels continue to receive platform-agnostic security fixes for ~8 additional months (details [here](https://github.com/chromiumembedded/cef/issues/3947)).

The [Spotify automated builder](https://cef-builds.spotifycdn.com/index.html) provides CEF builds for supported branches. Updating CEF branches is currently a manual process so there will likely be a delay between [Chromium release announcements](http://googlechromereleases.blogspot.com/) and the availability of associated CEF builds. See the [Chromium release calendar](https://chromiumdash.appspot.com/schedule) for estimated Chromium release dates and versions.

| Channel |Release Branch | Version | Branch Date | Last Refresh  | Windows Build Requirements | macOS Build Requirements | Linux Build Requirements |
|:--------|:--------------|:--------|:------------|:--------------|:---------------------------|:----------------------------|:-------------------------|
| Beta    | [7632](https://github.com/chromiumembedded/cef/tree/7632) | 145              | Jan 2026    | Mar 9, 2026  | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.6+ build system w/ 26.0 base SDK (Xcode 26.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| Stable  | [7559](https://github.com/chromiumembedded/cef/tree/7559) | 144              | Dec 2025    | Oct 6, 2026  | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.6+ build system w/ 26.0 base SDK (Xcode 26.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| LTC     | [7204](https://github.com/chromiumembedded/cef/tree/7204) | 138              | Jun 2025    | Apr 21, 2026 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.3323 SDK, Ninja | macOS 11.0+ deployment, 15.2+ build system w/ 15.4 base SDK (Xcode 16.3), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |

## Legacy Release Branches (Unsupported)

Legacy CEF builds are available from the [Spotify automated builder](https://cef-builds.spotifycdn.com/index.html) back to 2704 branch. Building legacy branches is not supported. If you choose to build a legacy branch you will need to solve any build errors on your own.

  * Newer legacy branches (within the past year) can often be built using the same tooling as current branches. You will need to (a) manually download depot_tools, (b) sync depot_tools to a revision that existed at the time of the branch, and (c) set the `DEPOT_TOOLS_UPDATE=0` environment variable to keep it from updating automatically.
  * Older legacy branches can potentially be built by downloading a CEF source archive at the desired branch from [here](https://github.com/chromiumembedded/cef/tags) and a Chromium source archive at the associated/required version from [here](https://gsdview.appspot.com/chromium-browser-official/), and then combining them to create the required directory structure.
  * See the Build Notes section at the end of this page for historical build details.

| Release Branch | Version | Branch Date | Windows Build Requirements | macOS Build Requirements | Linux Build Requirements |
|:---------------|:--------|:------------|:---------------------------|:----------------------------|:-------------------------|
| [7499](https://github.com/chromiumembedded/cef/tree/7499) | 143 | Nov 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.6+ build system w/ 26.0 base SDK (Xcode 26.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7444](https://github.com/chromiumembedded/cef/tree/7444) | 142 | Oct 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.6+ build system w/ 26.0 base SDK (Xcode 26.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7390](https://github.com/chromiumembedded/cef/tree/7390) | 141 | Sep 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.4654 SDK, Ninja | macOS 12.0+ deployment, 15.3+ build system w/ 15.5 base SDK (Xcode 16.4), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7339](https://github.com/chromiumembedded/cef/tree/7339) | 140 | Aug 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.3323 SDK, Ninja | macOS 12.0+ deployment, 15.3+ build system w/ 15.5 base SDK (Xcode 16.4), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7258](https://github.com/chromiumembedded/cef/tree/7258) | 139 | Jul 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.3323 SDK, Ninja | macOS 12.0+ deployment, 15.2+ build system w/ 15.4 base SDK (Xcode 16.3), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7151](https://github.com/chromiumembedded/cef/tree/7151) | 137 | May 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.3323 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7103](https://github.com/chromiumembedded/cef/tree/7103) | 136 | Apr 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.13.4, Win 10.0.26100.3323 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [7049](https://github.com/chromiumembedded/cef/tree/7049) | 135 | Mar 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6998](https://github.com/chromiumembedded/cef/tree/6998) | 134 | Feb 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6943](https://github.com/chromiumembedded/cef/tree/6943) | 133 | Jan 2025 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6834](https://github.com/chromiumembedded/cef/tree/6834) | 132 | Nov 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6778](https://github.com/chromiumembedded/cef/tree/6778) | 131 | Oct 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 14.5+ build system w/ 15.0 base SDK (Xcode 16.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6723](https://github.com/chromiumembedded/cef/tree/6723) | 130 | Sep 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6668](https://github.com/chromiumembedded/cef/tree/6668) | 129 | Aug 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621.2428 SDK, Ninja | macOS 11.0+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6613](https://github.com/chromiumembedded/cef/tree/6613) | 128 | Jul 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6533](https://github.com/chromiumembedded/cef/tree/6533) | 127 | Jun 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6478](https://github.com/chromiumembedded/cef/tree/6478) | 126 | May 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.9.2, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6422](https://github.com/chromiumembedded/cef/tree/6422) | 125 | Apr 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6367](https://github.com/chromiumembedded/cef/tree/6367) | 124 | Mar 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6312](https://github.com/chromiumembedded/cef/tree/6312) | 123 | Feb 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6261](https://github.com/chromiumembedded/cef/tree/6261) | 122 | Jan 2024 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6167](https://github.com/chromiumembedded/cef/tree/6167) | 121 | Dec 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6099](https://github.com/chromiumembedded/cef/tree/6099) | 120 | Nov 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [6045](https://github.com/chromiumembedded/cef/tree/6045) | 119 | Oct 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.5+ build system w/ 14.0 base SDK (Xcode 15.0), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [5993](https://github.com/chromiumembedded/cef/tree/5993) | 118 | Sep 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.0+ build system w/ 13.3 base SDK (Xcode 14.3), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [5938](https://github.com/chromiumembedded/cef/tree/5938) | 117 | Aug 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.15+ deployment, 13.0+ build system w/ 13.3 base SDK (Xcode 14.3), Ninja, 64-bit only | Ubuntu 20.04+, Debian 10+, Ninja |
| [5845](https://github.com/chromiumembedded/cef/tree/5845) | 116 | Jun 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.13+ deployment, 13.0+ build system w/ 13.3 base SDK (Xcode 14.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5790](https://github.com/chromiumembedded/cef/tree/5790) | 115 | May 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.13+ deployment, 13.0+ build system w/ 13.3 base SDK (Xcode 14.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5735](https://github.com/chromiumembedded/cef/tree/5735) | 114 | Apr 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0-14.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5672](https://github.com/chromiumembedded/cef/tree/5672) | 113 | Mar 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5615](https://github.com/chromiumembedded/cef/tree/5615) | 112 | Feb 2023 | Win 10+ deployment, Win 10+ build system w/ VS2022 17.5+, Win 10.0.22621 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5563](https://github.com/chromiumembedded/cef/tree/5563) | 111 | Jan 2023 | Win 10+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5481](https://github.com/chromiumembedded/cef/tree/5481) | 110 | Dec 2022 | Win 10+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5414](https://github.com/chromiumembedded/cef/tree/5414) | 109 | Nov 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12.5+ build system w/ 13.0 base SDK (Xcode 14.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5359](https://github.com/chromiumembedded/cef/tree/5359) | 108 | Oct 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5304](https://github.com/chromiumembedded/cef/tree/5304) | 107 | Sep 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5249](https://github.com/chromiumembedded/cef/tree/5249) | 106 | Aug 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5195](https://github.com/chromiumembedded/cef/tree/5195) | 105 | Jul 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5112](https://github.com/chromiumembedded/cef/tree/5112) | 104 | Jun 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.13+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5060](https://github.com/chromiumembedded/cef/tree/5060) | 103 | May 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.11+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [5005](https://github.com/chromiumembedded/cef/tree/5005) | 102 | Apr 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.11.13+, Win 10.0.20348 SDK, Ninja | macOS 10.11+ deployment, 12+ build system w/ 12.3 base SDK (Xcode 13.3), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4951](https://github.com/chromiumembedded/cef/tree/4951) | 101 | Mar 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 11.3+ build system w/ 12.1 base SDK (Xcode 13.2.1), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4896](https://github.com/chromiumembedded/cef/tree/4896) | 100 | Feb 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 11.3+ build system w/ 12.1 base SDK (Xcode 13.2.1), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4844](https://github.com/chromiumembedded/cef/tree/4844) | 99 | Jan 2022 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-13.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4758](https://github.com/chromiumembedded/cef/tree/4758) | 98 | Dec 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-13.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4692](https://github.com/chromiumembedded/cef/tree/4692) | 97 | Nov 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-13.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4664](https://github.com/chromiumembedded/cef/tree/4664) | 96 | Oct 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-13.0), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4638](https://github.com/chromiumembedded/cef/tree/4638) | 95 | Sep 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-12.5), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4606](https://github.com/chromiumembedded/cef/tree/4606) | 94 | Aug 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-12.5), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4577](https://github.com/chromiumembedded/cef/tree/4577) | 93 | Jul 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-12.5), Ninja, 64-bit only | Ubuntu 18.04+, Debian 10+, Ninja |
| [4515](https://github.com/chromiumembedded/cef/tree/4515) | 92 | Jun 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-12.5), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4472](https://github.com/chromiumembedded/cef/tree/4472) | 91 | Apr 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2-12.5), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4430](https://github.com/chromiumembedded/cef/tree/4430) | 90 | Mar 2021 | Win 7+ deployment, Win 10+ build system w/ VS2019 16.8.4+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4389](https://github.com/chromiumembedded/cef/tree/4389) | 89 | Feb 2021 | Win 7+ deployment, Win 10+ build system w/ VS2017 15.7.1+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4324](https://github.com/chromiumembedded/cef/tree/4324) | 88 | Dec 2020 | Win 7+ deployment, Win 10+ build system w/ VS2017 15.7.1+, Win 10.0.19041 SDK, Ninja | macOS 10.11+ deployment, 10.15.4+ build system w/ 11.0 base SDK (Xcode 12.2), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4280](https://github.com/chromiumembedded/cef/tree/4280) | 87 | Oct 2020 | Win 7+ deployment, Win 10+ build system w/ VS2017 15.7.1+, Win 10.0.19041 SDK, Ninja | macOS 10.10+ deployment, 10.14.4+ build system w/ 10.15.1 base SDK (Xcode 11.2), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4240](https://github.com/chromiumembedded/cef/tree/4240) | 86 | Sep 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.19041 SDK, Ninja | macOS 10.10-10.15, 10.10+ deployment target, 10.14.4+ build system w/ 10.15.1 base SDK (Xcode 11.2), Ninja, 64-bit only | Ubuntu 16.04+, Debian Sid+, Ninja |
| [4183](https://github.com/chromiumembedded/cef/tree/4183) | 85 | Jul 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.19041 SDK, Ninja | macOS 10.10-10.15, 10.10+ deployment target, 10.14.4+ build system w/ 10.15.1 base SDK (Xcode 11.2), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [4147](https://github.com/chromiumembedded/cef/tree/4147) | 84 | Jun 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.15, 10.10+ deployment target, 10.14.4+ build system w/ 10.15.1 base SDK (Xcode 11.2), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [4103](https://github.com/chromiumembedded/cef/tree/4103) | 83 | Apr 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.15, 10.10+ deployment target, 10.14.4+ build system w/ 10.15.1 base SDK (Xcode 11.2), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [4044](https://github.com/chromiumembedded/cef/tree/4044) | 81 | Mar 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3987](https://github.com/chromiumembedded/cef/tree/3987) | 80 | Feb 2020 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3945](https://github.com/chromiumembedded/cef/tree/3945) | 79 | Nov 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3904](https://github.com/chromiumembedded/cef/tree/3904) | 78 | Oct 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3865](https://github.com/chromiumembedded/cef/tree/3865) | 77 | Sep 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.18362 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3809](https://github.com/chromiumembedded/cef/tree/3809) | 76 | Jul 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.17763 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3770](https://github.com/chromiumembedded/cef/tree/3770) | 75 | Jun 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.17763 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3729](https://github.com/chromiumembedded/cef/tree/3729) | 74 (CEF 74, 3) | Mar 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.17763 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.13.2+ build system w/ 10.13+ base SDK (Xcode 9.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3683](https://github.com/chromiumembedded/cef/tree/3683) | 73 (CEF 73, 3) | Feb 2019 | Win 7+, VS2017 15.7.1+, Win 10.0.17134.0 or 10.0.17763 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3626](https://github.com/chromiumembedded/cef/tree/3626) | 72 (CEF 3) | Dec 2018 | Win 7+, VS2017 15.7.1+, Win 10.0.17134.0 or 10.0.17763 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3578](https://github.com/chromiumembedded/cef/tree/3578) | 71 (CEF 3) | Oct 2018 | Win 7+, VS2017 15.7.1+, Win10.0.17134.0 SDK, Ninja | macOS 10.10-10.14, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3538](https://github.com/chromiumembedded/cef/tree/3538) | 70 (CEF 3) | Sep 2018 | Win 7+, VS2017 15.7.1+, Win10.0.17134.0 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3497](https://github.com/chromiumembedded/cef/tree/3497) | 69 (CEF 3) | Jul 2018 | Win 7+, VS2017 15.7.1+, Win10.0.17134.0 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3440](https://github.com/chromiumembedded/cef/tree/3440) | 68 (CEF 3) | Jun 2018 | Win 7+, VS2017 15.7.1+, Win10.0.17134.0 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3396](https://github.com/chromiumembedded/cef/tree/3396) | 67 (CEF 3) | Apr 2018 | Win 7+, VS2017 15.3.2+, Win10.0.15063.468 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3359](https://github.com/chromiumembedded/cef/tree/3359) | 66 (CEF 3) | Mar 2018 | Win 7+, VS2017 15.3.2+, Win10.0.15063.468 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3325](https://github.com/chromiumembedded/cef/tree/3325) | 65 (CEF 3) | Jan 2018 | Win 7+, VS2017 15.3.2+, Win10.0.15063.468 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3282](https://github.com/chromiumembedded/cef/tree/3282) | 64 (CEF 3) | Dec 2017 | Win 7+, VS2017 15.3.2+, Win10.0.15063.468 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3239](https://github.com/chromiumembedded/cef/tree/3239) | 63 (CEF 3) | Oct 2017 | Win 7+, VS2017 15.3.2+, Win10.0.15063.468 SDK, Ninja | macOS 10.10-10.13, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3202](https://github.com/chromiumembedded/cef/tree/3202) | 62 (CEF 3) | Sep 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.10-10.12, 10.10+ deployment target, 10.12+ build system w/ 10.12+ base SDK (Xcode 8.3.2+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3163](https://github.com/chromiumembedded/cef/tree/3163) | 61 (CEF 3) | Jul 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 8.3+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3112](https://github.com/chromiumembedded/cef/tree/3112) | 60 (CEF 3) | Jun 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 8.3+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3071](https://github.com/chromiumembedded/cef/tree/3071) | 59 (CEF 3) | Apr 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 8.3+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Jessie+, Ninja |
| [3029](https://github.com/chromiumembedded/cef/tree/3029) | 58 (CEF 3) | Mar 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 8.3+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2987](https://github.com/chromiumembedded/cef/tree/2987) | 57 (CEF 3) | Jan 2017 | Win 7+, VS2015u3, Win10.0.14393 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.3.1+), 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2924](https://github.com/chromiumembedded/cef/tree/2924) | 56 (CEF 3) | Nov 2016 | Win 7+, VS2015u3, Win10.0.10586 SDK, Ninja | macOS 10.9-10.12, 10.9+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2883](https://github.com/chromiumembedded/cef/tree/2883) | 55 (CEF 3) | Oct 2016 | Win 7+, VS2015u3, Win10.0.10586 SDK, Ninja | macOS 10.9-10.12, 10.7+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2840](https://github.com/chromiumembedded/cef/tree/2840) | 54 (CEF 3) | Aug 2016 | Win 7+, VS2015u2 or VS2015u3, Win10.0.10586 SDK, Ninja | macOS 10.9-10.12, 10.7+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2785](https://github.com/chromiumembedded/cef/tree/2785) | 53 (CEF 3) | Jul 2016 | Win 7+, VS2015u2 or VS2015u3, Win10.0.10586 SDK, Ninja | macOS 10.9-10.11, 10.7+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.3.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2743](https://github.com/chromiumembedded/cef/tree/2743) | 52 (CEF 3) | May 2016 | Win 7+, VS2015u2 or VS2015u3, Win10.0.10586 SDK, Ninja | macOS 10.9-10.11, 10.7+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.1.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2704](https://github.com/chromiumembedded/cef/tree/2704) | 51 (CEF 3) | Apr 2016 | Win 7+, VS2015u2, Win10.0.10586 SDK, Ninja | macOS 10.9-10.11, 10.7+ deployment target, 10.9+ build system w/ 10.10+ base SDK (Xcode 7.1.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2623](https://github.com/chromiumembedded/cef/tree/2623) | 49 (CEF 3) | Jan 2016 | WinXP+, VS2013u4 or VS2015u1 (experimental), Win10 SDK, Ninja | macOS 10.6-10.11, 10.6+ deployment target, 10.7+ build system w/ 10.10+ base SDK (Xcode 7.1.1+), Ninja, 64-bit only | Ubuntu 14.04+, Debian Wheezy+, Ninja |
| [2526](https://github.com/chromiumembedded/cef/tree/2526) | 47 (CEF 3) | Oct 2015 | WinXP+, VS2013u4 or VS2015u1 (experimental), Win8.1 SDK, Ninja | macOS 10.6-10.11, 10.6+ deployment target, 10.10 base SDK, Xcode 6.1, Ninja, 64-bit only | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [2454](https://github.com/chromiumembedded/cef/tree/2454) | 45 (CEF 3) | Jul 2015 | WinXP+, VS2013u4, Win8.1 SDK, Ninja | macOS 10.6-10.10, 10.6+ deployment target, 10.9 base SDK, Xcode 6.1, Ninja, 64-bit only | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [2357](https://github.com/chromiumembedded/cef/tree/2357) | 43 (CEF 3) | Apr 2015 | WinXP+, VS2013u4, Win8.1 SDK, Ninja | macOS 10.6-10.10, 10.6+ deployment target, 10.9 base SDK, Xcode 6.1, Ninja, 64-bit only | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [2272](https://github.com/chromiumembedded/cef/tree/2272) | 41 (CEF 3) | Jan 2015 | WinXP+, VS2013u4, Win8.1 SDK, Ninja | macOS 10.6-10.10, 10.6+ deployment target, 10.9 base SDK, Xcode 6.1, Ninja, 64-bit only | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [2171](https://github.com/chromiumembedded/cef/tree/2171) | 39 (CEF 3) | Oct 2014 | WinXP+, VS2013u4, Win8.1 SDK, Ninja | macOS 10.6-10.9, 10.6+ SDK, Xcode 5.1.1, Ninja | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [2062](https://github.com/chromiumembedded/cef/tree/2062) | 37 (CEF 3) | Aug 2014 | WinXP+, VS2013, Win8 SDK, Ninja | macOS 10.6-10.9, 10.6+ SDK, Xcode 5.1.1, Ninja | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [1916](https://github.com/chromiumembedded/cef/tree/1916) | 35 (CEF 3) | Apr 2014 | WinXP+, VS2013, Win8 SDK, Ninja | macOS 10.6-10.9, 10.6+ SDK, Xcode 5.1.1, Ninja | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [1750](https://github.com/chromiumembedded/cef/tree/1750) | 33 (CEF 3) | Jan 2014 | WinXP+, VS2010-2013, Win8 SDK, Ninja | macOS 10.6-10.9, 10.6+ SDK, Xcode 5.1.1, Ninja | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [1650](https://github.com/chromiumembedded/cef/tree/1650) | 31 (CEF 3) | Oct 2013 | WinXP+, VS2010-2012, Win8 SDK, Ninja (optional) | macOS 10.6-10.9, 10.6+ SDK, Xcode 5.1.1, Ninja | Ubuntu 12.04+, Debian Wheezy+, Ninja |
| [1547](https://github.com/chromiumembedded/cef/tree/1547) | 29 (CEF 3) | Jul 2013 | WinXP+, VS2010-2012, Win8 SDK, Ninja (optional) | macOS 10.6-10.8, 10.6+ SDK, Xcode 3.2.6-4.x, Ninja (optional) | Ubuntu 12.04+, Debian Squeeze+, Ninja |
| [1453](https://github.com/chromiumembedded/cef/tree/1453) | 27 (CEF 1, 3) | Apr 2013 | WinXP+, VS2010, Win8 SDK, Ninja (optional) | macOS 10.6-10.8, 10.6+ SDK, Xcode 3.2.6-4.x, Ninja (optional) | Ubuntu 12.04+, Debian Squeeze+, Ninja (optional) |
| [1364](https://github.com/chromiumembedded/cef/tree/1364) | 25 (CEF 1, 3) | Jan 2013 | WinXP+, VS2010, Win8 SDK, Ninja (optional) | macOS 10.6-10.8, Xcode 3.2.6-4.x, Ninja (optional) | Ubuntu 12.04+, Debian Squeeze+, Ninja (optional) |
| [1271](https://github.com/chromiumembedded/cef/tree/1271) | 23 (CEF 1, 3) | Oct 2012 | WinXP+, VS2010, Win7 SDK   | macOS 10.6-10.8, 10.6+ SDK, Xcode 3.2.6-4.x | Ubuntu 12.04+, Debian Squeeze+ |
| [1180](https://github.com/chromiumembedded/cef/tree/1180) | 21 (CEF 1, 3) | Aug 2012 | WinXP+, VS2010, Win7 SDK   | macOS 10.6-10.7, 10.5+ SDK, Xcode 3.2.6-4.x | Ubuntu 12.04+, Debian Squeeze+ |
| [1084](https://github.com/chromiumembedded/cef/tree/1084) | 19 (CEF 1) | Apr 2012 | WinXP+, VS2008, Win7 SDK   | macOS 10.6-10.7, 10.5+ SDK, Xcode 3.2.6-4.x | Ubuntu 10.04+, Debian Squeeze+ |
| [1025](https://github.com/chromiumembedded/cef/tree/1025) | 18 (CEF 1) | Feb 2012 | WinXP+, VS2008, Win7 SDK   | macOS 10.6-10.7, 10.5+ SDK, Xcode 3.2.6-4.x | Ubuntu 10.04+, Debian Squeeze+ |
| [963](https://github.com/chromiumembedded/cef/tree/963) | 17 (CEF 1) | Dec 2011 | WinXP+, VS2008, Win7 SDK   | macOS 10.6-10.7, 10.5+ SDK, Xcode 3.2.6 | Ubuntu 10.04+, Debian Squeeze+ |

The following URL should be used for downloading release versions of CEF where YYYY is the release branch number.

  * https://github.com/chromiumembedded/cef/tree/YYYY

Note that 1025 and older branches contain only CEF1 source code and that 1547 and newer branches contain only CEF3 source code.

# Building from Source

Building from source code is currently supported on Windows, macOS and Linux platforms. Use of the Automated Method described below is recommended. Building the current CEF/Chromium master branch for local development is described on the [Master Build Quick Start](master_build_quick_start.md) page. Building the current CEF/Chromium stable branch automatically for production use is described on the  [Automated Build Setup](automated_build_setup.md#linux-configuration) page. For other branches see the build requirements listed in the "Release Branches" section above and the "Build Notes" section below.

## Automated Method

CEF provides tools for automatically downloading, building and packaging Chromium and CEF source code. These tools are the recommended way of building CEF locally and can also be integrated with automated build systems as described on the [Automated Build Setup](automated_build_setup.md) page. See the [Master Build Quick Start](master_build_quick_start.md) page for an example of the recommended workflow for local development builds.

These steps apply to the Git workflow only. The Git workflow is recommended for all users and supports CEF3 master and newer CEF3 release branches (1750+).

1\. Download the [automate-git.py script](https://github.com/chromiumembedded/cef/blob/master/tools/automate/automate-git.py). Use the most recent master version of this script even when building release branches.

2\. On Linux: Chromium requires that certain packages be installed. You can install them by running the [install-build-deps.sh script](master_build_quick_start.md#linux-setup) or by [explicitly running](automated_build_setup.md#linux-configuration) the necessary installation commands.

3\. Run the automate-git.py script at whatever interval is appropriate (for each CEF commit, once per day, once per week, etc).

To build master:

```sh
python /path/to/automate/automate-git.py --download-dir=/path/to/download
```

To build a release branch:

```sh
python /path/to/automate/automate-git.py --download-dir=/path/to/download --branch=2785
```

By default the script will download depot\_tools, Chromium and CEF source code, run Debug and Release builds of CEF, and create a binary distribution package containing the build artifacts in the "/path/to/download/chromium/src/cef/binary\_distrib" directory. Future runs of the script will perform the minimum work necessary (unless otherwise configured using command-line flags). For example, if there are no pending CEF or Chromium updates the script will do nothing.

If you run the script and CEF or Chromium updates are pending the "/path/to/download/chromium/src/cef" directory will be removed and replaced with a clean copy from "/path/to/download/cef\_(branch)" (specify the `--no-update` command-line flag to disable updates). Make sure to back up any changes that you made in the "/path/to/download/chromium/src/cef" directory before re-running the script.

The same download directory can be used for building multiple CEF branches (just specify a different `--branch` command-line value). The existing "/path/to/download/chromium/src/out" directory will be moved to "/path/to/download/out\_(previousbranch)" so that the build output from the previous branch is not lost. When you switch back to a previous branch the out directory will be restored to its original location.

The script will create a 32-bit build on Windows by default. To create a 64-bit build on Windows, macOS or Linux specify the `--x64-build` command-line flag. 32-bit builds on macOS are [no longer supported](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/sdsDCkq_zwo/yep65H8Eg3sJ) starting with 2272 branch so this flag is now required when building 2272+ on that platform.

If you receive Git errors when moving an existing checkout from one branch to another you can force a clean Chromium Git checkout (specify the  `--force-clean` command-line flag) and optionally a clean download of Chromium dependencies (specify the `--force-clean-deps` command-line flag). Any build output that currently exists in the "src/out" directory will be deleted. Re-downloading the Chromium dependencies can take approximately 30 minutes with a reasonably fast internet connection.

Add the `--help` command-line switch to output a complete list of supported command-line options.

## Manual Downloading

See the [Master Build Quick Start](master_build_quick_start.md) page for an example of the recommended developer workflow.

## Manual Building

See the [Master Build Quick Start](master_build_quick_start.md) page for an example of the recommended developer workflow.

## Manual Packaging

After building both Debug and Release configurations you can use the make\_distrib tool (.bat on Windows, .sh on macOS and Linux) to create a binary distribution.

```sh
cd /path/to/chromium/src/cef/tools
./make_distrib.sh --ninja-build
```

If the process succeeds a binary distribution package will be created in the /path/to/chromium/src/cef/binary\_distrib directory.

See the [make\_distrib.py](https://github.com/chromiumembedded/cef/blob/master/tools/make_distrib.py) script for additional usage options.

The resulting binary distribution can then be built using CMake and platform toolchains. See the README.txt file included with the binary distribution for more information.

## Build Notes

This section summarizes build-related requirements and options.

  * Building on most platforms will require at least 8GB of system memory.
  * [Ninja](https://ninja-build.org/) must be used when building newer CEF/Chromium branches.
  * [Clang](https://clang.llvm.org/) is used by default for compiling/linking Chromium/CEF on macOS in all branches, Linux starting in 2063 branch, and Windows starting in 3282 branch.
  * [GYP](https://gyp.gsrc.io/docs/UserDocumentation.md) is supported by 2785 branch and older. [GN](http://www.chromium.org/developers/gn-build-configuration) is supported by 2785 branch and newer, and required starting with 2840 branch. Set `CEF_USE_GN=1` to build 2785 branch with GN instead of GYP.
  * To perform a 64-bit build on Windows (any branch) or macOS (branch 2171 or older) set `GYP_DEFINES=target_arch=x64` (GYP only) or build the `out/[Debug|Release]_GN_x64` target (GN only). To perform a 32-bit Linux build on a 64-bit Linux system see instructions on the [Automated Build Setup](automated_build_setup.md#linux-configuration) page.
  * To perform an "official" build set `GYP_DEFINES=buildtype=Official` (GYP only) or `GN_DEFINES=is_official_build=true` (GN only). This will disable debugging code and enable additional link-time optimizations in Release builds. See instructions on the [Automated Build Setup](automated_build_setup.md) page for additional official build recommendations.
  * Windows -
    * If multiple versions of Visual Studio are installed on your system you can set the GYP\_MSVS\_VERSION environment variable to create project files for that version. For example, set the value to "2015" for VS2015 or "2017" for VS2017. Check the Chromium documentation for the correct value when using other Visual Studio versions.
    * If you wish to use Visual Studio for debugging and compiling in combination with a Ninja build you can set `GYP_GENERATORS=ninja,msvs-ninja` (GYP only) or `GN_ARGUMENTS=--ide=vs2017 --sln=cef --filters=//cef/*` (GN only) to generate both Ninja and VS project files. Visual Studio is supported only for debugging and compiling individual source files -- it will not build whole targets successfully. You must use Ninja when building CEF/Chromium targets.
    * For best local developer (non-official debug) build-time performance:
        * When using VS2015 set `GN_DEFINES=is_win_fastlink=true` for improved compile and link time (branches <= 3202).
        * When using VS2017 or VS2019 set `GN_DEFINES=use_jumbo_build=true` for improved compile and link time (branches <= 4044).
        * Component builds are supported by 3202 branch and newer and significantly reduce link time. Add `is_component_build=true` to GN_DEFINES in combination with the above VS-version-specific values. Component builds cannot be used to create a CEF binary distribution. See [issue #1617](https://github.com/chromiumembedded/cef/issues/1617#issuecomment-1464996498) for details.
  * macOS -
    * The combination of deployment target and base SDK version will determine the platforms supported by the resulting binaries. For proper functioning you must use the versions specified under build requirements for each branch.
    * 32-bit builds are no longer supported with 2272 branch and newer. See [here](https://groups.google.com/a/chromium.org/d/msg/chromium-dev/sdsDCkq_zwo/yep65H8Eg3sJ) for the Chromium announcement.
  * Linux -
    * CEF is developed and tested on the Ubuntu version specified in the "Release Branches" section. It should be possible to build and run CEF on other compatible Linux distributions but this is untested.
    * The libgtkglext1-dev package is required in branch 1547 and newer to support the off-screen rendering example in cefclient. This is only a requirement for cefclient and not a requirement for other applications using CEF.