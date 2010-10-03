CEF development has faced delays in the past due to the time lapse between
submitting patches to the Chromium and WebKit projects and having those patches
accepted. A CEF developer will hopefully earn Chromium commit access at some
point which should partly mitigate this problem but will not eliminate it
completely. There may still be instances where CEF requires changes to the
Chromium/WebKit code base that are not desired by the Chromium/WebKit projects
as a whole. To address this situation in the near term and with a minimum of
maintenance headaches (i.e. without forking) this project adds a patch
capability as part of the CEF build process. The patch capability works as
follows:

1. The CEF developer creates one or more patch files containing all required
   changes to the Chromium/WebKit code base and places those patch files in the
   "patches" subdirectory.
2. The CEF developer adds an entry for each patch file in the "patch.cfg" file.
3. The CEF "patch" project, which is a build dependency for the "webkit\config"
   project, applies the patches to the Chromium/WebKit source tree using the
   patcher.py tool in the tools directory.  If necessary the patcher.py tool
   also rewrites the "patch_state.h" file which defines the CEF_PATCHES_APPLIED
   preprocessor value.

To disable automatic application of patches to the Chromium/WebKit code base
create an empty "NOPATCH" file in the "patch" directory. Sections of the CEF
code base that otherwise require patches will be disabled using the
CEF_PATCHES_APPLIED preprocessor value defined in the "patch_state.h" file. Be
warned that not applying all required patches may break important CEF
functionality.
