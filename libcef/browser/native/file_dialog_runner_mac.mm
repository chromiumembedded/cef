// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/file_dialog_runner_mac.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#include "base/mac/mac_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "cef/grit/cef_strings.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

std::u16string GetDescriptionFromMimeType(const std::string& mime_type) {
  // Check for wild card mime types and return an appropriate description.
  static const struct {
    const char* mime_type;
    int string_id;
  } kWildCardMimeTypes[] = {
      {"audio", IDS_AUDIO_FILES},
      {"image", IDS_IMAGE_FILES},
      {"text", IDS_TEXT_FILES},
      {"video", IDS_VIDEO_FILES},
  };

  for (size_t i = 0; i < base::size(kWildCardMimeTypes); ++i) {
    if (mime_type == std::string(kWildCardMimeTypes[i].mime_type) + "/*")
      return l10n_util::GetStringUTF16(kWildCardMimeTypes[i].string_id);
  }

  return std::u16string();
}

void AddFilters(NSPopUpButton* button,
                const std::vector<std::u16string>& accept_filters,
                bool include_all_files,
                std::vector<std::vector<std::u16string>>* all_extensions) {
  for (size_t i = 0; i < accept_filters.size(); ++i) {
    const std::u16string& filter = accept_filters[i];
    if (filter.empty())
      continue;

    std::vector<std::u16string> extensions;
    std::u16string description;

    size_t sep_index = filter.find('|');
    if (sep_index != std::string::npos) {
      // Treat as a filter of the form "Filter Name|.ext1;.ext2;.ext3".
      description = filter.substr(0, sep_index);

      const std::vector<std::u16string>& ext =
          base::SplitString(filter.substr(sep_index + 1), u";",
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (size_t x = 0; x < ext.size(); ++x) {
        const std::u16string& file_ext = ext[x];
        if (!file_ext.empty() && file_ext[0] == '.')
          extensions.push_back(file_ext);
      }
    } else if (filter[0] == '.') {
      // Treat as an extension beginning with the '.' character.
      extensions.push_back(filter);
    } else {
      // Otherwise convert mime type to one or more extensions.
      const std::string& ascii = base::UTF16ToASCII(filter);
      std::vector<base::FilePath::StringType> ext;
      net::GetExtensionsForMimeType(ascii, &ext);
      if (!ext.empty()) {
        for (size_t x = 0; x < ext.size(); ++x)
          extensions.push_back(u"." + base::ASCIIToUTF16(ext[x]));
        description = GetDescriptionFromMimeType(ascii);
      }
    }

    if (extensions.empty())
      continue;

    // Don't display a crazy number of extensions since the NSPopUpButton width
    // will keep growing.
    const size_t kMaxExtensions = 10;

    std::u16string ext_str;
    for (size_t x = 0; x < std::min(kMaxExtensions, extensions.size()); ++x) {
      const std::u16string& pattern = u"*" + extensions[x];
      if (x != 0)
        ext_str += u";";
      ext_str += pattern;
    }

    if (extensions.size() > kMaxExtensions)
      ext_str += u";...";

    if (description.empty()) {
      description = ext_str;
    } else {
      description += u" (" + ext_str + u")";
    }

    [button addItemWithTitle:base::SysUTF16ToNSString(description)];

    all_extensions->push_back(extensions);
  }

  // Add the *.* filter, but only if we have added other filters (otherwise it
  // is implied).
  if (include_all_files && !all_extensions->empty()) {
    [button addItemWithTitle:base::SysUTF8ToNSString("All Files (*)")];
    all_extensions->push_back(std::vector<std::u16string>());
  }
}

}  // namespace

// Used to manage the file type filter in the NSSavePanel/NSOpenPanel.
@interface CefFilterDelegate : NSObject {
 @private
  NSSavePanel* panel_;
  std::vector<std::vector<std::u16string>> extensions_;
  int selected_index_;
}
- (id)initWithPanel:(NSSavePanel*)panel
    andAcceptFilters:(const std::vector<std::u16string>&)accept_filters
      andFilterIndex:(int)index;
- (void)setFilter:(int)index;
- (int)filter;
- (void)filterSelectionChanged:(id)sender;
- (void)setFileExtension;
@end

@implementation CefFilterDelegate

