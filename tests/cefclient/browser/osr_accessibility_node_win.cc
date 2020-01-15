// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

// This class implements our accessible proxy object that handles moving
// data back and forth between MSAA clients and CefClient renderers.
// Sample implementation based on ui\accessibility\ax_platform_node_win.h

#include "tests/cefclient/browser/osr_accessibility_node.h"

#if defined(CEF_USE_ATL)

#include <atlbase.h>
#include <oleacc.h>
#include <string>

#include "tests/cefclient/browser/osr_accessibility_helper.h"

namespace client {

// Return CO_E_OBJNOTCONNECTED for accessible objects thar still exists but the
// window and/or object it references has been destroyed.
#define DATACHECK(node) (node) ? S_OK : CO_E_OBJNOTCONNECTED
#define VALID_CHILDID(varChild) ((varChild.vt == VT_I4))

namespace {

// Helper function to convert a rectangle from client coordinates to screen
// coordinates.
void ClientToScreen(HWND hwnd, LPRECT lpRect) {
  if (lpRect) {
    POINT ptTL = {lpRect->left, lpRect->top};
    POINT ptBR = {lpRect->right, lpRect->bottom};
    // Win32 API only provides the call for a point.
    ClientToScreen(hwnd, &ptTL);
    ClientToScreen(hwnd, &ptBR);
    SetRect(lpRect, ptTL.x, ptTL.y, ptBR.x, ptBR.y);
  }
}

// Helper function to convert to MSAARole
int AxRoleToMSAARole(const std::string& role_string) {
  if (role_string == "alert")
    return ROLE_SYSTEM_ALERT;
  if (role_string == "application")
    return ROLE_SYSTEM_APPLICATION;
  if (role_string == "buttonDropDown")
    return ROLE_SYSTEM_BUTTONDROPDOWN;
  if (role_string == "popUpButton")
    return ROLE_SYSTEM_BUTTONMENU;
  if (role_string == "checkBox")
    return ROLE_SYSTEM_CHECKBUTTON;
  if (role_string == "comboBox")
    return ROLE_SYSTEM_COMBOBOX;
  if (role_string == "dialog")
    return ROLE_SYSTEM_DIALOG;
  if (role_string == "genericContainer")
    return ROLE_SYSTEM_GROUPING;
  if (role_string == "group")
    return ROLE_SYSTEM_GROUPING;
  if (role_string == "image")
    return ROLE_SYSTEM_GRAPHIC;
  if (role_string == "link")
    return ROLE_SYSTEM_LINK;
  if (role_string == "locationBar")
    return ROLE_SYSTEM_GROUPING;
  if (role_string == "menuBar")
    return ROLE_SYSTEM_MENUBAR;
  if (role_string == "menuItem")
    return ROLE_SYSTEM_MENUITEM;
  if (role_string == "menuListPopup")
    return ROLE_SYSTEM_MENUPOPUP;
  if (role_string == "tree")
    return ROLE_SYSTEM_OUTLINE;
  if (role_string == "treeItem")
    return ROLE_SYSTEM_OUTLINEITEM;
  if (role_string == "tab")
    return ROLE_SYSTEM_PAGETAB;
  if (role_string == "tabList")
    return ROLE_SYSTEM_PAGETABLIST;
  if (role_string == "pane")
    return ROLE_SYSTEM_PANE;
  if (role_string == "progressIndicator")
    return ROLE_SYSTEM_PROGRESSBAR;
  if (role_string == "button")
    return ROLE_SYSTEM_PUSHBUTTON;
  if (role_string == "radioButton")
    return ROLE_SYSTEM_RADIOBUTTON;
  if (role_string == "scrollBar")
    return ROLE_SYSTEM_SCROLLBAR;
  if (role_string == "splitter")
    return ROLE_SYSTEM_SEPARATOR;
  if (role_string == "slider")
    return ROLE_SYSTEM_SLIDER;
  if (role_string == "staticText")
    return ROLE_SYSTEM_STATICTEXT;
  if (role_string == "textField")
    return ROLE_SYSTEM_TEXT;
  if (role_string == "titleBar")
    return ROLE_SYSTEM_TITLEBAR;
  if (role_string == "toolbar")
    return ROLE_SYSTEM_TOOLBAR;
  if (role_string == "webView")
    return ROLE_SYSTEM_GROUPING;
  if (role_string == "window")
    return ROLE_SYSTEM_WINDOW;
  if (role_string == "client")
    return ROLE_SYSTEM_CLIENT;
  // This is the default role for MSAA.
  return ROLE_SYSTEM_CLIENT;
}

static inline int MiddleX(const CefRect& rect) {
  return rect.x + rect.width / 2;
}

static inline int MiddleY(const CefRect& rect) {
  return rect.y + rect.height / 2;
}

}  // namespace

struct CefIAccessible : public IAccessible {
 public:
  // Implement IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  //
  // IAccessible methods.
  //
  // Retrieves the child element or child object at a given point on the screen.
  STDMETHODIMP accHitTest(LONG x_left, LONG y_top, VARIANT* child) override;

