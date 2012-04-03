// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_command_line.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"

namespace {

class CefCommandLineImpl : public CefCommandLine {
 public:
  CefCommandLineImpl()
    : command_line_(CommandLine::NO_PROGRAM) {
  }

  virtual void InitFromArgv(int argc, const char* const* argv) OVERRIDE {
#if !defined(OS_WIN)
    AutoLock lock_scope(this);
    command_line_.InitFromArgv(argc, argv);
#else
    NOTREACHED() << "method not supported on this platform";
#endif
  }

  virtual void InitFromString(const CefString& command_line) OVERRIDE {
#if defined(OS_WIN)
    AutoLock lock_scope(this);
    command_line_.ParseFromString(command_line);
#else
    NOTREACHED() << "method not supported on this platform";
#endif
  }

  virtual CefString GetCommandLineString() OVERRIDE {
    AutoLock lock_scope(this);
    return command_line_.GetCommandLineString();
  }

  virtual CefString GetProgram() OVERRIDE {
    AutoLock lock_scope(this);
    return command_line_.GetProgram().value();
  }

  virtual void SetProgram(const CefString& program) OVERRIDE {
    AutoLock lock_scope(this);
    command_line_.SetProgram(FilePath(program));
  }

  virtual bool HasSwitches() OVERRIDE {
    AutoLock lock_scope(this);
    return (command_line_.GetSwitches().size() > 0);
  }

  virtual bool HasSwitch(const CefString& name) OVERRIDE {
    AutoLock lock_scope(this);
    return command_line_.HasSwitch(name);
  }

  virtual CefString GetSwitchValue(const CefString& name) OVERRIDE {
    AutoLock lock_scope(this);
    return command_line_.GetSwitchValueNative(name);
  }

  virtual void GetSwitches(SwitchMap& switches) OVERRIDE {
    AutoLock lock_scope(this);
    const CommandLine::SwitchMap& map = command_line_.GetSwitches();
    CommandLine::SwitchMap::const_iterator it = map.begin();
    for (; it != map.end(); ++it)
      switches.insert(std::make_pair(it->first, it->second));
  }

  virtual void AppendSwitch(const CefString& name) OVERRIDE {
    AutoLock lock_scope(this);
    command_line_.AppendSwitch(name);
  }

  virtual void AppendSwitchWithValue(const CefString& name,
                                     const CefString& value) OVERRIDE {
    AutoLock lock_scope(this);
    command_line_.AppendSwitchNative(name, value);
  }

  virtual bool HasArguments() OVERRIDE {
    AutoLock lock_scope(this);
    return (command_line_.GetArgs().size() > 0);
  }

  virtual void GetArguments(ArgumentList& arguments) OVERRIDE {
    AutoLock lock_scope(this);
    const CommandLine::StringVector& vec = command_line_.GetArgs();
    CommandLine::StringVector::const_iterator it = vec.begin();
    for (; it != vec.end(); ++it)
      arguments.push_back(*it);
  }

  virtual void AppendArgument(const CefString& argument) OVERRIDE {
    AutoLock lock_scope(this);
    command_line_.AppendArgNative(argument);
  }

 private:
  CommandLine command_line_;

  IMPLEMENT_REFCOUNTING(CefCommandLineImpl);
  IMPLEMENT_LOCKING(CefCommandLineImpl);
};

}  // namespace

// static
CefRefPtr<CefCommandLine> CefCommandLine::CreateCommandLine() {
  return new CefCommandLineImpl();
}
