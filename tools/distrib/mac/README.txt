Chromium Embedded Framework (CEF) Binary Distribution
-------------------------------------------------------------------------------

CEF Revision:        $CEF_REV$
Chromium Revision:   $CHROMIUM_REV$
Date:                $DATE$

This distribution contains all files necessary to build an application using
CEF.  Please read the included LICENSE.txt file for licensing terms and
restrictions.


CONTENTS
--------

cefclient   Contains the cefclient sample application configured to build
            using the files in this distribution.

Debug       Contains libcef.dylib and other libraries required to run the debug
            version of CEF-based applications.

docs        Contains C++ API documentation generated from the CEF header files.

include     Contains all required CEF and NPAPI-related header files.  Read
            the include/internal/npapi/README-TRANSFER.txt file for more
            information about the NPAPI-related header files.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dylib and other libraries required to run the
            release version of CEF-based applications.

Resources   Contains images and resources required by applications using CEF.
            The contents of this folder should be transferred to the
            Contents/Resources folder in the app bundle.

tools       Scripts that perform post-processing on Mac release targets.


USAGE
-----

Xcode 3 and 4: Open the cefclient.xcodeproj project and build.

Please visit the CEF Website for additional usage information.

http://code.google.com/p/chromiumembedded
