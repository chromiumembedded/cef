CONTENTS
--------

bazel       Contains Bazel configuration files shared by all targets.

cmake       Contains CMake configuration files shared by all targets.

Debug       Contains the "Chromium Embedded Framework.framework" and other
            components required to run the debug version of CEF-based
            applications.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains the "Chromium Embedded Framework.framework" and other
            components required to run the release version of CEF-based
            applications.

tests/      Directory of tests that demonstrate CEF usage.

  cefclient Contains the cefclient sample application configured to build
            using the files in this distribution. This application demonstrates
            a wide range of CEF functionalities.

  cefsimple Contains the cefsimple sample application configured to build
            using the files in this distribution. This application demonstrates
            the minimal functionality required to create a browser window.

  ceftests  Contains unit tests that exercise the CEF APIs.

  gmock     Contains the Google C++ Mocking Framework used by the ceftests
            target.

  gtest     Contains the Google C++ Testing Framework used by the ceftests
            target.

  shared    Contains source code shared by the cefclient and ceftests targets.


USAGE
-----

Building using CMake:
  CMake can be used to generate project files in many different formats. See
  usage instructions at the top of the CMakeLists.txt file.

Building using Bazel:
  Bazel can be used to build CEF-based applications. CEF support for Bazel is
  considered experimental. For current development status see
  https://github.com/chromiumembedded/cef/issues/3757.

  To build the bundled cefclient sample application using Bazel:

  1. Install Bazelisk [https://github.com/bazelbuild/bazelisk/blob/master/README.md]
  2. Build using Bazel:
     $ bazel build //tests/cefclient
  3. Run using Bazel:
     $ bazel run //tests/cefclient

  Other sample applications (cefsimple, ceftests) can be built in the same way.

  Additional notes:
  - To generate a Debug build add `-c dbg` (both `build` and `run`
    command-line).
  - To generate an Intel 64-bit cross-compile build on an ARM64 host add
    `--cpu=darwin_x86_64` (both `build` and `run` command-line).
  - To generate an ARM64 cross-compile build on an Intel 64-bit host add
    `--cpu=darwin_arm64` (both `build` and `run` command-line).
  - To pass arguments using the `run` command add `-- [...]` at the end.

Please visit the CEF Website for additional usage information.

https://github.com/chromiumembedded/cef/
