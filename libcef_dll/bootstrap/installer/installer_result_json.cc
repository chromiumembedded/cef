// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_result_json.h"

#include "base/strings/string_number_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"

namespace cef_installer::internal {

base::DictValue BuildResultJsonValue(const Result& result) {
  base::DictValue dict;
  dict.Set("success", result.success);
  dict.Set("outcome", OutcomeToString(result.outcome));

  if (!result.success) {
    dict.Set("error_code", result.error_code);
    dict.Set("error_name", ExitCodeToString(result.error_code));
    dict.Set("error_message", result.error_message);
  }

  if (!result.warnings.empty()) {
    base::ListValue warning_list;
    for (const auto& warning : result.warnings) {
      warning_list.Append(warning);
    }
    dict.Set("warnings", std::move(warning_list));
  }

  if (result.retention_plan) {
    dict.Set("max_age_days", result.retention_max_age_days);
    dict.Set("eligible", result.retention_plan->eligible);
    dict.Set("store_blocked", result.retention_plan->store_blocked);
    dict.Set("registrations_committed", result.registrations_committed);
    dict.Set("versions_pruned", result.versions_pruned);
    dict.Set("retry_required", result.retry_required);
    if (!result.retention_plan->blocker.empty()) {
      dict.Set("store_blocker", result.retention_plan->blocker);
    }
    base::ListValue registrations;
    for (const auto& report : result.retention_plan->registrations) {
      base::DictValue item;
      item.Set("appid", report.entry.uuid);
      item.Set("platform", report.entry.platform);
      item.Set("vmin", report.entry.vmin);
      item.Set("vmax", report.entry.vmax);
      item.Set("abi_hash", report.entry.abi_hash);
      item.Set("evidence_kind",
               RetentionEvidenceKindToString(report.evidence.kind));
      if (report.evidence.timestamp != 0) {
        item.Set("evidence_time",
                 base::NumberToString(report.evidence.timestamp));
      }
      if (report.age_days) {
        item.Set("age_days", base::NumberToString(*report.age_days));
      }
      item.Set("decision",
               RetentionRegistrationDecisionToString(report.decision));
      item.Set("reason", RetentionReasonToString(report.reason));
      if (!report.evidence.diagnostic.empty()) {
        item.Set("diagnostic", report.evidence.diagnostic);
      }
      registrations.Append(std::move(item));
    }
    dict.Set("registrations", std::move(registrations));

    base::ListValue versions;
    for (const auto& report : result.retention_plan->versions) {
      base::DictValue item;
      item.Set("version", report.version.ToString());
      item.Set("platform", report.platform);
      item.Set("required_before", report.required_before);
      item.Set("required_after", report.required_after);
      item.Set("decision", RetentionVersionDecisionToString(report.decision));
      item.Set("reason", RetentionReasonToString(report.reason));
      item.Set("expected_removal", report.expected_removal);
      item.Set("cleanup_deferred", report.cleanup_deferred);
      versions.Append(std::move(item));
    }
    dict.Set("versions", std::move(versions));
  }

  if (!result.libcef_path.empty()) {
    dict.Set("libcef_path", result.libcef_path.AsUTF8Unsafe());
  }
  if (result.is_bundled) {
    dict.Set("is_bundled", true);
  }
  if (!result.installed_version.empty()) {
    dict.Set("installed_version", result.installed_version);
  }
  if (!result.version_full.empty()) {
    dict.Set("version_full", result.version_full);
  }
  return dict;
}

}  // namespace cef_installer::internal
