// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "include/cef_devtools_message_observer.h"
#include "include/cef_input_handler.h"
#include "include/cef_parser.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUrl[] = "https://tests/ime-input";

enum class Operation {
  Set,
  Commit,
  FinishKeep,
  FinishDiscard,
  Cancel,
  Script,
  Blur,
  Refocus,
  Geometry,
  Navigate,
  Destroy,
  KeysCommit,
  KeysFinish,
  SetCancel,
  Submit,
  FocusRoot,
  FocusChild,
  ChromiumSet,
};
enum class Editor { Input, Textarea, ContentEditable };
enum class KeyboardApi {
  Legacy,
#if CEF_API_ADDED(CEF_NEXT)
  Portable,
#endif
};
enum class Scenario {
  Commit,
  CommitUnicode,
  CompositionCommit,
  UpdateCommit,
  FinishKeep,
  FinishDiscard,
  Cancel,
  EmptyUpdate,
  ReplaceSelection,
  CommitSelection,
  CompositionUnicode,
  RepeatSession,
  EmptyCommit,
  EmptyCompositionCommit,
  EmptyCommitSelection,
  EmptySetWithoutComposition,
  FinishWithoutComposition,
  CancelWithoutComposition,
  RepeatedFinishCancel,
  OmittedSelection,
  OutOfBoundsSelection,
  ReversedSelection,
  ExplicitCompositionReplacement,
  ExplicitCommitReplacement,
  OutOfBoundsReplacement,
  CursorBeforeEnd,
  CursorAfterEnd,
  CursorOutOfBounds,
  MultipleUnderlines,
  UnderlineStyles,
  CompositionTextColor,
  CancelBeforeInput,
  CancelCompositionBeforeInput,
  ReadOnly,
  Disabled,
  MaxLengthCommit,
  MaxLengthComposition,
  Multiline,
  LongText,
  FocusOtherEditor,
  BlurDuringComposition,
  RemoveDuringComposition,
  NavigateDuringComposition,
  DestroyDuringComposition,
  SameOriginIframe,
  CrossOriginIframe,
  ScrollGeometry,
  ResizeGeometry,
  SelectionCallbacks,
  SelectionFocus,
  SelectionExternalBeginFrame,
  PasswordSelection,
  PasswordUnicode,
  PasswordUnicodeChromium,
  PasswordTypeChange,
  PasswordIframe,
  PasswordCrossOriginIframe,
  CursorBeforeStart,
  EmptySetSelection,
  InvalidUnderlineRanges,
  NoUnderlines,
  OffThreadCommit,
  OffThreadComposition,
  OffThreadFinishKeep,
  OffThreadFinishDiscard,
  OffThreadCancel,
  CancelCompositionStart,
  CrossOriginIframeChromium,
  SetCompositionRange,
  KeyboardCommitNavigation,
  KeyboardFinishNavigation,
  CancelNavigation,
  MonitoringCancelAcrossFrames,
  MonitoringCommitAcrossFrames,
  MonitoringFinishAcrossFrames,
  MonitoringRetargetAcrossFrames,
};

struct ExpectedEvent {
  std::string type;
  std::u16string data;
  std::string input_type;
  bool composing = false;
  bool prevented = false;
};

bool IsCrossFrameMonitoring(Scenario scenario) {
  return scenario == Scenario::MonitoringCancelAcrossFrames ||
         scenario == Scenario::MonitoringCommitAcrossFrames ||
         scenario == Scenario::MonitoringFinishAcrossFrames ||
         scenario == Scenario::MonitoringRetargetAcrossFrames;
}

struct Step {
  Operation operation;
  std::u16string text;
  CefRange selection = CefRange::InvalidRange();
  std::u16string value;
  CefRange result_selection;
  std::vector<ExpectedEvent> events;
  CefRange composition = CefRange::InvalidRange();
  CefRange replacement = CefRange::InvalidRange();
  int cursor = 0;
  std::vector<CefCompositionUnderline> underlines;
  std::string script;
  bool check_selection_callback = false;
  std::optional<std::u16string> primary_value;
  std::vector<cef_color_t> paint_colors;
  std::vector<cef_color_t> absent_paint_colors;
  bool check_bounds_callback = true;
  bool unchanged_selection_callback = false;
  bool compare_underline_mask = false;
  bool no_underline = false;
  std::optional<CefRange> feedback_selection;
  bool redact_selected_text = false;
  bool no_bounds_callback = false;
};

std::vector<ExpectedEvent> CompositionEvents(
    const std::u16string& text,
    bool start,
    bool end,
    const std::u16string& selected = {}) {
  std::vector<ExpectedEvent> events;
  if (start) {
    events.push_back({"compositionstart", selected});
  }
  events.push_back({"compositionupdate", text});
  events.push_back({"beforeinput", text, "insertCompositionText", true});
  events.push_back({"input", text, "insertCompositionText", true});
  if (end) {
    events.push_back({"compositionend", text});
  }
  return events;
}

std::string JsonString(const std::u16string& text) {
  auto value = CefValue::Create();
  value->SetString(CefString(text));
  return CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
}

bool CompositionTextColorEnabled() {
  return CefIsFeatureEnabledForTests("CompositionForegroundMarkers");
}

// The browser creation mode is independent of the command/report protocol so
// windowed coverage can use this fixture when the public Ime* APIs support it.
// Native IME adapters are intentionally not involved: these tests require no
// installed input method, particular keyboard layout, or toolkit dependency.
class ImeInputTestHandler : public RoutingTestHandler,
                            public CefRenderHandler,
#if CEF_API_ADDED(CEF_NEXT)
                            public CefInputHandler,
#endif
                            public CefKeyboardHandler,
                            public CefFocusHandler,
                            public CefDevToolsMessageObserver {
 public:
  ImeInputTestHandler(Scenario scenario,
                      Editor editor,
                      float scale,
                      bool osr,
                      KeyboardApi keyboard_api,
                      bool start_in_child = false)
      : scenario_(scenario),
        editor_(editor),
        scale_(scale),
        osr_(osr),
        keyboard_api_(keyboard_api),
        start_in_child_(start_in_child) {
    BuildSteps(scenario);
    if (IsIframe()) {
      origin_x_ = 70;
      scroll_y_ = -20;
    } else if (start_in_child_) {
      origin_x_ = 70;
      origin_y_ = 140;
    }
  }

  CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
#if CEF_API_ADDED(CEF_NEXT)
  CefRefPtr<CefInputHandler> GetInputHandler() override { return this; }
#endif
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

  bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override {
    if (source == FOCUS_SOURCE_NAVIGATION) {
      got_navigation_focus_ = true;
    } else if (source == FOCUS_SOURCE_SYSTEM && submitting_) {
      got_submission_focus_ = true;
      MaybeFinishSubmission();
    }
    return false;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    RoutingTestHandler::OnAfterCreated(browser);
    if (scenario_ == Scenario::SelectionExternalBeginFrame) {
      SendBeginFrame();
    }
  }

#if CEF_API_ADDED(CEF_NEXT)
  bool OnKeyboardEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyboardInputEvent& event,
                       const CefInputEventContext&) override {
    if (keyboard_api_ != KeyboardApi::Portable) {
      return false;
    }
    AcknowledgeBarrier(browser, event.type, event.windows_key_code);
    return true;
  }
