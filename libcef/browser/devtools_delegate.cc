// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/devtools_delegate.h"

#include <algorithm>
#include <string>

#include "libcef/browser/net/devtools_scheme_handler.h"
#include "libcef/common/content_client.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_agent_host.h"
#include "base/time/time.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "grit/cef_resources.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const int kBackLog = 10;

class TCPServerSocketFactory
    : public devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {
  }

 private:
  // DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

scoped_ptr<devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory>
    CreateSocketFactory(uint16_t port) {
  return scoped_ptr<
      devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory>(
          new TCPServerSocketFactory("127.0.0.1", port));
}

}  // namespace

// CefDevToolsDelegate

CefDevToolsDelegate::CefDevToolsDelegate(uint16_t port) {
  devtools_http_handler_.reset(new devtools_http_handler::DevToolsHttpHandler(
      CreateSocketFactory(port),
      std::string(),
      this,
      base::FilePath(),
      base::FilePath(),
      std::string(),
      CefContentClient::Get()->GetUserAgent()));
}

CefDevToolsDelegate::~CefDevToolsDelegate() {
  DCHECK(!devtools_http_handler_.get());
}

void CefDevToolsDelegate::Stop() {
  // Release the reference before deleting the handler. Deleting the handler
  // will delete |this| and no members of |this| should be accessed after that
  // call.
  devtools_http_handler::DevToolsHttpHandler* handler =
      devtools_http_handler_.release();
  delete handler;
}

std::string CefDevToolsDelegate::GetDiscoveryPageHTML() {
  return CefContentClient::Get()->GetDataResource(
      IDR_CEF_DEVTOOLS_DISCOVERY_PAGE, ui::SCALE_FACTOR_NONE).as_string();
}

std::string CefDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return std::string();
}

std::string CefDevToolsDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

content::DevToolsExternalAgentProxyDelegate*
CefDevToolsDelegate::HandleWebSocketConnection(const std::string& path) {
  return nullptr;
}

std::string CefDevToolsDelegate::GetChromeDevToolsURL() {
  return base::StringPrintf("%s://%s/inspector.html",
      content::kChromeDevToolsScheme, scheme::kChromeDevToolsHost);
}

// CefDevToolsManagerDelegate

CefDevToolsManagerDelegate::CefDevToolsManagerDelegate() {
}

CefDevToolsManagerDelegate::~CefDevToolsManagerDelegate() {
}

base::DictionaryValue* CefDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  return NULL;
}
