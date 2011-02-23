// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _DOM_DOCUMENT_IMPL_H
#define _DOM_DOCUMENT_IMPL_H

#include "include/cef.h"
#include <map>

namespace WebKit {
class WebFrame;
class WebNode;
};

class CefBrowserImpl;

class CefDOMDocumentImpl : public CefThreadSafeBase<CefDOMDocument>
{
public:
  CefDOMDocumentImpl(CefBrowserImpl* browser,
                     WebKit::WebFrame* frame);
  virtual ~CefDOMDocumentImpl();

  virtual Type GetType();
  virtual CefRefPtr<CefDOMNode> GetDocument();
  virtual CefRefPtr<CefDOMNode> GetBody();
  virtual CefRefPtr<CefDOMNode> GetHead();
  virtual CefString GetTitle();
  virtual CefRefPtr<CefDOMNode> GetElementById(const CefString& id);
  virtual CefRefPtr<CefDOMNode> GetFocusedNode();
  virtual bool HasSelection();
  virtual CefRefPtr<CefDOMNode> GetSelectionStartNode();
  virtual int GetSelectionStartOffset();
  virtual CefRefPtr<CefDOMNode> GetSelectionEndNode();
  virtual int GetSelectionEndOffset();
  virtual CefString GetSelectionAsMarkup();
  virtual CefString GetSelectionAsText();
  virtual CefString GetBaseURL();
  virtual CefString GetCompleteURL(const CefString& partialURL);

  CefBrowserImpl* GetBrowser() { return browser_; }
  WebKit::WebFrame* GetFrame() { return frame_; }

  // The document maintains a map of all existing node objects.
  CefRefPtr<CefDOMNode> GetOrCreateNode(const WebKit::WebNode& node);
  void RemoveNode(const WebKit::WebNode& node);

  // Must be called before the object is destroyed.
  void Detach();

  // Verify that the object exists and is being accessed on the UI thread.
  bool VerifyContext();

protected:
  CefBrowserImpl* browser_;
  WebKit::WebFrame* frame_;

  typedef std::map<WebKit::WebNode,CefDOMNode*> NodeMap;
  NodeMap node_map_;
};

#endif // _DOM_DOCUMENT_IMPL_H
