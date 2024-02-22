// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/renderer/dom_node_impl.h"

#include "libcef/common/tracker.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/thread_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_select_element.h"

using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebInputElement;
using blink::WebNode;
using blink::WebSelectElement;
using blink::WebString;
using FormControlType = blink::mojom::FormControlType;

namespace {

cef_dom_form_control_type_t GetCefFormControlType(FormControlType type) {
  switch (type) {
    case FormControlType::kButtonButton:
      return DOM_FORM_CONTROL_TYPE_BUTTON_BUTTON;
    case FormControlType::kButtonSubmit:
      return DOM_FORM_CONTROL_TYPE_BUTTON_SUBMIT;
    case FormControlType::kButtonReset:
      return DOM_FORM_CONTROL_TYPE_BUTTON_RESET;
    case FormControlType::kButtonSelectList:
      return DOM_FORM_CONTROL_TYPE_BUTTON_SELECT_LIST;
    case FormControlType::kButtonPopover:
      return DOM_FORM_CONTROL_TYPE_BUTTON_POPOVER;
    case FormControlType::kFieldset:
      return DOM_FORM_CONTROL_TYPE_FIELDSET;
    case FormControlType::kInputButton:
      return DOM_FORM_CONTROL_TYPE_INPUT_BUTTON;
    case FormControlType::kInputCheckbox:
      return DOM_FORM_CONTROL_TYPE_INPUT_CHECKBOX;
    case FormControlType::kInputColor:
      return DOM_FORM_CONTROL_TYPE_INPUT_COLOR;
    case FormControlType::kInputDate:
      return DOM_FORM_CONTROL_TYPE_INPUT_DATE;
    case FormControlType::kInputDatetimeLocal:
      return DOM_FORM_CONTROL_TYPE_INPUT_DATETIME_LOCAL;
    case FormControlType::kInputEmail:
      return DOM_FORM_CONTROL_TYPE_INPUT_EMAIL;
    case FormControlType::kInputFile:
      return DOM_FORM_CONTROL_TYPE_INPUT_FILE;
    case FormControlType::kInputHidden:
      return DOM_FORM_CONTROL_TYPE_INPUT_HIDDEN;
    case FormControlType::kInputImage:
      return DOM_FORM_CONTROL_TYPE_INPUT_IMAGE;
    case FormControlType::kInputMonth:
      return DOM_FORM_CONTROL_TYPE_INPUT_MONTH;
    case FormControlType::kInputNumber:
      return DOM_FORM_CONTROL_TYPE_INPUT_NUMBER;
    case FormControlType::kInputPassword:
      return DOM_FORM_CONTROL_TYPE_INPUT_PASSWORD;
    case FormControlType::kInputRadio:
      return DOM_FORM_CONTROL_TYPE_INPUT_RADIO;
    case FormControlType::kInputRange:
      return DOM_FORM_CONTROL_TYPE_INPUT_RANGE;
    case FormControlType::kInputReset:
      return DOM_FORM_CONTROL_TYPE_INPUT_RESET;
    case FormControlType::kInputSearch:
      return DOM_FORM_CONTROL_TYPE_INPUT_SEARCH;
    case FormControlType::kInputSubmit:
      return DOM_FORM_CONTROL_TYPE_INPUT_SUBMIT;
    case FormControlType::kInputTelephone:
      return DOM_FORM_CONTROL_TYPE_INPUT_TELEPHONE;
    case FormControlType::kInputText:
      return DOM_FORM_CONTROL_TYPE_INPUT_TEXT;
    case FormControlType::kInputTime:
      return DOM_FORM_CONTROL_TYPE_INPUT_TIME;
    case FormControlType::kInputUrl:
      return DOM_FORM_CONTROL_TYPE_INPUT_URL;
    case FormControlType::kInputWeek:
      return DOM_FORM_CONTROL_TYPE_INPUT_WEEK;
    case FormControlType::kOutput:
      return DOM_FORM_CONTROL_TYPE_OUTPUT;
    case FormControlType::kSelectOne:
      return DOM_FORM_CONTROL_TYPE_SELECT_ONE;
    case FormControlType::kSelectMultiple:
      return DOM_FORM_CONTROL_TYPE_SELECT_MULTIPLE;
    case FormControlType::kSelectList:
      return DOM_FORM_CONTROL_TYPE_SELECT_LIST;
    case FormControlType::kTextArea:
      return DOM_FORM_CONTROL_TYPE_TEXT_AREA;
  }
  return DOM_FORM_CONTROL_TYPE_UNSUPPORTED;
}

}  // namespace