  // Performs the object's default action.
  STDMETHODIMP accDoDefaultAction(VARIANT var_id) override;

  // Retrieves the specified object's current screen location.
  STDMETHODIMP accLocation(LONG* x_left,
                           LONG* y_top,
                           LONG* width,
                           LONG* height,
                           VARIANT var_id) override;

  // Traverses to another UI element and retrieves the object.
  STDMETHODIMP accNavigate(LONG nav_dir, VARIANT start, VARIANT* end) override;

  // Retrieves an IDispatch interface pointer for the specified child.
  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** disp_child) override;

  // Retrieves the number of accessible children.
  STDMETHODIMP get_accChildCount(LONG* child_count) override;

  // Retrieves a string that describes the object's default action.
  STDMETHODIMP get_accDefaultAction(VARIANT var_id,
                                    BSTR* default_action) override;

  // Retrieves the tooltip description.
  STDMETHODIMP get_accDescription(VARIANT var_id, BSTR* desc) override;

  // Retrieves the object that has the keyboard focus.
  STDMETHODIMP get_accFocus(VARIANT* focus_child) override;

  // Retrieves the specified object's shortcut.
  STDMETHODIMP get_accKeyboardShortcut(VARIANT var_id,
                                       BSTR* access_key) override;

  // Retrieves the name of the specified object.
  STDMETHODIMP get_accName(VARIANT var_id, BSTR* name) override;

  // Retrieves the IDispatch interface of the object's parent.
  STDMETHODIMP get_accParent(IDispatch** disp_parent) override;

  // Retrieves information describing the role of the specified object.
  STDMETHODIMP get_accRole(VARIANT var_id, VARIANT* role) override;

  // Retrieves the current state of the specified object.
  STDMETHODIMP get_accState(VARIANT var_id, VARIANT* state) override;

  // Gets the help string for the specified object.
  STDMETHODIMP get_accHelp(VARIANT var_id, BSTR* help) override;

  // Retrieve or set the string value associated with the specified object.
  // Setting the value is not typically used by screen readers, but it's
  // used frequently by automation software.
  STDMETHODIMP get_accValue(VARIANT var_id, BSTR* value) override;
  STDMETHODIMP put_accValue(VARIANT var_id, BSTR new_value) override;

  // IAccessible methods not implemented.
  STDMETHODIMP get_accSelection(VARIANT* selected) override;
  STDMETHODIMP accSelect(LONG flags_sel, VARIANT var_id) override;
  STDMETHODIMP get_accHelpTopic(BSTR* help_file,
                                VARIANT var_id,
                                LONG* topic_id) override;
  STDMETHODIMP put_accName(VARIANT var_id, BSTR put_name) override;

