// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUTOMATION_PROGRAM_IMPL_H_
#define CEF_LIBCEF_BROWSER_AUTOMATION_PROGRAM_IMPL_H_
#pragma once

#include <vector>

#include "cef/include/cef_automation_program.h"
#include "cef/include/cef_values.h"

// CefAutomationProgram implementation.
//
// Stores a sequence of automation instructions that can be executed as a
// batch. The execution engine is a future step — this class provides the
// data structure and public API for building programs.
class CefAutomationProgramImpl : public CefAutomationProgram {
 public:
  CefAutomationProgramImpl();

  CefAutomationProgramImpl(const CefAutomationProgramImpl&) = delete;
  CefAutomationProgramImpl& operator=(const CefAutomationProgramImpl&) = delete;

  // CefAutomationProgram methods.
  int AddInstruction(InstructionType type,
                     CefRefPtr<CefDictionaryValue> params) override;
  int GetInstructionCount() override;
  void Clear() override;

  // Internal: access the instruction list for execution.
  struct Instruction {
    InstructionType type;
    CefRefPtr<CefDictionaryValue> params;
  };

  const std::vector<Instruction>& GetInstructions() const {
    return instructions_;
  }

 private:
  std::vector<Instruction> instructions_;

  IMPLEMENT_REFCOUNTING(CefAutomationProgramImpl);
};

#endif  // CEF_LIBCEF_BROWSER_AUTOMATION_PROGRAM_IMPL_H_
