// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"


// This is CEF's implementation of the SpeechRecognitionManagerDelegate
// interface. Based on chrome/browser/speech/
// chrome_speech_recognition_manager_delegate.[cc|h]
class CefSpeechRecognitionManagerDelegate
    : NON_EXPORTED_BASE(public content::SpeechRecognitionManagerDelegate),
      public content::SpeechRecognitionEventListener {
 public:
  CefSpeechRecognitionManagerDelegate();
  ~CefSpeechRecognitionManagerDelegate() override;

 protected:
  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnEnvironmentEstimationComplete(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id, const content::SpeechRecognitionResults& result) override;
  void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id, float volume,
                           float noise_volume) override;

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::Callback<void(bool ask_user, bool is_allowed)> callback) override;
  content::SpeechRecognitionEventListener* GetEventListener() override;
  bool FilterProfanities(int render_process_id) override;

 private:
  class WebContentsWatcher;

  // Callback called by |web_contents_watcher_| on the IO thread to signal
  // web contents closure.
  void WebContentsClosedCallback(int render_process_id, int render_view_id);

  scoped_refptr<WebContentsWatcher> web_contents_watcher_;
  bool filter_profanities_;

  DISALLOW_COPY_AND_ASSIGN(CefSpeechRecognitionManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
