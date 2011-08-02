// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "download_util.h"

#include "base/file_path.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"

namespace {

#if defined(OS_WIN)
// Returns whether the specified extension is automatically integrated into the
// windows shell.
// From chrome/browser/download/download_util.cc
bool IsShellIntegratedExtension(const string16& extension) {
  string16 extension_lower = StringToLowerASCII(extension);
  
  static const wchar_t* const integrated_extensions[] = {
    // See <http://msdn.microsoft.com/en-us/library/ms811694.aspx>.
    L"local",
    // Right-clicking on shortcuts can be magical.
    L"lnk",
  };
  
  for (int i = 0; i < arraysize(integrated_extensions); ++i) {
    if (extension_lower == integrated_extensions[i])
      return true;
  }
  
  // See <http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html>.
  // That vulnerability report is not exactly on point, but files become magical
  // if their end in a CLSID.  Here we block extensions that look like CLSIDs.
  if (extension_lower.size() > 0 && extension_lower.at(0) == L'{' &&
      extension_lower.at(extension_lower.length() - 1) == L'}')
    return true;
  
  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
// From chrome/browser/download/download_util.cc
bool IsReservedName(const string16& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  string16 filename_lower = StringToLowerASCII(filename);
  
  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(string16(known_devices[i]) + L".") == 0)
      return true;
  }
  
  static const wchar_t* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    L"desktop.ini",
    L"thumbs.db",
  };
  
  for (int i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }
  
  return false;
}
#endif  // OS_WIN

// Create an extension based on the file name and mime type.
// From chrome/browser/download/download_util.cc
void GenerateExtension(const FilePath& file_name,
                       const std::string& mime_type,
                       FilePath::StringType* generated_extension) {
  // We're worried about two things here:
  //
  // 1) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 2) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.
  
  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.
  
#if defined(OS_WIN)
  static const FilePath::CharType default_extension[] =
  FILE_PATH_LITERAL("download");
  
  // Rename shell-integrated extensions.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif
  
  if (extension.empty()) {
    // The GetPreferredExtensionForMimeType call will end up going to disk.  Do
    // this on another thread to avoid slowing the IO thread.
    // http://crbug.com/61827
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    net::GetPreferredExtensionForMimeType(mime_type, &extension);
  }
  
  generated_extension->swap(extension);
}

// Used to make sure we have a safe file extension and filename for a
// download.  |file_name| can either be just the file name or it can be a
// full path to a file.
// From chrome/browser/download/download_util.cc
void GenerateSafeFileName(const std::string& mime_type, FilePath* file_name) {
  // Make sure we get the right file extension
  FilePath::StringType extension;
  GenerateExtension(*file_name, mime_type, &extension);
  *file_name = file_name->ReplaceExtension(extension);
  
#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_name->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_name = file_name->DirName();
    if (file_name->value() == FilePath::kCurrentDirectory) {
      *file_name = FilePath(leaf_name);
    } else {
      *file_name = file_name->Append(leaf_name);
    }
  }
#endif
}
  
} // namespace

namespace download_util {

// Create a file name based on the response from the server.
// From chrome/browser/download/download_util.cc
void GenerateFileName(const GURL& url,
                      const std::string& content_disposition,
                      const std::string& referrer_charset,
                      const std::string& mime_type,
                      const std::string& suggested_name,
                      FilePath* generated_name) {
  string16 new_name = net::GetSuggestedFilename(GURL(url),
                                                content_disposition,
                                                referrer_charset,
                                                suggested_name,
                                                ASCIIToUTF16("download"));
  
  // TODO(evan): this code is totally wrong -- we should just generate
  // Unicode filenames and do all this encoding switching at the end.
  // However, I'm just shuffling wrong code around, at least not adding
  // to it.
#if defined(OS_WIN)
  *generated_name = FilePath(new_name);
#else
  *generated_name = FilePath(base::SysWideToNativeMB(UTF16ToWide(new_name)));
#endif
  
  DCHECK(!generated_name->empty());
  
  GenerateSafeFileName(mime_type, generated_name);
}

} // namespace download_util