  // Implement IDispatch
  STDMETHODIMP GetTypeInfoCount(unsigned int FAR* pctinfo) override;
  STDMETHODIMP GetTypeInfo(unsigned int iTInfo,
                           LCID lcid,
                           ITypeInfo FAR* FAR* ppTInfo) override;
  STDMETHODIMP GetIDsOfNames(REFIID riid,
                             OLECHAR FAR* FAR* rgszNames,
                             unsigned int cNames,
                             LCID lcid,
                             DISPID FAR* rgDispId) override;
  STDMETHODIMP Invoke(DISPID dispIdMember,
                      REFIID riid,
                      LCID lcid,
                      WORD wFlags,
                      DISPPARAMS FAR* pDispParams,
                      VARIANT FAR* pVarResult,
                      EXCEPINFO FAR* pExcepInfo,
                      unsigned int FAR* puArgErr) override;

  CefIAccessible(OsrAXNode* node) : ref_count_(0), node_(node) {}

  // Remove the node reference when OsrAXNode is destroyed, so that
  // MSAA clients get  CO_E_OBJNOTCONNECTED
  void MarkDestroyed() { node_ = nullptr; }

 protected:
  virtual ~CefIAccessible() {}

  // Ref Count
  ULONG ref_count_;
  // OsrAXNode* proxy object
  OsrAXNode* node_;
};

// Implement IUnknown
// *********************

// Handles ref counting and querying for other supported interfaces.
// We only support, IUnknown, IDispatch and IAccessible.
STDMETHODIMP CefIAccessible::QueryInterface(REFIID riid, void** ppvObject) {
  if (riid == IID_IAccessible)
    *ppvObject = static_cast<IAccessible*>(this);
  else if (riid == IID_IDispatch)
    *ppvObject = static_cast<IDispatch*>(this);
  else if (riid == IID_IUnknown)
    *ppvObject = static_cast<IUnknown*>(this);
  else
    *ppvObject = nullptr;

  if (*ppvObject)
    reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();

  return (*ppvObject) ? S_OK : E_NOINTERFACE;
}

// Increments COM objects refcount required by IUnknown for reference counting
STDMETHODIMP_(ULONG) CefIAccessible::AddRef() {
  return InterlockedIncrement((LONG volatile*)&ref_count_);
}

STDMETHODIMP_(ULONG) CefIAccessible::Release() {
  ULONG ulRefCnt = InterlockedDecrement((LONG volatile*)&ref_count_);
  if (ulRefCnt == 0) {
    // Remove reference from OsrAXNode
    if (node_)
      node_->Destroy();
    delete this;
  }

  return ulRefCnt;
}

// Implement IAccessible
// *********************

// Returns the parent IAccessible in the form of an IDispatch interface.
STDMETHODIMP CefIAccessible::get_accParent(IDispatch** ppdispParent) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (ppdispParent) {
      CefNativeAccessible* parent = node_->GetParentAccessibleObject();
      if (!parent) {
        // Find our parent window
        HWND hWnd = ::GetParent(node_->GetWindowHandle());
        // if we have a window attempt to get its IAccessible pointer
        if (hWnd) {
          AccessibleObjectFromWindow(hWnd, (DWORD)OBJID_CLIENT, IID_IAccessible,
                                     (void**)(&parent));
        }
      }

      if (parent)
        parent->AddRef();
      *ppdispParent = parent;
      retCode = (*ppdispParent) ? S_OK : S_FALSE;
    }
  } else {
    retCode = E_INVALIDARG;
  }
  return retCode;
}

// Returns the number of children we have for this element.
STDMETHODIMP CefIAccessible::get_accChildCount(long* pcountChildren) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode) && pcountChildren) {
    // Get Child node count for this from Accessibility tree
    *pcountChildren = node_->GetChildCount();
  } else {
    retCode = E_INVALIDARG;
  }
  return retCode;
}

// Returns a child IAccessible object.
STDMETHODIMP CefIAccessible::get_accChild(VARIANT varChild,
                                          IDispatch** ppdispChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    int numChilds = node_->GetChildCount();
    // Mark Leaf node if there are no child
    if (numChilds <= 0) {
      *ppdispChild = nullptr;
      return S_FALSE;
    } else {
      if (ppdispChild && VALID_CHILDID(varChild)) {
        if (varChild.lVal == CHILDID_SELF) {
          *ppdispChild = this;
        } else {
          // Convert to 0 based index and get Child Node.
          OsrAXNode* child = node_->ChildAtIndex(varChild.lVal - 1);
          // Fallback to focused node
          if (!child)
            child = node_->GetAccessibilityHelper()->GetFocusedNode();

          *ppdispChild = child->GetNativeAccessibleObject(node_);
        }
        if (*ppdispChild == nullptr)
          retCode = S_FALSE;
        else
          (*ppdispChild)->AddRef();
      }
    }
  }
  return retCode;
}

