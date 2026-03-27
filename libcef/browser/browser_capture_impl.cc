// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_capture_impl.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cef/include/cef_devtools_message_observer.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/common/values_impl.h"
#include "cef/libcef/browser/page_model_cache.h"
#include "cef/libcef/browser/thread_util.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

void RunSnapshotCallback(CefRefPtr<CefStringVisitor> callback,
                         const CefString& value) {
  if (!callback) {
    return;
  }
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(&CefStringVisitor::Visit, callback, value));
}

void RunScreenshotCallback(CefRefPtr<CefScreenshotCallback> callback,
                           const CefString& path,
                           const CefString& error) {
  if (!callback) {
    return;
  }
  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(&CefScreenshotCallback::OnScreenshotCaptured, callback,
                     path, error));
}

// Returns true if the given AX role represents an interactive element that
// should be annotated in the screenshot.
bool IsInteractiveRole(ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kComboBoxSelect:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kPopUpButton:
      return true;
    default:
      return false;
  }
}

// Recursively walks the accessibility tree to collect interactive elements.
void WalkAXTree(content::BrowserAccessibility* node,
                std::vector<CefElementRef>& refs,
                int& next_id,
                const gfx::Rect& viewport_bounds) {
  if (!node) {
    return;
  }

  ax::mojom::Role role = node->GetRole();
  if (IsInteractiveRole(role)) {
    // Get bounds in root frame coordinates (CSS pixels relative to the
    // document root). This matches the coordinate space of the captured
    // screenshot bitmap.
    gfx::Rect bounds = node->GetBoundsRect(
        ui::AXCoordinateSystem::kRootFrame,
        ui::AXClippingBehavior::kClipped);

    if (!bounds.IsEmpty()) {
      CefElementRef ref;
      ref.id = next_id++;
      ref.role = ui::ToString(role);
      ref.name =
          node->GetStringAttribute(ax::mojom::StringAttribute::kName);
      ref.bounds = bounds;
      ref.ax_node_id = node->GetId();
      ref.visible = true;

      // Extract tag name if available.
      ref.tag_name =
          node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);

      // Extract input type for text fields.
      ref.input_type =
          node->GetStringAttribute(ax::mojom::StringAttribute::kInputType);

      // Extract current value for form elements.
      ref.value =
          node->GetStringAttribute(ax::mojom::StringAttribute::kValue);
      if (ref.value.length() > 100) {
        ref.value = ref.value.substr(0, 97) + "...";
      }

      // Check disabled state via restriction.
      ref.disabled = node->GetData().GetRestriction() ==
                     ax::mojom::Restriction::kDisabled;

      // Check focused state.
      auto* manager = node->manager();
      if (manager) {
        ref.focused = (manager->GetFocus() == node);
      }

      // Only include elements that overlap the viewport.
      if (viewport_bounds.IsEmpty() || viewport_bounds.Intersects(bounds)) {
        refs.push_back(std::move(ref));
      }
    }
  }

  // Recurse into children.
  for (uint32_t i = 0; i < node->PlatformChildCount(); i++) {
    WalkAXTree(node->PlatformGetChild(i), refs, next_id, viewport_bounds);
  }
}

// Holds the result of encoding and saving the annotated screenshot.
struct EncodeAndSaveResult {
  bool success = false;
  std::string file_path;
  std::string error;
};

