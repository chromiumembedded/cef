// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "dom_node_impl.h"
#include "browser_impl.h"
#include "cef_thread.h"
#include "dom_document_impl.h"
#include "dom_event_impl.h"
#include "tracker.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAttribute.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMEventListener.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNamedNodeMap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/glue/form_field.h"

using WebKit::WebAttribute;
using WebKit::WebDocument;
using WebKit::WebDOMEvent;
using WebKit::WebDOMEventListener;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebFormControlElement;
using WebKit::WebNamedNodeMap;
using WebKit::WebNode;
using WebKit::WebString;


namespace {

// Wrapper implementation for WebDOMEventListener.
class CefDOMEventListenerWrapper : public WebDOMEventListener,
                                   public CefTrackObject
{
public:
  CefDOMEventListenerWrapper(CefBrowserImpl* browser, WebFrame* frame,
                             CefRefPtr<CefDOMEventListener> listener)
    : browser_(browser), frame_(frame), listener_(listener)
  {
    // Cause this object to be deleted immediately before the frame is closed.
    browser->UIT_AddFrameObject(frame, this);
  }
  virtual ~CefDOMEventListenerWrapper()
  {
    REQUIRE_UIT();
  }

  virtual void handleEvent(const WebDOMEvent& event)
  {
    CefRefPtr<CefDOMDocumentImpl> documentImpl;
    CefRefPtr<CefDOMEventImpl> eventImpl;
    
    if (!event.isNull()) {
      // Create CefDOMDocumentImpl and CefDOMEventImpl objects that are valid
      // only for the scope of this method.
      const WebDocument& document = frame_->document();
      if (!document.isNull()) {
        documentImpl = new CefDOMDocumentImpl(browser_, frame_);
        eventImpl = new CefDOMEventImpl(documentImpl, event);
      }
    }
    
    listener_->HandleEvent(eventImpl.get());
    
    if(eventImpl.get())
      eventImpl->Detach();
    if (documentImpl.get())
      documentImpl->Detach();
  }

protected:
  CefBrowserImpl* browser_;
  WebFrame* frame_;
  CefRefPtr<CefDOMEventListener> listener_;
};

} // namespace


CefDOMNodeImpl::CefDOMNodeImpl(CefRefPtr<CefDOMDocumentImpl> document,
                               const WebKit::WebNode& node)
    : document_(document), node_(node)
{
}

CefDOMNodeImpl::~CefDOMNodeImpl()
{
  REQUIRE_UIT();

  if (document_.get() && !node_.isNull()) {
    // Remove the node from the document.
    document_->RemoveNode(node_);
  }
}

CefDOMNodeImpl::Type CefDOMNodeImpl::GetType()
{
  if (!VerifyContext())
    return DOM_NODE_TYPE_UNSUPPORTED;

  switch (node_.nodeType()) {
    case WebNode::ElementNode:
      return DOM_NODE_TYPE_ELEMENT;
    case WebNode::AttributeNode:
      return DOM_NODE_TYPE_ATTRIBUTE;
    case WebNode::TextNode:
      return DOM_NODE_TYPE_TEXT;
    case WebNode::CDataSectionNode:
      return DOM_NODE_TYPE_CDATA_SECTION;
    case WebNode::EntityReferenceNode:
      return DOM_NODE_TYPE_ENTITY_REFERENCE;
    case WebNode::EntityNode:
      return DOM_NODE_TYPE_ENTITY;
    case WebNode::ProcessingInstructionsNode:
      return DOM_NODE_TYPE_PROCESSING_INSTRUCTIONS;
    case WebNode::CommentNode:
      return DOM_NODE_TYPE_COMMENT;
    case WebNode::DocumentNode:
      return DOM_NODE_TYPE_DOCUMENT;
    case WebNode::DocumentTypeNode:
      return DOM_NODE_TYPE_DOCUMENT_TYPE;
    case WebNode::DocumentFragmentNode:
      return DOM_NODE_TYPE_DOCUMENT_FRAGMENT;
    case WebNode::NotationNode:
      return DOM_NODE_TYPE_NOTATION;
    case WebNode::XPathNamespaceNode:
      return DOM_NODE_TYPE_XPATH_NAMESPACE;
    default:
      return DOM_NODE_TYPE_UNSUPPORTED;
  }
}

bool CefDOMNodeImpl::IsText()
{
  if (!VerifyContext())
    return false;

  return node_.isTextNode();
}

bool CefDOMNodeImpl::IsElement()
{
  if (!VerifyContext())
    return false;

  return node_.isElementNode();
}

bool CefDOMNodeImpl::IsSame(CefRefPtr<CefDOMNode> that)
{
  if (!VerifyContext())
    return false;

  CefDOMNodeImpl* impl = static_cast<CefDOMNodeImpl*>(that.get());
  if (!impl || !impl->VerifyContext())
    return false;

  return node_.equals(impl->node_);
}

