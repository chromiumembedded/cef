CONTENTS
--------

cmake       Contains CMake configuration files shared by all targets.

Debug       Contains libcef.so and other components required to run the debug
            version of CEF-based applications. By default these files should be
            placed in the same directory as the executable and will be copied
            there as part of the build process.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.so and other components required to run the release
            version of CEF-based applications. By default these files should be
            placed in the same directory as the executable and will be copied
            there as part of the build process.

Resources   Contains resources required by libcef.so. By default these files
            should be placed in the same directory as libcef.so and will be
            copied there as part of the build process.

tests/      Directory of tests that demonstrate CEF usage.

  cefclient Contains the cefclient sample application configured to build
            using the files in this distribution. This application demonstrates
            a wide range of CEF functionalities.

  cefsimple Contains the cefsimple sample application configured to build
            using the files in this distribution. This application demonstrates
            the minimal functionality required to create a browser window.

  ceftests  Contains unit tests that exercise the CEF APIs.

  gtest     Contains the Google C++ Testing Framework used by the ceftests
            target.

  shared    Contains source code shared by the cefclient and ceftests targets.


USAGE
-----

Building using CMake:
  CMake can be used to generate project files in many different formats. See
  usage instructions at the top of the CMakeLists.txt file.

Please visit the CEF Website for additional usage information.

https://bitbucket.org/chromiumembedded/cef/
