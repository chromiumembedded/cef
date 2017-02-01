// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/file_dialog_runner_win.h"

#include <commdlg.h>
#include <shlobj.h>

#include "libcef/browser/browser_host_impl.h"

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "cef/grit/cef_strings.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// From ui/base/dialogs/select_file_dialog_win.cc.

// Get the file type description from the registry. This will be "Text Document"
// for .txt files, "JPEG Image" for .jpg files, etc. If the registry doesn't
// have an entry for the file type, we return false, true if the description was
// found. 'file_ext' must be in form ".txt".
static bool GetRegistryDescriptionFromExtension(const std::wstring& file_ext,
                                                std::wstring* reg_description) {
  DCHECK(reg_description);
  base::win::RegKey reg_ext(HKEY_CLASSES_ROOT, file_ext.c_str(), KEY_READ);
  std::wstring reg_app;
  if (reg_ext.ReadValue(NULL, &reg_app) == ERROR_SUCCESS && !reg_app.empty()) {
    base::win::RegKey reg_link(HKEY_CLASSES_ROOT, reg_app.c_str(), KEY_READ);
    if (reg_link.ReadValue(NULL, reg_description) == ERROR_SUCCESS)
      return true;
  }
  return false;
}

// Set up a filter for a Save/Open dialog, which will consist of |file_ext| file
// extensions (internally separated by semicolons), |ext_desc| as the text
// descriptions of the |file_ext| types (optional), and (optionally) the default
// 'All Files' view. The purpose of the filter is to show only files of a
// particular type in a Windows Save/Open dialog box. The resulting filter is
// returned. The filters created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// Example:
//   file_ext: { "*.txt", "*.htm;*.html" }
//   ext_desc: { "Text Document" }
//   returned: "Text Document\0*.txt\0HTML Document\0*.htm;*.html\0"
//             "All Files\0*.*\0\0" (in one big string)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, it
// will be omitted from the filter, as it is likely a bogus extension.
std::wstring FormatFilterForExtensions(
    const std::vector<std::wstring>& file_ext,
    const std::vector<std::wstring>& ext_desc,
    bool include_all_files) {
  const std::wstring all_ext = L"*.*";
  const std::wstring all_desc =
      l10n_util::GetStringUTF16(IDS_APP_SAVEAS_ALL_FILES) +
      L" (" + all_ext + L")";

  DCHECK(file_ext.size() >= ext_desc.size());

  if (file_ext.empty())
    include_all_files = true;

  std::wstring result;

  for (size_t i = 0; i < file_ext.size(); ++i) {
    std::wstring ext = file_ext[i];
    std::wstring desc;
    if (i < ext_desc.size())
      desc = ext_desc[i];

    if (ext.empty()) {
      // Force something reasonable to appear in the dialog box if there is no
      // extension provided.
      include_all_files = true;
      continue;
    }

    if (desc.empty()) {
      DCHECK(ext.find(L'.') != std::wstring::npos);
      std::wstring first_extension = ext.substr(ext.find(L'.'));
      size_t first_separator_index = first_extension.find(L';');
      if (first_separator_index != std::wstring::npos)
        first_extension = first_extension.substr(0, first_separator_index);

      // Find the extension name without the preceeding '.' character.
      std::wstring ext_name = first_extension;
      size_t ext_index = ext_name.find_first_not_of(L'.');
      if (ext_index != std::wstring::npos)
        ext_name = ext_name.substr(ext_index);

      if (!GetRegistryDescriptionFromExtension(first_extension, &desc)) {
        // The extension doesn't exist in the registry.
        include_all_files = true;
      }
    }

    if (!desc.empty())
      desc += L" (" + ext + L")";
    else
      desc = ext;

    result.append(desc.c_str(), desc.size() + 1);  // Append NULL too.
    result.append(ext.c_str(), ext.size() + 1);
  }

  if (include_all_files) {
    result.append(all_desc.c_str(), all_desc.size() + 1);
    result.append(all_ext.c_str(), all_ext.size() + 1);
  }

  result.append(1, '\0');  // Double NULL required.
  return result;
}

std::wstring GetDescriptionFromMimeType(const std::string& mime_type) {
  // Check for wild card mime types and return an appropriate description.
  static const struct {
    const char* mime_type;
    int string_id;
  } kWildCardMimeTypes[] = {
    { "audio", IDS_AUDIO_FILES },
    { "image", IDS_IMAGE_FILES },
    { "text", IDS_TEXT_FILES },
    { "video", IDS_VIDEO_FILES },
  };

  for (size_t i = 0; i < arraysize(kWildCardMimeTypes); ++i) {
    if (mime_type == std::string(kWildCardMimeTypes[i].mime_type) + "/*")
      return l10n_util::GetStringUTF16(kWildCardMimeTypes[i].string_id);
  }

  return std::wstring();
}