// Check and returns the accessible name for element from accessibility tree
STDMETHODIMP CefIAccessible::get_accName(VARIANT varChild, BSTR* pszName) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pszName && VALID_CHILDID(varChild)) {
      std::wstring name = node_->AxName();
      CComBSTR bstrResult(name.c_str());
      *pszName = bstrResult.Detach();
    }
  } else {
    retCode = E_INVALIDARG;
  }
  return retCode;
}

// Check and returns the value for element from accessibility tree
STDMETHODIMP CefIAccessible::get_accValue(VARIANT varChild, BSTR* pszValue) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pszValue && VALID_CHILDID(varChild)) {
      std::wstring name = node_->AxValue();
      CComBSTR bstrResult(name.c_str());
      *pszValue = bstrResult.Detach();
    }
  } else {
    retCode = E_INVALIDARG;
  }
  return retCode;
}

// Check and returns the description for element from accessibility tree
STDMETHODIMP CefIAccessible::get_accDescription(VARIANT varChild,
                                                BSTR* pszDescription) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pszDescription && VALID_CHILDID(varChild)) {
      std::wstring name = node_->AxDescription();
      CComBSTR bstrResult(name.c_str());
      *pszDescription = bstrResult.Detach();
    }
  } else {
    retCode = E_INVALIDARG;
  }
  return retCode;
}

// Check and returns the MSAA Role for element from accessibility tree
STDMETHODIMP CefIAccessible::get_accRole(VARIANT varChild, VARIANT* pvarRole) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    // Get the accessibilty role and Map to MSAA Role
    if (pvarRole) {
      pvarRole->vt = VT_I4;
      pvarRole->lVal = AxRoleToMSAARole(node_->AxRole());
    } else {
      retCode = E_INVALIDARG;
    }
  }
  return retCode;
}

// Check and returns Accessibility State for element from accessibility tree
STDMETHODIMP CefIAccessible::get_accState(VARIANT varChild,
                                          VARIANT* pvarState) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pvarState) {
      pvarState->vt = VT_I4;
      pvarState->lVal =
          (GetFocus() == node_->GetWindowHandle()) ? STATE_SYSTEM_FOCUSED : 0;
      pvarState->lVal |= STATE_SYSTEM_PRESSED;
      pvarState->lVal |= STATE_SYSTEM_FOCUSABLE;

      // For child
      if (varChild.lVal == CHILDID_SELF) {
        DWORD dwStyle = GetWindowLong(node_->GetWindowHandle(), GWL_STYLE);
        pvarState->lVal |=
            ((dwStyle & WS_VISIBLE) == 0) ? STATE_SYSTEM_INVISIBLE : 0;
        pvarState->lVal |=
            ((dwStyle & WS_DISABLED) > 0) ? STATE_SYSTEM_UNAVAILABLE : 0;
      }
    } else {
      retCode = E_INVALIDARG;
    }
  }
  return retCode;
}

// Check and returns Accessibility Shortcut if any for element
STDMETHODIMP CefIAccessible::get_accKeyboardShortcut(
    VARIANT varChild,
    BSTR* pszKeyboardShortcut) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pszKeyboardShortcut && VALID_CHILDID(varChild))
      *pszKeyboardShortcut = ::SysAllocString(L"None");
    else
      retCode = E_INVALIDARG;
  }
  return retCode;
}

