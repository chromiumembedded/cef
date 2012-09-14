// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "cefclient/cefclient.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_runnable.h"
#include "cefclient/binding_test.h"
#include "cefclient/client_handler.h"
#include "cefclient/extension_test.h"
#include "cefclient/scheme_test.h"
#include "cefclient/string_util.h"

char szWorkingDir[512];  // The current working directory

// The global ClientHandler reference.
extern CefRefPtr<ClientHandler> g_handler;

void destroy(void) {
  CefQuitMessageLoop();
}

void TerminationSignalHandler(int signatl) {
  destroy();
}

// Callback for Debug > Get Source... menu item.
gboolean GetSourceActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunGetSourceTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Get Source... menu item.
gboolean GetTextActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunGetTextTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > JS Binding... menu item.
gboolean JSBindngActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunBindingTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > JS Extension... menu item.
gboolean JSExtensionActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunExtensionTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > JS Execute... menu item.
gboolean JSExecuteActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunJavaScriptExecuteTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Request... menu item.
gboolean RequestActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunRequestTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Local Storage... menu item.
gboolean LocalStorageActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunLocalStorageTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > XMLHttpRequest... menu item.
gboolean XMLHttpRequestActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunXMLHTTPRequestTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > WebURLRequest... menu item.
gboolean WebURLRequestActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunWebURLRequestTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > DOM Access... menu item.
gboolean DOMAccessActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunDOMAccessTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Scheme Handler... menu item.
gboolean SchemeHandlerActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunSchemeTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Popup Window... menu item.
gboolean PopupWindowActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunPopupTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Accelerated 2D Canvas:... menu item.
gboolean Accelerated2DCanvasActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunAccelerated2DCanvasTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Accelerated Layers:... menu item.
gboolean AcceleratedLayersActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunAcceleratedLayersTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > WebGL:... menu item.
gboolean WebGLActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunWebGLTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > HTML5 Video... menu item.
gboolean HTML5VideoActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunHTML5VideoTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Zoom In... menu item.
gboolean ZoomInActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    browser->SetZoomLevel(browser->GetZoomLevel() + 0.5);
  }

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Zoom Out... menu item.
gboolean ZoomOutActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    browser->SetZoomLevel(browser->GetZoomLevel() - 0.5);
  }

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Zoom Reset... menu item.
gboolean ZoomResetActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    browser->SetZoomLevel(0.0);
  }

  return FALSE;  // Don't stop this message.
}

gboolean DragDropActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    RunDragDropTest(browser);
  }

  return FALSE;  // Don't stop this message.
}

gboolean GeolocationActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    RunGeolocationTest(browser);
  }

  return FALSE;  // Don't stop this message.
}

gboolean ShowDevtoolsActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    browser->ShowDevTools();
  }

  return FALSE;  // Don't stop this message.
}

// Callback for Debug > Plugin Info... menu item.
gboolean PluginInfoActivated(GtkWidget* widget) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    RunPluginInfoTest(g_handler->GetBrowser());

  return FALSE;  // Don't stop this message.
}

// Callback for when you click the back button.
void BackButtonClicked(GtkButton* button) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    g_handler->GetBrowser()->GoBack();
}

// Callback for when you click the forward button.
void ForwardButtonClicked(GtkButton* button) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    g_handler->GetBrowser()->GoForward();
}

// Callback for when you click the stop button.
void StopButtonClicked(GtkButton* button) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    g_handler->GetBrowser()->StopLoad();
}

// Callback for when you click the reload button.
void ReloadButtonClicked(GtkButton* button) {
  if (g_handler.get() && g_handler->GetBrowserHwnd())
    g_handler->GetBrowser()->Reload();
}

// Callback for when you press enter in the URL box.
void URLEntryActivate(GtkEntry* entry) {
  if (!g_handler.get() || !g_handler->GetBrowserHwnd())
    return;

  const gchar* url = gtk_entry_get_text(entry);
  g_handler->GetBrowser()->GetMainFrame()->LoadURL(std::string(url).c_str());
}

// GTK utility functions ----------------------------------------------

GtkWidget* AddMenuEntry(GtkWidget* menu_widget, const char* text,
                        GCallback callback) {
  GtkWidget* entry = gtk_menu_item_new_with_label(text);
  g_signal_connect(entry, "activate", callback, NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), entry);
  return entry;
}

GtkWidget* CreateMenu(GtkWidget* menu_bar, const char* text) {
  GtkWidget* menu_widget = gtk_menu_new();
  GtkWidget* menu_header = gtk_menu_item_new_with_label(text);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_header), menu_widget);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_header);
  return menu_widget;
}

