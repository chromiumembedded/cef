// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/speech_recognition_manager_delegate.h"

#include <set>
#include <string>

#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
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

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

CefSpeechRecognitionManagerDelegate ::CefSpeechRecognitionManagerDelegate() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  filter_profanities_ =
      command_line->HasSwitch(switches::kEnableProfanityFilter);
}

CefSpeechRecognitionManagerDelegate ::~CefSpeechRecognitionManagerDelegate() =
    default;

void CefSpeechRecognitionManagerDelegate::OnRecognitionStart(int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {}

void CefSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result) {}

void CefSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {}

void CefSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id,
    float volume,
    float noise_volume) {}

void CefSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {}

void CefSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators properly set the |render_process_id| field.
  DCHECK_NE(context.render_process_id, 0);

  CEF_POST_TASK(CEF_IOT, base::BindOnce(std::move(callback), false, true));
}

content::SpeechRecognitionEventListener*
CefSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool CefSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  return filter_profanities_;
}
