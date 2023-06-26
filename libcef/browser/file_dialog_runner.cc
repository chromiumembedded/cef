// Copyright (c) 2022 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/file_dialog_runner.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/extensions/browser_extensions_util.h"

#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

using blink::mojom::FileChooserParams;

namespace {

// Creation of a file dialog can be triggered via various code paths, but they
// all eventually result in a call to ui::SelectFileDialog::Create. We intercept
// that call with CefSelectFileDialogFactory and redirect it to
// CefFileDialogManager::RunSelectFile. After triggering the CefDialogHandler
// callbacks that method calls ui::SelectFileDialog::Create again with
// |run_from_cef=false| to trigger creation of the default platform dialog.
class CefSelectFileDialogFactory final : public ui::SelectFileDialogFactory {
 public:
  CefSelectFileDialogFactory() = default;

  CefSelectFileDialogFactory(const CefSelectFileDialogFactory&) = delete;
  CefSelectFileDialogFactory& operator=(const CefSelectFileDialogFactory&) =
      delete;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

  bool IsCefFactory() const override { return true; }
};

// Delegates the running of the dialog to CefFileDialogManager.
class CefSelectFileDialog final : public ui::SelectFileDialog {
 public:
  CefSelectFileDialog(ui::SelectFileDialog::Listener* listener,
                      std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {
    DCHECK(listener_);
  }

  CefSelectFileDialog(const CefSelectFileDialog&) = delete;
  CefSelectFileDialog& operator=(const CefSelectFileDialog&) = delete;

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    // Try to determine the associated browser (with decreasing levels of
    // confidence).
    // 1. Browser associated with the SelectFilePolicy. This is the most
    // reliable mechanism if specified at the SelectFileDialog::Create call
    // site.
    if (select_file_policy_) {
      auto chrome_policy =
          static_cast<ChromeSelectFilePolicy*>(select_file_policy_.get());
      auto web_contents = chrome_policy->source_contents();
      if (web_contents) {
        browser_ = extensions::GetOwnerBrowserForHost(
            web_contents->GetRenderViewHost(), nullptr);
      }
      if (!browser_) {
        LOG(WARNING) << "No browser associated with SelectFilePolicy";
      }
    }

    // 2. Browser associated with the top-level native window (owning_window).
    // This should be reliable with windowed browsers. However, |owning_window|
    // will always be nullptr with windowless browsers.
    if (!browser_ && owning_window) {
      browser_ =
          CefBrowserHostBase::GetBrowserForTopLevelNativeWindow(owning_window);
      if (!browser_) {
        LOG(WARNING) << "No browser associated with top-level native window";
      }
    }

    // 3. Browser most likely to be focused. This may be somewhat iffy with
    // windowless browsers as there is no guarantee that the client has only
    // one browser focused at a time.
    if (!browser_) {
      browser_ = CefBrowserHostBase::GetLikelyFocusedBrowser();
      if (!browser_) {
        LOG(WARNING) << "No likely focused browser";
      }
    }

    if (!browser_) {
      LOG(ERROR)
          << "Failed to identify associated browser; canceling the file dialog";
      listener_->FileSelectionCanceled(params);
      return;
    }

    owning_window_ = owning_window;
    has_multiple_file_choices_ =
        file_types ? file_types->extensions.size() > 1 : true;

    browser_->RunSelectFile(listener_, std::move(select_file_policy_), type,
                            title, default_path, file_types, file_type_index,
                            default_extension, owning_window, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return owning_window == owning_window_;
  }

  void ListenerDestroyed() override {
    if (browser_) {
      browser_->SelectFileListenerDestroyed(listener_);
    }
    listener_ = nullptr;
  }

  bool HasMultipleFileTypeChoicesImpl() override {
    return has_multiple_file_choices_;
  }

 private:
  gfx::NativeWindow owning_window_ = nullptr;
  bool has_multiple_file_choices_ = false;

  CefRefPtr<CefBrowserHostBase> browser_;
};

ui::SelectFileDialog* CefSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new CefSelectFileDialog(listener, std::move(policy));
}

}  // namespace

namespace file_dialog_runner {

void RegisterFactory() {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<CefSelectFileDialogFactory>());
}

}  // namespace file_dialog_runner