CefString CefDOMNodeImpl::GetName()
{
  CefString str;
  if (!VerifyContext())
    return str;

  const WebString& name = node_.nodeName();
  if (!name.isNull())
    str = name;

  return str;
}

CefString CefDOMNodeImpl::GetValue()
{
  CefString str;
  if (!VerifyContext())
    return str;

  if (node_.isElementNode()) {
    const WebElement& element = node_.to<WebKit::WebElement>();
    if (element.isFormControlElement()) {
      // Retrieve the value from the form control element.
      const WebFormControlElement& formElement =
          node_.to<WebKit::WebFormControlElement>();

      webkit_glue::FormField formField(formElement);
      str = formField.value;
    }
  }

  if (str.empty()) {
    const WebString& value = node_.nodeValue();
    if (!value.isNull())
      str = value;
  }

  return str;
}

bool CefDOMNodeImpl::SetValue(const CefString& value)
{
  if (!VerifyContext())
    return false;

  if (node_.isElementNode())
    return false;

  return node_.setNodeValue(string16(value));
}

CefString CefDOMNodeImpl::GetAsMarkup()
{
  CefString str;
  if (!VerifyContext())
    return str;

  const WebString& markup = node_.createMarkup();
  if (!markup.isNull())
    str = markup;

  return str;
}

CefRefPtr<CefDOMDocument> CefDOMNodeImpl::GetDocument()
{
  if (!VerifyContext())
    return NULL;

  return document_.get();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetParent()
{
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.parentNode());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetPreviousSibling()
{
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.previousSibling());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetNextSibling()
{
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.nextSibling());
}

bool CefDOMNodeImpl::HasChildren()
{
  if (!VerifyContext())
    return false;

  return node_.hasChildNodes();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetFirstChild()
{
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.firstChild());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetLastChild()
{
  if (!VerifyContext())
    return NULL;

  return document_->GetOrCreateNode(node_.lastChild());
}

void CefDOMNodeImpl::AddEventListener(const CefString& eventType,
                                      CefRefPtr<CefDOMEventListener> listener,
                                      bool useCapture)
{
  if (!VerifyContext())
    return;

  node_.addEventListener(string16(eventType),
      new CefDOMEventListenerWrapper(document_->GetBrowser(),
          document_->GetFrame(), listener),
      useCapture);
}

CefString CefDOMNodeImpl::GetElementTagName()
{
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return str;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  const WebString& tagname = element.tagName();
  if (!tagname.isNull())
    str = tagname;

  return str;
}

bool CefDOMNodeImpl::HasElementAttributes()
{
  if (!VerifyContext())
    return false;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return false;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  return (element.attributes().length() > 0);
}

bool CefDOMNodeImpl::HasElementAttribute(const CefString& attrName)
{
  if (!VerifyContext())
    return false;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return false;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  return element.hasAttribute(string16(attrName));
}

CefString CefDOMNodeImpl::GetElementAttribute(const CefString& attrName)
{
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return str;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  const WebString& attr = element.getAttribute(string16(attrName));
  if (!attr.isNull())
    str = attr;

  return str;
}

void CefDOMNodeImpl::GetElementAttributes(AttributeMap& attrMap)
{
  if (!VerifyContext())
    return;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  const WebNamedNodeMap& map = element.attributes();
  unsigned int len = map.length();
  if (len == 0)
    return;

  string16 nameStr, valueStr;

  for (unsigned int i = 0; i < len; ++i) {
    const WebAttribute& attrib = map.attributeItem(i);
    string16 nameStr, valueStr;
    const WebString& name = attrib.localName();
    if (!name.isNull())
      nameStr = name;
    const WebString& value = attrib.value();
    if (!value.isNull())
      valueStr = value;

    attrMap.insert(std::make_pair(nameStr, valueStr));
  }
}

bool CefDOMNodeImpl::SetElementAttribute(const CefString& attrName,
                                         const CefString& value)
{
  if (!VerifyContext())
    return false;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return false;
  }

  WebElement element = node_.to<WebKit::WebElement>();
  return element.setAttribute(string16(attrName), string16(value));
}

CefString CefDOMNodeImpl::GetElementInnerText()
{
  CefString str;
  if (!VerifyContext())
    return str;

  if (!node_.isElementNode()) {
    NOTREACHED();
    return str;
  }

  const WebElement& element = node_.to<WebKit::WebElement>();
  const WebString& text = element.innerText();
  if (!text.isNull())
    str = text;

  return str;
}

void CefDOMNodeImpl::Detach()
{
  document_ = NULL;
  node_.assign(WebNode());
}

bool CefDOMNodeImpl::VerifyContext()
{
  if (!document_.get()) {
    NOTREACHED();
    return false;
  }
  if (!document_->VerifyContext())
    return false;
  if(node_.isNull()) {
    NOTREACHED();
    return false;
  }
  return true;
}
