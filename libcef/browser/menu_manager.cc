// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_manager.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context_menu_params_impl.h"
#include "libcef/browser/menu_runner.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/content_client.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "cef/grit/cef_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"

namespace {

CefString GetLabel(int message_id) {
  base::string16 label =
      CefContentClient::Get()->GetLocalizedString(message_id);
  DCHECK(!label.empty());
  return label;
}

const int kInvalidCommandId = -1;
const cef_event_flags_t kEmptyEventFlags = static_cast<cef_event_flags_t>(0);

class CefRunContextMenuCallbackImpl : public CefRunContextMenuCallback {
 public:
  typedef base::Callback<void(int,cef_event_flags_t)> Callback;

  explicit CefRunContextMenuCallbackImpl(const Callback& callback)
      : callback_(callback) {
  }

  ~CefRunContextMenuCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(callback_, kInvalidCommandId, kEmptyEventFlags);
      } else {
        CEF_POST_TASK(CEF_UIT,
            base::Bind(&CefRunContextMenuCallbackImpl::RunNow, callback_,
                       kInvalidCommandId, kEmptyEventFlags));
      }
    }
  }

  void Continue(int command_id, cef_event_flags_t event_flags) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(callback_, command_id, event_flags);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
          base::Bind(&CefRunContextMenuCallbackImpl::Continue, this,
                     command_id, event_flags));
    }
  }

  void Cancel() override {
    Continue(kInvalidCommandId, kEmptyEventFlags);
  }

  void Disconnect() {
    callback_.Reset();
  }

 private:
  static void RunNow(const Callback& callback,
                     int command_id,
                     cef_event_flags_t event_flags) {
    CEF_REQUIRE_UIT();
    callback.Run(command_id, event_flags);
  }

  Callback callback_;

  IMPLEMENT_REFCOUNTING(CefRunContextMenuCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRunContextMenuCallbackImpl);
};

}  // namespace

CefMenuManager::CefMenuManager(CefBrowserHostImpl* browser,
                               std::unique_ptr<CefMenuRunner> runner)
  : content::WebContentsObserver(browser->web_contents()),
    browser_(browser),
    runner_(std::move(runner)),
    custom_menu_callback_(NULL),
    weak_ptr_factory_(this) {
  DCHECK(web_contents());
  DCHECK(runner_.get());
  model_ = new CefMenuModelImpl(this, nullptr, false);
}

CefMenuManager::~CefMenuManager() {
  // The model may outlive the delegate if the context menu is visible when the
  // application is closed.
  model_->set_delegate(NULL);
}

void CefMenuManager::Destroy() {
  CancelContextMenu();
  runner_.reset(NULL);
}

bool CefMenuManager::IsShowingContextMenu() {
  if (!web_contents())
    return false;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  return (view && view->IsShowingContextMenu());
}

bool CefMenuManager::CreateContextMenu(
    const content::ContextMenuParams& params) {
  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |HandleContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  if (IsShowingContextMenu())
    return true;

  params_ = params;
  model_->Clear();

  // Create the default menu model.
  CreateDefaultModel();

  bool custom_menu = false;
  DCHECK(!custom_menu_callback_);

  // Give the client a chance to modify the model.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler =
        client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));
      CefRefPtr<CefFrame> frame = browser_->GetFocusedFrame();

      handler->OnBeforeContextMenu(browser_,
                                    frame,
                                    paramsPtr.get(),
                                    model_.get());

      MenuWillShow(model_);

      if (model_->GetCount() > 0) {
        CefRefPtr<CefRunContextMenuCallbackImpl> callbackImpl(
            new CefRunContextMenuCallbackImpl(
                base::Bind(&CefMenuManager::ExecuteCommandCallback,
                            weak_ptr_factory_.GetWeakPtr())));

        // This reference will be cleared when the callback is executed or
        // the callback object is deleted.
        custom_menu_callback_ = callbackImpl.get();

        if (handler->RunContextMenu(browser_,
                                    frame,
                                    paramsPtr.get(),
                                    model_.get(),
                                    callbackImpl.get())) {
          custom_menu = true;
        } else {
          // Callback should not be executed if the handler returns false.
          DCHECK(custom_menu_callback_);
          custom_menu_callback_ = NULL;
          callbackImpl->Disconnect();
        }
      }

      // Do not keep references to the parameters in the callback.
      paramsPtr->Detach(NULL);
      DCHECK(paramsPtr->HasOneRef());
      DCHECK(model_->VerifyRefCount());

      // Menu is empty so notify the client and return.
      if (model_->GetCount() == 0 && !custom_menu) {
        MenuClosed(model_);
        return true;
      }
    }
  }

  if (custom_menu)
    return true;
  return runner_->RunContextMenu(browser_, model_.get(), params_);
}

