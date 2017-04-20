// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/renderer/dom_node_impl.h"
#include "libcef/common/tracker.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/webkit_glue.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebSelectElement.h"

using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebElement;
using blink::WebFrame;
using blink::WebFormControlElement;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;

CefDOMNodeImpl::CefDOMNodeImpl(CefRefPtr<CefDOMDocumentImpl> document,
                               const blink::WebNode& node)
    : document_(document),
      node_(node) {
}

CefDOMNodeImpl::~CefDOMNodeImpl() {
  CEF_REQUIRE_RT();

  if (document_.get() && !node_.IsNull()) {
    // Remove the node from the document.
    document_->RemoveNode(node_);
  }
}

CefDOMNodeImpl::Type CefDOMNodeImpl::GetType() {
  if (!VerifyContext())
    return DOM_NODE_TYPE_UNSUPPORTED;

  return webkit_glue::GetNodeType(node_);
}

bool CefDOMNodeImpl::IsText() {
  if (!VerifyContext())
    return false;

  return node_.IsTextNode();
}

bool CefDOMNodeImpl::IsElement() {
  if (!VerifyContext())
    return false;

  return node_.IsElementNode();
}

// Logic copied from RenderViewImpl::IsEditableNode.
bool CefDOMNodeImpl::IsEditable() {
  if (!VerifyContext())
    return false;

  if (node_.IsContentEditable())
    return true;

  if (node_.IsElementNode()) {
    const WebElement& element = node_.ToConst<WebElement>();
    if (webkit_glue::IsTextControlElement(element))
      return true;

    // Also return true if it has an ARIA role of 'textbox'.
    for (unsigned i = 0; i < element.AttributeCount(); ++i) {
      if (base::LowerCaseEqualsASCII(element.AttributeLocalName(i).Utf8(),
                                     "role")) {
        if (base::LowerCaseEqualsASCII(element.AttributeValue(i).Utf8(),
                                       "textbox")) {
          return true;
        }
        break;
      }
    }
  }

  return false;
}

bool CefDOMNodeImpl::IsFormControlElement() {
  if (!VerifyContext())
    return false;

  if (node_.IsElementNode()) {
    const WebElement& element = node_.ToConst<WebElement>();
    return element.IsFormControlElement();
  }

  return false;
}

CefString CefDOMNodeImpl::GetFormControlElementType() {
  CefString str;
  if (!VerifyContext())
    return str;

  if (node_.IsElementNode()) {
    const WebElement& element = node_.ToConst<WebElement>();
    if (element.IsFormControlElement()) {
      // Retrieve the type from the form control element.
      const WebFormControlElement& formElement =
          node_.ToConst<WebFormControlElement>();

      const base::string16& form_control_type =
          formElement.FormControlType().Utf16();
      str = form_control_type;
    }
  }

  return str;
}

bool CefDOMNodeImpl::IsSame(CefRefPtr<CefDOMNode> that) {
  if (!VerifyContext())
    return false;

  CefDOMNodeImpl* impl = static_cast<CefDOMNodeImpl*>(that.get());
  if (!impl || !impl->VerifyContext())
    return false;

  return node_.Equals(impl->node_);
}

CefString CefDOMNodeImpl::GetName() {
  CefString str;
  if (!VerifyContext())
    return str;

  const WebString& name = webkit_glue::GetNodeName(node_);
  if (!name.IsNull())
    str = name.Utf16();

  return str;
}

CefString CefDOMNodeImpl::GetValue() {
  CefString str;
  if (!VerifyContext())
    return str;

  if (node_.IsElementNode()) {
    const WebElement& element = node_.ToConst<WebElement>();
    if (element.IsFormControlElement()) {
      // Retrieve the value from the form control element.
      const WebFormControlElement& formElement =
          node_.ToConst<WebFormControlElement>();

      base::string16 value;
      const base::string16& form_control_type =
          formElement.FormControlType().Utf16();
      if (form_control_type == base::ASCIIToUTF16("text")) {
        const WebInputElement& input_element =
            formElement.ToConst<WebInputElement>();
        value = input_element.Value().Utf16();
      } else if (form_control_type == base::ASCIIToUTF16("select-one")) {
        const WebSelectElement& select_element =
            formElement.ToConst<WebSelectElement>();
        value = select_element.Value().Utf16();
      }

      base::TrimWhitespace(value, base::TRIM_LEADING, &value);
      str = value;
    }
  }

  if (str.empty()) {
    const WebString& value = node_.NodeValue();
    if (!value.IsNull())
      str = value.Utf16();
  }

  return str;
}