std::wstring GetFilterString(
    const std::vector<base::string16>& accept_filters) {
  std::vector<std::wstring> extensions;
  std::vector<std::wstring> descriptions;

  for (size_t i = 0; i < accept_filters.size(); ++i) {
    const base::string16& filter = accept_filters[i];
    if (filter.empty())
      continue;

    size_t sep_index = filter.find('|');
    if (sep_index != base::string16::npos) {
      // Treat as a filter of the form "Filter Name|.ext1;.ext2;.ext3".
      const base::string16& desc = filter.substr(0, sep_index);
      const std::vector<base::string16>& ext =
          base::SplitString(filter.substr(sep_index + 1),
                            base::ASCIIToUTF16(";"),
                            base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      std::wstring ext_str;
      for (size_t x = 0; x < ext.size(); ++x) {
        const base::string16& file_ext = ext[x];
        if (!file_ext.empty() && file_ext[0] == '.') {
          if (!ext_str.empty())
            ext_str += L";";
          ext_str += L"*" + file_ext;
        }
      }
      if (!ext_str.empty()) {
        extensions.push_back(ext_str);
        descriptions.push_back(desc);
      }
    } else if (filter[0] == L'.') {
      // Treat as an extension beginning with the '.' character.
      extensions.push_back(L"*" + filter);
      descriptions.push_back(std::wstring());
    } else {
      // Otherwise convert mime type to one or more extensions.
      const std::string& ascii = base::UTF16ToASCII(filter);
      std::vector<base::FilePath::StringType> ext;
      std::wstring ext_str;
      net::GetExtensionsForMimeType(ascii, &ext);
      if (!ext.empty()) {
        for (size_t x = 0; x < ext.size(); ++x) {
          if (x != 0)
            ext_str += L";";
          ext_str += L"*." + ext[x];
        }
        extensions.push_back(ext_str);
        descriptions.push_back(GetDescriptionFromMimeType(ascii));
      }
    }
  }

  return FormatFilterForExtensions(extensions, descriptions, true);
}

// From chrome/browser/views/shell_dialogs_win.cc

bool RunOpenFileDialog(
    const CefFileDialogRunner::FileChooserParams& params,
    HWND owner,
    int* filter_index,
    base::FilePath* path) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH] = {0};

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  std::wstring directory;
  if (!params.default_file_name.empty()) {
    if (params.default_file_name.EndsWithSeparator()) {
      // The value is only a directory.
      directory = params.default_file_name.value();
    } else {
      // The value is a file name and possibly a directory.
      base::wcslcpy(filename, params.default_file_name.value().c_str(),
          arraysize(filename));
      directory = params.default_file_name.DirName().value();
    }
  }
  if (!directory.empty())
    ofn.lpstrInitialDir = directory.c_str();

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_OPEN_FILE_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER |
              OFN_ENABLESIZING;
  if (params.hidereadonly)
    ofn.Flags |= OFN_HIDEREADONLY;

  const std::wstring& filter = GetFilterString(params.accept_types);
  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
    // Indices into |lpstrFilter| start at 1.
    ofn.nFilterIndex = *filter_index + 1;
  }

  bool success = !!GetOpenFileName(&ofn);
  if (success) {
    *filter_index = ofn.nFilterIndex == 0 ? 0 : ofn.nFilterIndex - 1;
    *path = base::FilePath(filename);
  }
  return success;
}

bool RunOpenMultiFileDialog(
    const CefFileDialogRunner::FileChooserParams& params,
    HWND owner,
    int* filter_index,
    std::vector<base::FilePath>* paths) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  std::unique_ptr<wchar_t[]> filename(new wchar_t[UNICODE_STRING_MAX_CHARS]);
  filename[0] = 0;

  ofn.lpstrFile = filename.get();
  ofn.nMaxFile = UNICODE_STRING_MAX_CHARS;

  std::wstring directory;
  if (!params.default_file_name.empty()) {
    if (params.default_file_name.EndsWithSeparator()) {
      // The value is only a directory.
      directory = params.default_file_name.value();
    } else {
      // The value is a file name and possibly a directory.
      directory = params.default_file_name.DirName().value();
    }
  }
  if (!directory.empty())
    ofn.lpstrInitialDir = directory.c_str();

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_OPEN_FILES_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER |
              OFN_ALLOWMULTISELECT | OFN_ENABLESIZING;
  if (params.hidereadonly)
    ofn.Flags |= OFN_HIDEREADONLY;

  const std::wstring& filter = GetFilterString(params.accept_types);
  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
    // Indices into |lpstrFilter| start at 1.
    ofn.nFilterIndex = *filter_index + 1;
  }

  bool success = !!GetOpenFileName(&ofn);

  if (success) {
    std::vector<base::FilePath> files;
    const wchar_t* selection = ofn.lpstrFile;
    while (*selection) {  // Empty string indicates end of list.
      files.push_back(base::FilePath(selection));
      // Skip over filename and null-terminator.
      selection += files.back().value().length() + 1;
    }
    if (files.empty()) {
      success = false;
    } else if (files.size() == 1) {
      // When there is one file, it contains the path and filename.
      paths->swap(files);
    } else {
      // Otherwise, the first string is the path, and the remainder are
      // filenames.
      std::vector<base::FilePath>::iterator path = files.begin();
      for (std::vector<base::FilePath>::iterator file = path + 1;
           file != files.end(); ++file) {
        paths->push_back(path->Append(*file));
      }
    }
  }

  if (success)
    *filter_index = ofn.nFilterIndex == 0 ? 0 : ofn.nFilterIndex - 1;

  return success;
}

