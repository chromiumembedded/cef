// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_CONTROLLER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host_client.h"

namespace content {
class WebContents;
}

class CefDevToolsController : public content::DevToolsAgentHostClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // See CefDevToolsMessageObserver documentation.
    virtual bool OnDevToolsMessage(const std::string_view& message) = 0;
    virtual void OnDevToolsMethodResult(int message_id,
                                        bool success,
                                        const std::string_view& result) = 0;
    virtual void OnDevToolsEvent(const std::string_view& method,
                                 const std::string_view& params) = 0;
    virtual void OnDevToolsAgentAttached() = 0;
    virtual void OnDevToolsAgentDetached() = 0;

    virtual void OnDevToolsControllerDestroyed() = 0;

   protected:
    ~Observer() override = default;
  };

  // |inspected_contents| will outlive this object.
  explicit CefDevToolsController(content::WebContents* inspected_contents);

  CefDevToolsController(const CefDevToolsController&) = delete;
  CefDevToolsController& operator=(const CefDevToolsController&) = delete;

  ~CefDevToolsController() override;

  // See CefBrowserHost methods of the same name for documentation.
  bool SendDevToolsMessage(const std::string_view& message);
  int ExecuteDevToolsMethod(int message_id,
                            const std::string& method,
                            const base::Value::Dict* params);

  // |observer| must outlive this object or be removed.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::WeakPtr<CefDevToolsController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // content::DevToolsAgentHostClient implementation:
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override;
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override;

  bool EnsureAgentHost();

  const raw_ptr<content::WebContents> inspected_contents_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  int next_message_id_ = 1;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<CefDevToolsController> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_CONTROLLER_H_
