// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_lock.h"

#include <atomic>
#include <thread>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::GetMutexName;

namespace {

TEST(InstallerLockChildTest, HoldUntilKilled) {
  const base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  base::FilePath install_dir =
      command->GetSwitchValuePath("cef-lock-child-path");
  std::wstring event_name =
      command->GetSwitchValueNative("cef-lock-child-event");
  if (install_dir.empty() || event_name.empty()) {
    GTEST_SKIP();
  }
  auto lock = SingletonLock::Acquire(install_dir, INFINITE);
  ASSERT_TRUE(lock);
  HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, event_name.c_str());
  ASSERT_NE(nullptr, event);
  ASSERT_TRUE(SetEvent(event));
  CloseHandle(event);
  WaitForSingleObject(GetCurrentProcess(), INFINITE);
}

// Use a unique path for each test to avoid conflicts
base::FilePath GetTestPath(const std::string& suffix) {
  return base::FilePath::FromUTF8Unsafe("C:/test/cef_installer_lock_" + suffix);
}

TEST(InstallerLockTest, AcquireSuccess) {
  base::FilePath install_dir = GetTestPath("acquire_success");

  auto lock = SingletonLock::Acquire(install_dir, 0);

  ASSERT_NE(nullptr, lock);
  EXPECT_TRUE(lock->IsHeld());
}

TEST(InstallerLockTest, LockIsExclusive) {
  base::FilePath install_dir = GetTestPath("exclusive");

  // Acquire first lock
  auto lock1 = SingletonLock::Acquire(install_dir, 0);
  ASSERT_NE(nullptr, lock1);
  EXPECT_TRUE(lock1->IsHeld());

  // Try to acquire second lock with timeout=0 (non-blocking)
  auto lock2 = SingletonLock::Acquire(install_dir, 0);

  // Second acquisition should fail immediately
  EXPECT_EQ(nullptr, lock2);
}

TEST(InstallerLockTest, ReleasedOnDestruction) {
  base::FilePath install_dir = GetTestPath("released_on_destruction");

  {
    // Acquire lock in inner scope
    auto lock1 = SingletonLock::Acquire(install_dir, 0);
    ASSERT_NE(nullptr, lock1);
    EXPECT_TRUE(lock1->IsHeld());
  }
  // lock1 goes out of scope, should be released

  // Now we should be able to acquire it again
  auto lock2 = SingletonLock::Acquire(install_dir, 0);
  ASSERT_NE(nullptr, lock2);
  EXPECT_TRUE(lock2->IsHeld());
}

TEST(InstallerLockTest, TimeoutZeroNonBlocking) {
  base::FilePath install_dir = GetTestPath("timeout_zero");

  // Acquire first lock
  auto lock1 = SingletonLock::Acquire(install_dir, 0);
  ASSERT_NE(nullptr, lock1);

  // Measure time for second acquisition attempt
  base::TimeTicks start = base::TimeTicks::Now();
  auto lock2 = SingletonLock::Acquire(install_dir, 0);
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;

  // Should return immediately (within 100ms to account for system overhead)
  EXPECT_EQ(nullptr, lock2);
  EXPECT_LT(elapsed, base::Milliseconds(100));
}

TEST(InstallerLockTest, TimeoutExpires) {
  base::FilePath install_dir = GetTestPath("timeout_expires");

  // Acquire first lock
  auto lock1 = SingletonLock::Acquire(install_dir, 0);
  ASSERT_NE(nullptr, lock1);

  // Try to acquire with 100ms timeout from another thread. Same-thread
  // recursion is rejected immediately by the wrapper.
  base::TimeDelta elapsed;
  bool acquired = false;
  std::thread waiter([&] {
    base::TimeTicks start = base::TimeTicks::Now();
    auto lock2 = SingletonLock::Acquire(install_dir, 100);
    elapsed = base::TimeTicks::Now() - start;
    acquired = lock2 != nullptr;
  });
  waiter.join();

  // Should fail and take approximately 100ms
  EXPECT_FALSE(acquired);
  EXPECT_GE(elapsed, base::Milliseconds(90));   // At least 90ms
  EXPECT_LT(elapsed, base::Milliseconds(500));  // But not too long
}

TEST(InstallerLockTest, MutexNameConsistent) {
  // Same path should always produce the same mutex name
  base::FilePath path1 = base::FilePath::FromUTF8Unsafe("C:/Program Files/CEF");
  base::FilePath path2 = base::FilePath::FromUTF8Unsafe("C:/Program Files/CEF");

  std::wstring name1 = GetMutexName(path1);
  std::wstring name2 = GetMutexName(path2);

  EXPECT_EQ(name1, name2);
  EXPECT_TRUE(name1.find(L"Global\\CEF_Installer_") == 0);
}

