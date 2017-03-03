// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <string>

#include "libcef/browser/stream_impl.h"
#include "libcef/common/drag_data_impl.h"
#include "base/files/file_path.h"

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
    base::AutoLock lock_scope(lock_);
    drag_data = new CefDragDataImpl(data_);
  }
  return drag_data;
}

bool CefDragDataImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

bool CefDragDataImpl::IsLink() {
  base::AutoLock lock_scope(lock_);
  return (data_.url.is_valid() &&
          data_.file_contents_content_disposition.empty());
}

bool CefDragDataImpl::IsFragment() {
  base::AutoLock lock_scope(lock_);
  return (!data_.url.is_valid() &&
          data_.file_contents_content_disposition.empty() &&
          data_.filenames.empty());
}

bool CefDragDataImpl::IsFile() {
  base::AutoLock lock_scope(lock_);
  return (!data_.file_contents_content_disposition.empty() ||
          !data_.filenames.empty());
}

CefString CefDragDataImpl::GetLinkURL() {
  base::AutoLock lock_scope(lock_);
  return data_.url.spec();
}

CefString CefDragDataImpl::GetLinkTitle() {
  base::AutoLock lock_scope(lock_);
  return data_.url_title;
}

CefString CefDragDataImpl::GetLinkMetadata() {
  base::AutoLock lock_scope(lock_);
  return data_.download_metadata;
}

CefString CefDragDataImpl::GetFragmentText() {
  base::AutoLock lock_scope(lock_);
  return data_.text.is_null() ? CefString() : CefString(data_.text.string());
}

CefString CefDragDataImpl::GetFragmentHtml() {
  base::AutoLock lock_scope(lock_);
  return data_.html.is_null() ? CefString() : CefString(data_.html.string());
}

CefString CefDragDataImpl::GetFragmentBaseURL() {
  base::AutoLock lock_scope(lock_);
  return data_.html_base_url.spec();
}

CefString CefDragDataImpl::GetFileName() {
  base::AutoLock lock_scope(lock_);
  if (data_.file_contents_content_disposition.empty())
    return CefString();

  base::Optional<base::FilePath> filename =
      data_.GetSafeFilenameForImageFileContents();
  return filename ? CefString(filename->value()) : CefString();
}

size_t CefDragDataImpl::GetFileContents(CefRefPtr<CefStreamWriter> writer) {
  base::AutoLock lock_scope(lock_);
  if (data_.file_contents.empty())
    return 0;

  char* data = const_cast<char*>(data_.file_contents.c_str());
  size_t size = data_.file_contents.size();

  if (!writer.get())
    return size;

  return writer->Write(data, 1, size);
}

bool CefDragDataImpl::GetFileNames(std::vector<CefString>& names) {
  base::AutoLock lock_scope(lock_);
  if (data_.filenames.empty())
    return false;

  std::vector<ui::FileInfo>::const_iterator it =
      data_.filenames.begin();
  for (; it != data_.filenames.end(); ++it)
    names.push_back(it->path.value());

  return true;
}

void CefDragDataImpl::SetLinkURL(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.url = GURL(url.ToString());
}

void CefDragDataImpl::SetLinkTitle(const CefString& title) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.url_title = title.ToString16();
}

void CefDragDataImpl::SetLinkMetadata(const CefString& data) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.download_metadata = data.ToString16();
}

void CefDragDataImpl::SetFragmentText(const CefString& text) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.text = base::NullableString16(text.ToString16(), false);
}

void CefDragDataImpl::SetFragmentHtml(const CefString& fragment) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.html = base::NullableString16(fragment.ToString16(), false);
}

void CefDragDataImpl::SetFragmentBaseURL(const CefString& fragment) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.html_base_url = GURL(fragment.ToString());
}

void CefDragDataImpl::ResetFileContents() {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.file_contents.erase();
  data_.file_contents_source_url = GURL();
  data_.file_contents_filename_extension.erase();
  data_.file_contents_content_disposition.erase();
}

void CefDragDataImpl::AddFile(const CefString& path,
                              const CefString& display_name) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  data_.filenames.push_back(ui::FileInfo(base::FilePath(path),
                                         base::FilePath(display_name)));
}

void CefDragDataImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;
}
