// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/devtools/devtools_controller.h"

#include "libcef/browser/devtools/devtools_util.h"
#include "libcef/browser/thread_util.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/public/browser/devtools_agent_host.h"

CefDevToolsController::CefDevToolsController(
    content::WebContents* inspected_contents)
    : inspected_contents_(inspected_contents), weak_ptr_factory_(this) {
  DCHECK(inspected_contents_);
}

CefDevToolsController::~CefDevToolsController() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    AgentHostClosed(agent_host_.get());
  }

  for (auto& observer : observers_) {
    observer.OnDevToolsControllerDestroyed();
  }
}

bool CefDevToolsController::SendDevToolsMessage(
    const base::StringPiece& message) {
  CEF_REQUIRE_UIT();
  if (!EnsureAgentHost()) {
    return false;
  }

  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(message)));
  return true;
}

int CefDevToolsController::ExecuteDevToolsMethod(
    int suggested_message_id,
    const std::string& method,
    const base::Value::Dict* params) {
  CEF_REQUIRE_UIT();
  if (!EnsureAgentHost()) {
    return 0;
  }

  // Message IDs must always be increasing and unique.
  int message_id = suggested_message_id;
  if (message_id < next_message_id_) {
    message_id = next_message_id_++;
  } else {
    next_message_id_ = message_id + 1;
  }

  base::Value::Dict message;
  message.Set("id", message_id);
  message.Set("method", method);
  if (params) {
    message.Set("params", params->Clone());
  }

  std::string protocol_message;
  if (!base::JSONWriter::Write(message, &protocol_message)) {
    return 0;
  }

  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(protocol_message)));
  return message_id;
}

void CefDevToolsController::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(agent_host == agent_host_.get());
  agent_host_ = nullptr;
  for (auto& observer : observers_) {
    observer.OnDevToolsAgentDetached();
  }
}

void CefDevToolsController::AddObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.AddObserver(observer);
}

void CefDevToolsController::RemoveObserver(Observer* observer) {
  CEF_REQUIRE_UIT();
  observers_.RemoveObserver(observer);
}

void CefDevToolsController::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  if (observers_.empty()) {
    return;
  }

  base::StringPiece str_message(reinterpret_cast<const char*>(message.data()),
                                message.size());
  if (!devtools_util::ProtocolParser::IsValidMessage(str_message)) {
    LOG(WARNING) << "Invalid message: " << str_message.substr(0, 100);
    return;
  }

  devtools_util::ProtocolParser parser;

  for (auto& observer : observers_) {
    if (observer.OnDevToolsMessage(str_message)) {
      continue;
    }

    // Only perform parsing a single time.
    if (parser.Initialize(str_message) && parser.IsFailure()) {
      LOG(WARNING) << "Failed to parse message: " << str_message.substr(0, 100);
    }

    if (parser.IsEvent()) {
      observer.OnDevToolsEvent(parser.method_, parser.params_);
    } else if (parser.IsResult()) {
      observer.OnDevToolsMethodResult(parser.message_id_, parser.success_,
                                      parser.params_);
    }
  }
}

bool CefDevToolsController::EnsureAgentHost() {
  if (!agent_host_) {
    agent_host_ =
        content::DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
    if (agent_host_) {
      agent_host_->AttachClient(this);
      for (auto& observer : observers_) {
        observer.OnDevToolsAgentAttached();
      }
    }
  }
  return !!agent_host_;
}