#endif

  bool OnKeyEvent(CefRefPtr<CefBrowser> browser,
                  const CefKeyEvent& event,
                  CefEventHandle) override {
    EXPECT_EQ(KeyboardApi::Legacy, keyboard_api_);
    AcknowledgeBarrier(browser, event.type, event.windows_key_code);
    return true;
  }

  void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                              int message_id,
                              bool success,
                              const void*,
                              size_t) override {
    EXPECT_EQ(devtools_id_, message_id);
    EXPECT_TRUE(success);
    if (setting_color_scheme_) {
      setting_color_scheme_ = false;
      Prepare(browser);
    } else {
      if (IsCrossFrameMonitoring()) {
        // Let composition-driven layout and its bounds notification complete
        // before checking that the old widget's monitoring has stopped.
        Execute(browser,
                "requestAnimationFrame(() => requestAnimationFrame(() => "
                "window.testQuery({request:'scripted'})))");
      } else {
        SendBarrier(browser);
      }
    }
  }

  void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override {
    rect = CefRect(0, 0, view_width_, 200);
  }

  bool GetScreenInfo(CefRefPtr<CefBrowser>, CefScreenInfo& info) override {
    info.device_scale_factor = scale_;
    info.rect = info.available_rect = CefRect(0, 0, view_width_, 200);
    return true;
  }

  void OnPaint(CefRefPtr<CefBrowser> browser,
               PaintElementType,
               const RectList&,
               const void* buffer,
               int width,
               int height) override {
    SCOPED_TRACE(Context());
    if (waiting_for_paint_) {
      waiting_for_paint_ = false;
      FocusEditor(browser);
    }
    if (step_ < steps_.size() && sent_ && !steps_[step_].paint_colors.empty()) {
      const auto* pixels = static_cast<const uint32_t*>(buffer);
      bool found_all = true;
      for (auto color : steps_[step_].paint_colors) {
        bool found = false;
        for (int i = 0; i < width * height; ++i) {
          if (pixels[i] == color) {
            found = true;
            break;
          }
        }
        found_all &= found;
      }
      for (auto color : steps_[step_].absent_paint_colors) {
        for (int i = 0; i < width * height; ++i) {
          if (pixels[i] == color) {
            found_all = false;
            break;
          }
        }
      }
      if (found_all && !got_paint_ && steps_[step_].compare_underline_mask &&
          !last_bounds_.empty()) {
        std::vector<bool> mask;
        const auto& first = last_bounds_.front();
        const auto& last = last_bounds_.back();
        // When foreground coloring is enabled, sample the underline band so
        // glyph pixels do not obscure the differences between underline styles.
        const int top = CompositionTextColorEnabled()
                            ? first.y + std::max(0, first.height - 3)
                            : first.y;
        for (int y = top * scale_; y < (first.y + first.height) * scale_; ++y) {
          for (int x = first.x * scale_; x < (last.x + last.width) * scale_;
               ++x) {
            bool foreground = false;
            for (const auto& line : steps_[step_].underlines) {
              foreground |= pixels[y * width + x] == line.color;
            }
            mask.push_back(foreground);
          }
        }
        if (!underline_mask_.empty()) {
          EXPECT_NE(underline_mask_, mask);
          if (steps_[step_].no_underline) {
            EXPECT_LT(std::count(mask.begin(), mask.end(), true),
                      std::count(underline_mask_.begin(), underline_mask_.end(),
                                 true));
          }
        }
        underline_mask_ = std::move(mask);
      }
      got_paint_ |= found_all;
      MaybeAdvance(browser);
    }
  }

  void RunTest() override {
    if (osr_) {
      SetUseAlloyStyle(true, true);
    }
    std::string html = R"(
      <style>
      body { margin:0 }
      #editor { position:absolute; left:40px; top:40px; width:300px;
        height:40px; font:20px monospace; padding:0; border:0; margin:0;
        white-space:pre; }
      </style>
      <!-- Visible text releases Chromium's first-contentful-paint holding
           without waiting for the 500 ms timeout of an empty editable. Keep
           it outside the editor and use a color absent from paint assertions. -->
      <span style='position:absolute;left:0;top:0;color:#808080'>IME</span>
      EDITOR<script>
      let editor = document.getElementById('editor');
      const primary = editor;
      const other = editor.cloneNode(false);
      other.id = 'other'; other.style.top = '100px';
      document.body.appendChild(other);
      let loaded = false, focused = false;
      let step = -1, events = [], cancelInput = false, cancelStart = false;
      function notifyFocused() {
        if (loaded && document.hasFocus() && !focused) {
          focused = true;
          window.testQuery({request:'focused'});
        }
      }
      window.addEventListener('focus', notifyFocused);
      window.addEventListener('load', () => {
        loaded = true;
        window.testQuery({request:'loaded'});
      });
      function notifyEditorReady() {
        window.testQuery({request:'editorReady:' + Number(document.hasFocus()) +
          ':' + Number(document.activeElement === editor)});
      }
      let editorReady = false;
      // Setup clicks may be retried while the compositor's input routing is
      // becoming ready. Pending clicks must not move an initialized caret.
      document.addEventListener('mousedown', e => e.preventDefault());
      editor.addEventListener('click', e => {
        e.preventDefault();
        if (!editorReady) {
          editorReady = true;
          editor.focus();
          notifyEditorReady();
        }
      });
      function initialize(value, from, to) {
        setEditorText(value, from, to);
        window.testQuery({request:'initialized'});
      }
      function setEditorText(value, from, to) {
        if (editor.isContentEditable) {
          editor.textContent = value;
          const range = document.createRange();
          const node = editor.firstChild || editor;
          range.setStart(node, from); range.setEnd(node, to);
          const selection = window.getSelection();
          selection.removeAllRanges(); selection.addRange(range);
        } else {
          editor.value = value;
          editor.setSelectionRange(from, to);
        }
      }
      function selectionOffsets() {
        if (editor.tagName !== 'DIV')
          return [editor.selectionStart, editor.selectionEnd];
        const selection = window.getSelection();
        if (!selection.anchorNode || !editor.contains(selection.anchorNode))
          return [-1, -1];
        function offset(node, position) {
          const range = document.createRange();
          range.selectNodeContents(editor); range.setEnd(node, position);
          if (editor.querySelector('div,br')) {
            // Range.toString() omits layout-generated line breaks. Render the
            // prefix off-screen to measure the same plain text as innerText.
            const prefix = document.createElement('div');
            prefix.style.cssText = 'position:fixed;left:-10000px;white-space:pre';
            prefix.appendChild(range.cloneContents());
            document.body.appendChild(prefix);
            const length = prefix.innerText.length;
            prefix.remove();
            return length;
          }
          return range.toString().length;
        }
        return [offset(selection.anchorNode, selection.anchorOffset),
                offset(selection.focusNode, selection.focusOffset)];
      }
      function prepare(index) {
        step = index; events = [];
        window.testQuery({request:'prepared'});
      }
      function valueOf(element) {
        if (element.tagName !== 'DIV') return element.value;
        // An empty editable gets a placeholder BR; innerText exposes that as
        // a newline even though the editable text and caret range are empty.
        if (element.childNodes.length === 1 &&
            element.firstChild.nodeName === 'BR') return '';
        return element.innerText;
      }
      function report() {
        window.testQuery({request:JSON.stringify({step, events,
          value:valueOf(editor), primaryValue:valueOf(primary),
          focused:document.activeElement?.id ?? '',
          selection:selectionOffsets()})});
      }
      // A key-up acknowledgement after the IME command requests the report.
      // This also observes no-op/cancelled operations without a fixed delay.
      for (const type of ['compositionstart', 'compositionupdate',
                          'compositionend', 'beforeinput', 'input',
                          'keydown', 'keypress']) {
        document.addEventListener(type, e => {
          if (e.type === 'beforeinput' && cancelInput) e.preventDefault();
          if (e.type === 'compositionstart' && cancelStart) e.preventDefault();
          events.push({type:e.type, data:e.data ?? '',
            inputType:e.inputType ?? '', composing:e.isComposing ?? false,
            prevented:e.defaultPrevented});
        });
      }
      </script>)";
    const char* markup = editor_ == Editor::Input ? "<input id='editor'>"
                         : editor_ == Editor::Textarea
                             ? "<textarea id='editor'></textarea>"
                             : "<div id='editor' contenteditable></div>";
    html.replace(html.find("EDITOR"), 6, markup);
    if (IsFormNavigation()) {
      const std::string input = markup;
      const auto pos = html.find(input);
      html.replace(
          pos, input.size(),
          "<form action='" + std::string(kUrl) +
              "/submitted' method='get'>"
              "<input id='editor' name='k'>"
              "<button id='submit' type='submit'>Submit</button></form>");
      AddResource(std::string(kUrl) + "/submitted", "<body>submitted</body>",
                  "text/html");
    }
    if (scenario_ == Scenario::ResizeGeometry) {
      html.replace(html.find("left:40px"), 9, "left:calc(100vw - 360px)");
    }
    if (IsCrossFrameMonitoring()) {
      const std::string child_url = "https://other-tests/ime-input-child";
      AddResource(child_url, html, "text/html");
      html += "<iframe src='" + child_url +
              "' style='position:absolute;left:30px;top:100px;border:0;"
              "width:370px;height:100px'></iframe>";
    } else if (IsIframe()) {
      const std::string child_url = (scenario_ == Scenario::SameOriginIframe ||
                                     scenario_ == Scenario::PasswordIframe)
                                        ? std::string(kUrl) + "/child"
                                        : "https://other-tests/ime-input-child";
      AddResource(child_url, html, "text/html");
      html =
          "<style>body{margin:0}iframe{position:absolute;left:30px;top:20px;"
          "border:0;width:400px;height:200px}"
          "</style><iframe src='" +
          child_url + "'></iframe>";
    }
    AddResource(kUrl, html, "text/html");
    if (osr_) {
      CefWindowInfo info;
#if defined(OS_WIN)
      info.SetAsWindowless(GetDesktopWindow());
#else
      info.SetAsWindowless(kNullWindowHandle);
#endif
      info.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
      info.external_begin_frame_enabled =
          scenario_ == Scenario::SelectionExternalBeginFrame;
      if (CanReuseBrowser()) {
        CreateReusableBrowser(info, kUrl, CefBrowserSettings());
      } else {
        CefBrowserHost::CreateBrowser(info, this, kUrl, CefBrowserSettings(),
                                      nullptr, nullptr);
      }
    } else {
      CreateBrowser(kUrl);
    }
    SetTestTimeout(10000);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t,
               const CefString& request,
               bool,
               CefRefPtr<Callback> callback) override {
    callback->Success("");
    if (IsCrossFrameMonitoring() && request == "focused") {
      return true;
    }
    if (request == "loaded") {
      if (IsCrossFrameMonitoring()) {
        (frame->IsMain() ? root_frame_ : child_frame_) = frame;
        if (!root_frame_ || !child_frame_) {
          return true;
        }
        frame_ = start_in_child_ ? child_frame_ : root_frame_;
        browser->GetHost()->SetFocus(true);
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
        return true;
      }
      frame_ = frame;
      browser->GetHost()->SetFocus(true);
      if (IsIframe()) {
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
      } else {
        Execute(browser, "notifyFocused()");
      }
    } else if (request == "focused") {
      if (IsIframe()) {
        // Child-frame editor readiness is reported separately.
      } else if (osr_) {
        waiting_for_paint_ = true;
        browser->GetHost()->Invalidate(PET_VIEW);
      } else {
        FocusEditor(browser);
      }
    } else if (request.ToString().rfind("editorReady:", 0) == 0) {
      waiting_for_editor_ = false;
      EXPECT_EQ("editorReady:1:1", request.ToString());
      Execute(browser, "initialize(" + JsonString(initial_) + "," +
                           std::to_string(initial_selection_.from) + "," +
                           std::to_string(initial_selection_.to) + ")");
    } else if (request == "initialized") {
      if (scenario_ == Scenario::MultipleUnderlines ||
          scenario_ == Scenario::UnderlineStyles ||
          scenario_ == Scenario::CompositionTextColor) {
        // Blink substitutes the text color for composition underline colors
        // when the preferred scheme is dark. Isolate these color assertions
        // from the machine's appearance settings with a per-page override.
        setting_color_scheme_ = true;
        auto host = browser->GetHost();
        devtools_registration_ = host->AddDevToolsMessageObserver(this);
        auto feature = CefDictionaryValue::Create();
        feature->SetString("name", "prefers-color-scheme");
        feature->SetString("value", "light");
        auto features = CefListValue::Create();
        features->SetDictionary(0, feature);
        auto params = CefDictionaryValue::Create();
        params->SetList("features", features);
        devtools_id_ = host->ExecuteDevToolsMethod(
            0, "Emulation.setEmulatedMedia", params);
        EXPECT_GT(devtools_id_, 0);
      } else if (scenario_ == Scenario::PasswordTypeChange) {
        // Initialization selects plain text. Observe that selection before
        // arming the password step's strict redaction assertion, since the
        // message-router acknowledgement does not flush selection callbacks.
        waiting_for_initial_selection_ = true;
        MaybePrepareInitialSelection(browser);
      } else {
        Prepare(browser);
      }
    } else if (request == "prepared") {
      // Leave the message-router callback before dispatching browser input.
      CefPostTask(TID_UI,
                  base::BindOnce(&ImeInputTestHandler::Send, this, browser));
    } else if (request == "scripted") {
      SendBarrier(browser);
    } else {
      CheckReport(browser, request);
    }
    return true;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int http_status) override {
    RoutingTestHandler::OnLoadEnd(browser, frame, http_status);
    if (submitting_ && frame->IsMain()) {
      SCOPED_TRACE(Context());
      EXPECT_EQ(
          std::string(kUrl) + "/submitted?k=" +
              (scenario_ == Scenario::KeyboardCommitNavigation ? "osrimecommit"
               : scenario_ == Scenario::KeyboardFinishNavigation ? "done"
                                                                 : ""),
          frame->GetURL().ToString());
      EXPECT_EQ(200, http_status);
      EXPECT_TRUE(got_navigation_focus_);
      submission_loaded_ = true;
      MaybeFinishSubmission();
      return;
    }
    if (navigating_ && frame->IsMain()) {
      EXPECT_EQ(std::string(kUrl) + "/after", frame->GetURL().ToString());
      frame_ = frame;
      // The old editor and composition must not leak into the new document.
      Execute(browser, "window.testQuery({request:JSON.stringify({step:" +
                           std::to_string(step_) +
                           ",events:[],value:document.body.textContent,"
                           "selection:[0,0],primaryValue:''})})");
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    SCOPED_TRACE(Context());
    if (closing_during_composition_) {
      EXPECT_EQ(steps_.size() - 1, step_);
      ++step_;
    }
    EXPECT_EQ(steps_.size(), step_);
    devtools_registration_ = nullptr;
    frame_ = nullptr;
    root_frame_ = child_frame_ = nullptr;
    RoutingTestHandler::OnBeforeClose(browser);
  }

  void OnTextSelectionChanged(CefRefPtr<CefBrowser> browser,
                              const CefString& text,
                              const CefRange& range) override {
    if (step_ < steps_.size() && sent_ && steps_[step_].redact_selected_text) {
      SCOPED_TRACE(Context());
      EXPECT_TRUE(text.empty());
    }
    selected_text_ = text.ToString16();
    selected_range_ = range;
    ++selection_callback_count_;
    if (waiting_for_initial_selection_) {
      MaybePrepareInitialSelection(browser);
      return;
    }
    MaybeAdvance(browser);
  }

  void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser,
                                    const CefRange& range,
                                    const RectList& bounds) override {
    SCOPED_TRACE(Context());
    ++bounds_callback_count_;
    if (step_ >= steps_.size() || !sent_) {
      return;
    }
    const auto& expected = steps_[step_].composition;
    last_range_ = range;
    last_bounds_count_ = bounds.size();
    if (expected == CefRange::InvalidRange() || range != expected) {
      return;
    }
    EXPECT_EQ(expected.to - expected.from, bounds.size());
    for (const auto& rect : bounds) {
      EXPECT_GE(rect.x, origin_x_);
      EXPECT_LT(rect.x, origin_x_ + 300);
      EXPECT_GE(rect.y, origin_y_ - scroll_y_);
      EXPECT_LT(rect.y, origin_y_ + 40 - scroll_y_);
      EXPECT_GE(rect.width, 0);
      EXPECT_GT(rect.height, 0);
      EXPECT_LE(rect.height, 40);
    }
    if (steps_[step_].operation == Operation::Geometry) {
      ASSERT_EQ(geometry_reference_.size(), bounds.size());
      for (size_t i = 0; i < bounds.size(); ++i) {
        EXPECT_EQ(geometry_reference_[i].x + geometry_dx_, bounds[i].x);
        EXPECT_EQ(geometry_reference_[i].y + geometry_dy_, bounds[i].y);
        EXPECT_EQ(geometry_reference_[i].width, bounds[i].width);
        EXPECT_EQ(geometry_reference_[i].height, bounds[i].height);
      }
    }
    last_bounds_ = bounds;
    got_bounds_ = true;
    MaybeAdvance(browser);
  }

  void OnTestTimeout(int timeout_ms, bool treat_as_error) override {
    waiting_for_editor_ = false;
    SCOPED_TRACE(Context());
    ADD_FAILURE() << "IME step=" << step_ << ", reported=" << reported_
                  << ", got_bounds=" << got_bounds_ << ", last range=["
                  << last_range_.from << "," << last_range_.to
                  << "], last bounds count=" << last_bounds_count_
                  << ", selected range=[" << selected_range_.from << ","
                  << selected_range_.to
                  << "], waiting for ack=" << waiting_for_ack_
                  << ", got paint=" << got_paint_
                  << ", submission loaded=" << submission_loaded_
                  << ", submission focus=" << got_submission_focus_;
    RoutingTestHandler::OnTestTimeout(timeout_ms, treat_as_error);
  }

 private:
  // These cases require browser destruction or cleanup not yet implemented
  // for reusable leases. All other scenarios operate on fresh documents.
  bool CanReuseBrowser() const {
    switch (scenario_) {
      case Scenario::DestroyDuringComposition:
      case Scenario::MultipleUnderlines:
      case Scenario::UnderlineStyles:
      case Scenario::CompositionTextColor:
      case Scenario::PasswordUnicode:
      case Scenario::PasswordUnicodeChromium:
      case Scenario::SetCompositionRange:
      case Scenario::ResizeGeometry:
      case Scenario::SelectionExternalBeginFrame:
        return false;
      default:
        return true;
    }
  }

  void OnReusableBrowserReleased(CefRefPtr<CefBrowser> browser) override {
    EXPECT_EQ(steps_.size(), step_);
    devtools_registration_ = nullptr;
    frame_ = nullptr;
    root_frame_ = child_frame_ = nullptr;
    RoutingTestHandler::OnReusableBrowserReleased(browser);
  }

  std::string Context() const {
    const char* editor = editor_ == Editor::Input      ? "input"
                         : editor_ == Editor::Textarea ? "textarea"
                                                       : "contenteditable";
    return std::string(editor) + " scale=" + std::to_string(scale_) +
           " step=" + std::to_string(step_) +
           (IsCrossFrameMonitoring()
                ? (start_in_child_ ? " start=child" : " start=root")
                : "");
  }

  void BuildSteps(Scenario scenario) {
    auto set = [this](const std::u16string& text, CefRange selection,
                      bool start = true, const std::u16string& selected = {}) {
      steps_.push_back({Operation::Set, text, selection, text, selection,
                        CompositionEvents(text, start, false, selected),
                        CefRange(0, static_cast<uint32_t>(text.size()))});
    };
    auto commit = [this](const std::u16string& text, bool composition) {
      const auto end = static_cast<uint32_t>(text.size());
      std::vector<ExpectedEvent> events =
          composition
              ? CompositionEvents(text, false, true)
              : std::vector<ExpectedEvent>{{"beforeinput", text, "insertText"},
                                           {"input", text, "insertText"}};
      steps_.push_back({Operation::Commit, text, CefRange::InvalidRange(), text,
                        CefRange(end, end), events});
    };
    auto script = [this](const std::string& js, const std::u16string& value,
                         CefRange selection,
                         std::vector<ExpectedEvent> events = {}) {
      steps_.push_back({Operation::Script,
                        {},
                        CefRange::InvalidRange(),
                        value,
                        selection,
                        events});
      steps_.back().script = js;
    };
    auto finish = [this](Operation operation, const std::u16string& value,
                         CefRange selection,
                         std::vector<ExpectedEvent> events = {}) {
      steps_.push_back(
          {operation, {}, CefRange::InvalidRange(), value, selection, events});
    };
    switch (scenario) {
      case Scenario::Commit:
        commit(u"abc", false);
        break;
      case Scenario::CommitUnicode:
        commit(u"\u304b\u306a\U0001f642e\u0301", false);
        break;
      case Scenario::CompositionCommit:
        set(u"ka", CefRange(2, 2));
        commit(u"\u304b", true);
        break;
      case Scenario::UpdateCommit:
        set(u"k", CefRange(1, 1));
        set(u"kana", CefRange(1, 3), false);
        commit(u"\u304b\u306a", true);
        break;
      case Scenario::FinishKeep:
      case Scenario::FinishDiscard:
      case Scenario::OffThreadFinishKeep:
      case Scenario::OffThreadFinishDiscard: {
        const bool keep = scenario == Scenario::FinishKeep ||
                          scenario == Scenario::OffThreadFinishKeep;
        set(u"kana", CefRange(1, 3));
        finish(keep ? Operation::FinishKeep : Operation::FinishDiscard, u"kana",
               keep ? CefRange(1, 3) : CefRange(4, 4),
               {{"compositionend", u"kana"}});
        break;
      }
      case Scenario::Cancel:
      case Scenario::OffThreadCancel:
      case Scenario::EmptyUpdate:
        set(u"kana", CefRange(4, 4));
        steps_.push_back({scenario == Scenario::EmptyUpdate ? Operation::Set
                                                            : Operation::Cancel,
                          {},
                          CefRange(0, 0),
                          {},
                          CefRange(0, 0),
                          CompositionEvents({}, false, true)});
        break;
      case Scenario::ReplaceSelection:
        initial_ = u"prefixOLDsuffix";
        initial_selection_ = CefRange(6, 9);
        steps_.push_back({Operation::Set, u"ka", CefRange(1, 2),
                          u"prefixkasuffix", CefRange(7, 8),
                          CompositionEvents(u"ka", true, false, u"OLD"),
                          CefRange(6, 8)});
        steps_.push_back({Operation::Commit, u"\u304b",
                          CefRange::InvalidRange(), u"prefix\u304bsuffix",
                          CefRange(7, 7),
                          CompositionEvents(u"\u304b", false, true)});
        break;
      case Scenario::CommitSelection:
        initial_ = u"prefixOLDsuffix";
        initial_selection_ = CefRange(6, 9);
        steps_.push_back({Operation::Commit,
                          u"new",
                          CefRange::InvalidRange(),
                          u"prefixnewsuffix",
                          CefRange(9, 9),
                          {{"beforeinput", u"new", "insertText"},
                           {"input", u"new", "insertText"}}});
        break;
      case Scenario::CompositionUnicode:
        set(u"\u304b\u306a\U0001f642e\u0301", CefRange(4, 6));
        commit(u"\u304b\u306a\U0001f642e\u0301", true);
        break;
      case Scenario::RepeatSession:
        set(u"ka", CefRange(2, 2));
        commit(u"\u304b", true);
        steps_.push_back({Operation::Set, u"na", CefRange(2, 2), u"\u304bna",
                          CefRange(3, 3), CompositionEvents(u"na", true, false),
                          CefRange(1, 3)});
        steps_.push_back({Operation::Commit, u"\u306a",
                          CefRange::InvalidRange(), u"\u304b\u306a",
                          CefRange(2, 2),
                          CompositionEvents(u"\u306a", false, true)});
        break;
      case Scenario::EmptyCommit:
        commit({}, false);
        steps_.back().events.clear();
        commit(u"ok", false);
        break;
      case Scenario::EmptyCompositionCommit:
        set(u"ka", CefRange(2, 2));
        commit({}, true);
        commit(u"ok", false);
        break;
      case Scenario::EmptyCommitSelection:
        initial_ = u"abc";
        initial_selection_ = CefRange(1, 2);
        steps_.push_back(
            {Operation::Commit,
             {},
             CefRange::InvalidRange(),
             u"ac",
             CefRange(1, 1),
             {{"beforeinput", {}, "insertText"}, {"input", {}, "insertText"}}});
        break;
      case Scenario::EmptySetWithoutComposition:
        steps_.push_back(
            {Operation::Set, {}, CefRange(0, 0), {}, CefRange(0, 0), {}});
        commit(u"ok", false);
        break;
      case Scenario::EmptySetSelection:
        initial_ = u"abc";
        initial_selection_ = CefRange(1, 2);
        steps_.push_back({Operation::Set,
                          {},
                          CefRange(0, 0),
                          u"ac",
                          CefRange(1, 1),
                          {{"beforeinput", {}, "deleteContentBackward"},
                           {"input", {}, "deleteContentBackward"}}});
        break;
      case Scenario::FinishWithoutComposition:
        finish(Operation::FinishKeep, {}, CefRange(0, 0));
        finish(Operation::FinishDiscard, {}, CefRange(0, 0));
        commit(u"ok", false);
        break;
      case Scenario::CancelWithoutComposition:
        finish(Operation::Cancel, {}, CefRange(0, 0));
        commit(u"ok", false);
        break;
      case Scenario::RepeatedFinishCancel:
        set(u"ka", CefRange(2, 2));
        finish(Operation::FinishKeep, u"ka", CefRange(2, 2),
               {{"compositionend", u"ka"}});
        finish(Operation::FinishKeep, u"ka", CefRange(2, 2));
        finish(Operation::Cancel, u"ka", CefRange(2, 2));
        finish(Operation::Cancel, u"ka", CefRange(2, 2));
        break;
      case Scenario::OmittedSelection:
        set(u"kana", CefRange::InvalidRange());
        // CEF passes the sentinel endpoints as -1 to Blink, which clamps them
        // to the editable range rather than choosing an end-of-text caret.
        steps_.back().result_selection = CefRange(0, 0);
        commit(u"ok", true);
        break;
      case Scenario::OutOfBoundsSelection:
        set(u"kana", CefRange(100, 200));
        steps_.back().result_selection = CefRange(4, 4);
        commit(u"ok", true);
        break;
      case Scenario::ReversedSelection:
        set(u"kana", CefRange(3, 1));
        steps_.back().result_selection = CefRange(3, 3);
        commit(u"ok", true);
        break;
      case Scenario::ExplicitCompositionReplacement:
        initial_ = u"prefixOLDsuffix";
        initial_selection_ = CefRange(0, 0);
        set(u"ka", CefRange(1, 2), true, u"OLD");
        steps_.back().replacement = CefRange(6, 9);
        steps_.back().value = u"prefixkasuffix";
        steps_.back().result_selection = CefRange(7, 8);
        steps_.back().composition = CefRange(6, 8);
        finish(Operation::FinishDiscard, u"prefixkasuffix", CefRange(8, 8),
               {{"compositionend", u"ka"}});
        break;
      case Scenario::ExplicitCommitReplacement:
        initial_ = u"prefixOLDsuffix";
        initial_selection_ = CefRange(0, 0);
        commit(u"new", false);
        steps_.back().replacement = CefRange(6, 9);
        steps_.back().value = u"prefixnewsuffix";
        // Explicit replacement uses Blink's keep-selection operation.
        steps_.back().result_selection = CefRange(0, 0);
        break;
      case Scenario::OutOfBoundsReplacement:
        initial_ = u"abc";
        initial_selection_ = CefRange(1, 1);
        commit(u"X", false);
        steps_.back().replacement = CefRange(100, 200);
        steps_.back().value = u"abc";
        steps_.back().events.clear();
        steps_.back().result_selection = CefRange(1, 1);
        break;
      case Scenario::CursorBeforeEnd:
      case Scenario::CursorAfterEnd:
      case Scenario::CursorOutOfBounds:
      case Scenario::CursorBeforeStart:
        initial_ = u"abcd";
        initial_selection_ = CefRange(1, 1);
        commit(u"XY", false);
        steps_.back().value = u"aXYbcd";
        steps_.back().cursor = scenario == Scenario::CursorBeforeEnd     ? -1
                               : scenario == Scenario::CursorAfterEnd    ? 1
                               : scenario == Scenario::CursorBeforeStart ? -100
                                                                         : 100;
        steps_.back().result_selection =
            scenario == Scenario::CursorBeforeEnd     ? CefRange(2, 2)
            : scenario == Scenario::CursorAfterEnd    ? CefRange(4, 4)
            : scenario == Scenario::CursorBeforeStart ? CefRange(0, 0)
                                                      : CefRange(6, 6);
        break;
      case Scenario::NoUnderlines:
        set(u"ka", CefRange(2, 2));
        set(u"kana", CefRange(4, 4), false);
        commit(u"\u304b\u306a", true);
        break;
      case Scenario::InvalidUnderlineRanges:
        set(u"abcd", CefRange(4, 4));
        for (auto range :
             {CefRange(100, 200), CefRange(3, 1), CefRange(0, 0)}) {
          CefCompositionUnderline line;
          line.range = range;
          line.color = 0xFFFF0000;
          steps_.back().underlines.push_back(line);
        }
        commit(u"abcd", true);
        break;
      case Scenario::MultipleUnderlines:
      case Scenario::UnderlineStyles:
        set(u"abcd", CefRange(4, 4));
        for (uint32_t i = 0; i < 2; ++i) {
          CefCompositionUnderline line;
          line.range = CefRange(i * 2, i * 2 + 2);
          line.color = i ? 0xFF00FF00 : 0xFFFF0000;
          line.background_color = 0xFF0000FF;
          line.thick = i != 0;
          line.style = CEF_CUS_SOLID;
          steps_.back().underlines.push_back(line);
        }
        // Verify both span colors and backgrounds reach the rendered output.
        steps_.back().paint_colors = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF};
        steps_.back().compare_underline_mask = true;
        if (scenario == Scenario::UnderlineStyles) {
          for (auto style : {CEF_CUS_DOT, CEF_CUS_DASH, CEF_CUS_NONE}) {
            set(u"abcd", CefRange(4, 4), false);
            CefCompositionUnderline line;
            line.range = CefRange(0, 4);
            line.color = style == CEF_CUS_DOT ? 0xFFFF8000 : 0xFFFF00FF;
            line.background_color =
                style == CEF_CUS_NONE ? 0xFF00FFFF : 0xFF0000FF;
            line.style = style;
            line.thick = true;
            steps_.back().underlines = {line};
            steps_.back().paint_colors = {line.background_color};
            if (style != CEF_CUS_NONE || CompositionTextColorEnabled()) {
              steps_.back().paint_colors.push_back(line.color);
            } else {
              steps_.back().absent_paint_colors = {line.color};
            }
            steps_.back().compare_underline_mask = true;
            steps_.back().no_underline = style == CEF_CUS_NONE;
            steps_.back().check_bounds_callback = false;
          }
        }
        commit(u"abcd", true);
        break;
      case Scenario::CompositionTextColor: {
        set(u"abcd", CefRange(4, 4));
        CefCompositionUnderline line;
        line.range = CefRange(0, 4);
        line.color = 0xFFFF0000;
        line.background_color = 0xFF0000FF;
        line.style = CEF_CUS_NONE;
        steps_.back().underlines = {line};
        steps_.back().paint_colors = {line.background_color};
        // No underline can produce this red: it must come from the glyphs.
        if (CompositionTextColorEnabled()) {
          steps_.back().paint_colors.push_back(line.color);
        } else {
          steps_.back().absent_paint_colors = {line.color};
        }
        commit(u"abcd", true);
        break;
      }
      case Scenario::CancelBeforeInput:
        script("cancelInput = true", {}, CefRange(0, 0));
        commit(u"blocked", false);
        steps_.back().value.clear();
        steps_.back().result_selection = CefRange(0, 0);
        steps_.back().events = {
            {"beforeinput", u"blocked", "insertText", false, true}};
        script("cancelInput = false", {}, CefRange(0, 0));
        commit(u"ok", false);
        break;
      case Scenario::CancelCompositionBeforeInput:
        script("cancelInput = true", {}, CefRange(0, 0));
        set(u"ka", CefRange(2, 2));
        // Composition beforeinput is non-cancelable. preventDefault() has no
        // effect, and the normal input event and insertion still occur.
        commit(u"\u304b", true);
        break;
      case Scenario::CancelCompositionStart:
        script("cancelStart = true", {}, CefRange(0, 0));
        set(u"ka", CefRange(2, 2));
        // Chromium ignores the cancellation result of compositionstart.
        steps_.back().events.front().prevented = true;
        script("cancelStart = false", u"ka", CefRange(2, 2));
        set(u"kana", CefRange(4, 4), false);
        commit(u"\u304b", true);
        break;
      case Scenario::ReadOnly:
      case Scenario::Disabled:
        script(editor_ == Editor::ContentEditable
                   ? "editor.contentEditable = 'false'"
               : scenario == Scenario::ReadOnly ? "editor.readOnly = true"
                                                : "editor.disabled = true",
               {}, CefRange(0, 0));
        commit(u"blocked", false);
        steps_.back().value.clear();
        steps_.back().events =
            editor_ == Editor::ContentEditable || scenario == Scenario::Disabled
                ? std::vector<ExpectedEvent>()
                : std::vector<ExpectedEvent>{
                      {"beforeinput", u"blocked", "insertText"}};
        steps_.back().result_selection = CefRange(0, 0);
        break;
      case Scenario::MaxLengthCommit:
      case Scenario::MaxLengthComposition:
        script("editor.maxLength = 3", {}, CefRange(0, 0));
        if (scenario == Scenario::MaxLengthCommit) {
          commit(u"abcde", false);
          steps_.back().value = u"abc";
          steps_.back().result_selection = CefRange(3, 3);
          steps_.back().events.back().data = u"abc";
        } else {
          set(u"abcde", CefRange(5, 5));
          finish(Operation::FinishDiscard, u"abc", CefRange(3, 3),
                 CompositionEvents(u"abcde", false, true));
          steps_.back().events[2].data = u"abc";
        }
        break;
      case Scenario::Multiline:
        commit(u"a\nb", false);
        if (editor_ == Editor::Input) {
          steps_.back().value = u"a b";
          steps_.back().events.back().data = u"a b";
        } else {
          steps_.back().events = {{"beforeinput", u"a\nb", "insertText"},
                                  {"input", u"a", "insertText"},
                                  {"input", {}, "insertText"},
                                  {"input", u"b", "insertText"}};
        }
        break;
      case Scenario::LongText:
        // Exercise lengths well beyond keyboard events' inline text buffer.
        commit(std::u16string(4096, u'x'), false);
        break;
      case Scenario::OffThreadCommit:
        commit(u"\u304b\u306a\U0001f642", false);
        break;
      case Scenario::OffThreadComposition:
        set(u"kana", CefRange(4, 4));
        commit(u"\u304b\u306a", true);
        break;
      case Scenario::FocusOtherEditor:
        set(u"ka", CefRange(2, 2));
        script("other.focus(); editor = other", {}, CefRange(0, 0),
               {{"compositionend", u"ka"}});
        steps_.back().primary_value = u"ka";
        commit(u"new", false);
        steps_.back().primary_value = u"ka";
        break;
      case Scenario::BlurDuringComposition:
        set(u"ka", CefRange(2, 2));
        finish(Operation::Blur, u"ka", CefRange(2, 2),
               {{"compositionend", u"ka"}});
        finish(Operation::Refocus, u"ka", CefRange(2, 2));
        finish(Operation::Cancel, u"ka", CefRange(2, 2));
        break;
      case Scenario::RemoveDuringComposition:
        set(u"ka", CefRange(2, 2));
        script("editor.remove()", u"ka",
               editor_ == Editor::ContentEditable ? CefRange::InvalidRange()
                                                  : CefRange(2, 2));
        finish(Operation::Cancel, u"ka",
               editor_ == Editor::ContentEditable ? CefRange::InvalidRange()
                                                  : CefRange(2, 2));
        break;
      case Scenario::NavigateDuringComposition:
      case Scenario::DestroyDuringComposition:
        set(u"ka", CefRange(2, 2));
        finish(scenario == Scenario::NavigateDuringComposition
                   ? Operation::Navigate
                   : Operation::Destroy,
               {}, CefRange(0, 0));
        break;
      case Scenario::SameOriginIframe:
      case Scenario::CrossOriginIframe:
      case Scenario::CrossOriginIframeChromium:
        set(u"ka", CefRange(2, 2));
        if (scenario != Scenario::CrossOriginIframeChromium) {
          set(u"ka", CefRange(0, 1), false);
          steps_.back().check_bounds_callback = false;
        }
        commit(u"\u304b", true);
        if (scenario != Scenario::CrossOriginIframeChromium) {
          steps_.push_back({Operation::Set, u"na", CefRange(0, 1), u"\u304bna",
                            CefRange(1, 2),
                            CompositionEvents(u"na", true, false),
                            CefRange(1, 3)});
          finish(Operation::FinishDiscard, u"\u304bna", CefRange(3, 3),
                 {{"compositionend", u"na"}});
          steps_.push_back({Operation::Set, u"tmp", CefRange(3, 3),
                            u"\u304bnatmp", CefRange(6, 6),
                            CompositionEvents(u"tmp", true, false),
                            CefRange(3, 6)});
          finish(Operation::Cancel, u"\u304bna", CefRange(3, 3),
                 CompositionEvents({}, false, true));
        }
        break;
      case Scenario::MonitoringCancelAcrossFrames:
      case Scenario::MonitoringCommitAcrossFrames:
      case Scenario::MonitoringFinishAcrossFrames:
      case Scenario::MonitoringRetargetAcrossFrames: {
        set(u"ka", CefRange(2, 2));
        const auto away =
            start_in_child_ ? Operation::FocusRoot : Operation::FocusChild;
        const auto back =
            start_in_child_ ? Operation::FocusChild : Operation::FocusRoot;
        finish(away, {}, CefRange(0, 0));
        if (scenario == Scenario::MonitoringRetargetAcrossFrames) {
          set(u"na", CefRange(2, 2));
        } else if (scenario == Scenario::MonitoringCommitAcrossFrames) {
          commit(u"new", false);
        } else {
          finish(scenario == Scenario::MonitoringCancelAcrossFrames
                     ? Operation::Cancel
                     : Operation::FinishKeep,
                 {}, CefRange(0, 0));
        }
        finish(back, u"ka", CefRange(2, 2));
        auto unmonitored = [&]() {
          // Scripted cross-frame focus preserves the old composition. Change
          // its length so Chromium cannot suppress identical bounds updates.
          set(u"zzzz", CefRange(4, 4), false);
          auto& step = steps_.back();
          step.operation = Operation::ChromiumSet;
          step.composition = CefRange::InvalidRange();
          step.no_bounds_callback = true;
        };
        unmonitored();
        // The public API must restart monitoring on this widget and report the
        // new bounds after the preceding DevTools-only composition.
        set(u"ok!", CefRange(0, 1), false);
        if (scenario == Scenario::MonitoringRetargetAcrossFrames) {
          finish(away, u"na", CefRange(2, 2));
          unmonitored();
          set(u"ok!", CefRange(0, 1), false);
          finish(Operation::FinishDiscard, u"ok!", CefRange(3, 3),
                 {{"compositionend", u"ok!"}});
        } else {
          finish(Operation::FinishDiscard, u"ok!", CefRange(3, 3),
                 {{"compositionend", u"ok!"}});
        }
        break;
      }
      case Scenario::ScrollGeometry:
      case Scenario::ResizeGeometry:
        set(u"ka", CefRange(2, 2));
        steps_.push_back({Operation::Geometry,
                          {},
                          CefRange::InvalidRange(),
                          u"ka",
                          CefRange(2, 2),
                          {},
                          CefRange(0, 2)});
        commit(u"\u304b", true);
        break;
      case Scenario::SelectionCallbacks:
      case Scenario::SelectionExternalBeginFrame:
      case Scenario::PasswordSelection:
        if (scenario == Scenario::PasswordSelection &&
            editor_ == Editor::Input) {
          script("editor.type = 'password'", {}, CefRange(0, 0));
        }
        set(u"ka", CefRange(0, 2));
        set(u"kana", CefRange(1, 3), false);
        set(u"kana", CefRange(1, 3), false);
        steps_.back().check_bounds_callback = false;
        steps_.back().unchanged_selection_callback = true;
        finish(Operation::FinishKeep, u"kana", CefRange(1, 3),
               {{"compositionend", u"kana"}});
        steps_.back().unchanged_selection_callback = true;
        commit(u"X", false);
        steps_.back().value = u"kXa";
        steps_.back().result_selection = CefRange(2, 2);
        steps_.back().check_selection_callback = true;
        if (scenario == Scenario::PasswordSelection &&
            editor_ == Editor::Input) {
          // Password rendering can give Chromium's plain-text offsets a
          // different relationship to DOM offsets for surrogate/combining
          // characters. Keep that limitation separate from feedback delivery.
          break;
        }
        // The range alone cannot identify a selection: text changes can keep
        // the same offsets, including surrogate pairs and combining marks.
        set(u"\U0001f642e\u0301", CefRange(0, 2), true, u"");
        steps_.back().value = u"kX\U0001f642e\u0301a";
        steps_.back().result_selection = CefRange(2, 4);
        steps_.back().composition = CefRange(2, 6);
        set(u"\u304b\u306ae\u0301", CefRange(0, 2), false);
        steps_.back().value = u"kX\u304b\u306ae\u0301a";
        steps_.back().result_selection = CefRange(2, 4);
        steps_.back().composition = CefRange(2, 6);
        finish(Operation::Cancel, u"kXa", CefRange(2, 2),
               CompositionEvents({}, false, true));
        if (editor_ == Editor::ContentEditable) {
          steps_.back().events[2].data = u"\u304b\u306ae\u0301";
        }
        steps_.back().check_selection_callback = true;
        break;
      case Scenario::SelectionFocus:
        set(u"ka", CefRange(0, 2));
        finish(Operation::FinishKeep, u"ka", CefRange(0, 2),
               {{"compositionend", u"ka"}});
        steps_.back().unchanged_selection_callback = true;
        // A second editor has identical text/range but must report its own
        // selection. Switching back exercises the cache identity again.
        script("other.focus(); editor = other; setEditorText('ka',0,2)", u"ka",
               CefRange(0, 2));
        steps_.back().check_selection_callback = true;
        steps_.back().primary_value = u"ka";
        script("primary.focus(); editor = primary; setEditorText('ka',0,2)",
               u"ka", CefRange(0, 2));
        steps_.back().check_selection_callback = true;
        break;
      case Scenario::PasswordUnicode:
      case Scenario::PasswordUnicodeChromium:
        initial_ = u"kXa";
        initial_selection_ = CefRange(2, 2);
        script("editor.type = 'password'", initial_, initial_selection_);
        set(u"\U0001f642e\u0301", CefRange(0, 2));
        steps_.back().value = u"kX\U0001f642e\u0301a";
        // Characterize Chromium's masked-text offsets separately from DOM
        // offsets. The DevTools control establishes ownership without a CEF
        // workaround that changes injection or callback behavior.
        steps_.back().result_selection = CefRange(2, 5);
        steps_.back().composition = CefRange(2, 4);
        steps_.back().feedback_selection = CefRange(2, 4);
        steps_.back().check_selection_callback = true;
        if (scenario == Scenario::PasswordUnicodeChromium) {
          // DevTools does not request composition-bounds monitoring. Compare
          // DOM selection and TextInputState feedback without a CEF command.
          steps_.back().composition = CefRange::InvalidRange();
        }
        break;
      case Scenario::PasswordTypeChange:
        initial_ = u"secret";
        initial_selection_ = CefRange(0, 6);
        script("editor.type = 'password'", initial_, initial_selection_);
        steps_.back().check_selection_callback = true;
        steps_.back().redact_selected_text = true;
        script("editor.type = 'text'; editor.setSelectionRange(1,5)", initial_,
               CefRange(1, 5));
        steps_.back().check_selection_callback = true;
        steps_.back().redact_selected_text = true;
        script("other.focus(); editor = other; setEditorText('public',0,6)",
               u"public", CefRange(0, 6));
        steps_.back().check_selection_callback = true;
        script("editor.style.webkitTextSecurity = 'disc'", u"public",
               CefRange(0, 6));
        steps_.back().check_selection_callback = true;
        steps_.back().redact_selected_text = true;
        script(
            "editor.style.webkitTextSecurity = ''; "
            "editor.setSelectionRange(1,5)",
            u"public", CefRange(1, 5));
        steps_.back().check_selection_callback = true;
        steps_.back().redact_selected_text = true;
        break;
      case Scenario::PasswordIframe:
      case Scenario::PasswordCrossOriginIframe:
        script("editor.type = 'password'", {}, CefRange(0, 0));
        set(u"secret", CefRange(1, 5));
        finish(Operation::FinishKeep, u"secret", CefRange(1, 5),
               {{"compositionend", u"secret"}});
        steps_.back().check_selection_callback = true;
        break;
      case Scenario::SetCompositionRange:
      case Scenario::CancelNavigation:
        if (scenario == Scenario::CancelNavigation) {
          auto events = CompositionEvents(u"\u304b", true, false);
          const auto cancel_events = CompositionEvents({}, false, true);
          events.insert(events.end(), cancel_events.begin(),
                        cancel_events.end());
          finish(Operation::SetCancel, {}, CefRange(0, 0), events);
          finish(Operation::Submit, {}, CefRange(0, 0));
        } else {
          set(u"\u304b", CefRange(0, 1));
          steps_.back().replacement = CefRange(0, 1);
        }
        break;
      case Scenario::KeyboardCommitNavigation:
      case Scenario::KeyboardFinishNavigation: {
        // Finish deliberately follows ordinary keys without an active
        // composition, matching the legacy preservation/submission test.
        finish(scenario == Scenario::KeyboardCommitNavigation
                   ? Operation::KeysCommit
                   : Operation::KeysFinish,
               u"done", CefRange(4, 4));
        for (char16_t c : std::u16string(u"done")) {
          const std::u16string text(1, c);
          auto& events = steps_.back().events;
          events.push_back({"keydown"});
          events.push_back({"keypress"});
          events.push_back({"beforeinput", text, "insertText"});
          events.push_back({"input", text, "insertText"});
        }
        if (scenario == Scenario::KeyboardCommitNavigation) {
          steps_.back().text = u"osrimecommit";
          steps_.back().value = steps_.back().text;
          steps_.back().replacement = CefRange(0, 12);
          steps_.back().events.push_back(
              {"beforeinput", u"osrimecommit", "insertText"});
          steps_.back().events.push_back(
              {"input", u"osrimecommit", "insertText"});
        }
        finish(Operation::Submit, {}, CefRange(0, 0));
        break;
      }
    }
    // Editable text-input state supplements legacy selection notifications.
    // Verify selection feedback throughout successful composition operations.
    auto previous_selection = initial_selection_;
    for (auto& step : steps_) {
      if (editor_ == Editor::Input &&
          (scenario == Scenario::PasswordSelection ||
           scenario == Scenario::PasswordUnicode ||
           scenario == Scenario::PasswordUnicodeChromium ||
           scenario == Scenario::PasswordIframe ||
           scenario == Scenario::PasswordCrossOriginIframe)) {
        step.redact_selected_text = true;
      }
      if (step.composition != CefRange::InvalidRange() ||
          (step.operation == Operation::FinishDiscard && !step.events.empty() &&
           step.result_selection != previous_selection)) {
        step.check_selection_callback = true;
      }
      previous_selection = step.result_selection;
      if (scenario == Scenario::CrossOriginIframeChromium) {
        // This control checks Chromium routing and DOM behavior; CEF's OSR
        // composition callback is independently checked by the public path.
        step.composition = CefRange::InvalidRange();
      }
    }
  }

  void Execute(CefRefPtr<CefBrowser> browser, const std::string& script) {
    (frame_ ? frame_ : browser->GetMainFrame())
        ->ExecuteJavaScript(script, kUrl, 0);
  }

  void FocusEditor(CefRefPtr<CefBrowser> browser) {
    if (!waiting_for_editor_) {
      return;
    }
    if (osr_) {
      // A paint or scripted focus does not guarantee that input is enabled or
      // that child-frame hit-test data is installed. Require a DOM click before
      // dispatching test input, retrying only this non-mutating setup probe.
      CefMouseEvent event;
      event.x = 50;
      event.y = 50;
      if (IsCrossFrameMonitoring()) {
        event.x = origin_x_ + 10;
        event.y = origin_y_ + 10;
      } else if (IsIframe()) {
        event.x += 30;
        event.y += 20;
      }
      // Keep each probe's down/up together. The shared click helper delays
      // them independently, which would interleave successive retry pairs.
      auto host = browser->GetHost();
      host->SendMouseClickEvent(event, MBT_LEFT, false, 1);
      host->SendMouseClickEvent(event, MBT_LEFT, true, 1);
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&ImeInputTestHandler::FocusEditor, this, browser), 50);
    } else {
      Execute(browser, "editor.focus();notifyEditorReady()");
    }
  }

  void MaybePrepareInitialSelection(CefRefPtr<CefBrowser> browser) {
    if (selected_range_ != initial_selection_ || selected_text_ != initial_) {
      return;
    }
    waiting_for_initial_selection_ = false;
    Prepare(browser);
  }

  void Prepare(CefRefPtr<CefBrowser> browser) {
    sent_ = reported_ = got_bounds_ = got_paint_ = false;
    selection_count_before_ = selection_callback_count_;
    if (!steps_[step_].check_bounds_callback) {
      got_bounds_ = true;
    }
    Execute(browser, "prepare(" + std::to_string(step_) + ")");
  }

  void SendBeginFrame() {
    auto browser = GetBrowser();
    if (!browser || step_ >= steps_.size()) {
      return;
    }
    browser->GetHost()->SendExternalBeginFrame();
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&ImeInputTestHandler::SendBeginFrame, this), 16);
  }

  void SendBarrier(CefRefPtr<CefBrowser> browser) {
    // Ime* and keyboard dispatch share the renderer's widget input queue. The
    // unhandled key-up acknowledgement observes processing even when the IME
    // command intentionally emits no DOM event. A key-up inserts no text.
    waiting_for_ack_ = true;
#if CEF_API_ADDED(CEF_NEXT)
    if (keyboard_api_ == KeyboardApi::Portable) {
      CefKeyboardInputEvent event;
      event.type = CEF_KEYEVENT_KEYUP;
      event.dom_code = CEF_DOM_CODE_US_A;
      event.dom_key = CEF_DOM_KEY_UNICODE_FLAG | 'a';
      event.windows_key_code = 'A';
      event.text[0] = event.unmodified_text[0] = 'a';
      browser->GetHost()->SendKeyboardEvent(event);
      return;
    }
#endif
    CefKeyEvent event;
    event.type = KEYEVENT_KEYUP;
    event.windows_key_code = 'A';
    event.character = event.unmodified_character = 'a';
#if defined(OS_MAC)
    event.native_key_code = 0;  // kVK_ANSI_A
#elif defined(OS_LINUX)
    event.native_key_code = 'a';  // XKB_KEY_a
#else
    event.native_key_code = (0x1E << 16) | 0xC0000001;
#endif
    browser->GetHost()->SendKeyEvent(event);
  }

  void AcknowledgeBarrier(CefRefPtr<CefBrowser> browser,
                          cef_key_event_type_t type,
                          int windows_key_code) {
    if (type == KEYEVENT_KEYUP && windows_key_code == 'A' && waiting_for_ack_) {
      waiting_for_ack_ = false;
      Execute(browser, "report()");
    }
  }

  void Send(CefRefPtr<CefBrowser> browser) {
    sent_ = true;
    bounds_count_before_ = bounds_callback_count_;
    const auto& step = steps_[step_];
    if (scenario_ == Scenario::OffThreadCommit ||
        scenario_ == Scenario::OffThreadComposition ||
        scenario_ == Scenario::OffThreadFinishKeep ||
        scenario_ == Scenario::OffThreadFinishDiscard ||
        scenario_ == Scenario::OffThreadCancel) {
      CefPostTask(TID_IO, base::BindOnce(&ImeInputTestHandler::SendOffThread,
                                         this, browser, step));
      return;
    }
    auto host = browser->GetHost();
    if (step.operation == Operation::ChromiumSet ||
        ((scenario_ == Scenario::CrossOriginIframeChromium ||
          scenario_ == Scenario::PasswordUnicodeChromium) &&
         (step.operation == Operation::Set ||
          step.operation == Operation::Commit))) {
      // Chromium's DevTools path resolves the focused widget before issuing
      // the same renderer IME operations. This is a positive routing control,
      // not a replacement for testing the public CEF APIs.
      if (!devtools_registration_) {
        devtools_registration_ = host->AddDevToolsMessageObserver(this);
      }
      auto params = CefDictionaryValue::Create();
      params->SetString("text", CefString(step.text));
      if (step.operation != Operation::Commit) {
        params->SetInt("selectionStart", step.selection.from);
        params->SetInt("selectionEnd", step.selection.to);
      }
      // IDs belong to the browser's DevTools session, which can outlive this
      // handler when reused. Let CEF allocate the next ID across leases.
      devtools_id_ = host->ExecuteDevToolsMethod(
          0,
          step.operation == Operation::Commit ? "Input.insertText"
                                              : "Input.imeSetComposition",
          params);
      EXPECT_GT(devtools_id_, 0);
      return;
    }
    switch (step.operation) {
      case Operation::ChromiumSet:
        ADD_FAILURE() << "DevTools composition was not dispatched";
        return;
      case Operation::FocusRoot:
      case Operation::FocusChild:
        frame_ =
            step.operation == Operation::FocusRoot ? root_frame_ : child_frame_;
        origin_x_ = step.operation == Operation::FocusRoot ? 40 : 70;
        origin_y_ = step.operation == Operation::FocusRoot ? 40 : 140;
        Execute(browser,
                "step=" + std::to_string(step_) +
                    ";events=[];editor.focus();"
                    "requestAnimationFrame(() => requestAnimationFrame(() => "
                    "window.testQuery({request:'scripted'})))");
        return;
      case Operation::KeysCommit:
      case Operation::KeysFinish:
        SendTypedWord(browser);
        if (step.operation == Operation::KeysCommit) {
          host->ImeCommitText(CefString(step.text), step.replacement, 0);
        } else {
          host->ImeFinishComposingText(true);
        }
        break;
      case Operation::SetCancel: {
        CefCompositionUnderline line;
        line.range = CefRange(0, 1);
        line.color = 0xFF000000;
        host->ImeSetComposition(CefString(u"\u304b"), {line}, CefRange(0, 1),
                                CefRange(0, 1));
        host->ImeCancelComposition();
        break;
      }
      case Operation::Submit:
        submitting_ = true;
        got_submission_focus_ = false;
        Execute(browser, "document.getElementById('submit').click()");
        return;
      case Operation::Set: {
        std::vector<CefCompositionUnderline> underlines;
        if (!step.underlines.empty()) {
          underlines = step.underlines;
        } else if (!step.text.empty() && scenario_ != Scenario::NoUnderlines) {
          CefCompositionUnderline line;
          line.range = CefRange(0, static_cast<uint32_t>(step.text.size()));
          line.color = 0xFF000000;
          underlines.push_back(line);
        }
        host->ImeSetComposition(CefString(step.text), underlines,
                                step.replacement, step.selection);
        break;
      }
      case Operation::Commit:
        host->ImeCommitText(CefString(step.text), step.replacement,
                            step.cursor);
        break;
      case Operation::FinishKeep:
      case Operation::FinishDiscard:
        host->ImeFinishComposingText(step.operation == Operation::FinishKeep);
        break;
      case Operation::Cancel:
        host->ImeCancelComposition();
        break;
      case Operation::Script:
        if (scenario_ == Scenario::Disabled) {
          // Disabling a focused control can defer blur until the next render
          // lifecycle. Observe settled focus rather than racing that update.
          Execute(browser, step.script +
                               ";requestAnimationFrame(() => "
                               "requestAnimationFrame(() => "
                               "window.testQuery({request:'scripted'})))");
        } else {
          Execute(browser,
                  step.script + ";window.testQuery({request:'scripted'})");
        }
        return;
      case Operation::Blur:
        host->SetFocus(false);
        break;
      case Operation::Refocus:
        host->SetFocus(true);
        break;
      case Operation::Geometry:
        geometry_reference_ = last_bounds_;
        if (scenario_ == Scenario::ResizeGeometry) {
          geometry_dx_ = 200;
          origin_x_ = 240;
          view_width_ = 600;
          host->WasResized();
        } else {
          geometry_dy_ = -20;
          scroll_y_ = 20;
          Execute(browser,
                  "document.body.style.height='600px';"
                  "window.scrollTo(0,20);"
                  "window.testQuery({request:'scripted'})");
          return;
        }
        break;
      case Operation::Navigate:
        navigating_ = true;
        AddResource(std::string(kUrl) + "/after", "<body></body>", "text/html");
        browser->GetMainFrame()->LoadURL(std::string(kUrl) + "/after");
        return;
      case Operation::Destroy:
        closing_during_composition_ = true;
        DestroyTest();
        return;
    }
    SendBarrier(browser);
  }

  void SendOffThread(CefRefPtr<CefBrowser> browser, Step step) {
    CefString text(step.text);
    std::vector<CefCompositionUnderline> underlines;
    if (step.operation == Operation::Set) {
      CefCompositionUnderline line;
      line.range = CefRange(0, static_cast<uint32_t>(step.text.size()));
      line.color = 0xFF000000;
      underlines.push_back(line);
      browser->GetHost()->ImeSetComposition(text, underlines, step.replacement,
                                            step.selection);
    } else if (step.operation == Operation::Commit) {
      browser->GetHost()->ImeCommitText(text, step.replacement, step.cursor);
    } else if (step.operation == Operation::Cancel) {
      browser->GetHost()->ImeCancelComposition();
    } else {
      browser->GetHost()->ImeFinishComposingText(step.operation ==
                                                 Operation::FinishKeep);
    }
    // Off-thread calls must own their snapshots before posting to the UI
    // thread. Destroy/mutate the caller's original strings and spans now.
    text = "mutated";
    underlines.clear();
    CefPostTask(TID_UI, base::BindOnce(&ImeInputTestHandler::SendBarrier, this,
                                       browser));
  }

  void SendTypedWord(CefRefPtr<CefBrowser> browser) {
#if CEF_API_ADDED(CEF_NEXT)
    if (keyboard_api_ == KeyboardApi::Portable) {
      constexpr uint32_t codes[] = {CEF_DOM_CODE_US_D, CEF_DOM_CODE_US_O,
                                    CEF_DOM_CODE_US_N, CEF_DOM_CODE_US_E};
      const std::u16string word = u"done";
      for (size_t i = 0; i < word.size(); ++i) {
        CefKeyboardInputEvent event;
        event.dom_code = codes[i];
        event.dom_key = CEF_DOM_KEY_UNICODE_FLAG | word[i];
        event.text[0] = event.unmodified_text[0] = word[i];
        for (const auto type :
             {CEF_KEYEVENT_RAWKEYDOWN, CEF_KEYEVENT_CHAR, CEF_KEYEVENT_KEYUP}) {
          event.type = type;
          event.windows_key_code =
              type == CEF_KEYEVENT_CHAR ? word[i] : word[i] - u'a' + 'A';
          browser->GetHost()->SendKeyboardEvent(event);
        }
      }
      return;
    }
#endif
    // Native codes for D/O/N/E, matching the legacy OSR keyboard sequence.
#if defined(OS_MAC)
    constexpr int native_codes[] = {2, 31, 45, 14};
#elif defined(OS_WIN)
    constexpr int native_codes[] = {0x20, 0x18, 0x31, 0x12};
#endif
    const std::u16string word = u"done";
    for (size_t i = 0; i < word.size(); ++i) {
      CefKeyEvent event;
      event.windows_key_code = word[i] - u'a' + 'A';
      event.character = event.unmodified_character = word[i];
#if defined(OS_MAC)
      event.native_key_code = native_codes[i];
#elif defined(OS_WIN)
      event.native_key_code = (native_codes[i] << 16) | 1;
#else
      event.native_key_code = word[i];  // XKB keysym.
#endif
      event.type = KEYEVENT_RAWKEYDOWN;
      browser->GetHost()->SendKeyEvent(event);
      event.type = KEYEVENT_CHAR;
#if defined(OS_WIN)
      event.windows_key_code = word[i];
#endif
      browser->GetHost()->SendKeyEvent(event);
      event.type = KEYEVENT_KEYUP;
#if defined(OS_WIN)
      event.windows_key_code = word[i] - u'a' + 'A';
      event.native_key_code |= 0xC0000000;
#endif
      browser->GetHost()->SendKeyEvent(event);
    }
  }

  void MaybeFinishSubmission() {
    if (!submission_loaded_ || !got_submission_focus_ ||
        step_ >= steps_.size()) {
      return;
    }
    EXPECT_EQ(Operation::Submit, steps_[step_].operation);
    EXPECT_EQ(steps_.size() - 1, step_);
    ++step_;
    CefPostTask(TID_UI,
                base::BindOnce(&ImeInputTestHandler::DestroyTest, this));
  }

  void CheckReport(CefRefPtr<CefBrowser> browser, const CefString& request) {
    SCOPED_TRACE(Context());
    auto parsed = CefParseJSON(request, JSON_PARSER_RFC);
    if (!parsed || parsed->GetType() != VTYPE_DICTIONARY) {
      ADD_FAILURE() << "Invalid IME report: " << request.ToString();
      DestroyTest();
      return;
    }
    SCOPED_TRACE(request.ToString());
    auto result = parsed->GetDictionary();
    ASSERT_LT(step_, steps_.size());
    const auto& step = steps_[step_];
    if (scenario_ == Scenario::Disabled) {
      EXPECT_NE("editor", result->GetString("focused").ToString());
    }
    EXPECT_TRUE(sent_);
    EXPECT_FALSE(reported_);
    EXPECT_EQ(static_cast<int>(step_), result->GetInt("step"));
    EXPECT_EQ(step.value, result->GetString("value").ToString16());
    auto selection = result->GetList("selection");
    ASSERT_TRUE(selection);
    ASSERT_EQ(2U, selection->GetSize());
    EXPECT_EQ(step.result_selection.from,
              static_cast<uint32_t>(selection->GetInt(0)));
    EXPECT_EQ(step.result_selection.to,
              static_cast<uint32_t>(selection->GetInt(1)));
    auto events = result->GetList("events");
    ASSERT_TRUE(events);
    EXPECT_EQ(step.events.size(), events->GetSize());
    for (size_t i = 0; i < events->GetSize() && i < step.events.size(); ++i) {
      SCOPED_TRACE(i);
      auto event = events->GetDictionary(i);
      ASSERT_TRUE(event);
      const auto& expected = step.events[i];
      EXPECT_EQ(expected.type, event->GetString("type").ToString());
      EXPECT_EQ(expected.data, event->GetString("data").ToString16());
      EXPECT_EQ(expected.input_type, event->GetString("inputType").ToString());
      EXPECT_EQ(expected.composing, event->GetBool("composing"));
      EXPECT_EQ(expected.prevented, event->GetBool("prevented"));
    }
    if (step.primary_value) {
      EXPECT_EQ(*step.primary_value,
                result->GetString("primaryValue").ToString16());
    }
    reported_ = true;
    if (step.no_bounds_callback) {
      EXPECT_EQ(bounds_count_before_, bounds_callback_count_);
    }
    if (step.unchanged_selection_callback) {
      EXPECT_EQ(selection_count_before_, selection_callback_count_);
    }
    MaybeAdvance(browser);
  }

  void MaybeAdvance(CefRefPtr<CefBrowser> browser) {
    if (step_ >= steps_.size() || !reported_ ||
        (osr_ && steps_[step_].composition != CefRange::InvalidRange() &&
         !got_bounds_) ||
        (!steps_[step_].paint_colors.empty() && !got_paint_)) {
      return;
    }
    if (steps_[step_].check_selection_callback) {
      const auto& step = steps_[step_];
      const auto selection =
          step.feedback_selection.value_or(step.result_selection);
      const auto text = step.redact_selected_text
                            ? std::u16string()
                            : step.value.substr(selection.from,
                                                selection.to - selection.from);
      if (selected_range_ != selection || selected_text_ != text ||
          (scenario_ == Scenario::SelectionFocus &&
           step.operation == Operation::Script &&
           selection_callback_count_ == selection_count_before_)) {
        return;
      }
    }
    if (++step_ == steps_.size()) {
      DestroyTest();
      return;
    }
    Prepare(browser);
  }

  bool IsIframe() const {
    return scenario_ == Scenario::SameOriginIframe ||
           scenario_ == Scenario::CrossOriginIframe ||
           scenario_ == Scenario::CrossOriginIframeChromium ||
           scenario_ == Scenario::PasswordIframe ||
           scenario_ == Scenario::PasswordCrossOriginIframe;
  }

  bool IsCrossFrameMonitoring() const {
    return ::IsCrossFrameMonitoring(scenario_);
  }

  bool IsFormNavigation() const {
    return scenario_ == Scenario::KeyboardCommitNavigation ||
           scenario_ == Scenario::KeyboardFinishNavigation ||
           scenario_ == Scenario::CancelNavigation;
  }

  const Scenario scenario_;
  const Editor editor_;
  const float scale_;
  const bool osr_;
  const KeyboardApi keyboard_api_;
  const bool start_in_child_;
  std::u16string initial_;
  CefRange initial_selection_{0, 0};
  std::vector<Step> steps_;
  size_t step_ = 0;
  bool waiting_for_paint_ = false;
  bool waiting_for_editor_ = true;
  bool waiting_for_initial_selection_ = false;
  bool sent_ = false;
  bool reported_ = false;
  bool got_bounds_ = false;
  CefRange last_range_ = CefRange::InvalidRange();
  size_t last_bounds_count_ = 0;
  CefRefPtr<CefFrame> frame_;
  CefRefPtr<CefRegistration> devtools_registration_;
  int devtools_id_ = 0;
  bool setting_color_scheme_ = false;
  CefRange selected_range_ = CefRange::InvalidRange();
  std::u16string selected_text_;
  size_t selection_callback_count_ = 0;
  size_t selection_count_before_ = 0;
  bool waiting_for_ack_ = false;
  bool got_paint_ = false;
  bool navigating_ = false;
  bool closing_during_composition_ = false;
  bool submitting_ = false;
  bool submission_loaded_ = false;
  bool got_navigation_focus_ = false;
  bool got_submission_focus_ = false;
  int view_width_ = 400;
  int origin_x_ = 40;
  int origin_y_ = 40;
  size_t bounds_callback_count_ = 0;
  size_t bounds_count_before_ = 0;
  CefRefPtr<CefFrame> root_frame_;
  CefRefPtr<CefFrame> child_frame_;
  int scroll_y_ = 0;
  int geometry_dx_ = 0;
  int geometry_dy_ = 0;
  RectList last_bounds_;
  RectList geometry_reference_;
  std::vector<bool> underline_mask_;

  IMPLEMENT_REFCOUNTING(ImeInputTestHandler);
};

