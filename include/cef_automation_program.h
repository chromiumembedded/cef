// Copyright (c) 2026 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file must follow a specific format in order to
// support the CEF translator tool. See the translator.README.txt file in the
// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_AUTOMATION_PROGRAM_H_
#define CEF_INCLUDE_CEF_AUTOMATION_PROGRAM_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_values.h"

///
/// Represents a sequence of automation instructions that can be executed
/// as a single batch, reducing IPC round-trips. Each instruction in the
/// program is dispatched through the normal Blink event pipeline with
/// configurable delays between steps.
///
/// NOTE: This is an additive API. Existing CDP commands remain unchanged.
/// Agents must be retrained to use this interface.
///
/*--cef(source=library)--*/
class CefAutomationProgram : public virtual CefBaseRefCounted {
 public:
  ///
  /// Instruction types that can be added to the program.
  ///
  typedef enum {
    ///
    /// Navigate to a URL.
    /// Params: "url" (string, required).
    ///
    INSTRUCTION_NAVIGATE = 0,
    ///
    /// Wait for a condition (load, networkidle, selector).
    /// Params: "condition" (string: "load"|"networkidle"|"selector"),
    ///         "selector" (string, required when condition is "selector"),
    ///         "timeout" (int, optional, milliseconds).
    ///
    INSTRUCTION_WAIT,
    ///
    /// Click an element by selector.
    /// Params: "selector" (string, required),
    ///         "button" (string, optional: "left"|"right"|"middle").
    ///
    INSTRUCTION_CLICK,
    ///
    /// Type text into the focused element.
    /// Params: "text" (string, required),
    ///         "delay" (int, optional, milliseconds between keystrokes).
    ///
    INSTRUCTION_TYPE,
    ///
    /// Capture a text snapshot of the page.
    /// Params: none.
    ///
    INSTRUCTION_SNAPSHOT,
    ///
    /// Capture a screenshot.
    /// Params: "format" (string, optional: "png"|"jpeg"),
    ///         "quality" (int, optional, 0-100 for jpeg).
    ///
    INSTRUCTION_SCREENSHOT,
    ///
    /// Execute JavaScript (goes through normal Runtime.evaluate path).
    /// Params: "expression" (string, required),
    ///         "await_promise" (bool, optional).
    ///
    INSTRUCTION_EVALUATE,
    ///
    /// Wait for a specified duration.
    /// Params: "milliseconds" (int, required).
    ///
    INSTRUCTION_DELAY,
  } InstructionType;

  ///
  /// Create a new empty automation program.
  ///
  /*--cef()--*/
  static CefRefPtr<CefAutomationProgram> Create();

  ///
  /// Add an instruction to the program.
  /// |type| is the instruction type.
  /// |params| contains instruction-specific parameters as a dictionary.
  /// Returns the instruction index.
  ///
  /*--cef()--*/
  virtual int AddInstruction(InstructionType type,
                             CefRefPtr<CefDictionaryValue> params) = 0;

  ///
  /// Get the number of instructions in the program.
  ///
  /*--cef()--*/
  virtual int GetInstructionCount() = 0;

  ///
  /// Clear all instructions.
  ///
  /*--cef()--*/
  virtual void Clear() = 0;
};

///
/// Callback interface for automation program execution results.
///
/*--cef(source=client)--*/
class CefAutomationProgramCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called when the program completes execution.
  /// |success| is true if all instructions completed successfully.
  /// |results| contains a list of results, one per instruction.
  /// |failed_index| is the index of the first failed instruction, or -1.
  /// |error| contains an error message if success is false.
  ///
  /*--cef(optional_param=results,optional_param=error)--*/
  virtual void OnComplete(bool success,
                          CefRefPtr<CefListValue> results,
                          int failed_index,
                          const CefString& error) = 0;
};

#endif  // CEF_INCLUDE_CEF_AUTOMATION_PROGRAM_H_
