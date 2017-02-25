// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_style.h"

#include "tests/cefclient/browser/main_context.h"

namespace client {

namespace views_style {

namespace {

cef_color_t g_background_color = 0;
cef_color_t g_background_hover_color = 0;
cef_color_t g_text_color = 0;

int GetShade(int component) {
  return (component < 127) ? component + 75 : component - 75;
}

void MaybeInitialize() {
  static bool initialized = false;
  if (initialized)
    return;

  g_background_color = MainContext::Get()->GetBackgroundColor();
  if (g_background_color != 0) {
    // Use a slightly modified shade of the background color for hover.
    g_background_hover_color = CefColorSetARGB(255,
        GetShade(CefColorGetR(g_background_color)),
        GetShade(CefColorGetG(g_background_color)),
        GetShade(CefColorGetB(g_background_color)));

    // Invert the background color for text.
    g_text_color =  CefColorSetARGB(255,
        255 - CefColorGetR(g_background_color),
        255 - CefColorGetG(g_background_color),
        255 - CefColorGetB(g_background_color));
  }

  initialized = true;
}

}  // namespace

bool IsSet() {
  MaybeInitialize();
  return g_background_color != 0;
}

void ApplyTo(CefRefPtr<CefPanel> panel) {
  if (!IsSet())
    return;

  panel->SetBackgroundColor(g_background_color);
}

void ApplyTo(CefRefPtr<CefLabelButton> label_button) {
  if (!IsSet())
    return;

  // All text except disabled gets the same color.
  label_button->SetEnabledTextColors(g_text_color);
  label_button->SetTextColor(CEF_BUTTON_STATE_DISABLED,
                             g_background_hover_color);

  label_button->SetBackgroundColor(g_background_color);
}

void ApplyTo(CefRefPtr<CefTextfield> textfield) {
  if (!IsSet())
    return;

  textfield->SetBackgroundColor(g_background_color);
  textfield->SetTextColor(g_text_color);
}

void ApplyTo(CefRefPtr<CefMenuModel> menu_model) {
  if (!IsSet())
    return;

  // All text except non-hovered accelerator gets the same color.
  menu_model->SetColorAt(-1, CEF_MENU_COLOR_TEXT, g_text_color);
  menu_model->SetColorAt(-1, CEF_MENU_COLOR_TEXT_HOVERED, g_text_color);
  menu_model->SetColorAt(-1, CEF_MENU_COLOR_TEXT_ACCELERATOR,
                         g_background_hover_color);
  menu_model->SetColorAt(-1, CEF_MENU_COLOR_TEXT_ACCELERATOR_HOVERED,
                         g_text_color);

  menu_model->SetColorAt(-1, CEF_MENU_COLOR_BACKGROUND, g_background_color);
  menu_model->SetColorAt(-1, CEF_MENU_COLOR_BACKGROUND_HOVERED,
                         g_background_hover_color);

  // Recursively color sub-menus.
  for (int i = 0; i < menu_model->GetCount(); ++i) {
    if (menu_model->GetTypeAt(i) == MENUITEMTYPE_SUBMENU)
      ApplyTo(menu_model->GetSubMenuAt(i));
  }
}

}  // namespace views_style

}  // namespace client
