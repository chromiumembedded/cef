// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/automation_program_impl.h"

#include "base/logging.h"

// static
CefRefPtr<CefAutomationProgram> CefAutomationProgram::Create() {
  return new CefAutomationProgramImpl();
}

CefAutomationProgramImpl::CefAutomationProgramImpl() = default;

int CefAutomationProgramImpl::AddInstruction(
    InstructionType type,
    CefRefPtr<CefDictionaryValue> params) {
  DCHECK(params.get());
  if (!params.get()) {
    LOG(WARNING) << "AddInstruction called with null params";
    params = CefDictionaryValue::Create();
  }

  Instruction instruction;
  instruction.type = type;
  instruction.params = params;

  int index = static_cast<int>(instructions_.size());
  instructions_.push_back(std::move(instruction));
  return index;
}

int CefAutomationProgramImpl::GetInstructionCount() {
  return static_cast<int>(instructions_.size());
}

void CefAutomationProgramImpl::Clear() {
  instructions_.clear();
}