// The callback function for when the select folder dialog is opened.
int CALLBACK BrowseCallbackProc(HWND window,
                                UINT message,
                                LPARAM parameter,
                                LPARAM data)
{
  if (message == BFFM_INITIALIZED) {
    // WParam is TRUE since passing a path.
    // data lParam member of the BROWSEINFO structure.
    SendMessage(window, BFFM_SETSELECTION, TRUE, (LPARAM)data);
  }
  return 0;
}

bool RunOpenFolderDialog(
    const CefFileDialogRunner::FileChooserParams& params,
    HWND owner,
    base::FilePath* path) {
  wchar_t dir_buffer[MAX_PATH + 1] = {0};

  bool result = false;
  BROWSEINFO browse_info = {0};
  browse_info.hwndOwner = owner;
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_SELECT_FOLDER_DIALOG_TITLE);
  if (!title.empty())
    browse_info.lpszTitle = title.c_str();

  const std::wstring& file_path = params.default_file_name.value();
  if (!file_path.empty()) {
    // Highlight the current value.
    browse_info.lParam = (LPARAM)file_path.c_str();
    browse_info.lpfn = &BrowseCallbackProc;
  }

  LPITEMIDLIST list = SHBrowseForFolder(&browse_info);
  if (list) {
    STRRET out_dir_buffer;
    ZeroMemory(&out_dir_buffer, sizeof(out_dir_buffer));
    out_dir_buffer.uType = STRRET_WSTR;
    base::win::ScopedComPtr<IShellFolder> shell_folder;
    if (SHGetDesktopFolder(shell_folder.Receive()) == NOERROR) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                  &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        *path = base::FilePath(out_dir_buffer.pOleStr);
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      } else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          *path = base::FilePath(old_out_dir_buffer);
          result = true;
        }
      }
    }
    CoTaskMemFree(list);
  }

  return result;
}

bool RunSaveFileDialog(
    const CefFileDialogRunner::FileChooserParams& params,
    HWND owner,
    int* filter_index,
    base::FilePath* path) {
  OPENFILENAME ofn;

  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH] = {0};

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  std::wstring directory;
  if (!params.default_file_name.empty()) {
    if (params.default_file_name.EndsWithSeparator()) {
      // The value is only a directory.
      directory = params.default_file_name.value();
    } else {
      // The value is a file name and possibly a directory.
      base::wcslcpy(filename, params.default_file_name.value().c_str(),
          arraysize(filename));
      directory = params.default_file_name.DirName().value();
    }
  }
  if (!directory.empty())
    ofn.lpstrInitialDir = directory.c_str();

  std::wstring title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_SAVE_AS_DIALOG_TITLE);
  if (!title.empty())
    ofn.lpstrTitle = title.c_str();

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_EXPLORER | OFN_ENABLESIZING | OFN_NOCHANGEDIR |
              OFN_PATHMUSTEXIST;
  if (params.hidereadonly)
    ofn.Flags |= OFN_HIDEREADONLY;
  if (params.overwriteprompt)
    ofn.Flags |= OFN_OVERWRITEPROMPT;

  const std::wstring& filter = GetFilterString(params.accept_types);
  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
    // Indices into |lpstrFilter| start at 1.
    ofn.nFilterIndex = *filter_index + 1;
    // If a filter is specified and the default file name is changed then append
    // a file extension to the new name.
    ofn.lpstrDefExt = L"";
  }

  bool success = !!GetSaveFileName(&ofn);
  if (success) {
    *filter_index = ofn.nFilterIndex == 0 ? 0 : ofn.nFilterIndex - 1;
    *path = base::FilePath(filename);
  }
  return success;
}

}  // namespace

CefFileDialogRunnerWin::CefFileDialogRunnerWin() {
}

void CefFileDialogRunnerWin::Run(CefBrowserHostImpl* browser,
                                 const FileChooserParams& params,
                                 RunFileChooserCallback callback) {
  int filter_index = params.selected_accept_filter;
  std::vector<base::FilePath> files;

  HWND owner = browser->GetWindowHandle();

  if (params.mode == content::FileChooserParams::Open) {
    base::FilePath file;
    if (RunOpenFileDialog(params, owner, &filter_index, &file))
      files.push_back(file);
  } else if (params.mode == content::FileChooserParams::OpenMultiple) {
    RunOpenMultiFileDialog(params, owner, &filter_index, &files);
  } else if (params.mode == content::FileChooserParams::UploadFolder) {
    base::FilePath file;
    if (RunOpenFolderDialog(params, owner, &file))
      files.push_back(file);
  } else if (params.mode == content::FileChooserParams::Save) {
    base::FilePath file;
    if (RunSaveFileDialog(params, owner, &filter_index, &file))
      files.push_back(file);
  } else {
    NOTIMPLEMENTED();
  }

  callback.Run(filter_index, files);
}
