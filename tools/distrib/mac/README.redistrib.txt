REDISTRIBUTION
--------------

This binary distribution contains the below components. Components listed under
the "required" section must be redistributed with all applications using CEF.
Components listed under the "optional" section may be excluded if the related
features will not be used.

Applications using CEF on OS X must follow a specific app bundle structure.
Replace "cefclient" in the below example with your application name.

cefclient.app/
  Contents/
    Frameworks/
      Chromium Embedded Framework.framework/
        Chromium Embedded Framework <= main application library
        Libraries/
          ffmpegsumo.so <= HTML5 audio/video support library
          PDF.plugin <= Pepper plugin for PDF support
        Resources/
          cef.pak <= non-localized resources and strings
          cef_100_percent.pak <====^
          cef_200_percent.pak <====^
          devtools_resources.pak <=^
          crash_inspector, crash_report_sender <= breakpad support
          icudtl.dat <= unicode support
          natives_blob.bin, snapshot_blob.bin <= V8 initial snapshot
          en.lproj/, ... <= locale-specific resources and strings
          Info.plist
      cefclient Helper.app/
        Contents/
          Info.plist
          MacOS/
            cefclient Helper <= helper executable
          Pkginfo
      cefclient Helper EH.app/
        Contents/
          Info.plist
          MacOS/
            cefclient Helper EH <= helper executable
          Pkginfo
      cefclient Helper NP.app/
        Contents/
          Info.plist
          MacOS/
            cefclient Helper NP <= helper executable
          Pkginfo
      Info.plist
    MacOS/
      cefclient <= cefclient application executable
    Pkginfo
    Resources/
      binding.html, ... <= cefclient application resources

The "Chromium Embedded Framework.framework" is an unversioned framework that
contains CEF binaries and resources. Executables (cefclient, cefclient Helper,
etc) are linked to the "Chromium Embedded Framework" library using
install_name_tool and a path relative to @executable_path.

The "cefclient Helper" apps are used for executing separate processes
(renderer, plugin, etc) with different characteristics. They need to have
separate app bundles and Info.plist files so that, among other things, they
don't show dock icons. The "EH" helper, which is used when launching plugin
processes, has the MH_NO_HEAP_EXECUTION bit cleared to allow an executable
heap. The "NP" helper, which is used when launching NaCl plugin processes
only, has the MH_PIE bit cleared to disable ASLR. This is set up as part of
the build process using scripts from the tools/ directory. Examine the Xcode
project included with the binary distribution or the originating cefclient.gyp
file for a better idea of the script dependencies.

Required components:

* CEF framework library
    Chromium Embedded Framework.framework/Chromium Embedded Framework

* Unicode support
    Chromium Embedded Framework.framework/Resources/icudtl.dat

* V8 initial snapshot
    Chromium Embedded Framework.framework/Resources/natives_blob.bin
    Chromium Embedded Framework.framework/Resources/snapshot_blob.bin

Optional components:

* Localized resources
    Chromium Embedded Framework.framework/Resources/*.lproj/
  Note: Contains localized strings for WebKit UI controls. A .pak file is loaded
  from this folder based on the CefSettings.locale value. Only configured
  locales need to be distributed. If no locale is configured the default locale
  of "en" will be used. Locale file loading can be disabled completely using
  CefSettings.pack_loading_disabled.

* Other resources
    Chromium Embedded Framework.framework/Resources/cef.pak
    Chromium Embedded Framework.framework/Resources/cef_100_percent.pak
    Chromium Embedded Framework.framework/Resources/cef_200_percent.pak
    Chromium Embedded Framework.framework/Resources/devtools_resources.pak
  Note: Contains WebKit image and inspector resources. Pack file loading can be
  disabled completely using CefSettings.pack_loading_disabled. The resources
  directory path can be customized using CefSettings.resources_dir_path.

* FFmpeg audio and video support
    Chromium Embedded Framework.framework/Libraries/ffmpegsumo.so
  Note: Without this component HTML5 audio and video will not function.

* PDF support
    Chromium Embedded Framework.framework/Libraries/PDF.plugin

* Breakpad support
    Chromium Embedded Framework.framework/Resources/crash_inspector
    Chromium Embedded Framework.framework/Resources/crash_report_sender
    Chromium Embedded Framework.framework/Resources/Info.plist
  Note: Without these components breakpad support will not function.