TEST(InstallerLockTest, MutexNameDifferentPaths) {
  // Different paths should produce different mutex names
  base::FilePath path1 = base::FilePath::FromUTF8Unsafe("C:/CEF1");
  base::FilePath path2 = base::FilePath::FromUTF8Unsafe("C:/CEF2");

  std::wstring name1 = GetMutexName(path1);
  std::wstring name2 = GetMutexName(path2);

  EXPECT_NE(name1, name2);
}

TEST(InstallerLockTest, MutexNameCaseInsensitive) {
  // Windows paths are case-insensitive, mutex names should be too
  base::FilePath path1 = base::FilePath::FromUTF8Unsafe("C:/Program Files/CEF");
  base::FilePath path2 = base::FilePath::FromUTF8Unsafe("c:/program files/cef");

  std::wstring name1 = GetMutexName(path1);
  std::wstring name2 = GetMutexName(path2);

  EXPECT_EQ(name1, name2);
}

TEST(InstallerLockTest, MutexNameNormalizesSlashes) {
  // Forward and backward slashes should produce same result
  base::FilePath path1 = base::FilePath::FromUTF8Unsafe("C:/CEF/test");
  base::FilePath path2 = base::FilePath::FromUTF8Unsafe("C:\\CEF\\test");

  std::wstring name1 = GetMutexName(path1);
  std::wstring name2 = GetMutexName(path2);

  EXPECT_EQ(name1, name2);
}

TEST(InstallerLockTest, DifferentPathsCanLockSimultaneously) {
  base::FilePath path1 = GetTestPath("different_paths_1");
  base::FilePath path2 = GetTestPath("different_paths_2");

  // Both locks should succeed since they're different paths
  auto lock1 = SingletonLock::Acquire(path1, 0);
  auto lock2 = SingletonLock::Acquire(path2, 0);

  ASSERT_NE(nullptr, lock1);
  ASSERT_NE(nullptr, lock2);
  EXPECT_TRUE(lock1->IsHeld());
  EXPECT_TRUE(lock2->IsHeld());
}

TEST(InstallerLockTest, AbandonedOwnerIsAcquired) {
  base::FilePath install_dir = GetTestPath("abandoned_owner");
  std::wstring mutex_name = GetMutexName(install_dir);
  HANDLE owner_handle = nullptr;

  std::thread owner([&] {
    owner_handle = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
    ASSERT_NE(nullptr, owner_handle);
    // Exiting without ReleaseMutex abandons ownership. The handle stays open
    // in this process so the named object remains available to Acquire().
  });
  owner.join();

  auto lock = SingletonLock::Acquire(install_dir, 0);
  ASSERT_NE(nullptr, lock);
  EXPECT_TRUE(lock->IsHeld());
  EXPECT_TRUE(lock->was_abandoned());

  CloseHandle(owner_handle);
}

TEST(InstallerLockTest, ConsecutiveAbandonmentIsReported) {
  base::FilePath install_dir = GetTestPath("consecutive_abandonment");
  std::wstring mutex_name = GetMutexName(install_dir);
  HANDLE first_owner_handle = nullptr;
  std::thread first_owner([&] {
    first_owner_handle = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
    ASSERT_NE(nullptr, first_owner_handle);
  });
  first_owner.join();

  auto first_recovery = SingletonLock::Acquire(install_dir, 0);
  ASSERT_TRUE(first_recovery);
  EXPECT_TRUE(first_recovery->was_abandoned());
  first_recovery.reset();

  HANDLE second_owner_handle = nullptr;
  std::thread second_owner([&] {
    second_owner_handle =
        OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, mutex_name.c_str());
    ASSERT_NE(nullptr, second_owner_handle);
    ASSERT_EQ(WAIT_OBJECT_0,
              WaitForSingleObject(second_owner_handle, INFINITE));
  });
  second_owner.join();

  auto second_recovery = SingletonLock::Acquire(install_dir, 0);
  ASSERT_TRUE(second_recovery);
  EXPECT_TRUE(second_recovery->was_abandoned());

  CloseHandle(first_owner_handle);
  CloseHandle(second_owner_handle);
}

TEST(InstallerLockTest, WrongThreadReleaseIsRejected) {
  auto lock = SingletonLock::Acquire(GetTestPath("wrong_thread_release"), 0);
  ASSERT_TRUE(lock);
  SingletonLock* raw_lock = lock.release();

  EXPECT_DEATH(
      {
        std::thread wrong_thread([raw_lock] { delete raw_lock; });
        wrong_thread.join();
      },
      "");

  delete raw_lock;
}