struct ImeTestCase {
  Scenario scenario;
  Editor editor;
  float scale;
  KeyboardApi keyboard_api;
  bool start_in_child;
};

class ImeInputTestCase : public testing::Test {
 public:
  explicit ImeInputTestCase(ImeTestCase test_case) : test_case_(test_case) {}

 private:
  void TestBody() override {
    // Public Ime* methods currently support Alloy OSR only.
    CefRefPtr<ImeInputTestHandler> handler = new ImeInputTestHandler(
        test_case_.scenario, test_case_.editor, test_case_.scale, true,
        test_case_.keyboard_api, test_case_.start_in_child);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }

  const ImeTestCase test_case_;
};

bool RunAtHighDpi(Scenario scenario) {
  // Retain representative DIP bounds, frame-coordinate transformations,
  // rendering, and monitoring coverage at both scales. Text/event/lifecycle
  // variations exercise the same behavior at 1x.
  switch (scenario) {
    case Scenario::CompositionUnicode:
    case Scenario::MultipleUnderlines:
    case Scenario::UnderlineStyles:
    case Scenario::CompositionTextColor:
    case Scenario::ScrollGeometry:
    case Scenario::ResizeGeometry:
    case Scenario::SameOriginIframe:
    case Scenario::CrossOriginIframe:
    case Scenario::PasswordUnicode:
    case Scenario::SelectionExternalBeginFrame:
    case Scenario::MonitoringRetargetAcrossFrames:
      return true;
    default:
      return false;
  }
}

