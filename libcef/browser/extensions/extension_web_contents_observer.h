// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include <memory>

#include "base/observer_list.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/script_execution_observer.h"
#include "extensions/browser/script_executor.h"

namespace extensions {

// The CEF version of ExtensionWebContentsObserver.
class CefExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<CefExtensionWebContentsObserver> {
 public:
  ~CefExtensionWebContentsObserver() override;

  ScriptExecutor* script_executor() { return script_executor_.get(); }

 private:
  friend class content::WebContentsUserData<CefExtensionWebContentsObserver>;

  explicit CefExtensionWebContentsObserver(content::WebContents* web_contents);

  // content::WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

  // Our content script observers. Declare at top so that it will outlive all
  // other members, since they might add themselves as observers.
  base::ObserverList<ScriptExecutionObserver> script_execution_observers_;

  std::unique_ptr<ScriptExecutor> script_executor_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