// Return focused element from the accessibility tree
STDMETHODIMP CefIAccessible::get_accFocus(VARIANT* pFocusChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    OsrAXNode* focusedNode = node_->GetAccessibilityHelper()->GetFocusedNode();
    CefNativeAccessible* nativeObj = nullptr;
    if (focusedNode)
      nativeObj = focusedNode->GetNativeAccessibleObject(nullptr);

    if (nativeObj) {
      if (nativeObj == this) {
        pFocusChild->vt = VT_I4;
        pFocusChild->lVal = CHILDID_SELF;
      } else {
        pFocusChild->vt = VT_DISPATCH;
        pFocusChild->pdispVal = nativeObj;
        pFocusChild->pdispVal->AddRef();
      }
    } else {
      pFocusChild->vt = VT_EMPTY;
    }
  }
  return retCode;
}

// Return a selection list for multiple selection items.
STDMETHODIMP CefIAccessible::get_accSelection(VARIANT* pvarChildren) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pvarChildren)
      pvarChildren->vt = VT_EMPTY;
    else
      retCode = E_INVALIDARG;
  }
  return retCode;
}

// Return a string description of the default action of our element, eg. push
STDMETHODIMP CefIAccessible::get_accDefaultAction(VARIANT varChild,
                                                  BSTR* pszDefaultAction) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pszDefaultAction && VALID_CHILDID(varChild))
      *pszDefaultAction = ::SysAllocString(L"Push");
    else
      retCode = E_INVALIDARG;
  }
  return retCode;
}

// child item selectionor for an item to take focus.
STDMETHODIMP CefIAccessible::accSelect(long flagsSelect, VARIANT varChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (VALID_CHILDID(varChild)) {
      HWND hwnd = node_->GetWindowHandle();
      // we only support SELFLAG_TAKEFOCUS.
      if (((flagsSelect & SELFLAG_TAKEFOCUS) > 0) && (GetFocus() == hwnd)) {
        RECT rcWnd;
        GetClientRect(hwnd, &rcWnd);
        InvalidateRect(hwnd, &rcWnd, FALSE);
      } else {
        retCode = S_FALSE;
      }
    } else {
      retCode = E_INVALIDARG;
    }
  }

  return retCode;
}

// Returns back the screen coordinates of our element or one of its childs
STDMETHODIMP CefIAccessible::accLocation(long* pxLeft,
                                         long* pyTop,
                                         long* pcxWidth,
                                         long* pcyHeight,
                                         VARIANT varChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pxLeft && pyTop && pcxWidth && pcyHeight && VALID_CHILDID(varChild)) {
      CefRect loc = node_->AxLocation();
      RECT rcItem = {loc.x, loc.y, loc.x + loc.width, loc.y + loc.height};
      HWND hwnd = node_->GetWindowHandle();
      ClientToScreen(hwnd, &rcItem);

      *pxLeft = rcItem.left;
      *pyTop = rcItem.top;
      *pcxWidth = rcItem.right - rcItem.left;
      *pcyHeight = rcItem.bottom - rcItem.top;
    } else {
      retCode = E_INVALIDARG;
    }
  }
  return retCode;
}

// Allow clients to move the keyboard focus within the control
// Deprecated
STDMETHODIMP CefIAccessible::accNavigate(long navDir,
                                         VARIANT varStart,
                                         VARIANT* pvarEndUpAt) {
  return E_NOTIMPL;
}

// Check if the coordinates provided are within our element or child items.
STDMETHODIMP CefIAccessible::accHitTest(long xLeft,
                                        long yTop,
                                        VARIANT* pvarChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode)) {
    if (pvarChild) {
      pvarChild->vt = VT_EMPTY;

      CefRect loc = node_->AxLocation();
      RECT rcItem = {loc.x, loc.y, loc.x + loc.width, loc.y + loc.height};
      POINT pt = {xLeft, yTop};

      ClientToScreen(node_->GetWindowHandle(), &rcItem);

      if (PtInRect(&rcItem, pt)) {
        pvarChild->vt = VT_I4;
        pvarChild->lVal = 1;
      }
    } else {
      retCode = E_INVALIDARG;
    }
  }

  return retCode;
}

