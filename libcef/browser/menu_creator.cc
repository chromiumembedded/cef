// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_creator.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context_menu_params_impl.h"
#include "libcef/common/content_client.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "grit/cef_strings.h"

#if defined(OS_WIN)
#include "libcef/browser/menu_creator_runner_win.h"
#elif defined(OS_MACOSX)
#include "libcef/browser/menu_creator_runner_mac.h"
#elif defined(OS_LINUX)
#include "libcef/browser/menu_creator_runner_linux.h"
#endif

namespace {

CefString GetLabel(int message_id) {
  base::string16 label =
      CefContentClient::Get()->GetLocalizedString(message_id);
  DCHECK(!label.empty());
  return label;
}

}  // namespace

CefMenuCreator::CefMenuCreator(content::WebContents* web_contents,
                               CefBrowserHostImpl* browser)
  : content::WebContentsObserver(web_contents),
    browser_(browser) {
  DCHECK(web_contents);
  DCHECK(browser_);
  model_ = new CefMenuModelImpl(this);
}

CefMenuCreator::~CefMenuCreator() {
  // The model may outlive the delegate if the context menu is visible when the
  // application is closed.
  model_->set_delegate(NULL);
}

bool CefMenuCreator::IsShowingContextMenu() {
  if (!web_contents())
    return false;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  return (view && view->IsShowingContextMenu());
}

bool CefMenuCreator::CreateContextMenu(
    const content::ContextMenuParams& params) {
  if (!CreateRunner())
    return true;

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

  // Give the client a chance to modify the model.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
      CefRefPtr<CefContextMenuHandler> handler =
          client->GetContextMenuHandler();
      if (handler.get()) {
        CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
            new CefContextMenuParamsImpl(&params_));

        handler->OnBeforeContextMenu(browser_,
                                     browser_->GetFocusedFrame(),
                                     paramsPtr.get(),
                                     model_.get());

        // Do not keep references to the parameters in the callback.
        paramsPtr->Detach(NULL);
        DCHECK(paramsPtr->HasOneRef());
        DCHECK(model_->VerifyRefCount());

        // Menu is empty so notify the client and return.
        if (model_->GetCount() == 0) {
          MenuClosed(model_);
          return true;
        }
      }
  }

  return runner_->RunContextMenu(this);
}

void CefMenuCreator::CancelContextMenu() {
  if (IsShowingContextMenu())
    runner_->CancelContextMenu();
}

bool CefMenuCreator::CreateRunner() {
  if (!runner_.get()) {
    // Create the menu runner.
#if defined(OS_WIN)
    runner_.reset(new CefMenuCreatorRunnerWin);
#elif defined(OS_MACOSX)
    runner_.reset(new CefMenuCreatorRunnerMac);
#elif defined(OS_LINUX)
    runner_.reset(new CefMenuCreatorRunnerLinux);
#else
    // Need an implementation.
    NOTREACHED();
#endif
  }
  return (runner_.get() != NULL);
}

void CefMenuCreator::ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
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

void CefMenuCreator::MenuWillShow(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get())
    return;

  if (!web_contents())
    return;

  // Notify the host before showing the context menu.
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view)
    view->SetShowingContextMenu(true);
}

void CefMenuCreator::MenuClosed(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get())
    return;

  // Notify the client.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
      CefRefPtr<CefContextMenuHandler> handler =
          client->GetContextMenuHandler();
      if (handler.get()) {
        handler->OnContextMenuDismissed(browser_, browser_->GetFocusedFrame());
      }
  }

  if (IsShowingContextMenu() && web_contents()) {
    // Notify the host after closing the context menu.
    content::RenderWidgetHostView* view =
        web_contents()->GetRenderWidgetHostView();
    if (view)
      view->SetShowingContextMenu(false);
    web_contents()->NotifyContextMenuClosed(params_.custom_context);
  }
}

bool CefMenuCreator::FormatLabel(base::string16& label) {
  return runner_->FormatLabel(label);
}

void CefMenuCreator::CreateDefaultModel() {
  if (params_.is_editable) {
    // Editable node.
    model_->AddItem(MENU_ID_UNDO, GetLabel(IDS_MENU_UNDO));
    model_->AddItem(MENU_ID_REDO, GetLabel(IDS_MENU_REDO));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_CUT, GetLabel(IDS_MENU_CUT));
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_MENU_COPY));
    model_->AddItem(MENU_ID_PASTE, GetLabel(IDS_MENU_PASTE));
    model_->AddItem(MENU_ID_DELETE, GetLabel(IDS_MENU_DELETE));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_SELECT_ALL, GetLabel(IDS_MENU_SELECT_ALL));

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
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_MENU_COPY));
  } else if (!params_.page_url.is_empty() || !params_.frame_url.is_empty()) {
    // Page or frame.
    model_->AddItem(MENU_ID_BACK, GetLabel(IDS_MENU_BACK));
    model_->AddItem(MENU_ID_FORWARD, GetLabel(IDS_MENU_FORWARD));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_PRINT, GetLabel(IDS_MENU_PRINT));
    model_->AddItem(MENU_ID_VIEW_SOURCE, GetLabel(IDS_MENU_VIEW_SOURCE));

    if (!browser_->CanGoBack())
      model_->SetEnabled(MENU_ID_BACK, false);
    if (!browser_->CanGoForward())
      model_->SetEnabled(MENU_ID_FORWARD, false);
  }
}

void CefMenuCreator::ExecuteDefaultCommand(int command_id) {
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
