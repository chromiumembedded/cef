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

Optional components:

* Localized resources
    locales/
  Note: Contains localized strings for WebKit UI controls. A .pak file is loaded
  from this folder based on the CefSettings.locale value. Only configured
  locales need to be distributed. If no locale is configured the default locale
  of "en-US" will be used. Locale file loading can be disabled completely using
  CefSettings.pack_loading_disabled. The locales folder path can be customized
  using CefSettings.locales_dir_path.

* Other resources
    devtools_resources.pak
  Note: Contains WebKit image and inspector resources. Pack file loading can be
  disabled completely using CefSettings.pack_loading_disabled. The resources
  directory path can be customized using CefSettings.resources_dir_path.

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