bool RegisterImeTests(const char* name, Scenario scenario, int line) {
  for (auto editor :
       {Editor::Input, Editor::Textarea, Editor::ContentEditable}) {
    if ((scenario == Scenario::MaxLengthCommit ||
         scenario == Scenario::MaxLengthComposition ||
         scenario == Scenario::Disabled) &&
        editor == Editor::ContentEditable) {
      // maxlength is an input/textarea attribute, not a contenteditable limit.
      continue;
    }
    if ((scenario == Scenario::PasswordUnicode ||
         scenario == Scenario::PasswordUnicodeChromium ||
         scenario == Scenario::PasswordTypeChange ||
         scenario == Scenario::PasswordIframe ||
         scenario == Scenario::PasswordCrossOriginIframe ||
         scenario == Scenario::KeyboardCommitNavigation ||
         scenario == Scenario::KeyboardFinishNavigation ||
         scenario == Scenario::CancelNavigation) &&
        editor != Editor::Input) {
      continue;
    }
    for (float scale : {1.0f, 2.0f}) {
      if (scale == 2.0f && !RunAtHighDpi(scenario)) {
        continue;
      }
      for (const auto keyboard_api : {
               KeyboardApi::Legacy,
#if CEF_API_ADDED(CEF_NEXT)
               KeyboardApi::Portable,
#endif
           }) {
        for (bool start_in_child : {false, true}) {
          if (start_in_child && !IsCrossFrameMonitoring(scenario)) {
            continue;
          }
          std::string case_name = name;
          case_name += editor == Editor::Input      ? "Input"
                       : editor == Editor::Textarea ? "Textarea"
                                                    : "ContentEditable";
          case_name += scale == 1.0f ? "1x" : "2x";
          case_name += keyboard_api == KeyboardApi::Legacy
                           ? "SendKeyEvent"
                           : "SendKeyboardEvent";
          if (IsCrossFrameMonitoring(scenario)) {
            case_name += start_in_child ? "StartInChild" : "StartInRoot";
          }
          const ImeTestCase test_case{scenario, editor, scale, keyboard_api,
                                      start_in_child};
          // Register only supported combinations, preserving the suite name
          // so existing ImeInputTest.* filters continue to work.
          testing::RegisterTest("ImeInputTest", case_name.c_str(), nullptr,
                                nullptr, __FILE__, line,
                                [test_case]() -> testing::Test* {
                                  return new ImeInputTestCase(test_case);
                                });
        }
      }
    }
  }
  return true;
}

