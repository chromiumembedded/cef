// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr ULONG_PTR kProgress = 0x43454649;
constexpr ULONG_PTR kLifecycle = 0x43454652;
constexpr size_t kMaxPayload = 32 * 1024;

struct RawMessage {
  ULONG_PTR kind;
  std::string payload;
};

std::vector<RawMessage>& Messages() {
  static auto* messages = new std::vector<RawMessage>();
  return *messages;
}

bool g_cancel = false;
bool g_lifecycle_mode = false;
bool g_close_after_handoff = false;
bool g_non_pumping = false;
LRESULT g_lifecycle_result = 1;
size_t g_rejected = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_COPYDATA) {
    const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
    if (!data || !data->lpData || data->cbData < 2 ||
        data->cbData > kMaxPayload ||
        (data->dwData != kProgress && data->dwData != kLifecycle)) {
      ++g_rejected;
      return 0;
    }
    const char* bytes = static_cast<const char*>(data->lpData);
    if (bytes[data->cbData - 1] != '\0' ||
        std::memchr(bytes, '\0', data->cbData - 1)) {
      ++g_rejected;
      return 0;
    }
    Messages().push_back({data->dwData, std::string(bytes, data->cbData - 1)});
    return data->dwData == kProgress ? (g_cancel ? 2 : 1) : g_lifecycle_result;
  }
  if (msg == WM_CLOSE) {
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

void WriteEscaped(FILE* file, std::string_view value) {
  std::fputc('"', file);
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') {
      std::fputc('\\', file);
      std::fputc(c, file);
    } else if (c == '\n') {
      std::fputs("\\n", file);
    } else if (c == '\r') {
      std::fputs("\\r", file);
    } else if (c == '\t') {
      std::fputs("\\t", file);
    } else if (c < 0x20) {
      std::fprintf(file, "\\u%04x", c);
    } else {
      std::fputc(c, file);
    }
  }
  std::fputc('"', file);
}

void WriteArray(FILE* file, ULONG_PTR kind, bool raw_json) {
  bool first = true;
  std::fputc('[', file);
  for (const auto& message : Messages()) {
    if (message.kind != kind) {
      continue;
    }
    if (!first) {
      std::fputc(',', file);
    }
    if (raw_json) {
      std::fputs(message.payload.c_str(), file);
    } else {
      WriteEscaped(file, message.payload);
    }
    first = false;
  }
  std::fputc(']', file);
}

void WriteOutput(const char* path) {
  FILE* file = std::fopen(path, "w");
  if (!file) {
    return;
  }
  if (!g_lifecycle_mode) {
    WriteArray(file, kProgress, true);
  } else {
    std::fputs("{\"progress\":", file);
    WriteArray(file, kProgress, false);
    std::fputs(",\"lifecycle\":", file);
    WriteArray(file, kLifecycle, false);
    std::fprintf(file, ",\"rejected\":%zu}", g_rejected);
  }
  std::fputc('\n', file);
  std::fclose(file);
}

bool HasEvent(std::string_view event) {
  const std::string needle = "\"event\":\"" + std::string(event) + "\"";
  for (const auto& message : Messages()) {
    if (message.kind == kLifecycle &&
        message.payload.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: %s <output_path> [options]\n", argv[0]);
    return 1;
  }
  bool allow_copydata = false;
  for (int i = 2; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--cancel") {
      g_cancel = true;
    } else if (arg == "--lifecycle") {
      g_lifecycle_mode = true;
    } else if (arg == "--close-after-handoff") {
      g_close_after_handoff = true;
    } else if (arg == "--non-pumping") {
      g_non_pumping = true;
    } else if (arg == "--allow-copydata") {
      allow_copydata = true;
    } else if (arg.starts_with("--lifecycle-return=")) {
      g_lifecycle_result = static_cast<LRESULT>(std::strtoll(
          argv[i] + std::strlen("--lifecycle-return="), nullptr, 10));
    }
  }

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = ::GetModuleHandleW(nullptr);
  wc.lpszClassName = L"CefE2EProgressHelper";
  ::RegisterClassExW(&wc);
  HWND hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    return 1;
  }
  if (allow_copydata) {
    CHANGEFILTERSTRUCT filter = {sizeof(filter)};
    ::ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, &filter);
  }
  std::printf("%llu\n", static_cast<unsigned long long>(
                            reinterpret_cast<uintptr_t>(hwnd)));
  std::fflush(stdout);

  const ULONGLONG start = ::GetTickCount64();
  MSG msg;
  while (::GetTickCount64() - start < 30000) {
    if (g_non_pumping) {
      ::Sleep(100);
      continue;
    }
    if (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        break;
      }
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    } else {
      ::MsgWaitForMultipleObjects(0, nullptr, FALSE, 100, QS_ALLINPUT);
    }
    // Sent messages can be dispatched while PeekMessageW returns FALSE, so
    // inspect copied payloads after either branch and outside WndProc.
    if ((HasEvent("operation_result") && HasEvent("relaunch_started")) ||
        (g_close_after_handoff && HasEvent("relaunch_started"))) {
      break;
    }
  }
  ::DestroyWindow(hwnd);
  WriteOutput(argv[1]);
  return 0;
}
