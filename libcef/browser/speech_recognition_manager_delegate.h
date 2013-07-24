// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
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
  virtual ~CefSpeechRecognitionManagerDelegate();

 protected:
  // SpeechRecognitionEventListener methods.
  virtual void OnRecognitionStart(int session_id) OVERRIDE;
  virtual void OnAudioStart(int session_id) OVERRIDE;
  virtual void OnEnvironmentEstimationComplete(int session_id) OVERRIDE;
  virtual void OnSoundStart(int session_id) OVERRIDE;
  virtual void OnSoundEnd(int session_id) OVERRIDE;
  virtual void OnAudioEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionEnd(int session_id) OVERRIDE;
  virtual void OnRecognitionResults(
      int session_id, const content::SpeechRecognitionResults& result) OVERRIDE;
  virtual void OnRecognitionError(
      int session_id, const content::SpeechRecognitionError& error) OVERRIDE;
  virtual void OnAudioLevelsChange(int session_id, float volume,
                                   float noise_volume) OVERRIDE;

  // SpeechRecognitionManagerDelegate methods.
  virtual void GetDiagnosticInformation(bool* can_report_metrics,
                                        std::string* hardware_info) OVERRIDE;
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::Callback<void(bool ask_user, bool is_allowed)> callback) OVERRIDE;
  virtual content::SpeechRecognitionEventListener* GetEventListener() OVERRIDE;
  virtual bool FilterProfanities(int render_process_id) OVERRIDE;

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
