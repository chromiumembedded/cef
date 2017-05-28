#!/usr/bin/env python
# Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.
"""
This script implements a simple HTTP server for receiving crash report uploads
from a Breakpad/Crashpad client (any CEF-based application). This script is
intended for testing purposes only. An HTTPS server and a system such as Socorro
(https://wiki.mozilla.org/Socorro) should be used when uploading crash reports
from production applications.

Usage of this script is as follows:

1. Run this script from the command-line. The first argument is the server port
   number and the second argument is the directory where uploaded report
   information will be saved:

   > python crash_server.py 8080 /path/to/dumps

2. Create a "crash_reporter.cfg" file at the required platform-specific
   location. On Windows and Linux this file must be placed next to the main
   application executable. On macOS this file must be placed in the top-level
   app bundle Resources directory (e.g. "<appname>.app/Contents/Resources"). At
   a minimum it must contain a "ServerURL=http://localhost:8080" line under the
   "[Config]" section (make sure the port number matches the value specified in
   step 1). See comments in include/cef_crash_util.h for a complete
   specification of this file.

   Example file contents:

   [Config]
   ServerURL=http://localhost:8080
   # Disable rate limiting so that all crashes are uploaded.
   RateLimitEnabled=false
   MaxUploadsPerDay=0

   [CrashKeys]
   # The cefclient sample application sets these values (see step 5 below).
   testkey1=small
   testkey2=medium
   testkey3=large

3. Load one of the following URLs in the CEF-based application to cause a crash:

   Main (browser) process crash:   chrome://inducebrowsercrashforrealz
   Renderer process crash:         chrome://crash
   GPU process crash:              chrome://gpucrash

4. When this script successfully receives a crash report upload you will see
   console output like the following:

   01/10/2017 12:31:23: Dump <id>

   The "<id>" value is a 16 digit hexadecimal string that uniquely identifies
   the dump. Crash dumps and metadata (product state, command-line flags, crash
   keys, etc.) will be written to the "<id>.dmp" and "<id>.json" files
   underneath the directory specified in step 1.

   On Linux Breakpad uses the wget utility to upload crash dumps, so make sure
   that utility is installed. If the crash is handled correctly then you should
   see console output like the following when the client uploads a crash dump:

   --2017-01-10 12:31:22--  http://localhost:8080/
   Resolving localhost (localhost)... 127.0.0.1
   Connecting to localhost (localhost)|127.0.0.1|:8080... connected.
   HTTP request sent, awaiting response... 200 OK
   Length: unspecified [text/html]
   Saving to: '/dev/fd/3'
   Crash dump id: <id>

   On macOS when uploading a crash report to this script over HTTP you may
   receive an error like the following:

   "Transport security has blocked a cleartext HTTP (http://) resource load
   since it is insecure. Temporary exceptions can be configured via your app's
   Info.plist file."

   You can work around this error by adding the following key to the Helper app
   Info.plist file (e.g. "<appname>.app/Contents/Frameworks/
   <appname> Helper.app/Contents/Info.plist"):

   <key>NSAppTransportSecurity</key>
   <dict>
     <!--Allow all connections (for testing only!)-->
     <key>NSAllowsArbitraryLoads</key>
     <true/>
   </dict>

5. The cefclient sample application sets test crash key values in the browser
   and renderer processes. To work properly these values must also be defined
   in the "[CrashKeys]" section of "crash_reporter.cfg" as shown above.

   In tests/cefclient/browser/client_browser.cc (browser process):

   CefSetCrashKeyValue("testkey1", "value1_browser");
   CefSetCrashKeyValue("testkey2", "value2_browser");
   CefSetCrashKeyValue("testkey3", "value3_browser");

   In tests/cefclient/renderer/client_renderer.cc (renderer process):

   CefSetCrashKeyValue("testkey1", "value1_renderer");
   CefSetCrashKeyValue("testkey2", "value2_renderer");
   CefSetCrashKeyValue("testkey3", "value3_renderer");

   When crashing the browser or renderer processes with cefclient you should
   verify that the test crash key values are included in the metadata
   ("<id>.json") file. Some values may be chunked as described in
   include/cef_crash_util.h.
"""

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import cgi
import cStringIO
import datetime
import json
import os
import shutil
import sys
import uuid
import zlib


def print_msg(msg):
  """ Write |msg| to stdout and flush. """
  timestr = datetime.datetime.now().strftime("%m/%d/%Y %H:%M:%S")
  sys.stdout.write("%s: %s\n" % (timestr, msg))
  sys.stdout.flush()


# Key identifying the minidump file.
minidump_key = 'upload_file_minidump'


