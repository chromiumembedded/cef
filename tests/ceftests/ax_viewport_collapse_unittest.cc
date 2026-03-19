// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// Helper: parse a CDP JSON response into a CefDictionaryValue.
CefRefPtr<CefDictionaryValue> ParseResponse(const void* data, size_t size) {
  std::string json(static_cast<const char*>(data), size);
  CefRefPtr<CefValue> value =
      CefParseJSON(json.data(), json.size(), JSON_PARSER_RFC);
  if (value && value->GetType() == VTYPE_DICTIONARY) {
    return value->GetDictionary();
  }
  return nullptr;
}

// Helper: find a node in the AX tree response by string property.
// Searches nodes array for a node whose "name.value" matches |name|.
CefRefPtr<CefDictionaryValue> FindNodeByName(CefRefPtr<CefListValue> nodes,
                                             const std::string& name) {
  for (size_t i = 0; i < nodes->GetSize(); i++) {
    auto node = nodes->GetDictionary(i);
    if (node->HasKey("name")) {
      auto name_val = node->GetDictionary("name");
      if (name_val && name_val->HasKey("value") &&
          name_val->GetString("value").ToString() == name) {
        return node;
      }
    }
  }
  return nullptr;
}

// Helper: check if a node with the given name exists in the tree.
bool HasNodeWithName(CefRefPtr<CefListValue> nodes, const std::string& name) {
  return FindNodeByName(nodes, name) != nullptr;
}

// Helper: count nodes in tree.
size_t NodeCount(CefRefPtr<CefListValue> nodes) {
  return nodes->GetSize();
}

// Helper: get the "level" property from a node's properties array.
int GetLevelProperty(CefRefPtr<CefDictionaryValue> node) {
  if (!node->HasKey("properties")) {
    return -1;
  }
  auto props = node->GetList("properties");
  for (size_t i = 0; i < props->GetSize(); i++) {
    auto prop = props->GetDictionary(i);
    if (prop->GetString("name").ToString() == "level") {
      auto val = prop->GetDictionary("value");
      if (val) {
        return val->GetInt("value");
      }
    }
  }
  return -1;
}

// Helper: check if a node has empty childIds.
bool HasEmptyChildIds(CefRefPtr<CefDictionaryValue> node) {
  if (!node->HasKey("childIds")) {
    return true;
  }
  return node->GetList("childIds")->GetSize() == 0;
}

// Base class for viewport collapse tests.
class AxViewportCollapseTestHandler : public TestHandler {
 public:
  AxViewportCollapseTestHandler(cef_state_t collapse_state,
                                const char* url,
                                const char* html)
      : collapse_state_(collapse_state), url_(url), html_(html) {}

  AxViewportCollapseTestHandler(const AxViewportCollapseTestHandler&) = delete;
  AxViewportCollapseTestHandler& operator=(
      const AxViewportCollapseTestHandler&) = delete;

  void RunTest() override {
    AddResource(url_, html_, "text/html");

    CefBrowserSettings settings;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
    settings.ax_viewport_collapse = collapse_state_;
#endif
    CreateBrowserWithSettings(url_, settings);
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    registration_ =
        browser->GetHost()->AddDevToolsMessageObserver(new Observer(this));
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Page loaded. Fetch the full AX tree.
      GetBrowser()->GetHost()->ExecuteDevToolsMethod(
          ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
    }
  }

  // Subclasses implement this to verify the AX tree.
  virtual void VerifyTree(CefRefPtr<CefListValue> nodes) = 0;

  void DestroyTest() override {
    registration_ = nullptr;
    EXPECT_TRUE(got_result_);
    TestHandler::DestroyTest();
  }

 protected:
  class Observer : public CefDevToolsMessageObserver {
   public:
    explicit Observer(AxViewportCollapseTestHandler* handler)
        : handler_(handler) {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      EXPECT_TRUE(success) << "CDP method failed: "
                           << std::string(static_cast<const char*>(result),
                                          result_size);
      if (!success) {
        handler_->DestroyTest();
        return;
      }

      auto dict = ParseResponse(result, result_size);
      EXPECT_TRUE(dict.get());
      if (!dict) {
        handler_->DestroyTest();
        return;
      }

      auto nodes = dict->GetList("nodes");
      EXPECT_TRUE(nodes.get());
      if (!nodes) {
        handler_->DestroyTest();
        return;
      }

      handler_->got_result_ = true;
      handler_->VerifyTree(nodes);
      handler_->OnTreeVerified();
    }