bool CefDOMNodeImpl::SetValue(const CefString& value) {
  if (!VerifyContext())
    return false;

  if (node_.IsElementNode())
    return false;

  return webkit_glue::SetNodeValue(node_, WebString::FromUTF16(value));
}

CefString CefDOMNodeImpl::GetAsMarkup() {
  CefString str;
  if (!VerifyContext())
    return str;

  const WebString& markup = webkit_glue::CreateNodeMarkup(node_);
  if (!markup.IsNull())
    str = markup.Utf16();

  return str;
}

CefRefPtr<CefDOMDocument> CefDOMNodeImpl::GetDocument() {
  if (!VerifyContext())
    return NULL;

  return document_.get();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetParent() {
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.ParentNode());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetPreviousSibling() {
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.PreviousSibling());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetNextSibling() {
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.NextSibling());
}

bool CefDOMNodeImpl::HasChildren() {
  if (!VerifyContext())
    return false;

  return !node_.FirstChild().IsNull();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetFirstChild() {
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.FirstChild());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetLastChild() {
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.LastChild());
}

CefString CefDOMNodeImpl::GetElementTagName() {
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return str;
  }

  const WebElement& element = node_.ToConst<blink::WebElement>();
  const WebString& tagname = element.TagName();
  if (!tagname.IsNull())
    str = tagname.Utf16();

  return str;
}

bool CefDOMNodeImpl::HasElementAttributes() {
  if (!VerifyContext())
    return false;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return false;
  }

  const WebElement& element = node_.ToConst<blink::WebElement>();
  return (element.AttributeCount() > 0);
}

bool CefDOMNodeImpl::HasElementAttribute(const CefString& attrName) {
  if (!VerifyContext())
    return false;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return false;
  }

  const WebElement& element = node_.ToConst<blink::WebElement>();
  return element.HasAttribute(WebString::FromUTF16(attrName));
}

CefString CefDOMNodeImpl::GetElementAttribute(const CefString& attrName) {
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return str;
  }

  const WebElement& element = node_.ToConst<blink::WebElement>();
  const WebString& attr = element.GetAttribute(WebString::FromUTF16(attrName));
  if (!attr.IsNull())
    str = attr.Utf16();

  return str;
}

void CefDOMNodeImpl::GetElementAttributes(AttributeMap& attrMap) {
  if (!VerifyContext())
    return;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return;
  }

  const WebElement& element = node_.ToConst<blink::WebElement>();
  unsigned int len = element.AttributeCount();
  if (len == 0)
    return;

  for (unsigned int i = 0; i < len; ++i) {
    base::string16 name = element.AttributeLocalName(i).Utf16();
    base::string16 value = element.AttributeValue(i).Utf16();
    attrMap.insert(std::make_pair(name, value));
  }
}

bool CefDOMNodeImpl::SetElementAttribute(const CefString& attrName,
                                         const CefString& value) {
  if (!VerifyContext())
    return false;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return false;
  }

  WebElement element = node_.To<blink::WebElement>();
  element.SetAttribute(WebString::FromUTF16(attrName),
                       WebString::FromUTF16(value));
  return true;
}

CefString CefDOMNodeImpl::GetElementInnerText() {
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return str;
  }

  WebElement element = node_.To<blink::WebElement>();
  const WebString& text = element.TextContent();
  if (!text.IsNull())
    str = text.Utf16();

  return str;
}

CefRect CefDOMNodeImpl::GetElementBounds() {
  CefRect rect;
  if (!VerifyContext())
    return rect;

  if (!node_.IsElementNode()) {
    NOTREACHED();
    return rect;
  }

  WebElement element = node_.To<blink::WebElement>();
  blink::WebRect rc = element.BoundsInViewport();
  rect.Set(rc.x, rc.y, rc.width, rc.height);

  return rect;
}

void CefDOMNodeImpl::Detach() {
  document_ = NULL;
  node_.Assign(WebNode());
}

bool CefDOMNodeImpl::VerifyContext() {
  if (!document_.get()) {
    NOTREACHED();
    return false;
  }
  if (!document_->VerifyContext())
    return false;
  if (node_.IsNull()) {
    NOTREACHED();
    return false;
  }
  return true;
}
