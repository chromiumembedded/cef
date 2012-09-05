Chromium Embedded Framework (CEF) Binary Distribution
-------------------------------------------------------------------------------

Date:             $DATE$

CEF Version:      $CEF_VER$
CEF URL:          $CEF_URL$@$CEF_REV$

Chromium Verison: $CHROMIUM_VER$
Chromium URL:     $CHROMIUM_URL$@$CHROMIUM_REV$


This distribution contains all components necessary to build and distribute an
application using CEF. Please see the LICENSING section of this document for
licensing terms and conditions.


CONTENTS
--------

cefclient   Contains the cefclient sample application configured to build
            using the files in this distribution.

Debug       Contains libcef.dll and other components required to run the debug
            version of CEF-based applications. Also acts as the build target for
            the Debug build of cefclient.

docs        Contains C++ API documentation generated from the CEF header files.

include     Contains all required CEF and NPAPI-related header files.  Read
            the include/internal/npapi/README-TRANSFER.txt file for more
            information about the NPAPI-related header files.

lib         Contains Debug and Release versions of the libcef.lib library file
            that all CEF-based applications must link against.

libcef_dll  Contains the source code for the libcef_dll_wrapper static library
            that all applications using the CEF C++ API must link against.

Release     Contains libcef.dll and other components required to run the release
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


REDISTRIBUTION
--------------

This binary distribution contains the below components. Components listed under
the "required" section must be redistributed with all applications using CEF.
Components listed under the "optional" section may be excluded if the related
features will not be used.

Required components:

* CEF core library
    libcef.dll

* Unicode support
    icudt.dll

* Localized resources
    locales/
  Note: A .pak file is loaded from this folder based on the value of
  CefSettings.locale. Only configured locales need to be distributed. If no
  locale is configured the default locale of "en-US" will be used. The
  locales folder must exist in the same directory as libcef.dll.

* Other resources
    devtools_resources.pak
  Note: The pak file must exist in the same directory as libcef.dll.

Optional components:

* FFmpeg audio and video support
    avcodec-53.dll
    avformat-53.dll
    avutil-51.dll
  Note: Without these components HTML5 audio and video will not function.

* Angle and Direct3D support
    d3dcompiler_43.dll
    d3dx9_43.dll
    libEGL.dll
    libGLESv2.dll
  Note: Without these components the default ANGLE_IN_PROCESS graphics
  implementation for HTML5 accelerated content like 2D canvas, 3D CSS and
  WebGL will not function. To use the desktop GL graphics implementation which
  does not require these components (and does not work on all systems) set
  CefSettings.graphics_implementation to DESKTOP_IN_PROCESS.


LICENSING
---------

The CEF project is BSD licensed. Please read the LICENSE.txt file included with
this binary distribution for licensing terms and conditions. Other software
included in this distribution is provided under other licenses. Please visit the
below link for complete Chromium and third-party licensing information.

http://code.google.com/chromium/terms.html 
