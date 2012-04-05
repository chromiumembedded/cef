// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "libcef/browser_socket_stream_bridge.h"

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/socket_stream/socket_stream_job.h"
#include "net/websockets/websocket_job.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSocketStreamHandle.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"
#include "webkit/glue/websocketstreamhandle_delegate.h"

using webkit_glue::WebSocketStreamHandleBridge;

const int kNoSocketId = 0;

namespace {

MessageLoop* g_io_thread;
net::URLRequestContext* g_request_context;

class WebSocketStreamHandleBridgeImpl
    : public WebSocketStreamHandleBridge,
      public net::SocketStream::Delegate {
 public:
  WebSocketStreamHandleBridgeImpl(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate);

  // WebSocketStreamHandleBridge methods.
  virtual void Connect(const GURL& url);
  virtual bool Send(const std::vector<char>& data);
  virtual void Close();

  // net::SocketStream::Delegate methods.
  virtual void OnConnected(net::SocketStream* req,
                           int max_pending_send_allowed);
  virtual void OnSentData(net::SocketStream* req,
                          int amount_sent);
  virtual void OnReceivedData(net::SocketStream* req,
                              const char* data, int len);
  virtual void OnClose(net::SocketStream* req);

 private:
  virtual ~WebSocketStreamHandleBridgeImpl();

  // Runs on |g_io_thread|;
  void DoConnect(const GURL& url);
  void DoSend(std::vector<char>* data);
  void DoClose();

  // Runs on |message_loop_|;
  void DoOnConnected(int max_amount_send_allowed);
  void DoOnSentData(int amount_sent);
  void DoOnReceivedData(std::vector<char>* data);
  void DoOnClose();

  int socket_id_;
  MessageLoop* message_loop_;
  WebKit::WebSocketStreamHandle* handle_;
  webkit_glue::WebSocketStreamHandleDelegate* delegate_;

  scoped_refptr<net::SocketStreamJob> socket_;
  // Number of pending tasks to handle net::SocketStream::Delegate methods.
  base::subtle::Atomic32 num_pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamHandleBridgeImpl);
};

WebSocketStreamHandleBridgeImpl::WebSocketStreamHandleBridgeImpl(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate)
    : socket_id_(kNoSocketId),
      message_loop_(MessageLoop::current()),
      handle_(handle),
      delegate_(delegate),
      num_pending_tasks_(0) {
  net::WebSocketJob::EnsureInit();
}

WebSocketStreamHandleBridgeImpl::~WebSocketStreamHandleBridgeImpl() {
  DCHECK_EQ(socket_id_, kNoSocketId);
}

void WebSocketStreamHandleBridgeImpl::Connect(const GURL& url) {
  DCHECK(g_io_thread);
  AddRef();  // Released in DoOnClose().
  g_io_thread->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoConnect, this, url));
  if (delegate_)
    delegate_->WillOpenStream(handle_, url);
}

bool WebSocketStreamHandleBridgeImpl::Send(
    const std::vector<char>& data) {
  DCHECK(g_io_thread);
  g_io_thread->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoSend, this,
                 new std::vector<char>(data)));
  return true;
}

void WebSocketStreamHandleBridgeImpl::Close() {
  DCHECK(g_io_thread);
  g_io_thread->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoClose, this));
}

void WebSocketStreamHandleBridgeImpl::OnConnected(
    net::SocketStream* socket, int max_pending_send_allowed) {
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, 1);
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoOnConnected, this,
                 max_pending_send_allowed));
}

void WebSocketStreamHandleBridgeImpl::OnSentData(
    net::SocketStream* socket, int amount_sent) {
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, 1);
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoOnSentData, this,
                 amount_sent));
}

void WebSocketStreamHandleBridgeImpl::OnReceivedData(
    net::SocketStream* socket, const char* data, int len) {
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, 1);
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoOnReceivedData, this,
                 new std::vector<char>(data, data + len)));
}

void WebSocketStreamHandleBridgeImpl::OnClose(net::SocketStream* socket) {
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, 1);
  // Release socket_ on IO thread.
  socket_ = NULL;
  socket_id_ = kNoSocketId;
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebSocketStreamHandleBridgeImpl::DoOnClose, this));
}

void WebSocketStreamHandleBridgeImpl::DoConnect(const GURL& url) {
  DCHECK(MessageLoop::current() == g_io_thread);
  socket_ = net::SocketStreamJob::CreateSocketStreamJob(
      url, this, g_request_context->transport_security_state(),
      g_request_context->ssl_config_service());
  socket_->set_context(g_request_context);
  socket_->Connect();
}

void WebSocketStreamHandleBridgeImpl::DoSend(std::vector<char>* data) {
  DCHECK(MessageLoop::current() == g_io_thread);
  scoped_ptr<std::vector<char> > scoped_data(data);
  if (!socket_)
    return;
  if (!socket_->SendData(&(data->at(0)), data->size()))
    socket_->Close();
}

void WebSocketStreamHandleBridgeImpl::DoClose() {
  DCHECK(MessageLoop::current() == g_io_thread);
  if (!socket_)
    return;
  socket_->Close();
}

void WebSocketStreamHandleBridgeImpl::DoOnConnected(
    int max_pending_send_allowed) {
  DCHECK(MessageLoop::current() == message_loop_);
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, -1);
  if (delegate_)
    delegate_->DidOpenStream(handle_, max_pending_send_allowed);
}

void WebSocketStreamHandleBridgeImpl::DoOnSentData(int amount_sent) {
  DCHECK(MessageLoop::current() == message_loop_);
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, -1);
  if (delegate_)
    delegate_->DidSendData(handle_, amount_sent);
}

void WebSocketStreamHandleBridgeImpl::DoOnReceivedData(
    std::vector<char>* data) {
  DCHECK(MessageLoop::current() == message_loop_);
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, -1);
  scoped_ptr<std::vector<char> > scoped_data(data);
  if (delegate_)
    delegate_->DidReceiveData(handle_, &(data->at(0)), data->size());
}

void WebSocketStreamHandleBridgeImpl::DoOnClose() {
  DCHECK(MessageLoop::current() == message_loop_);
  base::subtle::NoBarrier_AtomicIncrement(&num_pending_tasks_, -1);
  // Don't handle OnClose if there are pending tasks.
  DCHECK_EQ(num_pending_tasks_, 0);
  DCHECK(!socket_);
  DCHECK_EQ(socket_id_, kNoSocketId);
  webkit_glue::WebSocketStreamHandleDelegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate)
    delegate->DidClose(handle_);
  Release();
}

}  // namespace

/* static */
void BrowserSocketStreamBridge::InitializeOnIOThread(
    net::URLRequestContext* request_context) {
  g_io_thread = MessageLoop::current();
  if ((g_request_context = request_context))
    g_request_context->AddRef();
}

void BrowserSocketStreamBridge::Cleanup() {
  g_io_thread = NULL;
  if (g_request_context)
    g_request_context->Release();
  g_request_context = NULL;
}

/* static */
webkit_glue::WebSocketStreamHandleBridge* BrowserSocketStreamBridge::Create(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  return new WebSocketStreamHandleBridgeImpl(handle, delegate);
}
