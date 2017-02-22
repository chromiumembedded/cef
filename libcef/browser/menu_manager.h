// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#define CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
#pragma once

#include "libcef/browser/menu_model_impl.h"

#include "libcef/browser/menu_runner.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/context_menu_params.h"

namespace content {
class RenderFrameHost;
class WebContents;
};

class CefBrowserHostImpl;
class CefRunContextMenuCallback;

class CefMenuManager : public CefMenuModelImpl::Delegate,
                       public content::WebContentsObserver {
 public:
  CefMenuManager(CefBrowserHostImpl* browser,
                 std::unique_ptr<CefMenuRunner> runner);
  ~CefMenuManager() override;

  // Delete the runner to free any platform constructs.
  void Destroy();

  // Returns true if the context menu is currently showing.
  bool IsShowingContextMenu();

  // Create the context menu.
  bool CreateContextMenu(const content::ContextMenuParams& params);
  void CancelContextMenu();

 private:
  // CefMenuModelImpl::Delegate methods.
  void ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                      int command_id,
                      cef_event_flags_t event_flags) override;
  void MenuWillShow(CefRefPtr<CefMenuModelImpl> source) override;
  void MenuClosed(CefRefPtr<CefMenuModelImpl> source) override;
  bool FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                   base::string16& label) override;

  void ExecuteCommandCallback(int command_id,
                              cef_event_flags_t event_flags);

  // Create the default menu model.
  void CreateDefaultModel();
  // Execute the default command handling.
  void ExecuteDefaultCommand(int command_id);

  // Returns true if the specified id is a custom context menu command.
  bool IsCustomContextMenuCommand(int command_id);

  // CefBrowserHostImpl pointer is guaranteed to outlive this object.
  CefBrowserHostImpl* browser_;

  std::unique_ptr<CefMenuRunner> runner_;

  CefRefPtr<CefMenuModelImpl> model_;
  content::ContextMenuParams params_;

  // Not owned by this class.
  CefRunContextMenuCallback* custom_menu_callback_;

  // Must be the last member.
  base::WeakPtrFactory<CefMenuManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefMenuManager);
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MANAGER_H_
