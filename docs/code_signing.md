This page explains how to sign CEF-based applications for distribution on Windows and macOS, including macOS notarization.

**Contents**

- [Overview](#overview)
- [Windows](#windows)
  - [Prerequisites](#windows-prerequisites)
  - [Sign application binaries and installers](#sign-application-binaries-and-installers)
  - [Test with a self-signed certificate](#test-with-a-self-signed-certificate)
  - [Create a signed catalog](#create-a-signed-catalog)
- [macOS](#macos)
  - [Prerequisites](#macos-prerequisites)
  - [Choose entitlements](#choose-entitlements)
  - [Sign the application bundle](#sign-the-application-bundle)
  - [Notarize and staple](#notarize-and-staple)
  - [Disk images and installer packages](#disk-images-and-installer-packages)
- [Release validation and troubleshooting](#release-validation-and-troubleshooting)

---

# Overview

Code signing identifies the publisher and allows signature verification to detect changes to signed content. Signing is a release packaging step: finish compiling, stripping, combining architectures, editing resources and constructing the final layout before signing. Generate archive hashes and update metadata from the final signed artifacts.

Use your own signing identity and protect its private key. A signing service or hardware-backed key can perform signing without exposing the key to the build machine. Keep credentials outside the source tree and build logs, and verify the returned files before packaging them.

On Windows, executable files use Authenticode signatures; a signed catalog can also authenticate resource files. On macOS, nested code and the enclosing bundles are signed in order, with entitlements assigned to the appropriate executable processes. Distribution outside the Mac App Store additionally involves Apple's notarization service.

File lists and entitlements can change between Chromium versions. Check the sources corresponding to the Chromium revision used by your CEF build. The guidance below reflects the sources inspected as of October 2026; the Chromium links point to the main branch and may change over time.

# Windows

## Windows prerequisites

Install the Windows SDK and make `signtool.exe` available in your command prompt. Catalog creation also uses `makecat.exe`. [Obtain](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options) an Authenticode code-signing certificate and configure access to its private key through the certificate store, a hardware token, or a signing service.

The commands below run in **Command Prompt**, with an identity in the current user's Personal (`My`) certificate store. Replace `CERTIFICATE_THUMBPRINT` and `TIMESTAMP_URL` with your certificate's thumbprint and an RFC 3161 timestamp endpoint supported by your certificate provider. A machine-store identity additionally requires `/sm`; hardware or remote signing may require provider-specific configuration.

See Microsoft's [SignTool reference](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool) for certificate selection and timestamp options. Chromium's [Windows updater signing script](https://chromium.googlesource.com/chromium/src/+/main/chrome/updater/win/signing/sign.py) demonstrates signing embedded binaries before the enclosing installer. That script handles Chromium updater packaging and is a reference, rather than a general CEF application signing tool.

## Sign application binaries and installers

Inventory the executable binaries shipped with your application: the main executable, subprocess executables, application DLLs, CEF DLLs and any native dependencies. Sign unsigned binaries that you distribute under your identity. Preserve valid third-party signatures when your packaging and loading policy permit them.

For the CEF bootstrap configuration described in [Sandbox Setup](sandbox_setup.md#windows) and [Installer](installer.md), the bootstrap executable and its adjacent `chrome_elf.dll` must use the same signing certificate. Sign these with your application identity, along with the client DLL. A separately downloaded CEF shared installation retains its distribution certificate and catalog.

After all resource modifications, sign each binary:

```bat
signtool sign /s My /sha1 CERTIFICATE_THUMBPRINT /fd SHA256 /tr TIMESTAMP_URL /td SHA256 /v "MyApp.exe"
signtool sign /s My /sha1 CERTIFICATE_THUMBPRINT /fd SHA256 /tr TIMESTAMP_URL /td SHA256 /v "MyApp.dll"
signtool sign /s My /sha1 CERTIFICATE_THUMBPRINT /fd SHA256 /tr TIMESTAMP_URL /td SHA256 /v "chrome_elf.dll"
```

Here `/sha1` selects a certificate by thumbprint; `/fd SHA256` selects the signature digest. Verify each resulting file under the Authenticode policy:

```bat
signtool verify /pa /all /tw /v "MyApp.exe"
signtool verify /pa /all /tw /v "MyApp.dll"
signtool verify /pa /all /tw /v "chrome_elf.dll"
```

Treat failures and warnings as release failures, including a missing timestamp. Repeat for every shipped binary. Then build your installer from the signed payload, sign the resulting installer `.exe` or `.msi`, and verify it with the same commands. Signing the installer does not sign its contained binaries. ZIP and tar archives carry the already signed files.

## Test with a self-signed certificate

For local development, you can test bootstrap signing using a self-signed certificate. This certificate is trusted only on machines where you explicitly install it; use a publicly trusted code-signing certificate for release distribution.

Run the following in **PowerShell** under the account that will launch the application. It creates a test certificate and trusts it in that user's certificate stores:

```powershell
$certificate = New-SelfSignedCertificate -DnsName "cef-signing-test.invalid" `
  -Type CodeSigning -CertStoreLocation Cert:\CurrentUser\My
$certificatePath = Join-Path $env:TEMP "cef-signing-test.cer"
Export-Certificate -Cert $certificate -FilePath $certificatePath
Import-Certificate -FilePath $certificatePath -CertStoreLocation Cert:\CurrentUser\TrustedPublisher
Import-Certificate -FilePath $certificatePath -CertStoreLocation Cert:\CurrentUser\Root
```

Replace the application directory and filenames below with your staged bootstrap executable, client DLL and adjacent `chrome_elf.dll`. Sign each with the newly created certificate:

```powershell
$appDir = "C:\path\to\app"
foreach ($name in "MyApp.exe", "MyApp.dll", "chrome_elf.dll") {
  $path = Join-Path $appDir $name
  Set-AuthenticodeSignature -FilePath $path -Certificate $certificate -HashAlgorithm SHA256
  signtool verify /pa /all /v $path
  if ($LASTEXITCODE -ne 0) { throw "Signature verification failed: $path" }
}
```

This local test omits timestamping. The release signing commands above include a timestamp and check for its presence. Keeping the returned certificate object also avoids accidentally selecting another certificate from the Personal store.

After testing, open `certmgr.msc` and remove this test certificate from **Personal**, **Trusted Root Certification Authorities** and **Trusted Publishers**, identifying it by `$certificate.Thumbprint`. Remove the exported `.cer` file as well. Removing the trust entries makes these test signatures untrusted on the machine; rebuild or re-sign the test binaries before subsequent use.

## Create a signed catalog

A catalog contains hashes for an explicit set of files, including data files such as `.pak`, `.dat` and `.json` that do not have embedded Authenticode signatures. For CEF shared installations, read the [Windows Installer guide](installer.md), particularly [Sign your binaries](installer.md#3-sign-your-binaries), and the [bootstrap installer implementation and configuration](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md). Its download-time distribution verification checks the catalog and the expected certificate; creating a catalog alone does not configure an application to trust a new publisher. The implementation guide's [Extended Configuration](https://github.com/chromiumembedded/cef/blob/master/libcef_dll/bootstrap/installer/README.md#extended-configuration) describes `certificate_thumbprint` for distributions signed with another identity, including the requirement for a distinct installation path and matching client-side load policy.

For a distribution you publish yourself, first stage the complete payload and sign its executable files. Then create a Catalog Definition File (`catalog.cdf`). This small example assumes `C:\staging` is the distribution root:

```ini
[CatalogHeader]
Name=catalog.cat
ResultDir=C:\staging
PublicVersion=0x00000200
EncodingType=0x00010001
CatalogVersion=2
HashAlgorithms=SHA256

[CatalogFiles]
<HASH>libcef=C:\staging\Release\libcef.dll
<HASH>resources=C:\staging\Resources\resources.pak
<HASH>icudtl=C:\staging\Resources\icudtl.dat
```

Generate one uniquely named entry for **every payload file**, using its actual staged path. The paths above are illustrative. Exclude the catalog itself and the temporary CDF. End the CDF with a newline. [MakeCat](https://learn.microsoft.com/en-us/windows/win32/seccrypto/using-makecat) computes catalog hashes from these files; do not substitute ordinary SHA-256 checksums for its file-hashing procedure.

```bat
makecat -v "C:\staging\catalog.cdf"
signtool sign /s My /sha1 CERTIFICATE_THUMBPRINT /fd SHA256 /tr TIMESTAMP_URL /td SHA256 /v "C:\staging\catalog.cat"
signtool verify /pa /all /tw /v "C:\staging\catalog.cat"
signtool verify /pa /v /c "C:\staging\catalog.cat" "C:\staging\Release\libcef.dll"
signtool verify /pa /v /c "C:\staging\catalog.cat" "C:\staging\Resources\resources.pak"
```

Verify every member against the signed catalog. Package the signed payload and `catalog.cat` together after removing the temporary CDF. If payload content changes, regenerate and re-sign the catalog. For an existing CEF shared distribution, preserve its files and catalog and follow the installer's configured certificate verification policy.

# macOS

## macOS prerequisites

Use a macOS signing machine with Xcode installed and selected through `xcode-select`. [Obtain](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/) a **Developer ID Application** certificate through the Apple Developer Program for distribution outside the Mac App Store. Import it and its private key into a keychain accessible to the signing user, and confirm the identity:

```sh
security find-identity -v -p codesigning
```

For an unattended build, [configure](https://developer.apple.com/forums/thread/712005) the signing keychain's unlock and private-key access before invoking `codesign`. If you change the user's keychain search list, save and restore it after signing. Lock a dedicated signing keychain when finished. Keep keychain passwords in your build system's secret store.

Use a Release, non-component CEF distribution. Assemble the final `.app` with its framework, resources and helper apps before signing. Preserve framework symlinks when copying it. The supplied CMake configuration constructs a versioned framework for newer Xcode versions; see the binary distribution's `README.txt` and [CEF framework copy macro](https://github.com/chromiumembedded/cef/blob/master/cmake/cef_macros.cmake.in).

Chromium's [macOS signing documentation](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/README.md), [component definitions](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/parts.py) and [signing implementation](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/signing.py) provide reference behavior. [sign_chrome.py](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/sign_chrome.py) expects Chrome's bundle layout and configuration, so a CEF application's packaging script must supply its own component paths.

## Choose entitlements

Enable the Hardened Runtime when signing executable processes. Start with the entitlement files from the matching Chromium revision and adapt them to your application's features:

| Component | Chromium reference | Application guidance |
| --- | --- | --- |
| Main application | [app-entitlements.plist](https://chromium.googlesource.com/chromium/src/+/main/chrome/app/app-entitlements.plist) | Start with this file from the matching Chromium revision. Review capability changes against the features your application supports. |
| Renderer helper | [helper-renderer-entitlements.plist](https://chromium.googlesource.com/chromium/src/+/main/chrome/app/helper-renderer-entitlements.plist) | As of October 2026, Chromium enables `com.apple.security.cs.allow-jit` for V8. |
| GPU helper | [helper-gpu-entitlements.plist](https://chromium.googlesource.com/chromium/src/+/main/chrome/app/helper-gpu-entitlements.plist) | As of October 2026, Chromium also enables `com.apple.security.cs.allow-jit` here. Check the matching revision. |
| Generic, Alerts and Plugin helpers | [parts.py](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/parts.py) | Review each role. The example below assigns an empty entitlement dictionary to these helpers; older releases or custom helper roles may need different settings. |
| Framework and dylibs | [parts.py](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/parts.py) | Process entitlements belong on executables, rather than on their loaded libraries. |

Use the unbranded `app-entitlements.plist` listed above. Chromium also has an `app-entitlements-chrome.plist` for Google Chrome, but its identity-specific entitlements are tied to Google's signing identity and should not be copied into a CEF application. Chromium's development signing mode adds `com.apple.security.get-task-allow` to permit debugging; omit that entitlement from release signatures. See the [Chromium signing documentation](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/README.md) for these distinctions.

For the example below, prepare `app-entitlements.plist`, `renderer-entitlements.plist`, `gpu-entitlements.plist` and `helper-entitlements.plist` beside your signing script. Copy Chromium's `app-entitlements.plist` from the matching revision for the main application. As of October 2026, it enables audio input, Bluetooth, camera, printing, USB, location and photo-library access. Review these capabilities against your application's supported features before changing them. Capability entitlements do not replace required `Info.plist` usage descriptions or user consent.

The renderer and GPU files can start with this dictionary:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>com.apple.security.cs.allow-jit</key>
  <true/>
</dict>
</plist>
```

For the generic helper's empty entitlement file, replace the dictionary with `<dict/>`. Keep the main application's separate `app-entitlements.plist` prepared above. If a custom subprocess executable serves multiple roles, its entitlements must cover the roles it actually executes. Entitlements on the outer application do not grant those capabilities to separate helper processes.

Chromium's renderer and GPU entitlement files provide a starting point, rather than a promise of compatibility with every CEF version. Avoid adding `allow-unsigned-executable-memory`, `disable-executable-page-protection` or `disable-library-validation` indiscriminately. Investigate a demonstrated requirement before granting an exception.

## Sign the application bundle

Sign from the inside out: nested libraries and executables, their containing bundles, then the outer application. Explicitly sign Mach-O libraries in the CEF framework's `Libraries` directory. Avoid `--deep` for signing: it does not reliably discover code in nonstandard locations and cannot assign the correct entitlements to each process. See Apple's [code signing guidance](https://developer.apple.com/library/archive/technotes/tn2206/_index.html).

Signing an `.app` bundle also signs its main executable, identified by `CFBundleExecutable` in `Contents/Info.plist` and located in `Contents/MacOS`. You do not need a separate signing command for that binary. This applies to both the main application and each helper app. Additional executables inside a bundle must be signed explicitly before signing the enclosing bundle.

This **Bash** example handles the usual CEF sample layout. Replace `MyApp` and the signing identity; run it from the directory containing your entitlement files. The helper names must match the names and process roles produced by your build.

```bash
#!/bin/bash
set -euo pipefail

APP="/path/to/MyApp.app"
IDENTITY="Developer ID Application: Example Company (TEAMID)"
FRAMEWORK="$APP/Contents/Frameworks/Chromium Embedded Framework.framework"
PAYLOAD="$FRAMEWORK"
if [[ -d "$FRAMEWORK/Versions/Current" ]]; then
  PAYLOAD="$FRAMEWORK/Versions/Current"
fi

# Sign actual Mach-O files, leaving JSON and other resources to the bundle seal.
if [[ -d "$PAYLOAD/Libraries" ]]; then
  while IFS= read -r -d '' library; do
    if /usr/bin/file -b "$library" | /usr/bin/grep -q 'Mach-O'; then
      codesign --force --sign "$IDENTITY" --timestamp "$library"
      codesign --verify --strict --verbose=2 "$library"
    fi
  done < <(find "$PAYLOAD/Libraries" -type f -print0)
fi

# Sign the framework bundle, not just its main library file.
codesign --force --sign "$IDENTITY" --timestamp "$FRAMEWORK"

# Sign CEF helper app bundles.
for role in "" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)"; do
  helper="$APP/Contents/Frameworks/MyApp Helper${role}.app"
  [[ -d "$helper" ]] || continue
  case "$role" in
    " (Renderer)") entitlements="renderer-entitlements.plist" ;;
    " (GPU)") entitlements="gpu-entitlements.plist" ;;
    *) entitlements="helper-entitlements.plist" ;;
  esac
  codesign --force --sign "$IDENTITY" --options runtime --timestamp \
    --entitlements "$entitlements" "$helper"
done

# Sign the final outer app bundle after all nested components are signed.
codesign --force --sign "$IDENTITY" --options runtime --timestamp \
  --entitlements "app-entitlements.plist" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
```

The loop skips helpers absent from the selected CEF release. Confirm that every helper your build requires is present; the loop is not a bundle-completeness check. Extend the signing inventory for additional frameworks, crash handlers, launchers, native modules or other embedded code. Sign these before their containing bundle. For a standalone executable, use `--options runtime` to enable the Hardened Runtime and pass `--entitlements` if the process needs additional capabilities. For example, an executable that uses V8's JIT needs the `com.apple.security.cs.allow-jit` entitlement; a launcher that does not need additional capabilities can omit the entitlement file. Dylibs and native library modules do not need these process-specific options or entitlements. Traverse actual files once rather than signing both a framework symlink and its target. A framework with multiple versions needs each version signed separately.

Use `--keychain /path/to/signing.keychain-db` on signing commands if the identity resides in a dedicated keychain. Inspect the main application and each helper to confirm the identity, runtime flags and entitlements:

```sh
codesign --display --verbose=4 "/path/to/MyApp.app"
codesign --display --entitlements :- "/path/to/MyApp.app/Contents/Frameworks/MyApp Helper (Renderer).app"
```

## Notarize and staple

Use `notarytool` for notarization. Apple's [notarization overview](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution) and [custom workflow guide](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow) describe authentication, submission and ticket attachment.

Store credentials in a keychain profile. This command prompts for your Apple Account, team and app-specific password; an App Store Connect API key is another supported authentication method:

```sh
xcrun notarytool store-credentials "myapp-notary"
```

Submit an archive of the signed app, and record the returned submission ID:

```sh
ditto -c -k --sequesterRsrc --keepParent "/path/to/MyApp.app" "/path/to/MyApp-notarization.zip"
xcrun notarytool submit "/path/to/MyApp-notarization.zip" \
  --keychain-profile "myapp-notary" --wait --output-format json
```

Require status **Accepted** before proceeding. Retrieve the log if rejected or if you need to investigate warnings:

```sh
xcrun notarytool log SUBMISSION_ID --keychain-profile "myapp-notary" notarization-log.json
```

Staple the ticket to the submitted app, validate it, and assess Gatekeeper acceptance:

```sh
xcrun stapler staple "/path/to/MyApp.app"
xcrun stapler validate "/path/to/MyApp.app"
codesign --verify --deep --strict --verbose=2 "/path/to/MyApp.app"
spctl --assess --type execute --verbose=2 "/path/to/MyApp.app"
```

ZIP files cannot be stapled. For ZIP distribution, create a **new** archive from the stapled app and distribute that archive. Preserve symlinks and bundle metadata. A timeout while waiting does not necessarily mean rejection; query the existing submission with `notarytool info SUBMISSION_ID` before resubmitting.

## Disk images and installer packages

For a DMG, put the signed and stapled app into the final image, then sign the image with your Developer ID Application identity:

```sh
codesign --force --sign "Developer ID Application: Example Company (TEAMID)" \
  --timestamp "/path/to/MyApp.dmg"
```

Submit the DMG with `notarytool submit`, require acceptance, then run `stapler staple` and `stapler validate` on the DMG. Creating or signing a DMG does not replace signing the enclosed application.

A flat `.pkg` uses a separate **Developer ID Installer** identity. Build it from the signed app, then sign it with `productsign`:

```sh
productsign --sign "Developer ID Installer: Example Company (TEAMID)" \
  "/path/to/MyApp-unsigned.pkg" "/path/to/MyApp.pkg"
pkgutil --check-signature "/path/to/MyApp.pkg"
```

Submit and staple the signed package using the same notarization workflow, then check it with `spctl --assess --type install --verbose=2`. Chromium's [signing documentation](https://chromium.googlesource.com/chromium/src/+/main/chrome/installer/mac/signing/README.md#the-installer-identity) explains the separate installer identity, and Apple's [code signing guide](https://developer.apple.com/library/archive/technotes/tn2206/_index.html) covers framework and container signing.

# Release validation and troubleshooting

Test the final downloaded artifact on a clean system, including installation, first launch, renderer startup, GPU rendering, enabled media features, normal exit and relaunch. Exercise every architecture you distribute. A valid signature does not by itself prove the application works.

On Windows, verify each embedded signature, the installer and any catalog members. Check that the bootstrap and adjacent `chrome_elf.dll` use the same certificate, and that shared-install catalog verification uses the expected distribution certificate. A successful signature check does not guarantee the absence of SmartScreen reputation warnings.

On macOS, verify the extracted or installed app as well as its shipping container. Test launch through Finder after a real browser download so quarantine and Gatekeeper behavior are exercised. Check a stapled app on a machine without network access when offline launch is required.

If macOS reports unsigned code or an invalid sealed resource, inspect the specific path in the verification output or notarization log. Check framework libraries and executable code in resource directories, finish all bundle changes before signing, and repeat signing from the affected inner component outward. For helper crashes, inspect that helper's own entitlements and the system crash report. See Apple's [common notarization issues](https://developer.apple.com/documentation/security/resolving-common-notarization-issues).

The Hardened Runtime's library validation can reject libraries signed by another team. Prefer signing your application and its bundled libraries with the same Team ID. For library-validation failures, check the loaded libraries' signatures and Team IDs before adding a `disable-library-validation` exception.

Automated signing should stop on verification failure. Retry temporary timestamp-service errors with a bounded delay, while preserving certificate, signature and notarization errors for investigation. Publish only after signature, catalog or notarization checks and application smoke tests succeed.
