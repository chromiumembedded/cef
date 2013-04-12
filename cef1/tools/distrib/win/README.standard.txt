CONTENTS
--------

cefclient   Contains the cefclient sample application configured to build
            using the files in this distribution.

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
            should be placed in the same directory as libcef.dll. By default
            these files should be placed in the same directory as libcef.dll
            and will be copied there as part of the build process.


USAGE
-----

Visual Studio 2012 and Visual Studio 2010:
  Open the cefclient2010.sln solution in Visual Studio and build.

Visual Studio 2008:
  Open the cefclient2008.sln solution in Visual Studio and build.

Visual Studio 2005:
  1. Open the cefclient.vcproj and libcef_dll_wrapper.vcproj files in a text
     editor. Change Version="9.00" to Version="8.00".
  2. Open the cefclient2005.sln file in a text editor. Change "Version 9.00" to
     "Version 8.00".
  3. Open the cefclient2005.sln solution in Visual Studio and build.

Please visit the CEF Website for additional usage information.

http://code.google.com/p/chromiumembedded