GtkWidget* CreateMenuBar() {
  GtkWidget* menu_bar = gtk_menu_bar_new();
  GtkWidget* debug_menu = CreateMenu(menu_bar, "Tests");

  AddMenuEntry(debug_menu, "Get Source",
               G_CALLBACK(GetSourceActivated));
  AddMenuEntry(debug_menu, "Get Text",
               G_CALLBACK(GetTextActivated));
  AddMenuEntry(debug_menu, "JS Binding",
               G_CALLBACK(JSBindngActivated));
  AddMenuEntry(debug_menu, "JS Extension",
               G_CALLBACK(JSExtensionActivated));
  AddMenuEntry(debug_menu, "JS Execute",
               G_CALLBACK(JSExecuteActivated));
  AddMenuEntry(debug_menu, "Request",
               G_CALLBACK(RequestActivated));
  AddMenuEntry(debug_menu, "Local Storage",
               G_CALLBACK(LocalStorageActivated));
  AddMenuEntry(debug_menu, "XMLHttpRequest",
               G_CALLBACK(XMLHttpRequestActivated));
  AddMenuEntry(debug_menu, "DOM Access",
               G_CALLBACK(DOMAccessActivated));
  AddMenuEntry(debug_menu, "Scheme Handler",
               G_CALLBACK(SchemeHandlerActivated));
  AddMenuEntry(debug_menu, "Popup Window",
               G_CALLBACK(PopupWindowActivated));
  AddMenuEntry(debug_menu, "Accelerated 2D Canvas",
               G_CALLBACK(Accelerated2DCanvasActivated));
  AddMenuEntry(debug_menu, "Accelerated Layers",
               G_CALLBACK(AcceleratedLayersActivated));
  AddMenuEntry(debug_menu, "WebGL",
               G_CALLBACK(WebGLActivated));
  AddMenuEntry(debug_menu, "HTML5 Video",
               G_CALLBACK(HTML5VideoActivated));
  AddMenuEntry(debug_menu, "Zoom In",
               G_CALLBACK(ZoomInActivated));
  AddMenuEntry(debug_menu, "Zoom Out",
               G_CALLBACK(ZoomOutActivated));
  AddMenuEntry(debug_menu, "Zoom Reset",
               G_CALLBACK(ZoomResetActivated));
  AddMenuEntry(debug_menu, "Drag & Drop",
               G_CALLBACK(DragDropActivated));
  AddMenuEntry(debug_menu, "Geolocation",
               G_CALLBACK(GeolocationActivated));
  AddMenuEntry(debug_menu, "Show DevTools",
               G_CALLBACK(ShowDevtoolsActivated));
  AddMenuEntry(debug_menu, "Plugin Info",
               G_CALLBACK(PluginInfoActivated));

  return menu_bar;
}

// WebViewDelegate::TakeFocus in the test webview delegate.
static gboolean HandleFocus(GtkWidget* widget,
                            GdkEventFocus* focus) {
  if (g_handler.get() && g_handler->GetBrowserHwnd()) {
    // Give focus to the browser window.
    g_handler->GetBrowser()->SetFocus(true);
  }

  return TRUE;
}

int main(int argc, char *argv[]) {
  if (!getcwd(szWorkingDir, sizeof (szWorkingDir)))
    return -1;

  GtkWidget* window;

  gtk_init(&argc, &argv);

  // Parse command line arguments.
  AppInitCommandLine(argc, argv);

  CefSettings settings;
  CefRefPtr<CefApp> app;

  // Populate the settings based on command line arguments.
  AppGetSettings(settings, app);

  // Initialize CEF.
  CefInitialize(settings, app);

  // Register the V8 extension handler.
  InitExtensionTest();

  // Register the scheme handler.
  InitSchemeTest();

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  g_signal_connect(window, "focus", G_CALLBACK(&HandleFocus), NULL);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* menu_bar = CreateMenuBar();

  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();
  // Turn off the labels on the toolbar buttons.
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

  GtkToolItem* back = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
  g_signal_connect(back, "clicked",
                   G_CALLBACK(BackButtonClicked), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back, -1 /* append */);

  GtkToolItem* forward = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
  g_signal_connect(forward, "clicked",
                   G_CALLBACK(ForwardButtonClicked), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward, -1 /* append */);

  GtkToolItem* reload = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
  g_signal_connect(reload, "clicked",
                   G_CALLBACK(ReloadButtonClicked), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload, -1 /* append */);

  GtkToolItem* stop = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
  g_signal_connect(stop, "clicked",
                   G_CALLBACK(StopButtonClicked), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop, -1 /* append */);

  GtkWidget* m_editWnd = gtk_entry_new();
  g_signal_connect(G_OBJECT(m_editWnd), "activate",
                   G_CALLBACK(URLEntryActivate), NULL);

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), m_editWnd);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1);  // append

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &window);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(destroy), NULL);

  // Create the handler.
  g_handler = new ClientHandler();
  g_handler->SetMainHwnd(vbox);
  g_handler->SetEditHwnd(m_editWnd);
  g_handler->SetButtonHwnds(GTK_WIDGET(back), GTK_WIDGET(forward),
                            GTK_WIDGET(reload), GTK_WIDGET(stop));

  // Create the browser view.
  CefWindowInfo window_info;
  CefBrowserSettings browserSettings;

  // Populate the settings based on command line arguments.
  AppGetBrowserSettings(browserSettings);

  window_info.SetAsChild(vbox);

  CefBrowser::CreateBrowserSync(window_info,
                                static_cast<CefRefPtr<CefClient> >(g_handler),
                                g_handler->GetStartupURL(), browserSettings);

  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show_all(GTK_WIDGET(window));

  // Install an signal handler so we clean up after ourselves.
  signal(SIGINT, TerminationSignalHandler);
  signal(SIGTERM, TerminationSignalHandler);

  CefRunMessageLoop();

  CefShutdown();

  return 0;
}

// Global functions

std::string AppGetWorkingDirectory() {
  return szWorkingDir;
}
