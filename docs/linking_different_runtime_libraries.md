This page explains how to link CEF against a different run-time library.


***

**Contents**

- [Introduction](#introduction)
- [Details](#details)
  - [Static Linking (without a CEF DLL)](#static-linking-without-a-cef-dll)
  - [Dynamic Linking (with a CEF DLL)](#dynamic-linking-with-a-cef-dll)
- [Additional Information](#additional-information)

---

# Introduction

Visual Studio supports multiple run-time libraries. The different libraries are identified by flags such as `/MD`, `/MT` and `/LD`. By default, CEF uses the `/MT` flag that is also used by the Chromium project. Different applications, however, can sometimes require different run-time libraries.

# Details

There are two ways currently to link CEF with your application.

## Static Linking (without a CEF DLL)

If you wish to link CEF with your application statically then all Chromium and CEF projects must use the same run-time flag as your application.  If your application already uses the `/MT` flag then you can build CEF statically as follows:

  1. Set up the environment required to build Chromium and CEF as described on the project page and build it.
  1. Link your application against the resulting libcef\_static.lib file.

If, however, your application does not use the `/MT` flag then you're effectively out of luck. Important parts of Chromium will not compile with a flag other than `/MT`.

## Dynamic Linking (with a CEF DLL)

If you prefer to link CEF with your application dynamically then the process is faster. CEF provides a binary distribution on the downloads page that contains everything you need to link CEF with your application including source code for the libcef\_dll\_wrapper project. If your application does not use the `/MT` flag then you will need to rebuild the libcef\_dll\_wrapper project with the same flags as your application. Unlike with static linking, you will not need to rebuild all of CEF or Chromium in order to do this.

  1. Download a CEF binary release from the project downloads page.
  1. Run CMake with the additional `-DCEF_RUNTIME_LIBRARY_FLAG=/MD` command-line flag (`/MD` can be replaced with other flags as appropriate).
  1. Open cef.sln in Visual Studio.
  1. Right click on the libcef\_dll\_wrapper project and choose the "Project Only -> Build Only libcef\_dll\_wrapper" option.

Sandbox support (linking `cef_sandbox.lib`) is only possible when your application is built with the `/MT` flag. To disable sandbox usage run CMake with the additional `-DUSE_SANDBOX=Off` command-line flag.

# Additional Information

More information on Microsoft run-time flags can be found here:
http://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx