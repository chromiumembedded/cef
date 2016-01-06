// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/streams_private/streams_private_api.h"

#include <limits.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "cef/libcef/common/extensions/api/streams_private.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/stream_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "net/http/http_response_headers.h"

namespace extensions {
namespace {

void CreateResponseHeadersDictionary(const net::HttpResponseHeaders* headers,
                                     base::DictionaryValue* result) {
  if (!headers)
    return;

  void* iter = NULL;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    base::Value* existing_value = NULL;
    if (result->Get(header_name, &existing_value)) {
      base::StringValue* existing_string_value =
          static_cast<base::StringValue*>(existing_value);
      existing_string_value->GetString()->append(", ").append(header_value);
    } else {
      result->SetString(header_name, header_value);
    }
  }
}

}  // namespace

namespace streams_private = api::streams_private;

// static
StreamsPrivateAPI* StreamsPrivateAPI::Get(content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

StreamsPrivateAPI::StreamsPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

StreamsPrivateAPI::~StreamsPrivateAPI() {
}

void StreamsPrivateAPI::ExecuteMimeTypeHandler(
    const std::string& extension_id,
    int tab_id,
    scoped_ptr<content::StreamInfo> stream,
    const std::string& view_id,
    int64_t expected_content_size,
    bool embedded,
    int render_process_id,
    int render_frame_id) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  // If the mime handler uses MimeHandlerViewGuest, the MimeHandlerViewGuest
  // will take ownership of the stream. Otherwise, store the stream handle in
  // |streams_| and fire an event notifying the extension.
  if (handler->HasPlugin()) {
    GURL handler_url(Extension::GetBaseURLFromExtensionId(extension_id).spec() +
                     handler->handler_url());
    scoped_ptr<StreamContainer> stream_container(new StreamContainer(
        std::move(stream), tab_id, embedded, handler_url, extension_id));
    MimeHandlerStreamManager::Get(browser_context_)
        ->AddStream(view_id, std::move(stream_container), render_process_id,
                    render_frame_id);
    return;
  }
  // Create the event's arguments value.
  streams_private::StreamInfo info;
  info.mime_type = stream->mime_type;
  info.original_url = stream->original_url.spec();
  info.stream_url = stream->handle->GetURL().spec();
  info.tab_id = tab_id;
  info.embedded = embedded;

  if (!view_id.empty()) {
    info.view_id.reset(new std::string(view_id));
  }

  int size = -1;
  if (expected_content_size <= INT_MAX)
    size = expected_content_size;
  info.expected_content_size = size;

  CreateResponseHeadersDictionary(stream->response_headers.get(),
                                  &info.response_headers.additional_properties);

  scoped_ptr<Event> event(
      new Event(events::STREAMS_PRIVATE_ON_EXECUTE_MIME_TYPE_HANDLER,
                streams_private::OnExecuteMimeTypeHandler::kEventName,
                streams_private::OnExecuteMimeTypeHandler::Create(info)));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));

  GURL url = stream->handle->GetURL();
  streams_[extension_id][url] = make_linked_ptr(stream->handle.release());
}

void StreamsPrivateAPI::AbortStream(const std::string& extension_id,
                                    const GURL& stream_url,
                                    const base::Closure& callback) {
  StreamMap::iterator extension_it = streams_.find(extension_id);
  if (extension_it == streams_.end()) {
    callback.Run();
    return;
  }

  StreamMap::mapped_type* url_map = &extension_it->second;
  StreamMap::mapped_type::iterator url_it = url_map->find(stream_url);
  if (url_it == url_map->end()) {
    callback.Run();
    return;
  }

  url_it->second->AddCloseListener(callback);
  url_map->erase(url_it);
}

void StreamsPrivateAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  streams_.erase(extension->id());
}

StreamsPrivateAbortFunction::StreamsPrivateAbortFunction() {
}

ExtensionFunction::ResponseAction StreamsPrivateAbortFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &stream_url_));
  StreamsPrivateAPI::Get(browser_context())->AbortStream(
      extension_id(), GURL(stream_url_), base::Bind(
          &StreamsPrivateAbortFunction::OnClose, this));
  return RespondLater();
}

void StreamsPrivateAbortFunction::OnClose() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(NoArguments());
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<StreamsPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<StreamsPrivateAPI>*
StreamsPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
