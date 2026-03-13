#!/usr/bin/env python3
# Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Base class for CEF installer E2E tests."""

import contextlib
import ctypes
from ctypes import wintypes
import json
import http.server
import os
import shutil
import ssl
import struct
import subprocess
import tempfile
import threading
import time
import unittest
import zlib

from e2e_config import get_test_versions

# Resolved from CEF_E2E_BUILD_DIR environment variable.
BUILD_DIR = None
# Path to testdata/ directory containing certificates.
TESTDATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..',
                            'testdata')
_INTEGRITY_FOOTER_MAGIC = 0xCEF09E1F4A3B2C1D

# Command-line flags (must match installer_bootstrap_helpers.h).
FLAG_UPDATE = '/cef-update'
FLAG_UNINSTALL = '/cef-uninstall'
FLAG_UNINSTALL_RELAUNCHED = '/cef-uninstall-relaunched'
FLAG_RETENTION_DRY_RUN = '/cef-retention-dry-run'
FLAG_RETENTION_APPLY = '/cef-retention-apply'
FLAG_RETENTION_MAX_AGE_DAYS = '/cef-max-age-days'
FLAG_HEADLESS = '/cef-headless'
FLAG_FORCECHECK = '/cef-forcecheck'
FLAG_BACKGROUND = '/cef-background'
FLAG_PARENT = '/cef-parent'
FLAG_INSTALL_PATH = '--cef-install-path'
FLAG_DOWNLOAD_PATH = '--cef-download-path'

# Exit codes (must match installer_constants.h).
EXIT_SUCCESS = 0
EXIT_CONFIG_ERROR = 100
EXIT_NETWORK_ERROR = 101
EXIT_SIGNATURE_ERROR = 102
EXIT_NO_MATCHING_VERSION = 103
EXIT_EXTRACTION_ERROR = 104
EXIT_INSTALL_ERROR = 105
EXIT_DATABASE_ERROR = 106
EXIT_LOCK_TIMEOUT = 107
EXIT_CANCELLED = 108
EXIT_RELAUNCHED = 109
EXIT_NO_SENTINEL = 110
EXIT_SENTINEL_READ_ERROR = 111
EXIT_SENTINEL_OWNER_MISMATCH = 112
EXIT_POLICY_DENIED = 113
EXIT_INDEX_ERROR = 114
EXIT_POLICY_ERROR = 119


class _TrusteeW(ctypes.Structure):
  _fields_ = [
      ('multiple_trustee', ctypes.c_void_p),
      ('multiple_trustee_operation', wintypes.DWORD),
      ('trustee_form', wintypes.DWORD),
      ('trustee_type', wintypes.DWORD),
      ('name', ctypes.c_void_p),
  ]


class _ExplicitAccessW(ctypes.Structure):
  _fields_ = [
      ('access_permissions', wintypes.DWORD),
      ('access_mode', wintypes.DWORD),
      ('inheritance', wintypes.DWORD),
      ('trustee', _TrusteeW),
  ]


