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

Debug       Contains libcef.dll and other DLLs required to run the debug version
            of CEF-based applications. Also acts as the build target for the
            Debug build of cefclient.

docs        Contains C++ API documentation generated from the CEF header files.

include     Contains all required CEF and NPAPI-related header files.  Read
            the include/internal/npapi/README-TRANSFER.txt file for more
            information about the NPAPI-related header files.

lib         Contains Debug and Release versions of the libcef.lib library file
            that all CEF-based applications must link against.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dll and other DLLs required to run the release
            version of CEF-based applications. Also acts as the build target for
            the Release build of cefclient.


USAGE
-----

Visual Studio 2010: Open the cefclient2010.sln solution and build.
Visual Studio 2008: Open the cefclient2008.sln solution and build.
  * If using VS2008 Express Edition add atlthunk.lib to the cefclient
    Configuration Properties > Linker > Input > Additional Dependencies
Visual Studio 2005: Open the cefclient2005.sln solution and build.

Please visit the CEF Website for additional usage information.

http://code.google.com/p/chromiumembedded