// Forces the default action of our element. In simplest cases, send a click.
STDMETHODIMP CefIAccessible::accDoDefaultAction(VARIANT varChild) {
  HRESULT retCode = DATACHECK(node_);
  if (SUCCEEDED(retCode) && VALID_CHILDID(varChild)) {
    // doing our default action for out button is to simply click the button.
    CefRefPtr<CefBrowser> browser = node_->GetBrowser();
    if (browser) {
      CefMouseEvent mouse_event;
      const CefRect& rect = node_->AxLocation();
      mouse_event.x = MiddleX(rect);
      mouse_event.y = MiddleY(rect);

      mouse_event.modifiers = 0;
      browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, false, 1);
      browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, true, 1);
    }
  } else {
    retCode = E_INVALIDARG;
  }

  return retCode;
}

// Set the name for an element in the accessibility tree
STDMETHODIMP CefIAccessible::put_accName(VARIANT varChild, BSTR szName) {
  return E_NOTIMPL;
}

// Set the value for an element in the accessibility tree
STDMETHODIMP CefIAccessible::put_accValue(VARIANT varChild, BSTR szValue) {
  return E_NOTIMPL;
}

// Return E_NOTIMPL as no help file/ topic
STDMETHODIMP CefIAccessible::get_accHelp(VARIANT varChild, BSTR* pszHelp) {
  return E_NOTIMPL;
}

STDMETHODIMP CefIAccessible::get_accHelpTopic(BSTR* pszHelpFile,
                                              VARIANT varChild,
                                              long* pidTopic) {
  return E_NOTIMPL;
}

// IDispatch - We are not going to return E_NOTIMPL from IDispatch methods and
// let Active Accessibility implement the IAccessible interface for them.
STDMETHODIMP CefIAccessible::GetTypeInfoCount(unsigned int FAR* pctinfo) {
  return E_NOTIMPL;
}

STDMETHODIMP CefIAccessible::GetTypeInfo(unsigned int iTInfo,
                                         LCID lcid,
                                         ITypeInfo FAR* FAR* ppTInfo) {
  return E_NOTIMPL;
}

STDMETHODIMP CefIAccessible::GetIDsOfNames(REFIID riid,
                                           OLECHAR FAR* FAR* rgszNames,
                                           unsigned int cNames,
                                           LCID lcid,
                                           DISPID FAR* rgDispId) {
  return E_NOTIMPL;
}

STDMETHODIMP CefIAccessible::Invoke(DISPID dispIdMember,
                                    REFIID riid,
                                    LCID lcid,
                                    WORD wFlags,
                                    DISPPARAMS FAR* pDispParams,
                                    VARIANT FAR* pVarResult,
                                    EXCEPINFO FAR* pExcepInfo,
                                    unsigned int FAR* puArgErr) {
  return E_NOTIMPL;
}

void OsrAXNode::NotifyAccessibilityEvent(std::string event_type) const {
  if (event_type == "focus") {
    // Notify Screen Reader of focus change
    ::NotifyWinEvent(EVENT_OBJECT_FOCUS, GetWindowHandle(), OBJID_CLIENT,
                     node_id_);
  }
}

void OsrAXNode::Destroy() {
  CefIAccessible* ptr = static_cast<CefIAccessible*>(platform_accessibility_);
  if (ptr)
    ptr->MarkDestroyed();
  platform_accessibility_ = nullptr;
}

// Create and return NSAccessibility Implementation Object for Window
CefNativeAccessible* OsrAXNode::GetNativeAccessibleObject(OsrAXNode* parent) {
  if (!platform_accessibility_) {
    platform_accessibility_ = new CefIAccessible(this);
    platform_accessibility_->AddRef();
    SetParent(parent);
  }
  return platform_accessibility_;
}

}  // namespace client

#else  // !defined(CEF_USE_ATL)

namespace client {

void OsrAXNode::NotifyAccessibilityEvent(std::string event_type) const {}

void OsrAXNode::Destroy() {}

CefNativeAccessible* OsrAXNode::GetNativeAccessibleObject(OsrAXNode* parent) {
  return nullptr;
}

}  // namespace client

#endif  // !defined(CEF_USE_ATL)