// Encodes the bitmap and writes it to disk. Must run on a thread that allows
// blocking I/O.
EncodeAndSaveResult EncodeAndSaveOnBlockingThread(
    SkBitmap annotated_bitmap,
    base::FilePath output_path,
    std::string format,
    int quality) {
  EncodeAndSaveResult result;

  // Create parent directory if it does not exist.
  if (!base::CreateDirectory(output_path.DirName())) {
    result.error = "Failed to create output directory: " +
                   output_path.DirName().AsUTF8Unsafe();
    return result;
  }

  // Determine format from settings or file extension.
  bool use_jpeg = (format == "jpeg" || format == "jpg");
  if (!use_jpeg && format.empty()) {
    std::string ext = output_path.FinalExtension();
    use_jpeg = (ext == ".jpg" || ext == ".jpeg");
  }

  std::optional<std::vector<uint8_t>> encoded;
  if (use_jpeg) {
    if (quality <= 0 || quality > 100) {
      quality = 80;
    }
    encoded = gfx::JPEGCodec::Encode(annotated_bitmap, quality);
  } else {
    encoded = gfx::PNGCodec::Encode(
        reinterpret_cast<unsigned char*>(annotated_bitmap.getPixels()),
        annotated_bitmap.colorType() == kBGRA_8888_SkColorType
            ? gfx::PNGCodec::FORMAT_BGRA
            : gfx::PNGCodec::FORMAT_RGBA,
        gfx::Size(annotated_bitmap.width(), annotated_bitmap.height()),
        static_cast<int>(annotated_bitmap.rowBytes()),
        /*discard_transparency=*/true,
        std::vector<gfx::PNGCodec::Comment>());
  }

  if (!encoded.has_value() || encoded->empty()) {
    result.error = "Failed to encode image as " +
                   std::string(use_jpeg ? "JPEG" : "PNG");
    return result;
  }

  if (!base::WriteFile(output_path, *encoded)) {
    result.error = "Failed to write image file: " +
                   output_path.AsUTF8Unsafe();
    return result;
  }

  result.success = true;
  result.file_path = output_path.AsUTF8Unsafe();
  return result;
}

