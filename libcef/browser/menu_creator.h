// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#define CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#pragma once

#include "libcef/browser/menu_model_impl.h"

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/context_menu_params.h"

namespace content {
class RenderFrameHost;
class WebContents;
};

class CefBrowserHostImpl;

class CefMenuCreator : public CefMenuModelImpl::Delegate,
                       public content::WebContentsObserver {
 public:
  // Used for OS-specific menu implementations.
  class Runner {
  public:
    virtual ~Runner() {}
    virtual bool RunContextMenu(CefMenuCreator* manager) =0;
    virtual void CancelContextMenu() {}
    virtual bool FormatLabel(base::string16& label) { return false; }
  };

  CefMenuCreator(content::WebContents* web_contents,
                 CefBrowserHostImpl* browser);
  ~CefMenuCreator() override;

  // Returns true if the context menu is currently showing.
  bool IsShowingContextMenu();

  // Create the context menu.
  bool CreateContextMenu(const content::ContextMenuParams& params);
  void CancelContextMenu();

  CefBrowserHostImpl* browser() { return browser_; }
  ui::MenuModel* model() { return model_->model(); }
  const content::ContextMenuParams& params() const { return params_; }

 private:
  // Create the menu runner if it doesn't already exist.
  bool CreateRunner();

  // CefMenuModelImpl::Delegate methods.
  void ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                      int command_id,
                      cef_event_flags_t event_flags) override;
  void MenuWillShow(CefRefPtr<CefMenuModelImpl> source) override;
  void MenuClosed(CefRefPtr<CefMenuModelImpl> source) override;
  bool FormatLabel(base::string16& label) override;

  // Create the default menu model.
  void CreateDefaultModel();
  // Execute the default command handling.
  void ExecuteDefaultCommand(int command_id);

  // CefBrowserHostImpl pointer is guaranteed to outlive this object.
  CefBrowserHostImpl* browser_;

  CefRefPtr<CefMenuModelImpl> model_;
  content::ContextMenuParams params_;
  scoped_ptr<Runner> runner_;

  DISALLOW_COPY_AND_ASSIGN(CefMenuCreator);
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