void CefMenuManager::CancelContextMenu() {
  if (IsShowingContextMenu()) {
    if (custom_menu_callback_)
      custom_menu_callback_->Cancel();
    else
      runner_->CancelContextMenu();
  }
}

void CefMenuManager::ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                                    int command_id,
                                    cef_event_flags_t event_flags) {
  // Give the client a chance to handle the command.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler =
        client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));

      bool handled = handler->OnContextMenuCommand(
          browser_,
          browser_->GetFocusedFrame(),
          paramsPtr.get(),
          command_id,
          event_flags);

      // Do not keep references to the parameters in the callback.
      paramsPtr->Detach(NULL);
      DCHECK(paramsPtr->HasOneRef());

      if (handled)
        return;
    }
  }

  // Execute the default command handling.
  ExecuteDefaultCommand(command_id);
}

void CefMenuManager::MenuWillShow(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get())
    return;

  if (!web_contents())
    return;

  // May be called multiple times.
  if (IsShowingContextMenu())
    return;

  // Notify the host before showing the context menu.
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(true);
}

void CefMenuManager::MenuClosed(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get())
    return;

  if (!web_contents())
    return;

  DCHECK(IsShowingContextMenu());

  // Notify the client.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler =
        client->GetContextMenuHandler();
    if (handler.get()) {
      handler->OnContextMenuDismissed(browser_, browser_->GetFocusedFrame());
    }
  }

  // Notify the host after closing the context menu.
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(false);
  web_contents()->NotifyContextMenuClosed(params_.custom_context);
}

bool CefMenuManager::FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                                 base::string16& label) {
  return runner_->FormatLabel(label);
}

void CefMenuManager::ExecuteCommandCallback(int command_id,
                                            cef_event_flags_t event_flags) {
  DCHECK(IsShowingContextMenu());
  DCHECK(custom_menu_callback_);
  if (command_id != kInvalidCommandId)
    ExecuteCommand(model_, command_id, event_flags);
  MenuClosed(model_);
  custom_menu_callback_ = NULL;
}

void CefMenuManager::CreateDefaultModel() {
  if (!params_.custom_items.empty()) {
    // Custom menu items originating from the renderer process. For example,
    // plugin placeholder menu items or Flash menu items.
    for (size_t i = 0; i < params_.custom_items.size(); ++i) {
      content::MenuItem menu_item = params_.custom_items[i];
      menu_item.action += MENU_ID_CUSTOM_FIRST;
      DCHECK_LE(static_cast<int>(menu_item.action), MENU_ID_CUSTOM_LAST);
      model_->AddMenuItem(menu_item);
    }
    return;
  }

  if (params_.is_editable) {
    // Editable node.
    model_->AddItem(MENU_ID_UNDO, GetLabel(IDS_CONTENT_CONTEXT_UNDO));
    model_->AddItem(MENU_ID_REDO, GetLabel(IDS_CONTENT_CONTEXT_REDO));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_CUT, GetLabel(IDS_CONTENT_CONTEXT_CUT));
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
    model_->AddItem(MENU_ID_PASTE, GetLabel(IDS_CONTENT_CONTEXT_PASTE));
    model_->AddItem(MENU_ID_DELETE, GetLabel(IDS_CONTENT_CONTEXT_DELETE));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_SELECT_ALL,
                    GetLabel(IDS_CONTENT_CONTEXT_SELECTALL));

    if (!(params_.edit_flags & CM_EDITFLAG_CAN_UNDO))
      model_->SetEnabled(MENU_ID_UNDO, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_REDO))
      model_->SetEnabled(MENU_ID_REDO, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_CUT))
      model_->SetEnabled(MENU_ID_CUT, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_COPY))
      model_->SetEnabled(MENU_ID_COPY, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_PASTE))
      model_->SetEnabled(MENU_ID_PASTE, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_DELETE))
      model_->SetEnabled(MENU_ID_DELETE, false);
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_SELECT_ALL))
      model_->SetEnabled(MENU_ID_SELECT_ALL, false);

    if(!params_.misspelled_word.empty()) {
      // Always add a separator before the list of dictionary suggestions or
      // "No spelling suggestions".
      model_->AddSeparator();

      if (!params_.dictionary_suggestions.empty()) {
        for (size_t i = 0;
             i < params_.dictionary_suggestions.size() &&
                 MENU_ID_SPELLCHECK_SUGGESTION_0 + i <=
                    MENU_ID_SPELLCHECK_SUGGESTION_LAST;
             ++i) {
          model_->AddItem(MENU_ID_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
              params_.dictionary_suggestions[i].c_str());
        }

        // When there are dictionary suggestions add a separator before "Add to
        // dictionary".
        model_->AddSeparator();
      } else {
        model_->AddItem(
            MENU_ID_NO_SPELLING_SUGGESTIONS,
            GetLabel(IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
        model_->SetEnabled(MENU_ID_NO_SPELLING_SUGGESTIONS, false);
      }

      model_->AddItem(
            MENU_ID_ADD_TO_DICTIONARY,
            GetLabel(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));
    }
  } else if (!params_.selection_text.empty()) {
    // Something is selected.
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
  } else if (!params_.page_url.is_empty() || !params_.frame_url.is_empty()) {
    // Page or frame.
    model_->AddItem(MENU_ID_BACK, GetLabel(IDS_CONTENT_CONTEXT_BACK));
    model_->AddItem(MENU_ID_FORWARD, GetLabel(IDS_CONTENT_CONTEXT_FORWARD));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_PRINT, GetLabel(IDS_CONTENT_CONTEXT_PRINT));
    model_->AddItem(MENU_ID_VIEW_SOURCE,
                    GetLabel(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE));

    if (!browser_->CanGoBack())
      model_->SetEnabled(MENU_ID_BACK, false);
    if (!browser_->CanGoForward())
      model_->SetEnabled(MENU_ID_FORWARD, false);
  }
}

