// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "net/base/escape.h"
#include "net/socket/tcp_listen_socket.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kTargetTypePage[] = "page";

class Target : public content::DevToolsTarget {
 public:
  explicit Target(content::WebContents* web_contents);

  virtual std::string GetId() const OVERRIDE { return id_; }
  virtual std::string GetType() const OVERRIDE { return kTargetTypePage; }
  virtual std::string GetTitle() const OVERRIDE { return title_; }
  virtual std::string GetDescription() const OVERRIDE { return std::string(); }
  virtual GURL GetUrl() const OVERRIDE { return url_; }
  virtual GURL GetFaviconUrl() const OVERRIDE { return favicon_url_; }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const
      OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string title_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

Target::Target(content::WebContents* web_contents) {
  agent_host_ =
      content::DevToolsAgentHost::GetOrCreateFor(
          web_contents->GetRenderViewHost());
  id_ = agent_host_->GetId();
  title_ = UTF16ToUTF8(net::EscapeForHTML(web_contents->GetTitle()));
  url_ = web_contents->GetURL();
  content::NavigationController& controller = web_contents->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  if (entry != NULL && entry->GetURL().is_valid())
    favicon_url_ = entry->GetFavicon().url;
  last_activity_time_ = web_contents->GetLastSelectedTime();
}

bool Target::Activate() const {
  content::RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return false;
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return false;
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return true;
}

bool Target::Close() const {
  content::RenderViewHost* rvh = agent_host_->GetRenderViewHost();
  if (!rvh)
    return false;
  rvh->ClosePage();
  return true;
}

}  // namespace

// CefDevToolsDelegate

CefDevToolsDelegate::CefDevToolsDelegate(int port) {
  devtools_http_handler_ = content::DevToolsHttpHandler::Start(
      new net::TCPListenSocketFactory("127.0.0.1", port),
      "",
      this);
}

CefDevToolsDelegate::~CefDevToolsDelegate() {
}

void CefDevToolsDelegate::Stop() {
  // The call below destroys this.
  devtools_http_handler_->Stop();
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

std::string CefDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return std::string();
}

scoped_ptr<content::DevToolsTarget> CefDevToolsDelegate::CreateNewTarget(
    const GURL& url) {
   return scoped_ptr<content::DevToolsTarget>();
}

scoped_ptr<content::DevToolsTarget> CefDevToolsDelegate::CreateTargetForId(
    const std::string& id) {
  scoped_ptr<content::DevToolsTarget> target;

  std::vector<content::RenderViewHost*> rvh_list =
      content::DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<content::RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    scoped_refptr<content::DevToolsAgentHost> agent_host(
        content::DevToolsAgentHost::GetOrCreateFor(*it));
    if (agent_host->GetId() == id) {
      content::WebContents* web_contents =
          content::WebContents::FromRenderViewHost(*it);
      target.reset(new Target(web_contents));
      break;
    }
  }

  return target.Pass();
}

void CefDevToolsDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  std::vector<content::RenderViewHost*> rvh_list =
      content::DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<content::RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(*it);
    if (web_contents)
      targets.push_back(new Target(web_contents));
  }
  callback.Run(targets);
}

scoped_ptr<net::StreamListenSocket>
    CefDevToolsDelegate::CreateSocketForTethering(
        net::StreamListenSocket::Delegate* delegate,
        std::string* name) {
  return scoped_ptr<net::StreamListenSocket>();
}

std::string CefDevToolsDelegate::GetDevToolsURL(content::RenderViewHost* rvh,
                                                bool http_scheme) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string port_str =
      command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
  DCHECK(!port_str.empty());
  int port;
  if (!base::StringToInt(port_str, &port))
    return std::string();

  scoped_refptr<content::DevToolsAgentHost> agent_host(
      content::DevToolsAgentHost::GetOrCreateFor(rvh));

  const std::string& page_id = agent_host->GetId();
  const std::string& host = http_scheme ?
      base::StringPrintf("http://localhost:%d/devtools/", port) :
      base::StringPrintf("%s://%s/devtools/", chrome::kChromeDevToolsScheme,
                         scheme::kChromeDevToolsHost);

  return base::StringPrintf(
      "%sdevtools.html?ws=localhost:%d/devtools/page/%s",
      host.c_str(),
      port,
      page_id.c_str());
}
