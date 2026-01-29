This page explains how to handle crashes in CEF-based applications.

**Contents**

- [Overview](#overview)
- [Configuration](#configuration)
  - [[Config] section](#config-section)
  - [[CrashKeys] section](#crashkeys-section)
- [Testing](#testing)
- [Decoding Minidumps](#decoding-minidumps)

---

# Overview

As a developer or vendor that cares about software quality you would like to know when and how your application is crashing in the wild. A crash reporting system is an important part of that feedback loop. Applications using CEF version 3.2883.1543 or newer have the option to save crash reports locally or upload them to a web server for later analysis. CEF generates minidump files that include exception state, callstacks, stack memory, and loaded modules. The crash report upload, if enabled, includes the minidump file and configurable crash metadata information such as product state, command-line flags and crash keys. Minidump files can be analyzed using existing systems such as [Socorro](https://wiki.mozilla.org/Socorro) or command-line tools (see the "Decoding Minidumps" section).

CEF provides a simple HTTP server script written in Python that can be used for testing crash report upload (see the Testing section below). See the Socorro documentation for details on how to build a robust crash reporting server for use with production applications.

The crash reporting capability in CEF/Chromium is implemented using [Crashpad](https://chromium.googlesource.com/crashpad/crashpad/+/master/README.md) on Windows and macOS, and [Breakpad](https://chromium.googlesource.com/breakpad/breakpad) on Linux.

# Configuration

Crash reporting is configured using an INI-style config file named "crash_reporter.cfg". On Windows and Linux this file must be placed next to the main application executable. On macOS this file must be placed in the top-level app bundle Resources directory (e.g. "<appname>.app/Contents/Resources"). See comments in [include/cef_crash_util.h](https://github.com/chromiumembedded/cef/blob/master/include/cef_crash_util.h) for the most up-to-date version of this documentation.

The "crash_reporter.cfg" file contents are defined as follows:

```sh
# Comments start with a hash character and must be on their own line.

[Config]
ProductName=<Value of the "prod" crash key; defaults to "cef">
ProductVersion=<Value of the "ver" crash key; defaults to the CEF version>
AppName=<Windows only; App-specific folder name component for storing crash
         information; default to "CEF">
ExternalHandler=<Windows only; Name of the external handler exe to use
                 instead of re-launching the main exe; default to empty>
BrowserCrashForwardingEnabled=<macOS only; True if browser process crashes
                              should be forwarded to the system crash
                              reporter; default to false>
ServerURL=<crash server URL; default to empty>
RateLimitEnabled=<True if uploads should be rate limited; default to true>
MaxUploadsPerDay=<Max uploads per 24 hours, used if rate limit is enabled;
                  default to 5>
MaxDatabaseSizeInMb=<Total crash report disk usage greater than this value
                     will cause older reports to be deleted; default to 20>
MaxDatabaseAgeInDays=<Crash reports older than this value will be deleted;
                      default to 5>

[CrashKeys]
my_key1=<small|medium|large>
my_key2=<small|medium|large>
```

## [Config] section

Values in the "[Config]" section work as follows:

* If "ProductName" and/or "ProductVersion" are set then the specified values will be included in the crash dump metadata. On macOS if these values are set to empty then they will be retrieved from the Info.plist file using the "CFBundleName" and "CFBundleShortVersionString" keys respectively.
* If "AppName" is set on Windows then crash report information (metrics, database and dumps) will be stored locally on disk under the "C:\Users\\[CurrentUser]\AppData\Local\\[AppName]\User Data" folder. On other platforms the CefSettings.user_data_path value will be used.
* If "ExternalHandler" is set on Windows then the specified exe will be launched as the crashpad-handler instead of re-launching the main process exe. The value can be an absolute path or a path relative to the main exe directory. On Linux the CefSettings.browser_subprocess_path value will be used. On macOS the existing subprocess app bundle will be used.
* If "BrowserCrashForwardingEnabled" is set to true on macOS then browser process crashes will be forwarded to the system crash reporter. This results in the crash UI dialog being displayed to the user and crash reports being logged under "~/Library/Logs/DiagnosticReports". Forwarding of crash reports from non-browser processes and Debug builds is always disabled.
* If "ServerURL" is set then crashes will be uploaded as a multi-part POST request to the specified URL. Otherwise, reports will only be stored locally on disk.
* If "RateLimitEnabled" is set to true then crash report uploads will be rate limited as follows:
    * If "MaxUploadsPerDay" is set to a positive value then at most the specified number of crashes will be uploaded in each 24 hour period.
    * If crash upload fails due to a network or server error then an incremental backoff delay up to a maximum of 24 hours will be applied for retries.
    * If a backoff delay is applied and "MaxUploadsPerDay" is > 1 then the "MaxUploadsPerDay" value will be reduced to 1 until the client is restarted. This helps to avoid an upload flood when the network or server error is resolved.
    * Rate limiting is not supported on Linux.
* If "MaxDatabaseSizeInMb" is set to a positive value then crash report storage on disk will be limited to that size in megabytes. For example, on Windows each dump is about 600KB so a "MaxDatabaseSizeInMb" value of 20 equates to about 34 crash reports stored on disk. Not supported on Linux.
* If "MaxDatabaseAgeInDays" is set to a positive value then crash reports older than the specified age in days will be deleted. Not supported on Linux.

## [CrashKeys] section

A maximum of 26 crash keys of each size can be specified for use by the application. Crash key values will be truncated based on the specified size (small = 64 bytes, medium = 256 bytes, large = 1024 bytes). The value of crash keys can be set from any thread or process using the CefSetCrashKeyValue function. These key/value pairs will be sent to the crash server along with the crash dump file.

# Testing

CEF provides a [tools/crash_server.py](https://github.com/chromiumembedded/cef/blob/master/tools/crash_server.py) script that implements a simple HTTP server for receiving crash report uploads from a Breakpad/Crashpad client (any application using CEF version 3.2883.1543 or newer). This script is intended for testing purposes only. An HTTPS server and a system such as [Socorro](https://wiki.mozilla.org/Socorro) should be used when uploading crash reports from production applications. See comments in the crash_server.py script for the most up-to-date version of this documentation.

Usage of the crash_server.py script is as follows:

1\. Run the script from the command-line. The first argument is the server port number and the second argument is the directory where uploaded report information will be saved:

```sh
python crash_server.py 8080 /path/to/dumps
```

2\. Create a "crash_reporter.cfg" file at the required platform-specific location. On Windows and Linux this file must be placed next to the main application executable. On macOS this file must be placed in the top-level app bundle Resources directory (e.g. "<appname>.app/Contents/Resources"). At a minimum it must contain a "ServerURL=http://localhost:8080" line under the "[Config]" section (make sure the port number matches the value specified in step 1). See the Configuration section above for a complete specification of this file's contents.

Example file contents:

```
[Config]
# Product information.
ProductName=cefclient
ProductVersion=1.0.0

# Required to enable crash dump upload.
ServerURL=http://localhost:8080

# Disable rate limiting so that all crashes are uploaded.
RateLimitEnabled=false
MaxUploadsPerDay=0

[CrashKeys]
# The cefclient sample application sets these values (see step 5 below).
testkey_small1=small
testkey_small2=small
testkey_medium1=medium
testkey_medium2=medium
testkey_large1=large
testkey_large2=large
```

3\. Load one of the following URLs in the CEF-based application to cause a crash:

* Main (browser) process crash:   chrome://inducebrowsercrashforrealz
* Renderer process crash:         chrome://crash
* GPU process crash:              chrome://gpucrash

4\. When the script successfully receives a crash report upload you will see console output like the following:

```
01/10/2017 12:31:23: Dump <id>
```

The "<id>" value is a 16 digit hexadecimal string that uniquely identifies the dump. Crash minidump contents and metadata (product state, command-line flags and crash keys) will be written to the "<id>.dmp" and "<id>.json" files respectively underneath the directory specified in step 1.

On Linux, Breakpad uses the `wget` utility to upload crash dumps, so make sure that utility is installed. If the crash is handled correctly then you should see console output like the following when the client uploads a crash dump:

```text
--2017-01-10 12:31:22--  http://localhost:8080/
Resolving localhost (localhost)... 127.0.0.1
Connecting to localhost (localhost)|127.0.0.1|:8080... connected.
HTTP request sent, awaiting response... 200 OK
Length: unspecified [text/html]
Saving to: '/dev/fd/3'
Crash dump id: <id>
```

On macOS, when uploading a crash report to the script over HTTP you may receive an error like the following:

```
Transport security has blocked a cleartext HTTP (http://) resource load
since it is insecure. Temporary exceptions can be configured via your app's
Info.plist file.
```

You can work around this error by adding the "NSAppTransportSecurity" key to the Info.plist file in your application's Helper.app bundle (e.g. "tests/cefclient/resources/mac/helper-Info.plist" before building cefclient, or "<appname>.app/Contents/Frameworks/<appname> Helper. app/Contents/Info.plist" in the resulting app bundle):

```html
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <!--Other existing keys here...-->
  <key>NSAppTransportSecurity</key>
  <dict>
    <!--Allow all connections (for testing only!)-->
    <key>NSAllowsArbitraryLoads</key>
    <true/>
  </dict>
</dict>
</plist>
```

5\. The cefclient sample application sets test crash key values in the browser and renderer processes. To work properly these values must also be defined in the "[CrashKeys]" section of "crash_reporter.cfg" as shown above.

In [tests/cefclient/browser/client_browser.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefclient/browser/client_browser.cc) (browser process):

```
CefSetCrashKeyValue("testkey_small1", "value1_small_browser");
CefSetCrashKeyValue("testkey_small2", "value2_small_browser");
CefSetCrashKeyValue("testkey_medium1", "value1_medium_browser");
CefSetCrashKeyValue("testkey_medium2", "value2_medium_browser");
CefSetCrashKeyValue("testkey_large1", "value1_large_browser");
CefSetCrashKeyValue("testkey_large2", "value2_large_browser");
```

In [tests/cefclient/renderer/client_renderer.cc](https://github.com/chromiumembedded/cef/blob/master/tests/cefclient/renderer/client_renderer.cc) (renderer process):

```
CefSetCrashKeyValue("testkey_small1", "value1_small_renderer");
CefSetCrashKeyValue("testkey_small2", "value2_small_renderer");
CefSetCrashKeyValue("testkey_medium1", "value1_medium_renderer");
CefSetCrashKeyValue("testkey_medium2", "value2_medium_renderer");
CefSetCrashKeyValue("testkey_large1", "value1_large_renderer");
CefSetCrashKeyValue("testkey_large2", "value2_large_renderer");
```

When crashing the browser or renderer processes with cefclient you should verify that the test crash key values are included in the metadata ("<id>.json") file. Some values may be chunked as described in the Configuration section above.

Example metadata for a browser process crash:

```json
{
  "LOG_FATAL": "debug_urls.cc:208: Check failed: false. \n0   Chromium Embedded Framework         0x00000001161a1a0e _ZN4base5debug10StackTraceC2Ev + 30\n1   Chromium Embedded Framework         0x00000001161a1a75 _ZN4base5debug10StackTraceC1Ev + 21\n2   Chromium Embedded Fr",
  "guid": "6ada20da-7306-477e-a715-ada36448a23f",
  "num-switches": "3",
  "pid": "2073",
  "platform": "macos",
  "product": "cefclient",
  "ptype": "browser",
  "switch-1": "--url=chrome:\/\/inducebrowsercrashforrealz",
  "switch-2": "--lang=en-US",
  "testkey_large1": "value1_large_browser", 
  "testkey_large2": "value2_large_browser", 
  "testkey_medium1": "value1_medium_browser", 
  "testkey_medium2": "value2_medium_browser", 
  "testkey_small1": "value1_small_browser", 
  "testkey_small2": "value2_small_browser", 
  "version": "1.0.0",
}
```

Example metadata for a renderer process crash:

```json
{
  "discardable-memory-allocated": "4194304",
  "discardable-memory-free": "3198976",
  "guid": "6ada20da-7306-477e-a715-ada36448a23f",
  "num-switches": "15",
  "origin_mismatch_origin": "https:\/\/www.google.com",
  "origin_mismatch_redirects": "2",
  "origin_mismatch_same_page": "0"
  "origin_mismatch_transition": "1",
  "origin_mismatch_url-1": "https:\/\/www.google.com\/?gws_rd=ssl",
  "pid": "2706",
  "platform": "macos",
  "product": "cefclient",
  "ptype": "renderer",
  "rvinit_main_frame_id": "2",
  "rvinit_proxy_id": "-2",
  "rvinit_view_id": "1",
  "switch-1": "--disable-databases",
  "switch-2": "--lang=en-US",
  "switch-3": "--lang=en-US",
  "switch-4": "--enable-pinch",
  "switch-5": "--num-raster-threads=4",
  "switch-6": "--enable-gpu-rasterization",
  "switch-7": "--enable-zero-copy",
  "switch-8": "--enable-gpu-memory-buffer-compositor-resources",
  "switch-9": "--enable-main-frame-before-activation",
  "switch-10": "--renderer-client-id=3",
  "testkey_large1": "value1_large_renderer", 
  "testkey_large2": "value2_large_renderer", 
  "testkey_medium1": "value1_medium_renderer", 
  "testkey_medium2": "value2_medium_renderer", 
  "testkey_small1": "value1_small_renderer", 
  "testkey_small2": "value2_small_renderer", 
  "v8-ignition": "N",
  "version": "1.0.0",
}
```

Example metadata for a GPU process crash:

```json
{
  "guid": "6ada20da-7306-477e-a715-ada36448a23f",
  "num-switches": "17",
  "pid": "1664",
  "platform": "macos",
  "product": "cefclient",
  "ptype": "gpu-process",
  "switch-1": "--lang=en-US",
  "switch-2": "--supports-dual-gpus=true",
  "switch-3": "--gpu-driver-bug-workarounds=0,1,10,23,25,35,38,45,51,59,61,62,63,64,66,70,71,73,81,82,83,86,88,89",
  "switch-4": "--gpu-vendor-id=0x1002",
  "switch-5": "--gpu-device-id=0x6821",
  "switch-6": "--gpu-driver-vendor",
  "switch-7": "--gpu-driver-version",
  "switch-8": "--gpu-driver-date",
  "switch-9": "--gpu-secondary-vendor-ids=0x8086",
  "switch-10": "--gpu-secondary-device-ids=0x0d26"
  "switch-11": "--gpu-active-vendor-id=0x8086",
  "switch-12": "--gpu-active-device-id=0x0d26",
  "switch-13": "--lang=en-US",
  "version": "1.0.0",
}
```

# Decoding Minidumps

The crash report upload, if enabled, includes the minidump file with exception state, callstacks, stack memory, and loaded modules. The minidump file can be decoded using the `dump_syms` and `minidump_stackwalk` tools from Breakpad as described [here](https://www.chromium.org/developers/decoding-crash-dumps).

The Breakpad tools can be built from a local Chromium checkout on Windows, MacOS and Linux as follows:

1\. Download the Chromium code (for example, by using [CEF tooling](master_build_quick_start.md)).

2\. Write the following contents to the `out/Release/args.gn` file:

```
is_component_build=false
is_debug=false
target_cpu="x64"
is_official_build=true
```

3\. Generate the config and build:

```sh
gn gen out/Release
ninja -C out/Release dump_syms minidump_stackwalk
```

4\. Windows requires the `msdia140.dll` COM DLL. It must be placed next to `dump_syms.exe` or registered with the system:

```
regsvr32 /s msdia140.dll
```

Visual Studio 2022 includes a 64-bit version of this DLL at "C:\Program Files\Microsoft Visual Studio\2022\Professional\DIA SDK\bin\amd64". The 64-bit version 14.29.30133.0 or newer is recommended to avoid failures when loading large (> 4GB) PDB files ([details](https://developercommunity.visualstudio.com/t/pdb-limit-of-4-gib-is-likely-to-be-a-problem-in-a/904784)).