void CefMenuManager::ExecuteDefaultCommand(int command_id) {
  if (IsCustomContextMenuCommand(command_id)) {
    if (web_contents()) {
      web_contents()->ExecuteCustomContextMenuCommand(
          command_id - MENU_ID_CUSTOM_FIRST, params_.custom_context);
    }
    return;
  }

  // If the user chose a replacement word for a misspelling, replace it here.
  if (command_id >= MENU_ID_SPELLCHECK_SUGGESTION_0 &&
      command_id <= MENU_ID_SPELLCHECK_SUGGESTION_LAST) {
    const size_t suggestion_index =
        static_cast<size_t>(command_id) - MENU_ID_SPELLCHECK_SUGGESTION_0;
    if (suggestion_index < params_.dictionary_suggestions.size()) {
      browser_->ReplaceMisspelling(
          params_.dictionary_suggestions[suggestion_index]);
    }
    return;
  }

  switch (command_id) {
  // Navigation.
  case MENU_ID_BACK:
    browser_->GoBack();
    break;
  case MENU_ID_FORWARD:
    browser_->GoForward();
    break;
  case MENU_ID_RELOAD:
    browser_->Reload();
    break;
  case MENU_ID_RELOAD_NOCACHE:
    browser_->ReloadIgnoreCache();
    break;
  case MENU_ID_STOPLOAD:
    browser_->StopLoad();
    break;

  // Editing.
  case MENU_ID_UNDO:
    browser_->GetFocusedFrame()->Undo();
    break;
  case MENU_ID_REDO:
    browser_->GetFocusedFrame()->Redo();
    break;
  case MENU_ID_CUT:
    browser_->GetFocusedFrame()->Cut();
    break;
  case MENU_ID_COPY:
    browser_->GetFocusedFrame()->Copy();
    break;
  case MENU_ID_PASTE:
    browser_->GetFocusedFrame()->Paste();
    break;
  case MENU_ID_DELETE:
    browser_->GetFocusedFrame()->Delete();
    break;
  case MENU_ID_SELECT_ALL:
    browser_->GetFocusedFrame()->SelectAll();
    break;

  // Miscellaneous.
  case MENU_ID_FIND:
    // TODO(cef): Implement.
    NOTIMPLEMENTED();
    break;
  case MENU_ID_PRINT:
    browser_->Print();
    break;
  case MENU_ID_VIEW_SOURCE:
    browser_->GetFocusedFrame()->ViewSource();
    break;

  // Spell checking.
  case MENU_ID_ADD_TO_DICTIONARY:
    browser_->GetHost()->AddWordToDictionary(params_.misspelled_word);
    break;

  default:
    break;
  }
}

bool CefMenuManager::IsCustomContextMenuCommand(int command_id) {
  // Verify that the command ID is in the correct range.
  if (command_id < MENU_ID_CUSTOM_FIRST || command_id > MENU_ID_CUSTOM_LAST)
    return false;

  command_id -= MENU_ID_CUSTOM_FIRST;

  // Verify that the specific command ID was passed from the renderer process.
  if (!params_.custom_items.empty()) {
    for (size_t i = 0; i < params_.custom_items.size(); ++i) {
      if (static_cast<int>(params_.custom_items[i].action) == command_id)
        return true;
    }
  }
  return false;
}
