// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/common/command_line_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

CefCommandLineImpl::CefCommandLineImpl(base::CommandLine* value,
                                       bool will_delete,
                                       bool read_only)
    : CefValueBase<CefCommandLine, base::CommandLine>(
          value,
          nullptr,
          will_delete ? kOwnerWillDelete : kOwnerNoDelete,
          read_only,
          nullptr) {}

CefCommandLineImpl::CefCommandLineImpl(const base::CommandLine& value)
    : CefCommandLineImpl(const_cast<base::CommandLine*>(&value),
                         /*will_delete=*/false,
                         /*read_only=*/true) {}

bool CefCommandLineImpl::IsValid() {
  return !detached();
}

bool CefCommandLineImpl::IsReadOnly() {
  return read_only();
}

CefRefPtr<CefCommandLine> CefCommandLineImpl::Copy() {
  CEF_VALUE_VERIFY_RETURN(false, nullptr);
  return new CefCommandLineImpl(new base::CommandLine(const_value().argv()),
                                true, false);
}

void CefCommandLineImpl::InitFromArgv(int argc, const char* const* argv) {
#if !BUILDFLAG(IS_WIN)
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  mutable_value()->InitFromArgv(argc, argv);
#else
  DCHECK(false) << "method not supported on this platform";
#endif
}

void CefCommandLineImpl::InitFromString(const CefString& command_line) {
#if BUILDFLAG(IS_WIN)
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  const std::wstring& str16 = command_line;
  mutable_value()->ParseFromString(str16);
#else
  DCHECK(false) << "method not supported on this platform";
#endif
}

void CefCommandLineImpl::Reset() {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  base::CommandLine::StringVector argv;
  argv.push_back(mutable_value()->GetProgram().value());
  mutable_value()->InitFromArgv(argv);

  const base::CommandLine::SwitchMap& map = mutable_value()->GetSwitches();
  const_cast<base::CommandLine::SwitchMap*>(&map)->clear();
}

void CefCommandLineImpl::GetArgv(std::vector<CefString>& argv) {
  CEF_VALUE_VERIFY_RETURN_VOID(false);
  for (const auto& arg : const_value().argv()) {
    argv.push_back(arg);
  }
}

CefString CefCommandLineImpl::GetCommandLineString() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetCommandLineString();
}

CefString CefCommandLineImpl::GetProgram() {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetProgram().value();
}

void CefCommandLineImpl::SetProgram(const CefString& program) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  mutable_value()->SetProgram(base::FilePath(program));
}

bool CefCommandLineImpl::HasSwitches() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (const_value().GetSwitches().size() > 0);
}

bool CefCommandLineImpl::HasSwitch(const CefString& name) {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return const_value().HasSwitch(base::ToLowerASCII(name.ToString()));
}

CefString CefCommandLineImpl::GetSwitchValue(const CefString& name) {
  CEF_VALUE_VERIFY_RETURN(false, CefString());
  return const_value().GetSwitchValueNative(
      base::ToLowerASCII(name.ToString()));
}

void CefCommandLineImpl::GetSwitches(SwitchMap& switches) {
  CEF_VALUE_VERIFY_RETURN_VOID(false);
  for (const auto& [key, value] : const_value().GetSwitches()) {
    switches.insert(std::make_pair(key, value));
  }
}

void CefCommandLineImpl::AppendSwitch(const CefString& name) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  mutable_value()->AppendSwitch(name.ToString());
}

void CefCommandLineImpl::AppendSwitchWithValue(const CefString& name,
                                               const CefString& value) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
#if BUILDFLAG(IS_WIN)
  mutable_value()->AppendSwitchNative(name.ToString(), value.ToWString());
#else
  mutable_value()->AppendSwitchNative(name.ToString(), value.ToString());
#endif
}

void CefCommandLineImpl::RemoveSwitch(const CefString& name) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
  mutable_value()->RemoveSwitch(base::ToLowerASCII(name.ToString()));
}

bool CefCommandLineImpl::HasArguments() {
  CEF_VALUE_VERIFY_RETURN(false, false);
  return (const_value().GetArgs().size() > 0);
}

void CefCommandLineImpl::GetArguments(ArgumentList& arguments) {
  CEF_VALUE_VERIFY_RETURN_VOID(false);
  for (const auto& arg : const_value().GetArgs()) {
    arguments.push_back(arg);
  }
}

void CefCommandLineImpl::AppendArgument(const CefString& argument) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
#if BUILDFLAG(IS_WIN)
  mutable_value()->AppendArgNative(argument.ToWString());
#else
  mutable_value()->AppendArgNative(argument.ToString());
#endif
}

void CefCommandLineImpl::PrependWrapper(const CefString& wrapper) {
  CEF_VALUE_VERIFY_RETURN_VOID(true);
#if BUILDFLAG(IS_WIN)
  mutable_value()->PrependWrapper(wrapper.ToWString());
#else
  mutable_value()->PrependWrapper(wrapper.ToString());
#endif
}

// CefCommandLine implementation.

// static
CefRefPtr<CefCommandLine> CefCommandLine::CreateCommandLine() {
  return new CefCommandLineImpl(
      new base::CommandLine(base::CommandLine::NO_PROGRAM), true, false);
}

// static
CefRefPtr<CefCommandLine> CefCommandLine::GetGlobalCommandLine() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
  // Uses a singleton reference object.
  static CefRefPtr<CefCommandLineImpl> commandLinePtr;
#pragma clang diagnostic pop
  if (!commandLinePtr.get()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line) {
      commandLinePtr = new CefCommandLineImpl(command_line, false, true);
    }
  }
  return commandLinePtr.get();
}