class _ScopedDirectoryWriteDeny:
  """Temporarily deny directory-entry creation while preserving DACL repair."""

  _SE_FILE_OBJECT = 1
  _DACL_SECURITY_INFORMATION = 0x00000004
  _WIN_WORLD_SID = 1
  _SECURITY_MAX_SID_SIZE = 68
  _DENY_ACCESS = 3
  _TRUSTEE_IS_SID = 0
  _TRUSTEE_IS_WELL_KNOWN_GROUP = 5
  # FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_WRITE_EA |
  # FILE_WRITE_ATTRIBUTES. WRITE_DAC and deletion remain available so cleanup
  # can always restore the original descriptor and remove the fixture.
  _DIRECTORY_WRITE_MASK = 0x00000116

  def __init__(self, path):
    self._path = os.path.abspath(path)
    self._security_descriptor = ctypes.c_void_p()
    self._original_dacl = ctypes.c_void_p()
    self._new_dacl = ctypes.c_void_p()
    self._applied = False
    self._advapi32 = ctypes.WinDLL('advapi32', use_last_error=True)
    self._kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    self._configure_functions()

  def _configure_functions(self):
    self._advapi32.GetNamedSecurityInfoW.argtypes = [
        wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD,
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_void_p)
    ]
    self._advapi32.GetNamedSecurityInfoW.restype = wintypes.DWORD
    self._advapi32.CreateWellKnownSid.argtypes = [
        wintypes.DWORD, ctypes.c_void_p, ctypes.c_void_p,
        ctypes.POINTER(wintypes.DWORD)
    ]
    self._advapi32.CreateWellKnownSid.restype = wintypes.BOOL
    self._advapi32.SetEntriesInAclW.argtypes = [
        wintypes.ULONG,
        ctypes.POINTER(_ExplicitAccessW), ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_void_p)
    ]
    self._advapi32.SetEntriesInAclW.restype = wintypes.DWORD
    self._advapi32.SetNamedSecurityInfoW.argtypes = [
        wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p
    ]
    self._advapi32.SetNamedSecurityInfoW.restype = wintypes.DWORD
    self._kernel32.LocalFree.argtypes = [ctypes.c_void_p]
    self._kernel32.LocalFree.restype = ctypes.c_void_p

  @staticmethod
  def _raise_security_error(result, operation):
    raise OSError(result, f'{operation} failed with Windows error {result}')

  def apply(self):
    path_buffer = ctypes.create_unicode_buffer(self._path)
    result = self._advapi32.GetNamedSecurityInfoW(
        path_buffer, self._SE_FILE_OBJECT, self._DACL_SECURITY_INFORMATION,
        None, None, ctypes.byref(self._original_dacl), None,
        ctypes.byref(self._security_descriptor))
    if result:
      self._raise_security_error(result, 'GetNamedSecurityInfoW')

    sid_size = wintypes.DWORD(self._SECURITY_MAX_SID_SIZE)
    world_sid = ctypes.create_string_buffer(sid_size.value)
    if not self._advapi32.CreateWellKnownSid(self._WIN_WORLD_SID, None,
                                             world_sid, ctypes.byref(sid_size)):
      error = ctypes.get_last_error()
      self._release()
      raise ctypes.WinError(error)

    access = _ExplicitAccessW()
    access.access_permissions = self._DIRECTORY_WRITE_MASK
    access.access_mode = self._DENY_ACCESS
    access.inheritance = 0
    access.trustee.multiple_trustee = None
    access.trustee.multiple_trustee_operation = 0
    access.trustee.trustee_form = self._TRUSTEE_IS_SID
    access.trustee.trustee_type = self._TRUSTEE_IS_WELL_KNOWN_GROUP
    access.trustee.name = ctypes.cast(world_sid, ctypes.c_void_p)
    result = self._advapi32.SetEntriesInAclW(1, ctypes.byref(access),
                                             self._original_dacl,
                                             ctypes.byref(self._new_dacl))
    if result:
      self._release()
      self._raise_security_error(result, 'SetEntriesInAclW')

    result = self._advapi32.SetNamedSecurityInfoW(
        path_buffer, self._SE_FILE_OBJECT, self._DACL_SECURITY_INFORMATION,
        None, None, self._new_dacl, None)
    if result:
      self._release()
      self._raise_security_error(result, 'SetNamedSecurityInfoW')
    self._applied = True

  def restore(self):
    if not self._applied:
      return
    path_buffer = ctypes.create_unicode_buffer(self._path)
    result = self._advapi32.SetNamedSecurityInfoW(
        path_buffer, self._SE_FILE_OBJECT, self._DACL_SECURITY_INFORMATION,
        None, None, self._original_dacl, None)
    if result:
      self._raise_security_error(result, 'DACL restoration')
    self._applied = False
    self._release()

  def _release(self):
    if self._new_dacl.value:
      self._kernel32.LocalFree(self._new_dacl)
      self._new_dacl = ctypes.c_void_p()
    if self._security_descriptor.value:
      self._kernel32.LocalFree(self._security_descriptor)
      self._security_descriptor = ctypes.c_void_p()


