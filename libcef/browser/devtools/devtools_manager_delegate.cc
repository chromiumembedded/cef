// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/devtools/devtools_manager_delegate.h"

#include <stdint.h>

#include <vector>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
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

  TCPServerSocketFactory(const TCPServerSocketFactory&) = delete;
  TCPServerSocketFactory& operator=(const TCPServerSocketFactory&) = delete;

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) !=
        net::OK) {
      return std::unique_ptr<net::ServerSocket>();
    }
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;
};

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // See if the user specified a port on the command line. Specifying 0 will
  // result in the selection of an ephemeral port and the port number will be
  // printed as part of the WebSocket endpoint URL to stderr. If a cache
  // directory path is provided the port will also be written to the
  // <cache-dir>/DevToolsActivePort file.
  //
  // It's not allowed to bind ports between 0 and 1024 exclusive because
  // they're normally restricted to root on Posix-based systems.
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int port = 0;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);

    if (base::StringToInt(port_str, &port) &&
        (0 == port || (port >= 1024 && port < 65535))) {
      return std::unique_ptr<content::DevToolsSocketFactory>(
          new TCPServerSocketFactory("127.0.0.1", port));
    } else {
      DLOG(WARNING) << "Invalid http debugger port number '" << port_str << "'";
    }
  }

  return nullptr;
}

}  //  namespace

// CefDevToolsManagerDelegate ----------------------------------------------

// static
void CefDevToolsManagerDelegate::StartHttpHandler(
    content::BrowserContext* browser_context) {
  std::unique_ptr<content::DevToolsSocketFactory> socket_factory =
      CreateSocketFactory();
  if (!socket_factory) {
    return;
  }
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(socket_factory), browser_context->GetPath(), base::FilePath());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler(
        base::OnceClosure());
  }
}

// static
void CefDevToolsManagerDelegate::StopHttpHandler() {
  // This is a no-op if the server was never started.
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

CefDevToolsManagerDelegate::CefDevToolsManagerDelegate() = default;

CefDevToolsManagerDelegate::~CefDevToolsManagerDelegate() = default;

scoped_refptr<content::DevToolsAgentHost>
CefDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    content::DevToolsManagerDelegate::TargetType target_type) {
  // This is reached when the user selects "Open link in new tab" from the
  // DevTools interface.
  // TODO(cef): Consider exposing new API to support this.
  return nullptr;
}

std::string CefDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_CEF_DEVTOOLS_DISCOVERY_PAGE);
}

bool CefDevToolsManagerDelegate::HasBundledFrontendResources() {
  return true;
}
