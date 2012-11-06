// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webblobregistry_impl.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using WebKit::WebBlobData;
using WebKit::WebURL;
using webkit_blob::BlobData;

namespace {

MessageLoop* g_io_thread;
webkit_blob::BlobStorageController* g_blob_storage_controller;

// Creates a new BlobData from WebBlobData.
BlobData* NewBlobData(const WebBlobData& data) {
  BlobData* blob = new BlobData;
  size_t i = 0;
  WebBlobData::Item item;
  while (data.itemAt(i++, item)) {
    switch (item.type) {
      case WebBlobData::Item::TypeData:
        if (!item.data.isEmpty()) {
          // WebBlobData does not allow partial data.
          DCHECK(!item.offset && item.length == -1);
          blob->AppendData(item.data);
        }
        break;
      case WebBlobData::Item::TypeFile:
        if (item.length) {
          blob->AppendFile(
              webkit_base::WebStringToFilePath(item.filePath),
              static_cast<uint64>(item.offset),
              static_cast<uint64>(item.length),
              base::Time::FromDoubleT(item.expectedModificationTime));
        }
        break;
      case WebBlobData::Item::TypeBlob:
        if (item.length) {
          blob->AppendBlob(
              item.blobURL,
              static_cast<uint64>(item.offset),
              static_cast<uint64>(item.length));
        }
        break;
      default:
        NOTREACHED();
    }
  }
  blob->set_content_type(data.contentType().utf8().data());
  blob->set_content_disposition(data.contentDisposition().utf8().data());
  return blob;
}

}  // namespace

/* static */
void BrowserWebBlobRegistryImpl::InitializeOnIOThread(
    webkit_blob::BlobStorageController* blob_storage_controller) {
  g_io_thread = MessageLoop::current();
  g_blob_storage_controller = blob_storage_controller;
}

/* static */
void BrowserWebBlobRegistryImpl::Cleanup() {
  g_io_thread = NULL;
  g_blob_storage_controller = NULL;
}

BrowserWebBlobRegistryImpl::BrowserWebBlobRegistryImpl() {
}

void BrowserWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, WebBlobData& data) {
  DCHECK(g_io_thread);
  GURL thread_safe_url = url;  // WebURL uses refcounted strings.
  g_io_thread->PostTask(FROM_HERE, base::Bind(
      &BrowserWebBlobRegistryImpl::AddFinishedBlob, this,
      thread_safe_url, make_scoped_refptr(NewBlobData(data))));
}

void BrowserWebBlobRegistryImpl::registerBlobURL(
    const WebURL& url, const WebURL& src_url) {
  DCHECK(g_io_thread);
  GURL thread_safe_url = url;
  GURL thread_safe_src_url = src_url;
  g_io_thread->PostTask(FROM_HERE,  base::Bind(
      &BrowserWebBlobRegistryImpl::CloneBlob, this,
      thread_safe_url, thread_safe_src_url));
}

void BrowserWebBlobRegistryImpl::unregisterBlobURL(const WebURL& url) {
  DCHECK(g_io_thread);
  GURL thread_safe_url = url;
  g_io_thread->PostTask(FROM_HERE, base::Bind(
      &BrowserWebBlobRegistryImpl::RemoveBlob, this,
      thread_safe_url));
}

BrowserWebBlobRegistryImpl::~BrowserWebBlobRegistryImpl() {
}

void BrowserWebBlobRegistryImpl::AddFinishedBlob(
    const GURL& url, BlobData* blob_data) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->AddFinishedBlob(url, blob_data);
}

void BrowserWebBlobRegistryImpl::CloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->CloneBlob(url, src_url);
}

void BrowserWebBlobRegistryImpl::RemoveBlob(const GURL& url) {
  DCHECK(g_blob_storage_controller);
  g_blob_storage_controller->RemoveBlob(url);
}
