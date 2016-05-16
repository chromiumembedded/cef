CONTENTS
--------

cmake       Contains CMake configuration files shared by all targets.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dll, libcef.lib and other components required to
            build and run the release version of CEF-based applications. By
            default these files should be placed in the same directory as the
            executable.

Resources   Contains resources required by libcef.dll. By default these files
            should be placed in the same directory as libcef.dll.


USAGE
-----

Building using CMake:
  CMake can be used to generate project files in many different formats. See
  usage instructions at the top of the CMakeLists.txt file.

Please visit the CEF Website for additional usage information.

https://bitbucket.org/chromiumembedded/cef/
