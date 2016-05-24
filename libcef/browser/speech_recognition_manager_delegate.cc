// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/speech_recognition_manager_delegate.h"

#include <set>
#include <string>

#include "libcef/common/cef_switches.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

// Simple utility to get notified when a WebContents is closed or crashes.
// Both the callback site and the callback thread are passed by the caller in
// the constructor. There is no restriction on the constructor, however this
// class must be destroyed on the UI thread, due to the NotificationRegistrar
// dependency.
class CefSpeechRecognitionManagerDelegate::WebContentsWatcher
    : public base::RefCountedThreadSafe<WebContentsWatcher>,
      public content::NotificationObserver {
 public:
  typedef base::Callback<void(int render_process_id, int render_view_id)>
      WebContentsClosedCallback;

  WebContentsWatcher(WebContentsClosedCallback web_contents_closed_callback,
             BrowserThread::ID callback_thread)
      : web_contents_closed_callback_(web_contents_closed_callback),
        callback_thread_(callback_thread) {
  }

  // Starts monitoring the WebContents corresponding to the given
  // |render_process_id|, |render_view_id| pair, invoking
  // |web_contents_closed_callback_| if closed/unloaded.
  void Watch(int render_process_id, int render_view_id) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
          &WebContentsWatcher::Watch, this, render_process_id, render_view_id));
      return;
    }

    WebContents* web_contents = NULL;
    content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
    if (render_view_host)
      web_contents = WebContents::FromRenderViewHost(render_view_host);
    DCHECK(web_contents);

    // Avoid multiple registrations on |registrar_| for the same |web_contents|.
    if (registered_web_contents_.find(web_contents) !=
        registered_web_contents_.end()) {
      return;
    }
    registered_web_contents_.insert(web_contents);

    // Lazy initialize the registrar.
    if (!registrar_.get())
      registrar_.reset(new content::NotificationRegistrar());

    registrar_->Add(this,
                    content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<WebContents>(web_contents));
  }

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED, type);

    WebContents* web_contents = content::Source<WebContents>(source).ptr();
    int render_process_id = web_contents->GetRenderProcessHost()->GetID();
    int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();

    registrar_->Remove(this,
                       content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                       content::Source<WebContents>(web_contents));
    registered_web_contents_.erase(web_contents);

    BrowserThread::PostTask(callback_thread_, FROM_HERE, base::Bind(
        web_contents_closed_callback_, render_process_id, render_view_id));
  }

 private:
  friend class base::RefCountedThreadSafe<WebContentsWatcher>;

  ~WebContentsWatcher() override {
    // Must be destroyed on the UI thread due to |registrar_| non thread-safety.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  // Lazy-initialized and used on the UI thread to handle web contents
  // notifications (tab closing).
  std::unique_ptr<content::NotificationRegistrar> registrar_;

  // Keeps track of which WebContent(s) have been registered, in order to avoid
  // double registrations on |registrar_|
  std::set<content::WebContents*> registered_web_contents_;

  // Callback used to notify, on the thread specified by |callback_thread_| the
  // closure of a registered tab.
  WebContentsClosedCallback web_contents_closed_callback_;
  content::BrowserThread::ID callback_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsWatcher);
};

CefSpeechRecognitionManagerDelegate
::CefSpeechRecognitionManagerDelegate() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  filter_profanities_ =
      command_line->HasSwitch(switches::kEnableProfanityFilter);
}

CefSpeechRecognitionManagerDelegate
::~CefSpeechRecognitionManagerDelegate() {
}

void CefSpeechRecognitionManagerDelegate::WebContentsClosedCallback(
    int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  // |manager| becomes NULL if a browser shutdown happens between the post of
  // this task (from the UI thread) and this call (on the IO thread). In this
  // case we just return.
  if (!manager)
    return;

  manager->AbortAllSessionsForRenderView(render_process_id, render_view_id);
}

void CefSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {
  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Register callback to auto abort session on tab closure.
  // |web_contents_watcher_| is lazyly istantiated on the first call.
  if (!web_contents_watcher_.get()) {
    web_contents_watcher_ = new WebContentsWatcher(
        base::Bind(
            &CefSpeechRecognitionManagerDelegate::WebContentsClosedCallback,
            base::Unretained(this)),
        BrowserThread::IO);
  }
  web_contents_watcher_->Watch(context.render_process_id,
                               context.render_view_id);
}

void CefSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
}

void CefSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {
}

void CefSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void CefSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void CefSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
}

void CefSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id, const content::SpeechRecognitionResults& result) {
}

void CefSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
}

void CefSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
}

void CefSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
}

void CefSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::Callback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators properly set the |render_process_id| field.
  DCHECK_NE(context.render_process_id, 0);

  callback.Run(false, true);
}

content::SpeechRecognitionEventListener*
CefSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool CefSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  return filter_profanities_;
}