   private:
    AxViewportCollapseTestHandler* handler_;
    IMPLEMENT_REFCOUNTING(Observer);
  };

  // Called after VerifyTree. Default: destroy. Subclasses can override.
  virtual void OnTreeVerified() { DestroyTest(); }

  cef_state_t collapse_state_;
  const char* url_;
  const char* html_;
  int next_message_id_ = 0;
  bool got_result_ = false;
  CefRefPtr<CefRegistration> registration_;

  IMPLEMENT_REFCOUNTING(AxViewportCollapseTestHandler);
};

// Test page with landmarks, headings, and interactive elements.
// A large spacer pushes the "offscreen" section out of the viewport.
const char kTestUrl[] = "https://tests/AxViewportCollapse";

const char kTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <nav aria-label="Main Navigation">
    <a href="#">Home</a>
  </nav>
  <main>
    <h1>In Viewport Heading</h1>
    <button id="btn-visible">Visible Button</button>
  </main>
  <div style="height:10000px;"></div>
  <footer aria-label="Footer Info">
    <h2>Offscreen Heading</h2>
    <button id="btn-offscreen">Offscreen Button</button>
    <input type="text" id="input-offscreen" value="offscreen">
  </footer>
  <nav aria-label="Offscreen Nav">
    <a href="#">Offscreen Link</a>
  </nav>
</body>
</html>
)";

// Test: STATE_DEFAULT or STATE_DISABLED → full tree, no collapsing.
class CollapseOffHandler : public AxViewportCollapseTestHandler {
 public:
  explicit CollapseOffHandler(cef_state_t state)
      : AxViewportCollapseTestHandler(state, kTestUrl, kTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // All nodes should be present, including off-screen ones.
    EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Button"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Heading"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Visible Button"));
    EXPECT_TRUE(HasNodeWithName(nodes, "In Viewport Heading"));

    // Off-screen landmarks should be fully serialized (with children),
    // not collapsed summaries.
    auto footer = FindNodeByName(nodes, "Footer Info");
    EXPECT_TRUE(footer.get());
    if (footer) {
      EXPECT_FALSE(HasEmptyChildIds(footer));
    }
  }
};

// Test: STATE_ENABLED → off-screen landmarks summarized, interactive pruned.
class CollapseEnabledHandler : public AxViewportCollapseTestHandler {
 public:
  CollapseEnabledHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED, kTestUrl, kTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // In-viewport nodes: fully serialized.
    EXPECT_TRUE(HasNodeWithName(nodes, "In Viewport Heading"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Visible Button"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Main Navigation"));

    // Off-screen landmarks: should be present as summaries (role + name).
    auto footer = FindNodeByName(nodes, "Footer Info");
    EXPECT_TRUE(footer.get());
    if (footer) {
      EXPECT_TRUE(HasEmptyChildIds(footer));
    }

    auto offscreen_nav = FindNodeByName(nodes, "Offscreen Nav");
    EXPECT_TRUE(offscreen_nav.get());
    if (offscreen_nav) {
      EXPECT_TRUE(HasEmptyChildIds(offscreen_nav));
    }

    // Off-screen heading inside a landmark: pruned along with the landmark's
    // subtree (landmark is summarized with empty childIds).
    EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Heading"));

    // Off-screen interactive elements: should be pruned (absent).
    EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Button"));
    EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Link"));
  }
};

// Page with headings at different levels off-screen.
const char kHeadingTestUrl[] = "https://tests/AxViewportCollapseHeading";

const char kHeadingTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <main>
    <p>In viewport content</p>
  </main>
  <div style="height:10000px;"></div>
  <h1>Offscreen H1</h1>
  <h2>Offscreen H2</h2>
  <h3>Offscreen H3</h3>
</body>
</html>
)";

// Test: Off-screen headings include correct level property.
class OffscreenHeadingLevelHandler : public AxViewportCollapseTestHandler {
 public:
  OffscreenHeadingLevelHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED,
                                      kHeadingTestUrl,
                                      kHeadingTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    auto h1 = FindNodeByName(nodes, "Offscreen H1");
    EXPECT_TRUE(h1.get());
    if (h1) {
      EXPECT_EQ(1, GetLevelProperty(h1));
      EXPECT_TRUE(HasEmptyChildIds(h1));
    }