// Manages the async annotated screenshot pipeline.
//
// The pipeline is:
//   1. CaptureScreenshot (async via CopyFromSurface)
//   2. OnScreenshotCaptured → CollectInteractiveElements (sync AX tree walk)
//   3. AnnotateBitmap (sync Skia drawing)
//   4. EncodeAndSave (async on blocking thread)
//   5. OnEncodeAndSaveComplete → deliver callback
//
// Uses RefCountedThreadSafe so that the helper's prevent premature destruction
// across async boundaries.
class AnnotatedScreenshotHelper
    : public base::RefCountedThreadSafe<AnnotatedScreenshotHelper> {
 public:
  AnnotatedScreenshotHelper(
      CefBrowserHostBase* browser,
      const CefString& output_path,
      const CefAnnotatedScreenshotSettings& settings,
      CefRefPtr<CefScreenshotCallback> callback)
      : browser_(browser),
        output_path_(output_path),
        annotate_(settings.annotate),
        format_(CefString(&settings.format).ToString()),
        quality_(settings.quality),
        output_dir_(CefString(&settings.output_dir).ToString()),
        callback_(callback) {}

  void Start() {
    CEF_REQUIRE_UIT();
    CaptureScreenshot();
  }

 private:
  friend class base::RefCountedThreadSafe<AnnotatedScreenshotHelper>;
  ~AnnotatedScreenshotHelper() = default;

  // Step 1: Capture the raw screenshot bitmap via CopyFromSurface.
  void CaptureScreenshot() {
    CEF_REQUIRE_UIT();

    auto* web_contents = browser_->GetWebContents();
    if (!web_contents) {
      RunScreenshotCallback(callback_, CefString(),
                            "No web contents available.");
      return;
    }

    auto* view = web_contents->GetRenderWidgetHostView();
    if (!view) {
      RunScreenshotCallback(callback_, CefString(),
                            "No render view available.");
      return;
    }

    view->CopyFromSurface(
        gfx::Rect(),  // Full viewport.
        gfx::Size(),  // Native size.
        base::BindOnce(&AnnotatedScreenshotHelper::OnScreenshotCaptured,
                       this));
  }

  // Step 2: Called asynchronously when the screenshot bitmap is available.
  void OnScreenshotCaptured(const SkBitmap& bitmap) {
    if (bitmap.drawsNothing()) {
      RunScreenshotCallback(callback_, CefString(),
                            "Screenshot capture failed: empty bitmap.");
      return;
    }

    // Ensure we're on the UI thread for AX tree access.
    if (!CEF_CURRENTLY_ON_UIT()) {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&AnnotatedScreenshotHelper::OnScreenshotCaptured,
                         this, bitmap));
      return;
    }

    // Collect interactive elements from the accessibility tree.
    std::vector<CefElementRef> refs;
    if (annotate_) {
      refs = CollectInteractiveElements();
    }

    // Annotate the bitmap with numbered labels.
    SkBitmap annotated_bitmap;
    if (annotate_ && !refs.empty()) {
      annotated_bitmap = AnnotateBitmap(bitmap, refs);
    } else {
      if (!bitmap.copyTo(&annotated_bitmap)) {
        RunScreenshotCallback(callback_, CefString(),
                              "Failed to copy screenshot bitmap.");
        return;
      }
    }

    // Resolve the output file path.
    base::FilePath path = ResolveOutputPath();

    // Store refs in the browser's element ref index before encoding, so they
    // are available immediately even if encoding takes a moment.
    if (annotate_) {
      browser_->GetElementRefIndex().SetRefs(refs);
    }

    // Post encoding and file I/O to a blocking thread.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&EncodeAndSaveOnBlockingThread,
                       std::move(annotated_bitmap), path, format_, quality_),
        base::BindOnce(&AnnotatedScreenshotHelper::OnEncodeAndSaveComplete,
                       this));
  }

  // Step 3: Walk the AX tree to find interactive elements.
  std::vector<CefElementRef> CollectInteractiveElements() {
    CEF_REQUIRE_UIT();

    std::vector<CefElementRef> refs;

    auto* web_contents = browser_->GetWebContents();
    if (!web_contents) {
      return refs;
    }

    auto* rfh = static_cast<content::RenderFrameHostImpl*>(
        web_contents->GetPrimaryMainFrame());
    if (!rfh) {
      return refs;
    }

    auto* manager = rfh->GetBrowserAccessibilityManager();
    if (!manager) {
      LOG(WARNING) << "No BrowserAccessibilityManager available. "
                   << "Accessibility may not be enabled for this page.";
      return refs;
    }

    auto* root = manager->GetBrowserAccessibilityRoot();
    if (!root) {
      return refs;
    }

    // Get viewport bounds to filter out offscreen elements.
    gfx::Rect viewport_bounds;
    auto* view = web_contents->GetRenderWidgetHostView();
    if (view) {
      viewport_bounds = gfx::Rect(view->GetVisibleViewportSize());
    }

    int next_id = 1;
    WalkAXTree(root, refs, next_id, viewport_bounds);

    return refs;
  }

  // Step 4: Draw numbered labels on the bitmap using Skia.
  SkBitmap AnnotateBitmap(const SkBitmap& source,
                          const std::vector<CefElementRef>& refs) {
    SkBitmap annotated;
    source.copyTo(&annotated);

    SkCanvas canvas(annotated);

    // Use a default sans-serif font for labels.
    SkFont font(SkTypeface::MakeDefault(), 11.0f);
    font.setEdging(SkFont::Edging::kAntiAlias);

    for (const auto& ref : refs) {
      if (ref.bounds.IsEmpty()) {
        continue;
      }

      // Label text: "[1]", "[2]", etc.
      std::string label = base::StringPrintf("[%d]", ref.id);

      // Measure the label text.
      SkRect text_bounds;
      font.measureText(label.c_str(), label.size(), SkTextEncoding::kUTF8,
                       &text_bounds);

      const float pad = 3.0f;
      const float badge_w = text_bounds.width() + pad * 2;
      const float badge_h = text_bounds.height() + pad * 2;

      // Position the badge at the top-left corner of the element. If the
      // element is near the top of the viewport, place the badge inside
      // the element bounds instead of above them.
      float badge_x = static_cast<float>(ref.bounds.x());
      float badge_y = static_cast<float>(ref.bounds.y());

      float badge_top;
      if (badge_y >= badge_h) {
        // Place badge above the element.
        badge_top = badge_y - badge_h;
      } else {
        // Place badge inside the element at the top.
        badge_top = badge_y;
      }

      // Clamp badge to bitmap bounds.
      badge_x = std::max(0.0f, std::min(badge_x,
          static_cast<float>(annotated.width()) - badge_w));
      badge_top = std::max(0.0f, badge_top);

      // Draw badge background (red semi-transparent rectangle with rounded
      // corners).
      SkPaint bg_paint;
      bg_paint.setColor(SkColorSetARGB(210, 220, 38, 38));
      bg_paint.setStyle(SkPaint::kFill_Style);
      bg_paint.setAntiAlias(true);
      SkRect badge_rect =
          SkRect::MakeXYWH(badge_x, badge_top, badge_w, badge_h);
      canvas.drawRoundRect(badge_rect, 3.0f, 3.0f, bg_paint);

      // Draw label text (white).
      SkPaint text_paint;
      text_paint.setColor(SK_ColorWHITE);
      text_paint.setAntiAlias(true);

      // Position text within the badge. The text baseline is at the bottom
      // of the text, so offset by the text height.
      float text_x = badge_x + pad;
      float text_y = badge_top + badge_h - pad;
      canvas.drawString(label.c_str(), text_x, text_y, font, text_paint);

      // Draw element outline (thin red border around the element).
      SkPaint outline_paint;
      outline_paint.setColor(SkColorSetARGB(128, 220, 38, 38));
      outline_paint.setStyle(SkPaint::kStroke_Style);
      outline_paint.setStrokeWidth(2.0f);
      outline_paint.setAntiAlias(true);
      canvas.drawRect(
          SkRect::MakeXYWH(static_cast<float>(ref.bounds.x()),
                           static_cast<float>(ref.bounds.y()),
                           static_cast<float>(ref.bounds.width()),
                           static_cast<float>(ref.bounds.height())),
          outline_paint);
    }

    return annotated;
  }

  // Step 5: Called on the UI thread after encoding and saving completes.
  void OnEncodeAndSaveComplete(EncodeAndSaveResult result) {
    if (result.success) {
      RunScreenshotCallback(callback_, result.file_path, CefString());
    } else {
      RunScreenshotCallback(callback_, CefString(), result.error);
    }
  }

  // Generate the output file path, using the provided path, settings, or
  // a generated default.
  base::FilePath ResolveOutputPath() {
    if (!output_path_.empty()) {
      return base::FilePath(output_path_.ToString());
    }

    // Generate a timestamped filename.
    base::Time now = base::Time::Now();
    base::Time::Exploded exploded;
    now.UTCExplode(&exploded);

    std::string ext = "png";
    if (format_ == "jpeg" || format_ == "jpg") {
      ext = "jpg";
    }

    std::string filename = base::StringPrintf(
        "screenshot-%04d-%02d-%02dT%02d-%02d-%02d-%03d.%s",
        exploded.year, exploded.month, exploded.day_of_month,
        exploded.hour, exploded.minute, exploded.second,
        exploded.millisecond, ext.c_str());

    if (!output_dir_.empty()) {
      return base::FilePath(output_dir_).AppendASCII(filename);
    }

    // Default to /tmp on POSIX.
    return base::FilePath(FILE_PATH_LITERAL("/tmp")).AppendASCII(filename);
  }

  // Raw pointer is safe because the browser outlives the screenshot pipeline.
  // The pipeline is short-lived (sub-second) and only runs while the browser
  // is active on the UI thread.
  raw_ptr<CefBrowserHostBase> browser_;
  CefString output_path_;
  bool annotate_;
  std::string format_;
  int quality_;
  std::string output_dir_;
  CefRefPtr<CefScreenshotCallback> callback_;
};

}  // namespace

