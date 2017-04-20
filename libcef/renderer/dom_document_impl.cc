// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/dom_node_impl.h"
#include "libcef/renderer/thread_util.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebRange.h"

using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebNode;
using blink::WebRange;
using blink::WebString;
using blink::WebURL;


CefDOMDocumentImpl::CefDOMDocumentImpl(CefBrowserImpl* browser,
                                       WebFrame* frame)
    : browser_(browser),
      frame_(frame) {
  const WebDocument& document = frame_->GetDocument();
  DCHECK(!document.IsNull());
}

CefDOMDocumentImpl::~CefDOMDocumentImpl() {
  CEF_REQUIRE_RT();

  // Verify that the Detach() method has been called.
  DCHECK(frame_ == NULL);
}

CefDOMDocumentImpl::Type CefDOMDocumentImpl::GetType() {
  if (!VerifyContext())
    return DOM_DOCUMENT_TYPE_UNKNOWN;

  const WebDocument& document = frame_->GetDocument();
  if (document.IsHTMLDocument())
    return DOM_DOCUMENT_TYPE_HTML;
  if (document.IsXHTMLDocument())
    return DOM_DOCUMENT_TYPE_XHTML;
  if (document.IsPluginDocument())
    return DOM_DOCUMENT_TYPE_PLUGIN;
  return DOM_DOCUMENT_TYPE_UNKNOWN;
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetDocument() {
  const WebDocument& document = frame_->GetDocument();
  return GetOrCreateNode(document.GetDocument());
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetBody() {
  const WebDocument& document = frame_->GetDocument();
  return GetOrCreateNode(document.Body());
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetHead() {
  WebDocument document = frame_->GetDocument();
  return GetOrCreateNode(document.Head());
}

CefString CefDOMDocumentImpl::GetTitle() {
  CefString str;
  if (!VerifyContext())
    return str;

  const WebDocument& document = frame_->GetDocument();
  const WebString& title = document.Title();
  if (!title.IsNull())
    str = title.Utf16();

  return str;
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetElementById(const CefString& id) {
  const WebDocument& document = frame_->GetDocument();
  return GetOrCreateNode(document.GetElementById(WebString::FromUTF16(id)));
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetFocusedNode() {
  const WebDocument& document = frame_->GetDocument();
  return GetOrCreateNode(document.FocusedElement());
}

bool CefDOMDocumentImpl::HasSelection() {
  if (!VerifyContext() || !frame_->IsWebLocalFrame())
    return false;

  return frame_->ToWebLocalFrame()->HasSelection();
}

int CefDOMDocumentImpl::GetSelectionStartOffset() {
  if (!VerifyContext() || !frame_->IsWebLocalFrame())
    return 0;

  blink::WebLocalFrame* local_frame = frame_->ToWebLocalFrame();
  if (!!local_frame->HasSelection())
    return 0;

  const WebRange& range = local_frame->SelectionRange();
  if (range.IsNull())
    return 0;

  return range.StartOffset();
}

int CefDOMDocumentImpl::GetSelectionEndOffset() {
  if (!VerifyContext() || !frame_->IsWebLocalFrame())
    return 0;

  blink::WebLocalFrame* local_frame = frame_->ToWebLocalFrame();
  if (!!local_frame->HasSelection())
    return 0;

  const WebRange& range = local_frame->SelectionRange();
  if (range.IsNull())
    return 0;

  return range.EndOffset();
}

CefString CefDOMDocumentImpl::GetSelectionAsMarkup() {
  CefString str;
  if (!VerifyContext() || !frame_->IsWebLocalFrame())
    return str;

  blink::WebLocalFrame* local_frame = frame_->ToWebLocalFrame();
  if (!!local_frame->HasSelection())
    return str;

  const WebString& markup = local_frame->SelectionAsMarkup();
  if (!markup.IsNull())
    str = markup.Utf16();

  return str;
}

CefString CefDOMDocumentImpl::GetSelectionAsText() {
  CefString str;
  if (!VerifyContext() || !frame_->IsWebLocalFrame())
    return str;

  blink::WebLocalFrame* local_frame = frame_->ToWebLocalFrame();
  if (!!local_frame->HasSelection())
    return str;

  const WebString& text = local_frame->SelectionAsText();
  if (!text.IsNull())
    str = text.Utf16();

  return str;
}

CefString CefDOMDocumentImpl::GetBaseURL() {
  CefString str;
  if (!VerifyContext())
    return str;

  const WebDocument& document = frame_->GetDocument();
  const WebURL& url = document.BaseURL();
  if (!url.IsNull()) {
    GURL gurl = url;
    str = gurl.spec();
  }

  return str;
}

CefString CefDOMDocumentImpl::GetCompleteURL(const CefString& partialURL) {
  CefString str;
  if (!VerifyContext())
    return str;

  const WebDocument& document = frame_->GetDocument();
  const WebURL& url = document.CompleteURL(WebString::FromUTF16(partialURL));
  if (!url.IsNull()) {
    GURL gurl = url;
    str = gurl.spec();
  }

  return str;
}

CefRefPtr<CefDOMNode> CefDOMDocumentImpl::GetOrCreateNode(
    const blink::WebNode& node) {
  if (!VerifyContext())
    return NULL;

  // Nodes may potentially be null.
  if (node.IsNull())
    return NULL;

  if (!node_map_.empty()) {
    // Locate the existing node, if any.
    NodeMap::const_iterator it = node_map_.find(node);
    if (it != node_map_.end())
      return it->second;
  }

  // Create the new node object.
  CefRefPtr<CefDOMNode> nodeImpl(new CefDOMNodeImpl(this, node));
  node_map_.insert(std::make_pair(node, nodeImpl.get()));
  return nodeImpl;
}

void CefDOMDocumentImpl::RemoveNode(const blink::WebNode& node) {
  if (!VerifyContext())
    return;

  if (!node_map_.empty()) {
    NodeMap::iterator it = node_map_.find(node);
    if (it != node_map_.end())
      node_map_.erase(it);
  }
}

void CefDOMDocumentImpl::Detach() {
  if (!VerifyContext())
    return;

  // If you hit this assert it means that you are keeping references to node
  // objects beyond the valid scope.
  DCHECK(node_map_.empty());

  // If you hit this assert it means that you are keeping references to this
  // document object beyond the valid scope.
  DCHECK(HasOneRef());

  if (!node_map_.empty()) {
    NodeMap::const_iterator it = node_map_.begin();
    for (; it != node_map_.end(); ++it)
      static_cast<CefDOMNodeImpl*>(it->second)->Detach();
    node_map_.clear();
  }

  frame_ = NULL;
}

bool CefDOMDocumentImpl::VerifyContext() {
  if (!CEF_CURRENTLY_ON_RT() || frame_ == NULL) {
    NOTREACHED();
    return false;
  }
  return true;
}