- (id)initWithPanel:(NSSavePanel*)panel
    andAcceptFilters:(const std::vector<std::u16string>&)accept_filters
      andFilterIndex:(int)index {
  if (self = [super init]) {
    DCHECK(panel);
    panel_ = panel;
    selected_index_ = 0;

    NSPopUpButton* button = [[NSPopUpButton alloc] init];
    AddFilters(button, accept_filters, true, &extensions_);
    [button sizeToFit];
    [button setTarget:self];
    [button setAction:@selector(filterSelectionChanged:)];

    if (index < static_cast<int>(extensions_.size())) {
      [button selectItemAtIndex:index];
      [self setFilter:index];
    }

    [panel_ setAccessoryView:button];
  }
  return self;
}

// Set the current filter index.
- (void)setFilter:(int)index {
  DCHECK(index >= 0 && index < static_cast<int>(extensions_.size()));
  selected_index_ = index;

  // Set the selectable file types. For open panels this limits the files that
  // can be selected. For save panels this applies a default file extenion when
  // the dialog is dismissed if none is already provided.
  NSMutableArray* acceptArray = nil;
  if (!extensions_[index].empty()) {
    acceptArray = [[NSMutableArray alloc] init];
    for (size_t i = 0; i < extensions_[index].size(); ++i) {
      [acceptArray
          addObject:base::SysUTF16ToNSString(extensions_[index][i].substr(1))];
    }
  }
  [panel_ setAllowedFileTypes:acceptArray];

  if (![panel_ isKindOfClass:[NSOpenPanel class]]) {
    // For save panels set the file extension.
    [self setFileExtension];
  }
}

// Returns the current filter index.
- (int)filter {
  return selected_index_;
}

// Called when the selected filter is changed via the NSPopUpButton.
- (void)filterSelectionChanged:(id)sender {
  NSPopUpButton* button = (NSPopUpButton*)sender;
  [self setFilter:[button indexOfSelectedItem]];
}

// Set the extension on the currently selected file name.
- (void)setFileExtension {
  const std::vector<std::u16string>& filter = extensions_[selected_index_];
  if (filter.empty()) {
    // All extensions are allowed so don't change anything.
    return;
  }

  base::FilePath path(base::SysNSStringToUTF8([panel_ nameFieldStringValue]));

  // If the file name currently includes an extension from |filter| then don't
  // change anything.
  std::u16string extension = base::UTF8ToUTF16(path.Extension());
  if (!extension.empty()) {
    for (size_t i = 0; i < filter.size(); ++i) {
      if (filter[i] == extension)
        return;
    }
  }

  // Change the extension to the first value in |filter|.
  path = path.ReplaceExtension(base::UTF16ToUTF8(filter[0]));
  [panel_ setNameFieldStringValue:base::SysUTF8ToNSString(path.value())];
}

@end

CefFileDialogRunnerMac::CefFileDialogRunnerMac() : weak_ptr_factory_(this) {}

void CefFileDialogRunnerMac::Run(AlloyBrowserHostImpl* browser,
                                 const FileChooserParams& params,
                                 RunFileChooserCallback callback) {
  callback_ = std::move(callback);

  int filter_index = params.selected_accept_filter;
  NSView* owner = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetWindowHandle());
  auto weak_this = weak_ptr_factory_.GetWeakPtr();

  if (params.mode == blink::mojom::FileChooserParams::Mode::kOpen ||
      params.mode == blink::mojom::FileChooserParams::Mode::kOpenMultiple ||
      params.mode == blink::mojom::FileChooserParams::Mode::kUploadFolder) {
    RunOpenFileDialog(weak_this, params, owner, filter_index);
  } else if (params.mode == blink::mojom::FileChooserParams::Mode::kSave) {
    RunSaveFileDialog(weak_this, params, owner, filter_index);
  } else {
    NOTIMPLEMENTED();
  }
}

