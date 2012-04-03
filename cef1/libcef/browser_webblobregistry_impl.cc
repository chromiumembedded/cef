// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webblobregistry_impl.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"

using WebKit::WebBlobData;
using WebKit::WebURL;
using webkit_blob::BlobData;

namespace {

MessageLoop* g_io_thread;
webkit_blob::BlobStorageController* g_blob_storage_controller;

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
      thread_safe_url, make_scoped_refptr(new BlobData(data))));
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