class CrashHTTPRequestHandler(BaseHTTPRequestHandler):

  def __init__(self, dump_directory, *args):
    self._dump_directory = dump_directory
    BaseHTTPRequestHandler.__init__(self, *args)

  def _send_default_response_headers(self):
    """ Send default response headers. """
    self.send_response(200)
    self.send_header('Content-type', 'text/html')
    self.end_headers()

  def _parse_post_data(self, data):
    """ Returns a cgi.FieldStorage object for this request or None if this is
        not a POST request. """
    if self.command != 'POST':
      return None
    return cgi.FieldStorage(
        fp=cStringIO.StringIO(data),
        headers=self.headers,
        environ={
            'REQUEST_METHOD': 'POST',
            'CONTENT_TYPE': self.headers['Content-Type'],
        })

  def _get_chunk_size(self):
    # Read to the next "\r\n".
    size_str = self.rfile.read(2)
    while size_str[-2:] != b"\r\n":
      size_str += self.rfile.read(1)
    # Remove the trailing "\r\n".
    size_str = size_str[:-2]
    assert len(size_str) <= 4
    return int(size_str, 16)

  def _get_chunk_data(self, chunk_size):
    data = self.rfile.read(chunk_size)
    assert len(data) == chunk_size
    # Skip the trailing "\r\n".
    self.rfile.read(2)
    return data

  def _unchunk_request(self, compressed):
    """ Read a chunked request body. Optionally decompress the result. """
    if compressed:
      d = zlib.decompressobj(16 + zlib.MAX_WBITS)

    # Chunked format is: <size>\r\n<bytes>\r\n<size>\r\n<bytes>\r\n0\r\n
    unchunked = b""
    while True:
      chunk_size = self._get_chunk_size()
      print 'Chunk size 0x%x' % chunk_size
      if (chunk_size == 0):
        break
      chunk_data = self._get_chunk_data(chunk_size)
      if compressed:
        unchunked += d.decompress(chunk_data)
      else:
        unchunked += chunk_data

    if compressed:
      unchunked += d.flush()

    return unchunked

  def _create_new_dump_id(self):
    """ Breakpad requires a 16 digit hexadecimal dump ID. """
    return str(uuid.uuid4().get_hex().upper()[0:16])

  def do_GET(self):
    """ Default empty implementation for handling GET requests. """
    self._send_default_response_headers()
    self.wfile.write("<html><body><h1>GET!</h1></body></html>")

  def do_HEAD(self):
    """ Default empty implementation for handling HEAD requests. """
    self._send_default_response_headers()

  def do_POST(self):
    """ Handle a multi-part POST request submitted by Breakpad/Crashpad. """
    self._send_default_response_headers()

    # Create a unique ID for the dump.
    dump_id = self._create_new_dump_id()

    # Return the unique ID to the caller.
    self.wfile.write(dump_id)

    dmp_stream = None
    metadata = {}

    # Request body may be chunked and/or gzip compressed. For example:
    #
    # 3029 branch on Windows:
    #   User-Agent: Crashpad/0.8.0
    #   Host: localhost:8080
    #   Connection: Keep-Alive
    #   Transfer-Encoding: chunked
    #   Content-Type: multipart/form-data; boundary=---MultipartBoundary-vp5j9HdSRYK8DvX2DhtpqEbMNjSN1wnL---
    #   Content-Encoding: gzip
    #
    # 2987 branch on Windows:
    #   User-Agent: Crashpad/0.8.0
    #   Host: localhost:8080
    #   Connection: Keep-Alive
    #   Content-Type: multipart/form-data; boundary=---MultipartBoundary-qFhorGA40vDJ1fgmc2mjorL0fRfKOqup---
    #   Content-Length: 609894
    #
    # 2883 branch on Linux:
    #   User-Agent: Wget/1.15 (linux-gnu)
    #   Host: localhost:8080
    #   Accept: */*
    #   Connection: Keep-Alive
    #   Content-Type: multipart/form-data; boundary=--------------------------83572861f14cc736
    #   Content-Length: 32237
    #   Content-Encoding: gzip
    print self.headers

    chunked = 'Transfer-Encoding' in self.headers and self.headers['Transfer-Encoding'] == 'chunked'
    compressed = 'Content-Encoding' in self.headers and self.headers['Content-Encoding'] == 'gzip'
    if chunked:
      request_body = self._unchunk_request(compressed)
    else:
      content_length = int(self.headers[
          'Content-Length']) if 'Content-Length' in self.headers else 0
      if content_length > 0:
        request_body = self.rfile.read(content_length)
      else:
        request_body = self.rfile.read()
      if compressed:
        request_body = zlib.decompress(request_body, 16 + zlib.MAX_WBITS)

    # Parse the multi-part request.
    form_data = self._parse_post_data(request_body)
    for key in form_data.keys():
      if key == minidump_key and form_data[minidump_key].file:
        dmp_stream = form_data[minidump_key].file
      else:
        metadata[key] = form_data[key].value

    if dmp_stream is None:
      # Exit early if the request is invalid.
      print_msg('Invalid dump %s' % dump_id)
      return

    print_msg('Dump %s' % dump_id)

    # Write the minidump to file.
    dump_file = os.path.join(self._dump_directory, dump_id + '.dmp')
    with open(dump_file, 'wb') as fp:
      shutil.copyfileobj(dmp_stream, fp)

    # Write the metadata to file.
    meta_file = os.path.join(self._dump_directory, dump_id + '.json')
    with open(meta_file, 'w') as fp:
      json.dump(metadata, fp)


def HandleRequestsUsing(dump_store):
  return lambda *args: CrashHTTPRequestHandler(dump_directory, *args)


def RunCrashServer(port, dump_directory):
  """ Run the crash handler HTTP server. """
  httpd = HTTPServer(('', port), HandleRequestsUsing(dump_directory))
  print_msg('Starting httpd on port %d' % port)
  httpd.serve_forever()


# Program entry point.
if __name__ == "__main__":
  if len(sys.argv) != 3:
    print 'Usage: %s <port> <dump_directory>' % os.path.basename(sys.argv[0])
    sys.exit(1)

  # Create the dump directory if necessary.
  dump_directory = sys.argv[2]
  if not os.path.exists(dump_directory):
    os.makedirs(dump_directory)
  if not os.path.isdir(dump_directory):
    raise Exception('Directory does not exist: %s' % dump_directory)

  RunCrashServer(int(sys.argv[1]), dump_directory)