CefDOMNodeImpl::CefDOMNodeImpl(CefRefPtr<CefDOMDocumentImpl> document,
                               const blink::WebNode& node)
    : document_(document), node_(node) {}

CefDOMNodeImpl::~CefDOMNodeImpl() {
  CEF_REQUIRE_RT();

  if (document_.get() && !node_.IsNull()) {
    // Remove the node from the document.
    document_->RemoveNode(node_);
  }
}

CefDOMNodeImpl::Type CefDOMNodeImpl::GetType() {
  if (!VerifyContext()) {
    return DOM_NODE_TYPE_UNSUPPORTED;
  }

  return blink_glue::GetNodeType(node_);
}

bool CefDOMNodeImpl::IsText() {
  if (!VerifyContext()) {
    return false;
  }

  return node_.IsTextNode();
}

bool CefDOMNodeImpl::IsElement() {
  if (!VerifyContext()) {
    return false;
  }

  return node_.IsElementNode();
}

// Logic copied from RenderViewImpl::IsEditableNode.
bool CefDOMNodeImpl::IsEditable() {
  if (!VerifyContext()) {
    return false;
  }

  if (node_.IsContentEditable()) {
    return true;
  }

  if (node_.IsElementNode()) {
    const WebElement& element = node_.To<WebElement>();
    if (blink_glue::IsTextControlElement(element)) {
      return true;
    }

    // Also return true if it has an ARIA role of 'textbox'.
    for (unsigned i = 0; i < element.AttributeCount(); ++i) {
      if (base::EqualsCaseInsensitiveASCII(element.AttributeLocalName(i).Utf8(),
                                           "role")) {
        if (base::EqualsCaseInsensitiveASCII(element.AttributeValue(i).Utf8(),
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
  if (!VerifyContext()) {
    return false;
  }

  if (node_.IsElementNode()) {
    const WebElement& element = node_.To<WebElement>();
    return element.IsFormControlElement();
  }

  return false;
}

CefDOMNodeImpl::FormControlType CefDOMNodeImpl::GetFormControlElementType() {
  if (!VerifyContext()) {
    return DOM_FORM_CONTROL_TYPE_UNSUPPORTED;
  }

  if (node_.IsElementNode()) {
    const WebElement& element = node_.To<WebElement>();
    if (element.IsFormControlElement()) {
      // Retrieve the type from the form control element.
      const WebFormControlElement& formElement =
          node_.To<WebFormControlElement>();
      return GetCefFormControlType(formElement.FormControlType());
    }
  }

  return DOM_FORM_CONTROL_TYPE_UNSUPPORTED;
}

bool CefDOMNodeImpl::IsSame(CefRefPtr<CefDOMNode> that) {
  if (!VerifyContext()) {
    return false;
  }

  CefDOMNodeImpl* impl = static_cast<CefDOMNodeImpl*>(that.get());
  if (!impl || !impl->VerifyContext()) {
    return false;
  }

  return node_.Equals(impl->node_);
}

CefString CefDOMNodeImpl::GetName() {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  const WebString& name = blink_glue::GetNodeName(node_);
  if (!name.IsNull()) {
    str = name.Utf16();
  }

  return str;
}

CefString CefDOMNodeImpl::GetValue() {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  if (node_.IsElementNode()) {
    const WebElement& element = node_.To<WebElement>();
    if (element.IsFormControlElement()) {
      // Retrieve the value from the form control element.
      const WebFormControlElement& formElement =
          node_.To<WebFormControlElement>();

      std::u16string value = formElement.Value().Utf16();
      base::TrimWhitespace(value, base::TRIM_LEADING, &value);
      str = value;
    }
  }

  if (str.empty()) {
    const WebString& value = node_.NodeValue();
    if (!value.IsNull()) {
      str = value.Utf16();
    }
  }

  return str;
}

bool CefDOMNodeImpl::SetValue(const CefString& value) {
  if (!VerifyContext()) {
    return false;
  }

  if (node_.IsElementNode()) {
    return false;
  }

  return blink_glue::SetNodeValue(node_,
                                  WebString::FromUTF16(value.ToString16()));
}

CefString CefDOMNodeImpl::GetAsMarkup() {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  const WebString& markup = blink_glue::CreateNodeMarkup(node_);
  if (!markup.IsNull()) {
    str = markup.Utf16();
  }

  return str;
}

CefRefPtr<CefDOMDocument> CefDOMNodeImpl::GetDocument() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_.get();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetParent() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_->GetOrCreateNode(node_.ParentNode());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetPreviousSibling() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_->GetOrCreateNode(node_.PreviousSibling());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetNextSibling() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_->GetOrCreateNode(node_.NextSibling());
}

bool CefDOMNodeImpl::HasChildren() {
  if (!VerifyContext()) {
    return false;
  }

  return !node_.FirstChild().IsNull();
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetFirstChild() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_->GetOrCreateNode(node_.FirstChild());
}

CefRefPtr<CefDOMNode> CefDOMNodeImpl::GetLastChild() {
  if (!VerifyContext()) {
    return nullptr;
  }

  return document_->GetOrCreateNode(node_.LastChild());
}

CefString CefDOMNodeImpl::GetElementTagName() {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return str;
  }

  const WebElement& element = node_.To<blink::WebElement>();
  const WebString& tagname = element.TagName();
  if (!tagname.IsNull()) {
    str = tagname.Utf16();
  }

  return str;
}

bool CefDOMNodeImpl::HasElementAttributes() {
  if (!VerifyContext()) {
    return false;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return false;
  }

  const WebElement& element = node_.To<blink::WebElement>();
  return (element.AttributeCount() > 0);
}

bool CefDOMNodeImpl::HasElementAttribute(const CefString& attrName) {
  if (!VerifyContext()) {
    return false;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return false;
  }

  const WebElement& element = node_.To<blink::WebElement>();
  return element.HasAttribute(WebString::FromUTF16(attrName.ToString16()));
}

CefString CefDOMNodeImpl::GetElementAttribute(const CefString& attrName) {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return str;
  }

  const WebElement& element = node_.To<blink::WebElement>();
  const WebString& attr =
      element.GetAttribute(WebString::FromUTF16(attrName.ToString16()));
  if (!attr.IsNull()) {
    str = attr.Utf16();
  }

  return str;
}

void CefDOMNodeImpl::GetElementAttributes(AttributeMap& attrMap) {
  if (!VerifyContext()) {
    return;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return;
  }

  const WebElement& element = node_.To<blink::WebElement>();
  unsigned int len = element.AttributeCount();
  if (len == 0) {
    return;
  }

  for (unsigned int i = 0; i < len; ++i) {
    std::u16string name = element.AttributeLocalName(i).Utf16();
    std::u16string value = element.AttributeValue(i).Utf16();
    attrMap.insert(std::make_pair(name, value));
  }
}

bool CefDOMNodeImpl::SetElementAttribute(const CefString& attrName,
                                         const CefString& value) {
  if (!VerifyContext()) {
    return false;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return false;
  }

  WebElement element = node_.To<blink::WebElement>();
  element.SetAttribute(WebString::FromUTF16(attrName.ToString16()),
                       WebString::FromUTF16(value.ToString16()));
  return true;
}

CefString CefDOMNodeImpl::GetElementInnerText() {
  CefString str;
  if (!VerifyContext()) {
    return str;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return str;
  }

  WebElement element = node_.To<blink::WebElement>();
  const WebString& text = element.TextContent();
  if (!text.IsNull()) {
    str = text.Utf16();
  }

  return str;
}

CefRect CefDOMNodeImpl::GetElementBounds() {
  CefRect rect;
  if (!VerifyContext()) {
    return rect;
  }

  if (!node_.IsElementNode()) {
    DCHECK(false);
    return rect;
  }

  WebElement element = node_.To<blink::WebElement>();
  const auto& rc = element.BoundsInWidget();
  rect.Set(rc.x(), rc.y(), rc.width(), rc.height());

  return rect;
}

void CefDOMNodeImpl::Detach() {
  document_ = nullptr;
  node_.Assign(WebNode());
}

bool CefDOMNodeImpl::VerifyContext() {
  if (!document_.get()) {
    DCHECK(false);
    return false;
  }
  if (!document_->VerifyContext()) {
    return false;
  }
  if (node_.IsNull()) {
    DCHECK(false);
    return false;
  }
  return true;
}