    auto h2 = FindNodeByName(nodes, "Offscreen H2");
    EXPECT_TRUE(h2.get());
    if (h2) {
      EXPECT_EQ(2, GetLevelProperty(h2));
    }

    auto h3 = FindNodeByName(nodes, "Offscreen H3");
    EXPECT_TRUE(h3.get());
    if (h3) {
      EXPECT_EQ(3, GetLevelProperty(h3));
    }
  }
};

// Test: In-viewport parent's childIds don't reference pruned off-screen nodes.
class ChildIdsFilteredHandler : public AxViewportCollapseTestHandler {
 public:
  ChildIdsFilteredHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED, kTestUrl, kTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // Collect all nodeIds in the response.
    std::set<std::string> all_ids;
    for (size_t i = 0; i < nodes->GetSize(); i++) {
      auto node = nodes->GetDictionary(i);
      all_ids.insert(node->GetString("nodeId").ToString());
    }

    // Every childId reference should point to a serialized node.
    for (size_t i = 0; i < nodes->GetSize(); i++) {
      auto node = nodes->GetDictionary(i);
      if (node->HasKey("childIds")) {
        auto child_ids = node->GetList("childIds");
        for (size_t j = 0; j < child_ids->GetSize(); j++) {
          std::string child_id = child_ids->GetString(j).ToString();
          EXPECT_TRUE(all_ids.count(child_id) > 0)
              << "childId " << child_id << " references a non-serialized node";
        }
      }
    }
  }
};

// Page with position:fixed child inside off-screen parent.
const char kFixedTestUrl[] = "https://tests/AxViewportCollapseFixed";

const char kFixedTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <main><p>Viewport content</p></main>
  <div style="height:10000px;"></div>
  <div id="offscreen-parent" aria-label="Offscreen Parent">
    <div id="fixed-child" style="position:fixed;top:0;left:0;width:100px;height:30px;">
      <button id="fixed-btn">Fixed Button</button>
    </div>
  </div>
</body>
</html>
)";

// Test: position:fixed child visible despite off-screen parent.
class FixedPositionVisibleHandler : public AxViewportCollapseTestHandler {
 public:
  FixedPositionVisibleHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED,
                                      kFixedTestUrl,
                                      kFixedTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // The fixed-position button should be fully serialized even though
    // its parent is off-screen, because position:fixed places it in viewport.
    EXPECT_TRUE(HasNodeWithName(nodes, "Fixed Button"));

    // The off-screen parent itself should be pruned (not serialized).
    EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Parent"));
  }
};

// Page with nested off-screen structure.
const char kNestedTestUrl[] = "https://tests/AxViewportCollapseNested";

const char kNestedTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <main><p>Viewport content</p></main>
  <div style="height:10000px;"></div>
  <nav aria-label="Deep Nav">
    <h3>Nested Heading</h3>
    <button>Nested Button 1</button>
    <button>Nested Button 2</button>
  </nav>
</body>
</html>
)";

// Test: Deeply nested off-screen structure — landmark summarized, subtree
// pruned.
class NestedOffscreenHandler : public AxViewportCollapseTestHandler {
 public:
  NestedOffscreenHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED,
                                      kNestedTestUrl,
                                      kNestedTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // Off-screen landmark should be present as summary.
    auto nav = FindNodeByName(nodes, "Deep Nav");
    EXPECT_TRUE(nav.get());
    if (nav) {
      EXPECT_TRUE(HasEmptyChildIds(nav));
    }

    // Children of the off-screen landmark should be pruned entirely.
    EXPECT_FALSE(HasNodeWithName(nodes, "Nested Heading"));
    EXPECT_FALSE(HasNodeWithName(nodes, "Nested Button 1"));
    EXPECT_FALSE(HasNodeWithName(nodes, "Nested Button 2"));
  }
};

// Test: Runtime toggle via SetAxViewportCollapse.
class RuntimeToggleHandler : public AxViewportCollapseTestHandler {
 public:
  RuntimeToggleHandler()
      : AxViewportCollapseTestHandler(STATE_DEFAULT, kTestUrl, kTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    verify_ct_++;
    if (!toggled_) {
      // First fetch: full tree (setting is default/off).
      EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Button"));
      first_node_count_ = NodeCount(nodes);
    } else {
      // Second fetch: collapsed tree.
      EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Button"));
      EXPECT_TRUE(HasNodeWithName(nodes, "Footer Info"));
      EXPECT_LT(NodeCount(nodes), first_node_count_);
    }
  }