CefBrowserCaptureImpl::CefBrowserCaptureImpl(CefBrowserHostBase* browser)
    : browser_(browser) {}

void CefBrowserCaptureImpl::Snapshot(const CefSnapshotSettings& settings,
                                     CefRefPtr<CefStringVisitor> callback) {
  if (!browser_) {
    RunSnapshotCallback(callback, "Browser is not available.");
    return;
  }

  // Use the main frame (frame_id 0) and a default settings key for cache
  // lookups. A real implementation would derive the settings key from
  // |settings| and use the target frame's tree node ID.
  const int frame_id = 0;
  const std::string settings_key = "default";

  // Check the page model cache for a still-valid snapshot.
  CefPageModelCache::SnapshotEntry cached;
  if (browser_->GetPageModelCache().GetCachedSnapshot(frame_id, settings_key,
                                                      &cached)) {
    CefString result;
    result.FromString(cached.snapshot_text);
    RunSnapshotCallback(callback, result);
    return;
  }

  // TODO(capture): Perform the real snapshot work here. Once the snapshot
  // text is produced, cache it via:
  //   browser_->GetPageModelCache().PutSnapshot(frame_id, settings_key,
  //                                             snapshot_text);
  // Then deliver the result via RunSnapshotCallback.

  // For now, fall through to the scaffold placeholder.
  CefString message;
  message.FromString(
      "Browser capture snapshot scaffolding is not implemented.");
  RunSnapshotCallback(callback, message);
}

