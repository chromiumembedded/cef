// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <string>

#include "libcef/browser/stream_impl.h"
#include "libcef/common/drag_data_impl.h"
#include "base/files/file_path.h"
#include "net/base/filename_util.h"
#include "net/base/net_util.h"

#define CHECK_READONLY_RETURN_VOID() \
  if (read_only_) { \
    NOTREACHED() << "object is read only"; \
    return; \
  }

CefDragDataImpl::CefDragDataImpl(const content::DropData& data)
    : data_(data),
      read_only_(false) {
}

CefDragDataImpl::CefDragDataImpl()
    : read_only_(false) {
}

CefRefPtr<CefDragData> CefDragData::Create() {
  return new CefDragDataImpl();
}

CefRefPtr<CefDragData> CefDragDataImpl::Clone() {
  CefDragDataImpl* drag_data = NULL;
  {
    AutoLock lock_scope(this);
    drag_data = new CefDragDataImpl(data_);
  }
  return drag_data;
}

bool CefDragDataImpl::IsReadOnly() {
  AutoLock lock_scope(this);
  return read_only_;
}

bool CefDragDataImpl::IsLink() {
  AutoLock lock_scope(this);
  return (data_.url.is_valid() && data_.file_description_filename.empty());
}

bool CefDragDataImpl::IsFragment() {
  AutoLock lock_scope(this);
  return (!data_.url.is_valid() && data_.file_description_filename.empty() &&
          data_.filenames.empty());
}

bool CefDragDataImpl::IsFile() {
  AutoLock lock_scope(this);
  return (!data_.file_description_filename.empty() || !data_.filenames.empty());
}

CefString CefDragDataImpl::GetLinkURL() {
  AutoLock lock_scope(this);
  return data_.url.spec();
}

CefString CefDragDataImpl::GetLinkTitle() {
  AutoLock lock_scope(this);
  return data_.url_title;
}

CefString CefDragDataImpl::GetLinkMetadata() {
  AutoLock lock_scope(this);
  return data_.download_metadata;
}

CefString CefDragDataImpl::GetFragmentText() {
  AutoLock lock_scope(this);
  return data_.text.is_null() ? CefString() : CefString(data_.text.string());
}

CefString CefDragDataImpl::GetFragmentHtml() {
  AutoLock lock_scope(this);
  return data_.html.is_null() ? CefString() : CefString(data_.html.string());
}

CefString CefDragDataImpl::GetFragmentBaseURL() {
  AutoLock lock_scope(this);
  return data_.html_base_url.spec();
}

CefString CefDragDataImpl::GetFileName() {
  AutoLock lock_scope(this);
  if (data_.file_description_filename.empty())
    return CefString();

  base::FilePath file_name(CefString(data_.file_description_filename));
  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.BaseName().RemoveExtension().empty()) {
    CefString extension = file_name.Extension();
    // Retrieve the name from the URL.
    CefString suggested_file_name =
        net::GetSuggestedFilename(data_.url, "", "", "", "", "");
    file_name = base::FilePath(suggested_file_name).ReplaceExtension(extension);
  }
  return file_name.value();
}

size_t CefDragDataImpl::GetFileContents(CefRefPtr<CefStreamWriter> writer) {
  AutoLock lock_scope(this);
  if (data_.file_contents.empty())
    return 0;

  char* data = const_cast<char*>(data_.file_contents.c_str());
  size_t size = data_.file_contents.size();

  if (!writer.get())
    return size;

  return writer->Write(data, 1, size);
}

bool CefDragDataImpl::GetFileNames(std::vector<CefString>& names) {
  AutoLock lock_scope(this);
  if (data_.filenames.empty())
    return false;

  std::vector<ui::FileInfo>::const_iterator it =
      data_.filenames.begin();
  for (; it != data_.filenames.end(); ++it)
    names.push_back(it->path.value());

  return true;
}

void CefDragDataImpl::SetLinkURL(const CefString& url) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.url = GURL(url.ToString());
}

void CefDragDataImpl::SetLinkTitle(const CefString& title) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.url_title = title.ToString16();
}

void CefDragDataImpl::SetLinkMetadata(const CefString& data) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.download_metadata = data.ToString16();
}

void CefDragDataImpl::SetFragmentText(const CefString& text) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.text = base::NullableString16(text.ToString16(), false);
}

void CefDragDataImpl::SetFragmentHtml(const CefString& fragment) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.html = base::NullableString16(fragment.ToString16(), false);
}

void CefDragDataImpl::SetFragmentBaseURL(const CefString& fragment) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.html_base_url = GURL(fragment.ToString());
}

void CefDragDataImpl::ResetFileContents() {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.file_contents.erase();
  data_.file_description_filename.erase();
}

void CefDragDataImpl::AddFile(const CefString& path,
                              const CefString& display_name) {
  AutoLock lock_scope(this);
  CHECK_READONLY_RETURN_VOID();
  data_.filenames.push_back(ui::FileInfo(base::FilePath(path),
                                         base::FilePath(display_name)));
}

void CefDragDataImpl::SetReadOnly(bool read_only) {
  AutoLock lock_scope(this);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;
}
