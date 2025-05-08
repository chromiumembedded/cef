#include "build/build_config.h"

// Based on chrome/app/chrome_exe_main_win.cc.
// In 32-bit builds, the main thread starts with the default (small) stack size.
// The ARCH_CPU_32_BITS blocks here and below are in support of moving the main
// thread to a fiber with a larger stack size.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_32_BITS)

#include "base/debug/alias.h"
#include "include/internal/cef_app_win.h"

namespace {

// The information needed to transfer control to the large-stack fiber and later
// pass the main routine's exit code back to the small-stack fiber prior to
// termination.
struct FiberState {
  FiberState(wWinMainPtr wWinMain,
             HINSTANCE hInstance,
             LPWSTR lpCmdLine,
             int nCmdShow) {
    this->wWinMain = wWinMain;
    this->hInstance = hInstance;
    this->lpCmdLine = lpCmdLine;
    this->nCmdShow = nCmdShow;
  }

  FiberState(mainPtr main, int argc, char** argv) {
    this->main = main;
    this->argc = argc;
    this->argv = argv;
  }

  wWinMainPtr wWinMain = nullptr;
  HINSTANCE hInstance;
  LPWSTR lpCmdLine;
  int nCmdShow;

  mainPtr main = nullptr;
  int argc;
  char** argv;

  LPVOID original_fiber;
  int fiber_result;
};

// A PFIBER_START_ROUTINE function run on a large-stack fiber that calls the
// main routine, stores its return value, and returns control to the small-stack
// fiber. |params| must be a pointer to a FiberState struct.
void WINAPI FiberBinder(void* params) {
  auto* fiber_state = static_cast<FiberState*>(params);
  // Call the main routine from the fiber. Reusing the entry point minimizes
  // confusion when examining call stacks in crash reports - seeing wWinMain on
  // the stack is a handy hint that this is the main thread of the process.
  if (fiber_state->main) {
    fiber_state->fiber_result =
        fiber_state->main(fiber_state->argc, fiber_state->argv);
  } else {
    fiber_state->fiber_result =
        fiber_state->wWinMain(fiber_state->hInstance, nullptr,
                              fiber_state->lpCmdLine, fiber_state->nCmdShow);
  }

  // Switch back to the main thread to exit.
  ::SwitchToFiber(fiber_state->original_fiber);
}

int RunMainWithPreferredStackSize(FiberState& fiber_state) {
  enum class FiberStatus { kConvertFailed, kCreateFiberFailed, kSuccess };
  FiberStatus fiber_status = FiberStatus::kSuccess;
  // GetLastError result if fiber conversion failed.
  DWORD fiber_error = ERROR_SUCCESS;
  if (!::IsThreadAFiber()) {
    // Make the main thread's stack size 4 MiB so that it has roughly the same
    // effective size as the 64-bit build's 8 MiB stack.
    constexpr size_t kStackSize = 4 * 1024 * 1024;  // 4 MiB
    // Leak the fiber on exit.
    LPVOID original_fiber =
        ::ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
    if (original_fiber) {
      fiber_state.original_fiber = original_fiber;
      // Create a fiber with a bigger stack and switch to it. Leak the fiber on
      // exit.
      LPVOID big_stack_fiber = ::CreateFiberEx(
          0, kStackSize, FIBER_FLAG_FLOAT_SWITCH, FiberBinder, &fiber_state);
      if (big_stack_fiber) {
        ::SwitchToFiber(big_stack_fiber);
        // The fibers must be cleaned up to avoid obscure TLS-related shutdown
        // crashes.
        ::DeleteFiber(big_stack_fiber);
        ::ConvertFiberToThread();
        // Control returns here after CEF has finished running on FiberMain.
        return fiber_state.fiber_result;
      }
      fiber_status = FiberStatus::kCreateFiberFailed;
    } else {
      fiber_status = FiberStatus::kConvertFailed;
    }
    // If we reach here then creating and switching to a fiber has failed. This
    // probably means we are low on memory and will soon crash. Try to report
    // this error once crash reporting is initialized.
    fiber_error = ::GetLastError();
    base::debug::Alias(&fiber_error);
  }

  // If we are already a fiber then continue normal execution.
  // Intentionally crash if converting to a fiber failed.
  CHECK_EQ(fiber_status, FiberStatus::kSuccess);
  return -1;
}

}  // namespace

int CefRunWinMainWithPreferredStackSize(wWinMainPtr wWinMain,
                                        HINSTANCE hInstance,
                                        LPWSTR lpCmdLine,
                                        int nCmdShow) {
  CHECK(wWinMain && hInstance);
  FiberState fiber_state(wWinMain, hInstance, lpCmdLine, nCmdShow);
  return RunMainWithPreferredStackSize(fiber_state);
}

int CefRunMainWithPreferredStackSize(mainPtr main, int argc, char* argv[]) {
  CHECK(main);
  FiberState fiber_state(main, argc, argv);
  return RunMainWithPreferredStackSize(fiber_state);
}

#endif  // BUILDFLAG(IS_WIN) && defined(ARCH_CPU_32_BITS)
