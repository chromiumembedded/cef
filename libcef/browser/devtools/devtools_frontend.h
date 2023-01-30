// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_

#include <memory>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/devtools/devtools_file_manager.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class Value;
}

namespace content {
class NavigationHandle;
class RenderViewHost;
class WebContents;
}  // namespace content

class PrefService;

enum class ProtocolMessageType {
  METHOD,
  RESULT,
  EVENT,
};

class CefDevToolsFrontend : public content::WebContentsObserver,
                            public content::DevToolsAgentHostClient {
 public:
  CefDevToolsFrontend(const CefDevToolsFrontend&) = delete;
  CefDevToolsFrontend& operator=(const CefDevToolsFrontend&) = delete;

  static CefDevToolsFrontend* Show(
      AlloyBrowserHostImpl* inspected_browser,
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefBrowserSettings& settings,
      const CefPoint& inspect_element_at,
      base::OnceClosure frontend_destroyed_callback);

  void Activate();
  void Focus();
  void InspectElementAt(int x, int y);
  void Close();

  void CallClientFunction(
      const std::string& object_name,
      const std::string& method_name,
      const base::Value arg1 = {},
      const base::Value arg2 = {},
      const base::Value arg3 = {},
      base::OnceCallback<void(base::Value)> cb = base::NullCallback());

 private:
  CefDevToolsFrontend(AlloyBrowserHostImpl* frontend_browser,
                      content::WebContents* inspected_contents,
                      const CefPoint& inspect_element_at,
                      base::OnceClosure destroyed_callback);
  ~CefDevToolsFrontend() override;

  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;
  void HandleMessageFromDevToolsFrontend(base::Value::Dict message);

 private:
  // WebContentsObserver overrides
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainDocumentElementAvailable() override;
  void WebContentsDestroyed() override;

  void SendMessageAck(int request_id, base::Value::Dict arg);

  bool ProtocolLoggingEnabled() const;
  void LogProtocolMessage(ProtocolMessageType type,
                          const base::StringPiece& message);

  PrefService* GetPrefs() const;

  CefRefPtr<AlloyBrowserHostImpl> frontend_browser_;
  content::WebContents* inspected_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  CefPoint inspect_element_at_;
  base::OnceClosure frontend_destroyed_callback_;
  std::unique_ptr<content::DevToolsFrontendHost> frontend_host_;

  class NetworkResourceLoader;
  std::set<std::unique_ptr<NetworkResourceLoader>, base::UniquePtrComparator>
      loaders_;

  using ExtensionsAPIs = std::map<std::string, std::string>;
  ExtensionsAPIs extensions_api_;
  CefDevToolsFileManager file_manager_;

  const base::FilePath protocol_log_file_;

  base::WeakPtrFactory<CefDevToolsFrontend> weak_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_H_