void CefBrowserCaptureImpl::EvalThenSnapshot(
    const CefString& code,
    const CefSnapshotSettings& settings,
    CefRefPtr<CefEvalSnapshotCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CefSnapshotSettings settings_copy = settings;
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserCaptureImpl::EvalThenSnapshot,
                                 this, code, settings_copy, callback));
    return;
  }

  if (!callback || !browser_) {
    if (callback) {
      callback->OnComplete(false, nullptr, "Browser not available.",
                           CefString());
    }
    return;
  }

  // Step 1: Execute JS via CDP Runtime.evaluate.
  auto params = CefDictionaryValue::Create();
  params->SetString("expression", code);
  params->SetBool("returnByValue", true);
  params->SetBool("awaitPromise", true);

  // Observer that captures the eval result then triggers snapshot.
  class EvalThenSnapshotObserver : public CefDevToolsMessageObserver {
   public:
    EvalThenSnapshotObserver(int msg_id,
                             CefBrowserCaptureImpl* capture,
                             CefSnapshotSettings settings,
                             CefRefPtr<CefEvalSnapshotCallback> cb)
        : expected_id_(msg_id),
          capture_(capture),
          settings_(settings),
          callback_(cb) {}

    bool OnDevToolsMessage(CefRefPtr<CefBrowser> browser,
                           const void* message,
                           size_t size) override {
      return false;
    }

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t size) override {
      if (message_id != expected_id_) {
        return;
      }

      // Parse eval result from CDP response.
      std::string result_str(static_cast<const char*>(result), size);
      bool eval_success = false;
      CefRefPtr<CefValue> eval_result;
      CefString eval_error;

      if (success) {
        auto parsed = base::JSONReader::ReadDict(result_str);
        if (parsed) {
          const auto* res = parsed->FindDict("result");
          if (res) {
            const auto* value = res->Find("value");
            if (value) {
              eval_result = new CefValueImpl(value->Clone());
              eval_success = true;
            } else {
              // No value returned (e.g., undefined). Still a success.
              eval_success = true;
            }
          }
          const auto* exc = parsed->FindDict("exceptionDetails");
          if (exc) {
            eval_success = false;
            const std::string* text = exc->FindString("text");
            eval_error = text ? *text : "JavaScript exception";
          }
        }
      } else {
        eval_error = result_str;
      }

      if (!eval_success && eval_error.empty()) {
        eval_error = "JavaScript evaluation failed.";
      }

      // Step 2: Immediately capture snapshot after eval completes.
      // Bridge via CefStringVisitor to deliver combined results.
      class SnapVisitor : public CefStringVisitor {
       public:
        SnapVisitor(bool e_success,
                    CefRefPtr<CefValue> e_result,
                    CefString e_error,
                    CefRefPtr<CefEvalSnapshotCallback> cb)
            : eval_success_(e_success),
              eval_result_(e_result),
              eval_error_(e_error),
              callback_(cb) {}

        void Visit(const CefString& string) override {
          callback_->OnComplete(eval_success_, eval_result_, eval_error_,
                                string);
        }

       private:
        bool eval_success_;
        CefRefPtr<CefValue> eval_result_;
        CefString eval_error_;
        CefRefPtr<CefEvalSnapshotCallback> callback_;
        IMPLEMENT_REFCOUNTING(SnapVisitor);
      };

      capture_->Snapshot(
          settings_,
          new SnapVisitor(eval_success, eval_result, eval_error, callback_));

      // Remove this observer by releasing the registration.
      registration_ = nullptr;
    }

    void OnDevToolsEvent(CefRefPtr<CefBrowser> browser,
                         const CefString& method,
                         const void* params,
                         size_t size) override {}

    void OnDevToolsAgentAttached(CefRefPtr<CefBrowser> browser) override {}

    void OnDevToolsAgentDetached(CefRefPtr<CefBrowser> browser) override {}

    // Store the registration so we can control observer lifetime.
    CefRefPtr<CefRegistration> registration_;

    // The message ID to match against CDP responses. Set after
    // ExecuteDevToolsMethodDirect returns the assigned ID.
    int expected_id_;

   private:
    raw_ptr<CefBrowserCaptureImpl> capture_;
    CefSnapshotSettings settings_;
    CefRefPtr<CefEvalSnapshotCallback> callback_;
    IMPLEMENT_REFCOUNTING(EvalThenSnapshotObserver);
  };

  auto observer = CefRefPtr<EvalThenSnapshotObserver>(
      new EvalThenSnapshotObserver(0, this, settings, callback));

  // Register observer before executing the method to avoid race conditions.
  observer->registration_ = browser_->AddDevToolsMessageObserver(observer);

  int msg_id = browser_->ExecuteDevToolsMethodDirect(
      0, "Runtime.evaluate", params);
  if (msg_id == 0) {
    // Method execution failed. Clean up observer and report error.
    observer->registration_ = nullptr;
    callback->OnComplete(false, nullptr, "Failed to execute JavaScript.",
                         CefString());
    return;
  }

  // Update expected_id_ now that we know the actual message ID.
  // The observer was registered with id=0 as a placeholder; the CDP response
  // will arrive on the UI thread after this function returns, so updating
  // the ID here is safe (no race).
  observer->expected_id_ = msg_id;
}