#define IME_TEST(name)                             \
  [[maybe_unused]] const bool kRegisterIme##name = \
      RegisterImeTests(#name, Scenario::name, __LINE__);

IME_TEST(Commit)
IME_TEST(CommitUnicode)
IME_TEST(CompositionCommit)
IME_TEST(UpdateCommit)
IME_TEST(FinishKeep)
IME_TEST(FinishDiscard)
IME_TEST(Cancel)
IME_TEST(EmptyUpdate)
IME_TEST(ReplaceSelection)
IME_TEST(CommitSelection)
IME_TEST(CompositionUnicode)
IME_TEST(RepeatSession)
IME_TEST(EmptyCommit)
IME_TEST(EmptyCompositionCommit)
IME_TEST(EmptyCommitSelection)
IME_TEST(EmptySetWithoutComposition)
IME_TEST(FinishWithoutComposition)
IME_TEST(CancelWithoutComposition)
IME_TEST(RepeatedFinishCancel)
IME_TEST(OmittedSelection)
IME_TEST(OutOfBoundsSelection)
IME_TEST(ReversedSelection)
IME_TEST(ExplicitCompositionReplacement)
IME_TEST(ExplicitCommitReplacement)
IME_TEST(OutOfBoundsReplacement)
IME_TEST(CursorBeforeEnd)
IME_TEST(CursorAfterEnd)
IME_TEST(CursorOutOfBounds)
IME_TEST(MultipleUnderlines)
IME_TEST(UnderlineStyles)
IME_TEST(CompositionTextColor)
IME_TEST(CancelBeforeInput)
IME_TEST(CancelCompositionBeforeInput)
IME_TEST(ReadOnly)
IME_TEST(Disabled)
IME_TEST(MaxLengthCommit)
IME_TEST(MaxLengthComposition)
IME_TEST(Multiline)
IME_TEST(LongText)
IME_TEST(FocusOtherEditor)
IME_TEST(BlurDuringComposition)
IME_TEST(RemoveDuringComposition)
IME_TEST(NavigateDuringComposition)
IME_TEST(DestroyDuringComposition)
IME_TEST(SameOriginIframe)
IME_TEST(CrossOriginIframe)
IME_TEST(ScrollGeometry)
IME_TEST(ResizeGeometry)
IME_TEST(SelectionCallbacks)
IME_TEST(SelectionFocus)
IME_TEST(SelectionExternalBeginFrame)
IME_TEST(PasswordSelection)
IME_TEST(PasswordUnicode)
IME_TEST(PasswordUnicodeChromium)
IME_TEST(PasswordTypeChange)
IME_TEST(PasswordIframe)
IME_TEST(PasswordCrossOriginIframe)
IME_TEST(CursorBeforeStart)
IME_TEST(EmptySetSelection)
IME_TEST(InvalidUnderlineRanges)
IME_TEST(NoUnderlines)
IME_TEST(OffThreadCommit)
IME_TEST(OffThreadComposition)
IME_TEST(OffThreadFinishKeep)
IME_TEST(OffThreadFinishDiscard)
IME_TEST(OffThreadCancel)
IME_TEST(CancelCompositionStart)
IME_TEST(CrossOriginIframeChromium)
IME_TEST(SetCompositionRange)
IME_TEST(KeyboardCommitNavigation)
IME_TEST(KeyboardFinishNavigation)
IME_TEST(CancelNavigation)
IME_TEST(MonitoringCancelAcrossFrames)
IME_TEST(MonitoringCommitAcrossFrames)
IME_TEST(MonitoringFinishAcrossFrames)
IME_TEST(MonitoringRetargetAcrossFrames)

#undef IME_TEST

}  // namespace