def _get_build_dir():
  global BUILD_DIR
  if BUILD_DIR is None:
    BUILD_DIR = os.environ.get('CEF_E2E_BUILD_DIR', '')
  return BUILD_DIR


class E2ETestBase(unittest.TestCase):
  """Base class for CEF installer end-to-end tests.

    The exe lives in the build directory (component build requires all DLLs
    to be co-located). We create CefTestApp.exe as a copy of bootstrap.exe
    inside the build dir and embed each test's application config in that
    private copy. Test isolation comes from --cef-install-path and
    --cef-download-path pointing to per-test temp dirs.
    """

  def tearDown(self):
    # Print installer log on test failure to aid debugging.
    if hasattr(self, '_outcome'):
      # Python 3.4+
      result = self._outcome.result
      errors = (result.failures + result.errors) if result else []
      if any(test is self for test, _ in errors):
        log_path = os.path.join(self.install_dir, 'cef_installer.log')
        if os.path.isfile(log_path):
          with open(log_path) as f:
            print(f'\n--- cef_installer.log ---\n{f.read()}---')

  @contextlib.contextmanager
  def scoped_read_only_directory(self, path):
    """Deny write probes for an owned temp directory, then restore its DACL."""
    fixture = _ScopedDirectoryWriteDeny(path)
    try:
      fixture.apply()
    except OSError as error:
      self.skipTest(f'NTFS DACL fixture is unavailable: {error}')
    try:
      yield
    finally:
      fixture.restore()

  def setUp(self):
    build_dir = _get_build_dir()
    if not build_dir or not os.path.isdir(build_dir):
      self.skipTest(f'Build directory not found: {build_dir}')

    self.build_dir = build_dir
    self.temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, self.temp_dir, ignore_errors=True)

    self.install_dir = os.path.join(self.temp_dir, 'CEF')
    self.cdn_dir = os.path.join(self.temp_dir, 'cdn')

    # Derive test versions from the build's installer_config.json so they
    # stay in sync when the API version changes.
    versions = get_test_versions(build_dir)
    if versions is None:
      self.skipTest(f'installer_config.json not found in {build_dir}/gen/cef/')
    (self.test_version, self.test_version_update, self.test_version_higher,
     _) = versions

    # The exe must live in the build directory so it can find component
    # DLLs. Restore bootstrap.exe -> CefTestApp.exe for every test before any
    # resource edits.
    # The bootstrap fatals if the exe name is "bootstrap" (expects to be
    # renamed).
    src_exe = os.path.join(build_dir, 'bootstrap.exe')
    if not os.path.isfile(src_exe):
      self.skipTest(f'bootstrap.exe not found at {src_exe}')
    self.app_name = 'CefTestApp'
    self.bootstrap_exe = os.path.join(build_dir, f'{self.app_name}.exe')
    self._restore_bootstrap_executable(self.bootstrap_exe)

    self.mock_client_dll_src = os.path.join(build_dir,
                                            'cef_e2e_mock_client.dll')

    # Read the test certificate thumbprint.
    thumbprint_path = os.path.join(TESTDATA_DIR, 'test_thumbprint.txt')
    if os.path.isfile(thumbprint_path):
      with open(thumbprint_path) as f:
        self.test_thumbprint = f.read().strip()
    else:
      self.test_thumbprint = ''

  def _restore_bootstrap_executable(self, destination):
    """Unconditionally restore an unmodified bootstrap executable."""
    source = os.path.join(self.build_dir, 'bootstrap.exe')
    try:
      shutil.copy2(source, destination)
    except PermissionError as error:
      self.fail('Cannot restore bootstrap fixture while a prior test process '
                f'is using it: {destination}: {error}')

  def setup_launcher(self, app_name='TestApp'):
    """Copy bootstrap.exe and mock client DLL with matching names.

        The bootstrap derives the DLL name from the exe name
        (GetDefaultModuleValue), so TestApp.exe loads TestApp.dll.
        Both must be in the build directory for component builds.
        """
    app_exe = os.path.join(self.build_dir, f'{app_name}.exe')
    app_dll = os.path.join(self.build_dir, f'{app_name}.dll')
    self._restore_bootstrap_executable(app_exe)
    if os.path.isfile(self.mock_client_dll_src):
      shutil.copy2(self.mock_client_dll_src, app_dll)
    else:
      self.skipTest('cef_e2e_mock_client.dll not found')
    # Clean up launcher files after test.
    self.addCleanup(lambda: os.remove(app_dll)
                    if os.path.isfile(app_dll) else None)
    self.addCleanup(lambda: os.remove(app_exe)
                    if os.path.isfile(app_exe) else None)
    return app_exe, app_name

  def embed_test_config(self,
                        appid,
                        vmin,
                        abi_hash='',
                        vmax='',
                        exe=None,
                        **extra):
    """Embed a complete runtime config in an already restored executable."""
    config = {
        'appid': appid,
        'vmin': vmin,
        'abi_hash': abi_hash,
        'enable_explicit_modes': True,
    }
    if self.test_thumbprint:
      config['certificate_thumbprint'] = self.test_thumbprint
    if vmax:
      config['vmax'] = vmax
    config.update(extra)
    target = exe or self.bootstrap_exe
    self.embed_config_resource(target, config)
    return target

  def run_installer(self,
                    args=None,
                    use_local_cdn=True,
                    allow_configured_cdn=False,
                    install_path=None,
                    use_install_path_override=True,
                    timeout=30,
                    exe=None,
                    env=None):
    """Run the exe with args, return (exit_code, stdout, stderr)."""
    cmd = [exe or self.bootstrap_exe] + (args or [])
    # The bootstrap resolves CEF before loading a client DLL, which may
    # show a progress/error dialog. Ensure /cef-headless is always present
    # so E2E tests don't block on UI.
    if FLAG_HEADLESS not in cmd:
      cmd.append(FLAG_HEADLESS)
    # Pass --cef-download-path to prevent accidental real-CDN access. The one
    # explicit exception is allow_configured_cdn, used only with loopback HTTPS
    # URLs by the application-source process test below.
    if allow_configured_cdn:
      pass
    elif use_local_cdn and os.path.isdir(self.cdn_dir):
      cmd.append(f'{FLAG_DOWNLOAD_PATH}={self.cdn_dir}')
    elif not any(FLAG_DOWNLOAD_PATH in a for a in cmd):
      os.makedirs(self.cdn_dir, exist_ok=True)
      cmd.append(f'{FLAG_DOWNLOAD_PATH}={self.cdn_dir}')
    if use_install_path_override:
      path = install_path or self.install_dir
      cmd.append(f'{FLAG_INSTALL_PATH}={path}')
    run_env = os.environ.copy()
    if env:
      run_env.update(env)
    proc = subprocess.run(cmd,
                          capture_output=True,
                          timeout=timeout,
                          cwd=self.build_dir,
                          env=run_env)
    return proc.returncode, proc.stdout, proc.stderr

  def stage_bootstrap_in_install_root(self,
                                      install_root,
                                      app_name='ContainedCefTestApp'):
    """Stage a runnable component-build bootstrap beneath an install root."""
    os.makedirs(install_root, exist_ok=True)
    staged_exe = os.path.join(install_root, f'{app_name}.exe')
    shutil.copy2(self.bootstrap_exe, staged_exe)
    shutil.copy2(os.path.join(self.build_dir, 'chrome_elf.dll'),
                 os.path.join(install_root, 'chrome_elf.dll'))
    crash_config = os.path.join(self.build_dir, 'crash_reporter.cfg')
    if os.path.isfile(crash_config):
      shutil.copy2(crash_config, os.path.join(install_root,
                                              'crash_reporter.cfg'))
    return staged_exe

  @staticmethod
  def snapshot_uninstall_relaunch_dirs():
    """Return the existing installer-owned relaunch directories."""
    temp_root = tempfile.gettempdir()
    try:
      entries = os.scandir(temp_root)
    except OSError:
      return set()
    with entries:
      return {
          os.path.normcase(os.path.abspath(entry.path))
          for entry in entries
          if entry.is_dir(
              follow_symlinks=False) and entry.name.startswith('cef_uninstall_')
      }

  @staticmethod
  def _database_contains_app(install_root, appid):
    db_path = os.path.join(install_root, 'installer.json')
    if not os.path.isfile(db_path):
      return False
    with open(db_path, 'rb') as db_file:
      text = db_file.read().decode('utf-8', errors='replace')
    start = text.find('{')
    if start < 0:
      return False
    try:
      value, _ = json.JSONDecoder().raw_decode(text, start)
    except json.JSONDecodeError:
      return True
    return any(app.get('uuid') == appid for app in value.get('apps', []))

  @staticmethod
  def _read_version_index_versions(install_root):
    """Return the integrity-checked version/platform keys, or None."""
    index_path = os.path.join(install_root, 'versions.json')
    try:
      with open(index_path, 'rb') as index_file:
        raw = index_file.read()
    except OSError:
      return None
    if len(raw) < 16:
      return None
    content = raw[:-16]
    expected_crc, reserved, magic = struct.unpack('<IIQ', raw[-16:])
    if (reserved != 0 or magic != _INTEGRITY_FOOTER_MAGIC or
        zlib.crc32(content) & 0xffffffff != expected_crc):
      return None
    try:
      value = json.loads(content.decode('utf-8'))
    except (UnicodeDecodeError, json.JSONDecodeError):
      return None
    versions = value.get('versions') if isinstance(value, dict) else None
    if not isinstance(versions, list):
      return None
    keys = set()
    for entry in versions:
      if not isinstance(entry, dict):
        return None
      version = entry.get('version')
      platform = entry.get('platform')
      if not isinstance(version, str) or not isinstance(platform, str):
        return None
      key = (version, platform)
      if key in keys:
        return None
      keys.add(key)
    return keys

  def wait_for_relaunched_uninstall(self,
                                    appid,
                                    previous_dirs,
                                    expected_exe_name,
                                    expected_index_versions,
                                    install_root=None,
                                    timeout=15):
    """Wait for a child to publish the expected commit and exit, then clean."""
    install_root = install_root or self.install_dir
    expected_index_versions = set(expected_index_versions)
    deadline = time.time() + timeout
    relaunch_dir = None
    while time.time() < deadline and relaunch_dir is None:
      candidates = self.snapshot_uninstall_relaunch_dirs() - previous_dirs
      matching = [
          path for path in candidates
          if os.path.isfile(os.path.join(path, expected_exe_name))
      ]
      if len(matching) == 1:
        relaunch_dir = matching[0]
        self.addCleanup(shutil.rmtree, relaunch_dir, ignore_errors=True)
        break
      time.sleep(0.1)
    self.assertIsNotNone(relaunch_dir,
                         f'Expected one relaunched {expected_exe_name} child')

    registration_removed = False
    index_versions = None
    while time.time() < deadline:
      registration_removed = not self._database_contains_app(
          install_root, appid)
      index_versions = self._read_version_index_versions(install_root)
      if (registration_removed and index_versions == expected_index_versions):
        try:
          shutil.rmtree(relaunch_dir)
          return relaunch_dir
        except OSError:
          pass
      time.sleep(0.1)
    self.fail('Relaunched uninstall did not publish its complete logical '
              f'commit and exit before timeout: registration_removed='
              f'{registration_removed}, index_versions={index_versions}')

  def get_loopback_tls_certificate(self):
    """Return the checked-in, test-only localhost TLS certificate and key."""
    cert_path = os.path.join(TESTDATA_DIR, 'loopback_tls_cert.pem')
    key_path = os.path.join(TESTDATA_DIR, 'loopback_tls_key.pem')
    self.assertTrue(os.path.isfile(cert_path),
                    f'Loopback TLS certificate not found: {cert_path}')
    self.assertTrue(os.path.isfile(key_path),
                    f'Loopback TLS key not found: {key_path}')
    return cert_path, key_path

  def start_https_file_server(self, directory, cert_path, key_path):
    """Serve a directory over loopback HTTPS and record request paths."""
    requests = []

    class RecordingHandler(http.server.SimpleHTTPRequestHandler):

      def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=directory, **kwargs)

      def do_GET(self):
        requests.append(f'GET {self.path}')
        super().do_GET()

      def do_HEAD(self):
        requests.append(f'HEAD {self.path}')
        super().do_HEAD()

      def log_message(self, _format, *_args):
        pass

    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), RecordingHandler)
    server.daemon_threads = True
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert_path, key_path)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    def stop_server():
      server.shutdown()
      server.server_close()
      thread.join(timeout=5)

    self.addCleanup(stop_server)
    return f'https://localhost:{server.server_port}/', requests

  def assert_version_installed(self, version, platform='windows64'):
    """Assert that a version directory exists with valid metadata."""
    version_dir = os.path.join(self.install_dir, 'Versions', version, platform)
    self.assertTrue(os.path.isdir(version_dir),
                    f'Version directory not found: {version_dir}')
    metadata_path = os.path.join(version_dir, 'cef_version.json')
    self.assertTrue(os.path.isfile(metadata_path),
                    f'Metadata not found: {metadata_path}')

  def assert_version_not_installed(self, version, platform='windows64'):
    """Assert that a version directory does not exist."""
    version_dir = os.path.join(self.install_dir, 'Versions', version, platform)
    self.assertFalse(os.path.exists(version_dir),
                     f'Version directory still exists: {version_dir}')

  def build_cdn(self, version=None, abi_hash='a1b2c3d4e5f6'):
    """Build a local CDN directory with the given version.

        Can be called multiple times to add versions (manifests merge).
        """
    if version is None:
      version = self.test_version
    os.makedirs(self.cdn_dir, exist_ok=True)
    helper = os.path.join(self.build_dir, 'cef_e2e_build_test_cdn.exe')
    if not os.path.isfile(helper):
      self.skipTest('cef_e2e_build_test_cdn.exe not found')
    result = subprocess.run([
        helper, f'--output-dir={self.cdn_dir}', f'--version={version}',
        f'--abi-hash={abi_hash}'
    ],
                            capture_output=True,
                            encoding='utf-8',
                            errors='replace')
    self.assertEqual(0, result.returncode, f'CDN build failed: {result.stderr}')
    return version, abi_hash

  def embed_config_resource(self, exe_path, config_dict):
    """Embed a CEF_INSTALLER_CONFIG resource into an exe using ResourceHacker.

    Writes config JSON + .rc file to self.temp_dir, compiles with
    ResourceHacker, then replaces/adds the resource in the target exe.
    Fails the test (not skips) if CEF_RESOURCE_HACKER_PATH is not set.
    """
    rh_path = os.environ.get('CEF_RESOURCE_HACKER_PATH', '')
    if not rh_path or not os.path.isfile(rh_path):
      self.fail('CEF_RESOURCE_HACKER_PATH must be set to the path of '
                'ResourceHacker.exe (required for embedded config tests). '
                f'Current value: {rh_path!r}')

    self.embed_raw_config_resource(exe_path, json.dumps(config_dict))

  def embed_raw_config_resource(self, exe_path, config_text):
    """Embed raw CEF_INSTALLER_CONFIG text for malformed-config tests."""
    rh_path = os.environ.get('CEF_RESOURCE_HACKER_PATH', '')
    if not rh_path or not os.path.isfile(rh_path):
      self.fail('CEF_RESOURCE_HACKER_PATH must be set to the path of '
                'ResourceHacker.exe (required for embedded config tests). '
                f'Current value: {rh_path!r}')

    config_json_path = os.path.join(self.temp_dir, 'config.json')
    with open(config_json_path, 'w') as f:
      f.write(config_text)

    rc_path = os.path.join(self.temp_dir, 'config.rc')
    with open(rc_path, 'w') as f:
      f.write('CEF_INSTALLER_CONFIG RCDATA "config.json"\n')

    res_path = os.path.join(self.temp_dir, 'config.res')

    # Compile .rc -> .res (cwd must be temp_dir so relative "config.json" resolves).
    result = subprocess.run([
        rh_path, '-open', 'config.rc', '-save', 'config.res', '-action',
        'compile'
    ],
                            capture_output=True,
                            encoding='utf-8',
                            errors='replace',
                            cwd=self.temp_dir)
    self.assertEqual(0, result.returncode,
                     f'ResourceHacker compile failed: {result.stderr}')

    # Add/overwrite the resource in the exe.
    result = subprocess.run([
        rh_path, '-open', exe_path, '-save', exe_path, '-action',
        'addoverwrite', '-res', res_path, '-mask',
        'RCDATA,CEF_INSTALLER_CONFIG,'
    ],
                            capture_output=True,
                            encoding='utf-8',
                            errors='replace',
                            cwd=self.temp_dir)
    self.assertEqual(0, result.returncode,
                     f'ResourceHacker addoverwrite failed: {result.stderr}')

  def get_appid_hash(self, appid):
    """Compute SHA-1 of appid, return first 16 hex chars.

        Must match C++ GetAppidHash logic.
        """
    import hashlib
    h = hashlib.sha1(appid.encode('utf-8')).hexdigest()
    return h[:16].upper()

  def get_launch_dir(self):
    """Return path to the .launch/ directory under install_dir."""
    return os.path.join(self.install_dir, '.launch')

  @staticmethod
  def filetime_now():
    """Return current wall time as Windows FILETIME ticks."""
    return int((time.time() + 11644473600) * 10000000)

  @staticmethod
  def write_integrity_json(path, value):
    """Write compact JSON with the installer's CRC32 integrity footer."""
    content = json.dumps(value, separators=(',', ':')).encode('utf-8')
    footer = struct.pack('<IIQ',
                         zlib.crc32(content) & 0xffffffff, 0,
                         _INTEGRITY_FOOTER_MAGIC)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
      f.write(content)
      f.write(footer)

  def write_registration_database(self, apps):
    """Write a valid schema-1 installer.json with supplied app entries."""
    path = os.path.join(self.install_dir, 'installer.json')
    self.write_integrity_json(path, {
        'schema_version': 1,
        'apps': apps,
    })
    return path

  def write_liveness(self,
                     appid,
                     last_launch,
                     platform='windows64',
                     filename=None):
    """Write an integrity-valid liveness-only record."""
    appid_hash = self.get_appid_hash(appid)
    name = filename or f'{appid_hash}_{platform}'
    path = os.path.join(self.get_launch_dir(), name)
    self.write_integrity_json(path, {
        'appid': appid,
        'platform': platform,
        'last_launch': str(last_launch),
    })
    return path

  def write_health(self,
                   appid,
                   version,
                   pid_start_time,
                   platform='windows64',
                   filename=None,
                   last_update=None,
                   running=False,
                   confirmed=True,
                   consecutive_failures=0):
    """Write an integrity-valid health sentinel with controlled FILETIME."""
    if last_update is None:
      last_update = self.filetime_now()
    appid_hash = self.get_appid_hash(appid)
    name = filename or f'{appid_hash}_{version}_{platform}'
    path = os.path.join(self.get_launch_dir(), name)
    self.write_integrity_json(
        path, {
            'appid': appid,
            'pid': 0,
            'pid_start_time': str(pid_start_time),
            'consecutive_failures': consecutive_failures,
            'running': running,
            'confirmed': confirmed,
            'version': version,
            'platform': platform,
            'last_update': str(last_update),
        })
    return path

  def read_launch_state(self, version, appid_hash, platform='windows64'):
    """Read launch state from .launch/ directory.

        Filename: <appid_hash>_<version>_<platform>
        Returns dict or None.
        """
    launch_dir = self.get_launch_dir()
    path = os.path.join(launch_dir, f'{appid_hash}_{version}_{platform}')
    if not os.path.isfile(path):
      return None
    with open(path, 'rb') as f:
      raw = f.read()
    text = raw.decode('utf-8', errors='replace')
    start = text.find('{')
    if start < 0:
      return None
    try:
      obj, _ = json.JSONDecoder().raw_decode(text, start)
      return obj
    except json.JSONDecodeError:
      return None

  def assert_launch_state(self,
                          version,
                          appid_hash,
                          running,
                          failures,
                          platform='windows64'):
    """Assert launch state in .launch/ dir with expected running and failures."""
    state = self.read_launch_state(version, appid_hash, platform)
    self.assertIsNotNone(state,
                         f'Launch state not found for {version}/{appid_hash}')
    self.assertEqual(running, state.get('running'),
                     f'Expected running={running}, got {state}')
    self.assertEqual(failures, state.get('consecutive_failures'),
                     f'Expected failures={failures}, got {state}')
    last_update = state.get('last_update')
    self.assertIsInstance(last_update, str, f'Missing last_update: {state}')
    self.assertTrue(last_update.isdecimal(), f'Invalid last_update: {state}')
    self.assertGreater(int(last_update), 0, f'Zero last_update: {state}')

  def assert_no_launch_state(self, version, appid_hash, platform='windows64'):
    """Assert launch state file does not exist in .launch/ dir."""
    launch_dir = self.get_launch_dir()
    path = os.path.join(launch_dir, f'{appid_hash}_{version}_{platform}')
    self.assertFalse(os.path.exists(path),
                     f'Launch state file still exists: {path}')

  def read_liveness(self, appid_hash, platform='windows64'):
    """Read a health-off liveness-only record from .launch/."""
    path = os.path.join(self.get_launch_dir(), f'{appid_hash}_{platform}')
    if not os.path.isfile(path):
      return None
    with open(path, 'rb') as f:
      text = f.read().decode('utf-8', errors='replace')
    start = text.find('{')
    if start < 0:
      return None
    try:
      obj, _ = json.JSONDecoder().raw_decode(text, start)
      return obj
    except json.JSONDecodeError:
      return None

  def list_launch_files(self):
    """List all files in the .launch/ directory."""
    launch_dir = self.get_launch_dir()
    if not os.path.isdir(launch_dir):
      return []
    return os.listdir(launch_dir)

  def read_database(self):
    """Read and parse installer.json from the install directory.

        The database file has a CRC32 integrity footer after the JSON.
        We use raw_decode to parse only the JSON prefix.
        """
    db_path = os.path.join(self.install_dir, 'installer.json')
    with open(db_path, 'rb') as f:
      raw = f.read()
    text = raw.decode('utf-8', errors='replace')
    start = text.find('{')
    if start < 0:
      return {}
    obj, _ = json.JSONDecoder().raw_decode(text, start)
    return obj
