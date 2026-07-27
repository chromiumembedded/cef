CONTENTS
--------

bazel       Contains Bazel configuration files shared by all targets.

cmake       Contains CMake configuration files shared by all targets.

Debug       Contains libcef.dll, libcef.lib and other components required to
            build and run the debug version of CEF-based applications. By
            default these files should be placed in the same directory as the
            executable and will be copied there as part of the build process.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dll, libcef.lib and other components required to
            build and run the release version of CEF-based applications. By
            default these files should be placed in the same directory as the
            executable and will be copied there as part of the build process.

Resources   Contains resources required by libcef.dll. By default these files
            should be placed in the same directory as libcef.dll and will be
            copied there as part of the build process.

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
  To configure and build the normal cefclient sample with Visual Studio 2022:

     $ cmake -S . -B build -G "Visual Studio 17 2022" -A x64
     $ cmake --build build --config Debug --target cefclient

  For a 32-bit or ARM64 binary distribution, replace x64 with Win32 or arm64,
  respectively. See the usage instructions at the top of CMakeLists.txt for
  other generators and build options.

  Installer-managed cefclient:
  To build cefclient so it can locate or install compatible CEF on first run,
  use a separate build directory, add -DUSE_INSTALLER=On to the configure
  command above, and build the cefclient target in Release.

  USE_INSTALLER=On is Windows-only, requires USE_SANDBOX=On, and supports
  Release builds only. You only need to request cefclient; CMake builds its
  required libcef_dll_wrapper dependency and may create additional generator
  utility targets.

  The Release output contains:
  - cefclient.exe, a byte-for-byte copy of Release\bootstrap.exe
  - cefclient.dll, with the managed installer configuration embedded
  - chrome_elf.dll
  - normal compiler and linker artifacts, such as .lib, .exp, and .pdb files

  A compatible signed CEF distribution will be installed or located on first
  run.

  Before shipping:
  1. Open tests\cefclient\win\installer_config_managed.json and replace the
     sample appid with your application's permanent UUID.
  2. Sign cefclient.exe, cefclient.dll, and chrome_elf.dll. chrome_elf.dll must
     use the same signing certificate as cefclient.exe.
  3. Review the production identity, signing, publication, and installer
     requirements in the CEF Installer documentation:
     https://chromiumembedded.github.io/cef/installer.html

  This example supports normal first-run CEF resolution. It does not modify
  bootstrap.exe resources, so standalone installer mode and explicit commands
  such as /cef-update and /cef-uninstall are not enabled.

  Compatibility details: The embedded configuration accepts CEF releases from
  its selected API version through minor version 99 of the same API major. It
  uses explicit launch-health reporting, which cefclient handles automatically.

Building using Bazel:
  Bazel can be used to build CEF-based applications. CEF support for Bazel is
  considered experimental. For current development status see
  https://github.com/chromiumembedded/cef/issues/3757.

  To build the bundled cefclient sample application using Bazel:

  1. Install Bazelisk [https://github.com/bazelbuild/bazelisk/blob/master/README.md]
  2. Build using Bazel:
     $ bazel build //tests/cefclient
  3. Run using Bazel:
     $ bazel run //tests/cefclient/win:cefclient.exe

  Other sample applications (cefsimple, ceftests) can be built in the same way.

  Additional notes:
  - To generate a Debug build add `-c dbg` (both `build` and `run`
    command-line).
  - To pass arguments using the `run` command add `-- [...]` at the end.
  - Windows x86 and ARM64 builds using Bazel may be broken, see
    https://github.com/bazelbuild/bazel/issues/22164.

Please visit the CEF Website for additional usage information.

https://github.com/chromiumembedded/cef/