  void OnTreeVerified() override {
    if (!toggled_) {
      toggled_ = true;
      // Enable viewport collapse at runtime and re-fetch.
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
      GetBrowser()->GetHost()->SetAxViewportCollapse(true);
#endif
      // Re-fetch after a short delay to allow prefs to propagate.
      CefPostDelayedTask(
          TID_UI, base::BindOnce(&RuntimeToggleHandler::FetchTreeAgain, this),
          200);
    } else {
      EXPECT_EQ(2, verify_ct_);
      DestroyTest();
    }
  }

  void FetchTreeAgain() {
    got_result_ = false;
    GetBrowser()->GetHost()->ExecuteDevToolsMethod(
        ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
  }

 private:
  bool toggled_ = false;
  int verify_ct_ = 0;
  size_t first_node_count_ = 0;
};

// Small page that fits entirely in the viewport.
const char kAllVisibleTestUrl[] = "https://tests/AxViewportCollapseAllVisible";

const char kAllVisibleTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <nav aria-label="Site Nav">
    <a href="#">Home</a>
  </nav>
  <main>
    <h1>Main Heading</h1>
    <button>Click Me</button>
  </main>
  <footer aria-label="Site Footer">
    <p>Footer text</p>
  </footer>
</body>
</html>
)";

// Test: Collapse enabled but all content fits in viewport — no-op.
class AllVisibleHandler : public AxViewportCollapseTestHandler {
 public:
  AllVisibleHandler()
      : AxViewportCollapseTestHandler(STATE_ENABLED,
                                      kAllVisibleTestUrl,
                                      kAllVisibleTestHtml) {}

  void VerifyTree(CefRefPtr<CefListValue> nodes) override {
    // Everything fits in viewport, so all nodes should be fully serialized.
    EXPECT_TRUE(HasNodeWithName(nodes, "Site Nav"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Main Heading"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Click Me"));
    EXPECT_TRUE(HasNodeWithName(nodes, "Site Footer"));

    // Nav should have children (not collapsed).
    auto nav = FindNodeByName(nodes, "Site Nav");
    EXPECT_TRUE(nav.get());
    if (nav) {
      EXPECT_FALSE(HasEmptyChildIds(nav));
    }
  }
};

// Page with a landmark at 400px down. TestHandler creates an 800x600 window
// (~550px content height). At 100% zoom the nav is visible. At 300% zoom the
// viewport shrinks to ~183 CSS pixels tall, pushing the nav off-screen.
const char kZoomTestUrl[] = "https://tests/AxViewportCollapseZoom";

const char kZoomTestHtml[] = R"(
<html>
<body style="margin:0;padding:0;">
  <main>
    <h1>Top Content</h1>
  </main>
  <div style="height:400px;" aria-hidden="true"></div>
  <nav aria-label="Edge Nav">
    <a href="#">Edge Link</a>
  </nav>
</body>
</html>
)";

// Test: Zooming in pushes a near-viewport-edge landmark off-screen.
// Verifies coordinate spaces are correctly scaled.
class ZoomedCollapseHandler : public TestHandler {
 public:
  ZoomedCollapseHandler() = default;
  ZoomedCollapseHandler(const ZoomedCollapseHandler&) = delete;
  ZoomedCollapseHandler& operator=(const ZoomedCollapseHandler&) = delete;

  void RunTest() override {
    AddResource(kZoomTestUrl, kZoomTestHtml, "text/html");

    CefBrowserSettings settings;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
    settings.ax_viewport_collapse = STATE_ENABLED;
#endif
    CreateBrowserWithSettings(kZoomTestUrl, settings);
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    registration_ =
        browser->GetHost()->AddDevToolsMessageObserver(new Observer(this));
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Step 1: Fetch tree at default zoom. "Edge Nav" should be in viewport.
      GetBrowser()->GetHost()->ExecuteDevToolsMethod(
          ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
    }
  }

  void DestroyTest() override {
    registration_ = nullptr;
    EXPECT_EQ(2, tree_fetch_ct_);
    TestHandler::DestroyTest();
  }

 private:
  enum Step { kDefaultZoom = 1, kZoomedIn = 2 };