// static
void CefFileDialogRunnerMac::RunOpenFileDialog(
    base::WeakPtr<CefFileDialogRunnerMac> weak_this,
    const CefFileDialogRunner::FileChooserParams& params,
    NSView* view,
    int filter_index) {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];

  std::u16string title;
  if (!params.title.empty()) {
    title = params.title;
  } else {
    title = l10n_util::GetStringUTF16(
        params.mode == blink::mojom::FileChooserParams::Mode::kOpen
            ? IDS_OPEN_FILE_DIALOG_TITLE
            : (params.mode ==
                       blink::mojom::FileChooserParams::Mode::kOpenMultiple
                   ? IDS_OPEN_FILES_DIALOG_TITLE
                   : IDS_SELECT_FOLDER_DIALOG_TITLE));
  }
  [openPanel setTitle:base::SysUTF16ToNSString(title)];

  std::string filename, directory;
  if (!params.default_file_name.empty()) {
    if (params.mode == blink::mojom::FileChooserParams::Mode::kUploadFolder ||
        params.default_file_name.EndsWithSeparator()) {
      // The value is only a directory.
      directory = params.default_file_name.value();
    } else {
      // The value is a file name and possibly a directory.
      filename = params.default_file_name.BaseName().value();
      directory = params.default_file_name.DirName().value();
    }
  }
  if (!filename.empty()) {
    [openPanel setNameFieldStringValue:base::SysUTF8ToNSString(filename)];
  }
  if (!directory.empty()) {
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                          directory)]];
  }

  CefFilterDelegate* filter_delegate = nil;
  if (params.mode != blink::mojom::FileChooserParams::Mode::kUploadFolder &&
      !params.accept_types.empty()) {
    // Add the file filter control.
    filter_delegate =
        [[CefFilterDelegate alloc] initWithPanel:openPanel
                                andAcceptFilters:params.accept_types
                                  andFilterIndex:filter_index];
  }

  // Further panel configuration.
  [openPanel setAllowsOtherFileTypes:YES];
  [openPanel setAllowsMultipleSelection:
                 (params.mode ==
                  blink::mojom::FileChooserParams::Mode::kOpenMultiple)];
  [openPanel
      setCanChooseFiles:(params.mode !=
                         blink::mojom::FileChooserParams::Mode::kUploadFolder)];
  [openPanel
      setCanChooseDirectories:(params.mode == blink::mojom::FileChooserParams::
                                                  Mode::kUploadFolder)];
  [openPanel setShowsHiddenFiles:!params.hidereadonly];

  // Show panel.
  [openPanel
      beginSheetModalForWindow:[view window]
             completionHandler:^(NSInteger returnCode) {
               int filter_index_to_use = (filter_delegate != nil)
                                             ? [filter_delegate filter]
                                             : filter_index;
               if (returnCode == NSFileHandlingPanelOKButton) {
                 std::vector<base::FilePath> files;
                 files.reserve(openPanel.URLs.count);
                 for (NSURL* url in openPanel.URLs) {
                   if (url.isFileURL)
                     files.push_back(base::FilePath(url.path.UTF8String));
                 }
                 std::move(weak_this->callback_)
                     .Run(filter_index_to_use, files);
               } else {
                 std::move(weak_this->callback_)
                     .Run(filter_index_to_use, std::vector<base::FilePath>());
               }
             }];
}

// static
void CefFileDialogRunnerMac::RunSaveFileDialog(
    base::WeakPtr<CefFileDialogRunnerMac> weak_this,
    const CefFileDialogRunner::FileChooserParams& params,
    NSView* view,
    int filter_index) {
  NSSavePanel* savePanel = [NSSavePanel savePanel];

  std::u16string title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_SAVE_AS_DIALOG_TITLE);
  [savePanel setTitle:base::SysUTF16ToNSString(title)];

  std::string filename, directory;
  if (!params.default_file_name.empty()) {
    if (params.default_file_name.EndsWithSeparator()) {
      // The value is only a directory.
      directory = params.default_file_name.value();
    } else {
      // The value is a file name and possibly a directory.
      filename = params.default_file_name.BaseName().value();
      directory = params.default_file_name.DirName().value();
    }
  }
  if (!filename.empty()) {
    [savePanel setNameFieldStringValue:base::SysUTF8ToNSString(filename)];
  }
  if (!directory.empty()) {
    [savePanel setDirectoryURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                          directory)]];
  }

  CefFilterDelegate* filter_delegate = nil;
  if (!params.accept_types.empty()) {
    // Add the file filter control.
    filter_delegate =
        [[CefFilterDelegate alloc] initWithPanel:savePanel
                                andAcceptFilters:params.accept_types
                                  andFilterIndex:filter_index];
  }

  [savePanel setAllowsOtherFileTypes:YES];
  [savePanel setShowsHiddenFiles:!params.hidereadonly];

  // Show panel.
  [savePanel
      beginSheetModalForWindow:view.window
             completionHandler:^(NSInteger resultCode) {
               int filter_index_to_use = (filter_delegate != nil)
                                             ? [filter_delegate filter]
                                             : filter_index;
               if (resultCode == NSFileHandlingPanelOKButton) {
                 NSURL* url = savePanel.URL;
                 const char* path = url.path.UTF8String;
                 std::vector<base::FilePath> files(1, base::FilePath(path));
                 std::move(weak_this->callback_)
                     .Run(filter_index_to_use, files);
               } else {
                 std::move(weak_this->callback_)
                     .Run(filter_index_to_use, std::vector<base::FilePath>());
               }
             }];
}