TEST(InstallerLockTest, NormalAcquisitionIsNotAbandoned) {
  auto lock = SingletonLock::Acquire(GetTestPath("not_abandoned"), 0);
  ASSERT_NE(nullptr, lock);
  EXPECT_FALSE(lock->was_abandoned());
}

TEST(InstallerLockTest, ExistingSemaphoreFailsClosed) {
  base::FilePath install_dir = GetTestPath("semaphore_collision");
  std::wstring object_name = GetMutexName(install_dir);
  HANDLE semaphore = CreateSemaphoreW(nullptr, 1, 1, object_name.c_str());
  ASSERT_NE(nullptr, semaphore);

  EXPECT_EQ(nullptr, SingletonLock::Acquire(install_dir, 0));
  EXPECT_EQ(static_cast<DWORD>(ERROR_INVALID_HANDLE), GetLastError());

  CloseHandle(semaphore);
}

TEST(InstallerLockTest, AbandonedOwnerSerializesMultipleWaiters) {
  base::FilePath install_dir = GetTestPath("abandoned_multiple_waiters");
  std::wstring mutex_name = GetMutexName(install_dir);
  HANDLE owner_handle = nullptr;
  std::thread owner([&] {
    owner_handle = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
    ASSERT_NE(nullptr, owner_handle);
  });
  owner.join();

  std::atomic<int> active = 0;
  std::atomic<int> max_active = 0;
  std::atomic<int> abandoned = 0;
  std::atomic<bool> first = true;
  base::WaitableEvent first_acquired;
  base::WaitableEvent release_first;
  std::vector<std::thread> waiters;
  for (int i = 0; i < 3; ++i) {
    waiters.emplace_back([&] {
      auto lock = SingletonLock::Acquire(install_dir, INFINITE);
      ASSERT_TRUE(lock);
      if (lock->was_abandoned()) {
        abandoned.fetch_add(1);
      }
      int current = active.fetch_add(1) + 1;
      int observed = max_active.load();
      while (current > observed &&
             !max_active.compare_exchange_weak(observed, current)) {
      }
      if (first.exchange(false)) {
        first_acquired.Signal();
        release_first.Wait();
      }
      active.fetch_sub(1);
    });
  }

  first_acquired.Wait();
  EXPECT_EQ(1, active.load());
  release_first.Signal();
  for (auto& waiter : waiters) {
    waiter.join();
  }

  EXPECT_EQ(1, abandoned.load());
  EXPECT_EQ(1, max_active.load());
  CloseHandle(owner_handle);
}

TEST(InstallerLockTest, ProcessTerminationProducesAbandonment) {
  base::FilePath install_dir = GetTestPath("terminated_owner");
  std::wstring event_name = std::wstring(L"Local") + static_cast<wchar_t>(92) +
                            L"CEF_Installer_Lock_Test_" +
                            std::to_wstring(GetCurrentProcessId()) + L"_" +
                            std::to_wstring(GetTickCount64());
  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, event_name.c_str());
  ASSERT_NE(nullptr, event);

  wchar_t executable[MAX_PATH] = {};
  ASSERT_GT(GetModuleFileNameW(nullptr, executable, MAX_PATH), 0u);
  const wchar_t quote = static_cast<wchar_t>(34);
  std::wstring command_line(1, quote);
  command_line += executable;
  command_line += quote;
  command_line +=
      L" --single-process-tests "
      L"--gtest_filter=InstallerLockChildTest.HoldUntilKilled "
      L"--cef-lock-child-path=";
  command_line += quote;
  command_line += install_dir.value();
  command_line += quote;
  command_line += L" --cef-lock-child-event=" + event_name;
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  ASSERT_TRUE(CreateProcessW(nullptr, command_line.data(), nullptr, nullptr,
                             FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                             &startup, &process));
  CloseHandle(process.hThread);
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(event, 10000));
  CloseHandle(event);
  HANDLE keep_alive =
      OpenMutexW(SYNCHRONIZE, FALSE, GetMutexName(install_dir).c_str());
  ASSERT_NE(nullptr, keep_alive);

  ASSERT_TRUE(TerminateProcess(process.hProcess, 1));
  ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(process.hProcess, 10000));
  CloseHandle(process.hProcess);

  auto recovered = SingletonLock::Acquire(install_dir, 1000);
  ASSERT_TRUE(recovered);
  EXPECT_TRUE(recovered->was_abandoned());
  CloseHandle(keep_alive);
}

}  // namespace
}  // namespace cef_installer
