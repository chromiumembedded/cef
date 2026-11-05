// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/cefclient/browser/osr_touchpad_win.h"

// The SDK requires hidsdi.h before hidpi.h.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include <cstddef>
#include <utility>

#include "include/base/cef_logging.h"

namespace client {

namespace {

HWND g_touchpad_window = nullptr;
constexpr USAGE kDigitizerPage = 0x0d;
constexpr USAGE kTouchpadUsage = 0x05;
constexpr USAGE kContactId = 0x51;
constexpr USAGE kContactCount = 0x54;
constexpr USAGE kTipSwitch = 0x42;

bool HasUsage(const HIDP_VALUE_CAPS& cap, USAGE usage) {
  return cap.IsRange
             ? usage >= cap.Range.UsageMin && usage <= cap.Range.UsageMax
             : usage == cap.NotRange.Usage;
}

float NormalizeValue(ULONG value, const HIDP_VALUE_CAPS& cap) {
  int64_t signed_value = value;
  if (cap.LogicalMin < 0 && cap.BitSize && cap.BitSize <= 32) {
    const int64_t sign = int64_t{1} << (cap.BitSize - 1);
    signed_value = value & (sign - 1);
    if (value & sign) {
      signed_value -= sign;
    }
  }
  return NormalizeTouchpadAxis(signed_value, cap.LogicalMin, cap.LogicalMax);
}

}  // namespace

struct OsrTouchpadWin::Device {
  bool Initialize(HANDLE handle) {
    UINT size = 0;
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, nullptr, &size) !=
            0 ||
        !size) {
      return false;
    }
    data.resize(size);
    if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, data.data(), &size) ==
        static_cast<UINT>(-1)) {
      return false;
    }
    auto* preparsed = Preparsed();
    HIDP_CAPS caps = {};
    if (HidP_GetCaps(preparsed, &caps) != HIDP_STATUS_SUCCESS ||
        caps.UsagePage != kDigitizerPage || caps.Usage != kTouchpadUsage) {
      return false;
    }
    USHORT count = caps.NumberInputValueCaps;
    values.resize(count);
    if (!count || HidP_GetValueCaps(HidP_Input, values.data(), &count,
                                    preparsed) != HIDP_STATUS_SUCCESS) {
      return false;
    }
    values.resize(count);
    const auto has_value = [&](USAGE page, USAGE usage) {
      return std::any_of(values.begin(), values.end(), [&](const auto& cap) {
        return cap.UsagePage == page && HasUsage(cap, usage);
      });
    };
    if (!has_value(kDigitizerPage, kContactCount) ||
        !has_value(kDigitizerPage, kContactId) || !has_value(0x01, 0x30) ||
        !has_value(0x01, 0x31)) {
      return false;
    }
    usages.resize(
        HidP_MaxUsageListLength(HidP_Input, kDigitizerPage, preparsed));
    if (usages.empty()) {
      return false;
    }
    device_handle = handle;
    return true;
  }

  PHIDP_PREPARSED_DATA Preparsed() {
    return reinterpret_cast<PHIDP_PREPARSED_DATA>(data.data());
  }

  const HIDP_VALUE_CAPS* ReadValue(USAGE page,
                                   USAGE usage,
                                   USHORT link,
                                   char* report,
                                   ULONG size,
                                   ULONG& result) {
    for (const auto& cap : values) {
      if (cap.UsagePage == page && cap.LinkCollection == link &&
          cap.ReportID == static_cast<BYTE>(report[0]) &&
          HasUsage(cap, usage)) {
        if (HidP_GetUsageValue(HidP_Input, page, link, usage, &result,
                               Preparsed(), report,
                               size) == HIDP_STATUS_SUCCESS) {
          return &cap;
        }
      }
    }
    return nullptr;
  }

  std::optional<OsrTouchpadFrame> ReadReport(char* report, ULONG size) {
    invalid_report = false;
    ULONG count = 0;
    if (!ReadValue(kDigitizerPage, kContactCount, 0, report, size, count)) {
      // Other report IDs may carry buttons or vendor-specific information.
      return std::nullopt;
    }
    if (count > 256) {
      assembler.Reset();
      invalid_report = true;
      return std::nullopt;
    }
    std::set<USHORT> links;
    for (const auto& cap : values) {
      if (cap.UsagePage == kDigitizerPage && HasUsage(cap, kContactId) &&
          cap.ReportID == static_cast<BYTE>(report[0])) {
        links.insert(cap.LinkCollection);
      }
    }
    OsrTouchpadFrame contacts;
    for (auto link : links) {
      ULONG id = 0, x = 0, y = 0;
      const auto* id_cap =
          ReadValue(kDigitizerPage, kContactId, link, report, size, id);
      const auto* x_cap = ReadValue(0x01, 0x30, link, report, size, x);
      const auto* y_cap = ReadValue(0x01, 0x31, link, report, size, y);
      ULONG usage_count = static_cast<ULONG>(usages.size());
      if (!id_cap || !x_cap || !y_cap ||
          x_cap->LogicalMax <= x_cap->LogicalMin ||
          y_cap->LogicalMax <= y_cap->LogicalMin ||
          HidP_GetUsages(HidP_Input, kDigitizerPage, link, usages.data(),
                         &usage_count, Preparsed(), report,
                         size) != HIDP_STATUS_SUCCESS) {
        assembler.Reset();
        invalid_report = true;
        return std::nullopt;
      }
      const bool down = std::find(usages.begin(), usages.begin() + usage_count,
                                  kTipSwitch) != usages.begin() + usage_count;
      contacts.push_back(
          {id, NormalizeValue(x, *x_cap), NormalizeValue(y, *y_cap), down});
    }
    return assembler.AddReport(count, contacts);
  }

  HANDLE device_handle = nullptr;
  std::vector<BYTE> data;
  std::vector<HIDP_VALUE_CAPS> values;
  std::vector<USAGE> usages;
  OsrTouchpadFrameAssembler assembler;
  bool invalid_report = false;
};

