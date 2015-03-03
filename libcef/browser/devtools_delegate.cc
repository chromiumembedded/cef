// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/devtools_scheme_handler.h"
#include "libcef/common/content_client.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_agent_host.h"
#include "base/time/time.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "grit/cef_resources.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeOther[] = "other";

class TCPServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16 port, int backlog)
      : content::DevToolsHttpHandler::ServerSocketFactory(
            address, port, backlog) {}

 private:
  // content::DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> Create() const override {
    return scoped_ptr<net::ServerSocket>(
        new net::TCPServerSocket(NULL, net::NetLog::Source()));
  }

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory>
    CreateSocketFactory(uint16 port) {
  return scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory>(
      new TCPServerSocketFactory("127.0.0.1", port, 1));
}

class Target : public content::DevToolsTarget {
 public:
  explicit Target(scoped_refptr<content::DevToolsAgentHost> agent_host);

  std::string GetId() const override { return agent_host_->GetId(); }
  std::string GetParentId() const override { return std::string(); }
  std::string GetType() const override {
    switch (agent_host_->GetType()) {
      case content::DevToolsAgentHost::TYPE_WEB_CONTENTS:
        return kTargetTypePage;
      case content::DevToolsAgentHost::TYPE_SERVICE_WORKER:
        return kTargetTypeServiceWorker;
      default:
        break;
    }
    return kTargetTypeOther;
  }
  std::string GetTitle() const override {
    return agent_host_->GetTitle();
  }
  std::string GetDescription() const override { return std::string(); }
  GURL GetURL() const override { return agent_host_->GetURL(); }
  GURL GetFaviconURL() const override { return favicon_url_; }
  base::TimeTicks GetLastActivityTime() const override {
    return last_activity_time_;
  }
  bool IsAttached() const override {
    return agent_host_->IsAttached();
  }
  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const
      override {
    return agent_host_;
  }
  bool Activate() const override;
  bool Close() const override;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

Target::Target(scoped_refptr<content::DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
  if (content::WebContents* web_contents = agent_host_->GetWebContents()) {
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url_ = entry->GetFavicon().url;
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

bool Target::Activate() const {
  return agent_host_->Activate();
}

bool Target::Close() const {
  return agent_host_->Close();
}

}  // namespace

// CefDevToolsDelegate

CefDevToolsDelegate::CefDevToolsDelegate(uint16 port) {
  devtools_http_handler_.reset(content::DevToolsHttpHandler::Start(
      CreateSocketFactory(port),
      std::string(),
      this,
      base::FilePath()));
}

CefDevToolsDelegate::~CefDevToolsDelegate() {
  DCHECK(!devtools_http_handler_.get());
}

void CefDevToolsDelegate::Stop() {
  // Release the reference before deleting the handler. Deleting the handler
  // will delete |this| and no members of |this| should be accessed after that
  // call.
  content::DevToolsHttpHandler* handler = devtools_http_handler_.release();
  delete handler;
}

std::string CefDevToolsDelegate::GetDiscoveryPageHTML() {
  return CefContentClient::Get()->GetDataResource(
      IDR_CEF_DEVTOOLS_DISCOVERY_PAGE, ui::SCALE_FACTOR_NONE).as_string();
}

bool CefDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

base::FilePath CefDevToolsDelegate::GetDebugFrontendDir() {
  return base::FilePath();
}

scoped_ptr<net::ServerSocket>
    CefDevToolsDelegate::CreateSocketForTethering(std::string* name) {
  return scoped_ptr<net::ServerSocket>();
}

std::string CefDevToolsDelegate::GetChromeDevToolsURL() {
  return base::StringPrintf("%s://%s/devtools.html",
      content::kChromeDevToolsScheme, scheme::kChromeDevToolsHost);
}

// CefDevToolsManagerDelegate

CefDevToolsManagerDelegate::CefDevToolsManagerDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

CefDevToolsManagerDelegate::~CefDevToolsManagerDelegate() {
}

base::DictionaryValue* CefDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  return NULL;
}

std::string CefDevToolsManagerDelegate::GetPageThumbnailData(
    const GURL& url) {
  return std::string();
}

scoped_ptr<content::DevToolsTarget>
CefDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  return scoped_ptr<content::DevToolsTarget>();
}

void CefDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  content::DevToolsAgentHost::List agents =
      content::DevToolsAgentHost::GetOrCreateAll();
  for (content::DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    targets.push_back(new Target(*it));
  }
  callback.Run(targets);
}