  class Observer : public CefDevToolsMessageObserver {
   public:
    explicit Observer(ZoomedCollapseHandler* handler) : handler_(handler) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      EXPECT_TRUE(success) << "Step " << message_id << " failed: "
                           << std::string(static_cast<const char*>(result),
                                          result_size);
      if (!success) {
        handler_->DestroyTest();
        return;
      }

      auto dict = ParseResponse(result, result_size);
      EXPECT_TRUE(dict.get());
      if (!dict) {
        handler_->DestroyTest();
        return;
      }

      auto nodes = dict->GetList("nodes");
      EXPECT_TRUE(nodes.get());
      if (!nodes) {
        handler_->DestroyTest();
        return;
      }

      switch (static_cast<Step>(message_id)) {
        case kDefaultZoom: {
          // At default zoom, "Edge Nav" at ~400px is within the ~550px
          // content area. Fully serialized with children.
          auto nav = FindNodeByName(nodes, "Edge Nav");
          EXPECT_TRUE(nav.get());
          if (nav) {
            EXPECT_FALSE(HasEmptyChildIds(nav));
          }
          EXPECT_TRUE(HasNodeWithName(nodes, "Edge Link"));

          handler_->tree_fetch_ct_++;

          // Zoom in heavily (level 5.0 ≈ 300%). Viewport in CSS pixels
          // shrinks to ~183px tall, pushing "Edge Nav" at ~400px off-screen.
          browser->GetHost()->SetZoomLevel(5.0);
          CefPostDelayedTask(
              TID_UI,
              base::BindOnce(&ZoomedCollapseHandler::FetchTreeAgain, handler_),
              200);
          break;
        }

        case kZoomedIn: {
          // After zoom, "Edge Nav" should now be off-screen and collapsed.
          auto nav = FindNodeByName(nodes, "Edge Nav");
          EXPECT_TRUE(nav.get());
          if (nav) {
            EXPECT_TRUE(HasEmptyChildIds(nav));
          }

          // Its child should be pruned.
          EXPECT_FALSE(HasNodeWithName(nodes, "Edge Link"));

          // Top content should still be visible.
          EXPECT_TRUE(HasNodeWithName(nodes, "Top Content"));

          handler_->tree_fetch_ct_++;
          handler_->DestroyTest();
          break;
        }
      }
    }

   private:
    ZoomedCollapseHandler* handler_;
    IMPLEMENT_REFCOUNTING(Observer);
  };

  void FetchTreeAgain() {
    GetBrowser()->GetHost()->ExecuteDevToolsMethod(
        ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
  }

  int next_message_id_ = 0;
  int tree_fetch_ct_ = 0;
  CefRefPtr<CefRegistration> registration_;
  IMPLEMENT_REFCOUNTING(ZoomedCollapseHandler);
};

// Test: queryAXTree still returns off-screen nodes when collapse is enabled.
// Agents use queryAXTree for scroll targeting — filtering would break that.
class QueryAXTreeHandler : public TestHandler {
 public:
  QueryAXTreeHandler() = default;
  QueryAXTreeHandler(const QueryAXTreeHandler&) = delete;
  QueryAXTreeHandler& operator=(const QueryAXTreeHandler&) = delete;