OsrTouchpadWin::OsrTouchpadWin(HWND window, OsrTouchpadCallback callback)
    : window_(window), callback_(std::move(callback)) {}

OsrTouchpadWin::~OsrTouchpadWin() {
  SetFocused(false);
}

bool OsrTouchpadWin::SetFocused(bool focused) {
  if (focused) {
    RAWINPUTDEVICE device = {kDigitizerPage, kTouchpadUsage, RIDEV_DEVNOTIFY,
                             window_};
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
      LOG(WARNING) << "Cannot register Precision Touchpad input: "
                   << GetLastError();
      return false;
    }
    g_touchpad_window = window_;
    device_.reset();
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) ==
        0) {
      std::vector<RAWINPUTDEVICELIST> devices(count);
      const UINT found = GetRawInputDeviceList(devices.data(), &count,
                                               sizeof(RAWINPUTDEVICELIST));
      if (found != static_cast<UINT>(-1)) {
        for (UINT i = 0; i < found; ++i) {
          if (devices[i].dwType == RIM_TYPEHID) {
            auto candidate = std::make_unique<Device>();
            if (candidate->Initialize(devices[i].hDevice)) {
              device_ = std::move(candidate);
              break;
            }
          }
        }
      }
    }
    callback_(OsrTouchpadFrame());
  } else {
    if (g_touchpad_window == window_) {
      RAWINPUTDEVICE device = {kDigitizerPage, kTouchpadUsage, RIDEV_REMOVE,
                               nullptr};
      RegisterRawInputDevices(&device, 1, sizeof(device));
      g_touchpad_window = nullptr;
    }
    device_.reset();
    rejected_devices_.clear();
  }
  return true;
}

bool OsrTouchpadWin::available() const {
  return g_touchpad_window == window_ && device_;
}

void OsrTouchpadWin::OnInput(HRAWINPUT input) {
  if (g_touchpad_window != window_) {
    return;
  }
  UINT size = 0;
  if (GetRawInputData(input, RID_INPUT, nullptr, &size,
                      sizeof(RAWINPUTHEADER)) != 0 ||
      size < sizeof(RAWINPUTHEADER)) {
    return;
  }
  std::vector<BYTE> data(size);
  if (GetRawInputData(input, RID_INPUT, data.data(), &size,
                      sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) {
    callback_(std::nullopt);
    return;
  }
  const auto* raw = reinterpret_cast<const RAWINPUT*>(data.data());
  const size_t offset = offsetof(RAWINPUT, data) + offsetof(RAWHID, bRawData);
  if (raw->header.dwType != RIM_TYPEHID || size < offset ||
      !raw->data.hid.dwSizeHid ||
      raw->data.hid.dwCount > (size - offset) / raw->data.hid.dwSizeHid) {
    return;
  }
  const HANDLE handle = raw->header.hDevice;
  if (!device_) {
    if (rejected_devices_.contains(handle)) {
      return;
    }
    auto candidate = std::make_unique<Device>();
    if (!candidate->Initialize(handle)) {
      rejected_devices_.insert(handle);
      return;
    }
    device_ = std::move(candidate);
  }
  if (device_->device_handle != handle) {
    return;
  }
  for (DWORD i = 0; i < raw->data.hid.dwCount; ++i) {
    char* report = reinterpret_cast<char*>(data.data() + offset +
                                           i * raw->data.hid.dwSizeHid);
    if (auto frame = device_->ReadReport(report, raw->data.hid.dwSizeHid)) {
      callback_(frame);
    } else if (device_->invalid_report) {
      callback_(std::nullopt);
    }
  }
}

void OsrTouchpadWin::OnDeviceRemoved(HANDLE device) {
  rejected_devices_.erase(device);
  if (device_ && device_->device_handle == device) {
    device_.reset();
    callback_(std::nullopt);
  }
}

}  // namespace client
