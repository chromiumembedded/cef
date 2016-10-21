// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/devtools_manager_delegate.h"

#include <stdint.h>

#include <vector>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/common/content_client.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cef/grit/cef_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const int kBackLog = 10;

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // See if the user specified a port on the command line. Specifying 0 would
  // result in the selection of an ephemeral port but that doesn't make sense
  // for CEF where the URL is otherwise undiscoverable. Also, don't allow
  // binding of ports between 0 and 1024 exclusive because they're normally
  // restricted to root on Posix-based systems.
  uint16_t port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) &&
        temp_port >= 1024 && temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  if (port == 0)
    return nullptr;
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new TCPServerSocketFactory("127.0.0.1", port));
}

} //  namespace

// CefDevToolsManagerDelegate ----------------------------------------------

// static
void CefDevToolsManagerDelegate::StartHttpHandler(
    content::BrowserContext* browser_context) {
  std::unique_ptr<content::DevToolsSocketFactory> socket_factory =
      CreateSocketFactory();
  if (!socket_factory)
    return;
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(socket_factory),
      std::string(),
      browser_context->GetPath(),
      base::FilePath(),
      std::string(),
      CefContentClient::Get()->GetUserAgent());
}

// static
void CefDevToolsManagerDelegate::StopHttpHandler() {
  // This is a no-op if the server was never started.
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

CefDevToolsManagerDelegate::CefDevToolsManagerDelegate() {
}

CefDevToolsManagerDelegate::~CefDevToolsManagerDelegate() {
}

scoped_refptr<content::DevToolsAgentHost>
CefDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  // This is reached when the user selects "Open link in new tab" from the
  // DevTools interface.
  // TODO(cef): Consider exposing new API to support this.
  return nullptr;
}

std::string CefDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CEF_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

std::string CefDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}