  void RunTest() override {
    AddResource(kTestUrl, kTestHtml, "text/html");

    CefBrowserSettings settings;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
    settings.ax_viewport_collapse = STATE_ENABLED;
#endif
    CreateBrowserWithSettings(kTestUrl, settings);
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    registration_ =
        browser->GetHost()->AddDevToolsMessageObserver(new Observer(this));
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Get the document root node (also enables the DOM domain).
      GetBrowser()->GetHost()->ExecuteDevToolsMethod(
          ++next_message_id_, "DOM.getDocument", nullptr);
    }
  }

  void DestroyTest() override {
    registration_ = nullptr;
    EXPECT_TRUE(got_result_);
    TestHandler::DestroyTest();
  }

 protected:
  class Observer : public CefDevToolsMessageObserver {
   public:
    explicit Observer(QueryAXTreeHandler* handler) : handler_(handler) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      if (message_id == 1) {
        // DOM.getDocument succeeded. Extract root nodeId and query.
        EXPECT_TRUE(success)
            << "DOM.getDocument failed: "
            << std::string(static_cast<const char*>(result), result_size);
        if (!success) {
          handler_->DestroyTest();
          return;
        }
        auto dict = ParseResponse(result, result_size);
        auto root = dict ? dict->GetDictionary("root") : nullptr;
        int node_id = root ? root->GetInt("nodeId") : 0;
        EXPECT_GT(node_id, 0);
        if (node_id <= 0) {
          handler_->DestroyTest();
          return;
        }

        CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
        params->SetInt("nodeId", node_id);
        params->SetString("accessibleName", "Offscreen Button");
        browser->GetHost()->ExecuteDevToolsMethod(
            ++handler_->next_message_id_, "Accessibility.queryAXTree", params);
        return;
      }

      // queryAXTree result.
      EXPECT_TRUE(success) << "Accessibility.queryAXTree failed: "
                           << std::string(static_cast<const char*>(result),
                                          result_size);
      if (!success) {
        handler_->DestroyTest();
        return;
      }

      auto dict = ParseResponse(result, result_size);
      EXPECT_TRUE(dict.get());
      if (!dict) {
        handler_->DestroyTest();
        return;
      }

      auto nodes = dict->GetList("nodes");
      EXPECT_TRUE(nodes.get());
      if (!nodes) {
        handler_->DestroyTest();
        return;
      }

      // queryAXTree should still find the off-screen button even with
      // collapse enabled, because queryAXTree is not filtered.
      EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Button"));
      handler_->got_result_ = true;
      handler_->DestroyTest();
    }

   private:
    QueryAXTreeHandler* handler_;
    IMPLEMENT_REFCOUNTING(Observer);
  };

  int next_message_id_ = 0;
  bool got_result_ = false;
  CefRefPtr<CefRegistration> registration_;
  IMPLEMENT_REFCOUNTING(QueryAXTreeHandler);
};

// Test: Full agentic workflow — get collapsed tree, find off-screen landmark,
// scroll to it, re-get tree and verify it's now fully serialized.
class AgenticWorkflowHandler : public TestHandler {
 public:
  AgenticWorkflowHandler() = default;
  AgenticWorkflowHandler(const AgenticWorkflowHandler&) = delete;
  AgenticWorkflowHandler& operator=(const AgenticWorkflowHandler&) = delete;

  void RunTest() override {
    AddResource(kTestUrl, kTestHtml, "text/html");

    CefBrowserSettings settings;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
    settings.ax_viewport_collapse = STATE_ENABLED;
#endif
    CreateBrowserWithSettings(kTestUrl, settings);
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    registration_ =
        browser->GetHost()->AddDevToolsMessageObserver(new Observer(this));
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Step 1: Get the collapsed AX tree.
      GetBrowser()->GetHost()->ExecuteDevToolsMethod(
          ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
    }
  }

  void DestroyTest() override {
    registration_ = nullptr;
    EXPECT_EQ(2, tree_fetch_ct_);
    TestHandler::DestroyTest();
  }

 private:
  // CDP call sequence:
  //   1: Accessibility.getFullAXTree (collapsed)
  //   2: DOM.scrollIntoViewIfNeeded (scroll to off-screen landmark)
  //   3: Accessibility.getFullAXTree (after scroll)
  enum Step { kInitialTree = 1, kScroll = 2, kPostScrollTree = 3 };

  class Observer : public CefDevToolsMessageObserver {
   public:
    explicit Observer(AgenticWorkflowHandler* handler) : handler_(handler) {}
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      EXPECT_TRUE(success) << "Step " << message_id << " failed: "
                           << std::string(static_cast<const char*>(result),
                                          result_size);
      if (!success) {
        handler_->DestroyTest();
        return;
      }

      auto dict = ParseResponse(result, result_size);
      if (!dict) {
        handler_->DestroyTest();
        return;
      }