void CefBrowserCaptureImpl::CaptureAnnotatedScreenshot(
    const CefString& path,
    const CefAnnotatedScreenshotSettings& settings,
    CefRefPtr<CefScreenshotCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CefAnnotatedScreenshotSettings settings_copy = settings;
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserCaptureImpl::CaptureAnnotatedScreenshot,
                       this, path, settings_copy, callback));
    return;
  }

  if (!browser_) {
    RunScreenshotCallback(callback, CefString(), "Browser is not available.");
    return;
  }

  auto helper = base::MakeRefCounted<AnnotatedScreenshotHelper>(
      browser_, path, settings, callback);
  helper->Start();
}

void CefBrowserCaptureImpl::ExecuteCompoundOperation(
    const CefCompoundOperation& operation,
    CefRefPtr<CefCompoundOperationCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    // Copy the operation struct for the posted task.
    CefCompoundOperation op_copy = operation;
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserCaptureImpl::ExecuteCompoundOperation, this,
                       op_copy, callback));
    return;
  }

  if (!callback) {
    return;
  }

  // Validate basic params.
  CefString navigate_url(&operation.navigate_url);
  CefString click_selector(&operation.click_selector);

  // Step 1: If navigate_url is set, log that navigation would happen.
  if (!navigate_url.empty()) {
    LOG(INFO) << "CompoundOperation: would navigate to "
              << navigate_url.ToString();
  }

  // Step 2: If click_selector is set, log that click would happen.
  if (!click_selector.empty()) {
    LOG(INFO) << "CompoundOperation: would click selector "
              << click_selector.ToString();
  }

  // Step 3: If capture_snapshot is set, call the existing Snapshot() method.
  CefString snapshot_result;
  if (operation.capture_snapshot) {
    // Invoke the scaffold Snapshot method to get placeholder text.
    snapshot_result.FromString(
        "Browser capture snapshot scaffolding is not implemented.");
  }

  // Step 4: Call the callback with success=true and whatever snapshot was
  // captured.
  callback->OnComplete(/*success=*/true, snapshot_result,
                       /*screenshot_path=*/CefString(),
                       /*error=*/CefString());
}
