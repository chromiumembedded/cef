CONTENTS
--------

cefclient   Contains the cefclient sample application configured to build
            using the files in this distribution.

Debug       Contains libcef.dylib and other components required to run the debug
            version of CEF-based applications.

include     Contains all required CEF header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dylib and other components required to run the
            release version of CEF-based applications.

Resources   Contains images and resources required by applications using CEF.
            The contents of this folder should be transferred to the
            Contents/Resources folder in the app bundle.

tools       Scripts that perform post-processing on Mac release targets.


USAGE
-----

Xcode 3 and 4: Open the cefclient.xcodeproj project and build.

When using Xcode 4.2 or newer you will need to change the "Compiler for
C/C++/Objective-C" setting to "LLVM GCC 4.2" under "Build Settings" for
each target.

Please visit the CEF Website for additional usage information.

http://code.google.com/p/chromiumembedded