      switch (static_cast<Step>(message_id)) {
        case kInitialTree: {
          auto nodes = dict->GetList("nodes");
          EXPECT_TRUE(nodes.get());
          if (!nodes) {
            handler_->DestroyTest();
            return;
          }

          // Top nav should be in-viewport and fully serialized.
          auto nav = FindNodeByName(nodes, "Main Navigation");
          EXPECT_TRUE(nav.get());
          if (nav) {
            EXPECT_FALSE(HasEmptyChildIds(nav));
          }

          // Off-screen landmark should be a collapsed summary.
          auto footer = FindNodeByName(nodes, "Footer Info");
          EXPECT_TRUE(footer.get());
          if (footer) {
            EXPECT_TRUE(HasEmptyChildIds(footer));
            // Extract backendDOMNodeId to scroll to it.
            handler_->target_backend_node_id_ =
                footer->GetInt("backendDOMNodeId");
            EXPECT_GT(handler_->target_backend_node_id_, 0);
          }

          // Off-screen interactive nodes should be pruned.
          EXPECT_FALSE(HasNodeWithName(nodes, "Offscreen Button"));

          handler_->tree_fetch_ct_++;

          // Step 2: Scroll the off-screen landmark into view.
          CefRefPtr<CefDictionaryValue> params = CefDictionaryValue::Create();
          params->SetInt("backendNodeId", handler_->target_backend_node_id_);
          browser->GetHost()->ExecuteDevToolsMethod(
              ++handler_->next_message_id_, "DOM.scrollIntoViewIfNeeded",
              params);
          break;
        }

        case kScroll: {
          // Scroll succeeded. Wait for layout update, then re-fetch tree.
          CefPostDelayedTask(
              TID_UI,
              base::BindOnce(&AgenticWorkflowHandler::FetchTreeAgain, handler_),
              200);
          break;
        }

        case kPostScrollTree: {
          auto nodes = dict->GetList("nodes");
          EXPECT_TRUE(nodes.get());
          if (!nodes) {
            handler_->DestroyTest();
            return;
          }

          // After scrolling, the footer landmark should now be fully
          // serialized with children (no longer a collapsed summary).
          auto footer = FindNodeByName(nodes, "Footer Info");
          EXPECT_TRUE(footer.get());
          if (footer) {
            EXPECT_FALSE(HasEmptyChildIds(footer));
          }

          // The heading and button inside the footer should now be visible.
          EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Heading"));
          EXPECT_TRUE(HasNodeWithName(nodes, "Offscreen Button"));

          // The top nav that was previously in-viewport should now be
          // collapsed (scrolled out of view).
          auto nav = FindNodeByName(nodes, "Main Navigation");
          EXPECT_TRUE(nav.get());
          if (nav) {
            EXPECT_TRUE(HasEmptyChildIds(nav));
          }

          handler_->tree_fetch_ct_++;
          handler_->DestroyTest();
          break;
        }
      }
    }

   private:
    AgenticWorkflowHandler* handler_;
    IMPLEMENT_REFCOUNTING(Observer);
  };

  void FetchTreeAgain() {
    GetBrowser()->GetHost()->ExecuteDevToolsMethod(
        ++next_message_id_, "Accessibility.getFullAXTree", nullptr);
  }

  int next_message_id_ = 0;
  int tree_fetch_ct_ = 0;
  int target_backend_node_id_ = 0;
  CefRefPtr<CefRegistration> registration_;
  IMPLEMENT_REFCOUNTING(AgenticWorkflowHandler);
};

}  // namespace

TEST(AxViewportCollapseTest, CollapseDefault) {
  CefRefPtr<CollapseOffHandler> handler = new CollapseOffHandler(STATE_DEFAULT);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, CollapseDisabled) {
  CefRefPtr<CollapseOffHandler> handler =
      new CollapseOffHandler(STATE_DISABLED);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, CollapseEnabled) {
  CefRefPtr<CollapseEnabledHandler> handler = new CollapseEnabledHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, OffscreenHeadingLevel) {
  CefRefPtr<OffscreenHeadingLevelHandler> handler =
      new OffscreenHeadingLevelHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, ChildIdsFiltered) {
  CefRefPtr<ChildIdsFilteredHandler> handler = new ChildIdsFilteredHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, FixedPositionVisible) {
  CefRefPtr<FixedPositionVisibleHandler> handler =
      new FixedPositionVisibleHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, NestedOffscreen) {
  CefRefPtr<NestedOffscreenHandler> handler = new NestedOffscreenHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, RuntimeToggle) {
  CefRefPtr<RuntimeToggleHandler> handler = new RuntimeToggleHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, AllVisible) {
  CefRefPtr<AllVisibleHandler> handler = new AllVisibleHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, ZoomedCollapse) {
  CefRefPtr<ZoomedCollapseHandler> handler = new ZoomedCollapseHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, QueryAXTreeUnfiltered) {
  CefRefPtr<QueryAXTreeHandler> handler = new QueryAXTreeHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(AxViewportCollapseTest, AgenticWorkflow) {
  CefRefPtr<AgenticWorkflowHandler> handler = new AgenticWorkflowHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
