# Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from cef_json_builder import cef_json_builder
import datetime
import math
import os
import sys

# Class used to build the cefbuilds HTML file. Generate an index.json file using
# the cef_json_builder_example.py tool (or some other means) and then run:
# > python cef_html_builder.py index.json index.html.in index.html
#
# Expected HTML template format is:
#
# Header
# <section:platform_link>
#  $platform_name$
# </section:platform_link>
# <section:platform>
#   $platform_name$ Builds:
#   <section:version>
#     CEF version $cef_version$ Files:
#     <section:file>
#       File $file$ size $size$ sha1 $sha1$
#     </section:file>
#   </section:version>
# </section:platform>
# Footer
#
# Notes:
# - The variables ("$key$") available in each section generally match the key
#   names that exist in the JSON file for that section. Additional variables are
#   exposed where needed.
# - Some global variables like "$year$" will be replaced in the whole template
#   before further parsing occurs.

class cef_html_builder:
  """ Class used to build the cefbuilds HTML file. """

  def __init__(self, branding=''):
    """ Create a new cef_html_builder object. """
    self.clear()
    self._branding = branding

  def clear(self):
    """ Clear the contents of this object. """
    self._parts = {}
    return;

  @staticmethod
  def _token(key):
    # Returns the token representation of |key|
    return '$' + key + '$'

  @staticmethod
  def _section_tags(section):
    # Returns the start and end tags for |section|
    return ('<section:' + section + '>', '</section:' + section + '>')

  @staticmethod
  def _section_key(section):
    # Returns the replacement key for |section|
    return section + '_section'

  @staticmethod
  def _replace(str, key, value):
    # Replaces all instances of |key| with |value| in |str|.
    return str.replace(cef_html_builder._token(key), value)

  @staticmethod
  def _replace_all(str, dict):
    for key, value in dict.iteritems():
      str = cef_html_builder._replace(str, key, value)
    return str

  @staticmethod
  def _extract(str, section):
    # Extracts the |section| region and replaces it with a token named
    # "<section>_section".
    (start_tag, end_tag) = cef_html_builder._section_tags(section)
    start_pos = str.find(start_tag)
    end_pos = str.rfind(end_tag)
    if start_pos < 0 or end_pos < 0:
      raise Exception('Failed to find section %s' % section)
    top = str[:start_pos]
    middle = str[start_pos + len(start_tag):end_pos]
    bottom = str[end_pos + len(end_tag):]
    return (top + cef_html_builder._token(cef_html_builder._section_key(section)) + bottom, middle)

  def load(self, html_template):
    """ Load the specified |html_template| string. """
    self.clear()
    root = html_template

    # Extract the platform link section from root.
    (root, platform_link) = self._extract(root, 'platform_link')

    # Extract platform section from root.
    (root, platform) = self._extract(root, 'platform')

    # Extract version section from platform.
    (platform, version) = self._extract(platform, 'version')

    # Extract file section from version.
    (version, file) = self._extract(version, 'file')

    self._parts = {
      'root': root,
      'platform_link': platform_link,
      'platform': platform,
      'version': version,
      'file': file
    }

  @staticmethod
  def _get_platform_name(platform):
    return {
      'linux32': 'Linux 32-bit',
      'linux64': 'Linux 64-bit',
      'linuxarm': 'Linux ARM',
      'macosx64': 'Mac OS X 64-bit',
      'windows32': 'Windows 32-bit',
      'windows64': 'Windows 64-bit'
    }[platform]

  @staticmethod
  def _get_type_name(type):
    return {
      'standard': 'Standard Distribution',
      'minimal': 'Minimal Distribution',
      'client': 'Sample Application',
      'debug_symbols': 'Debug Symbols',
      'release_symbols': 'Release Symbols'
    }[type]

  @staticmethod
  def _get_date(date):
    return date.strftime('%m/%d/%Y')

  @staticmethod
  def _get_file_size(size):
    if (size == 0):
      return '0B'
    size_name = ('B', 'KB', 'MB', 'GB')
    i = int(math.floor(math.log(size, 1024)))
    p = math.pow(1024, i)
    s = round(size/p, 2)
    return '%.2f %s' % (s, size_name[i])

  @staticmethod
  def _get_cef_source_url(cef_version):
    branch = cef_version.split('.')[1]
    return 'https://bitbucket.org/chromiumembedded/cef/get/%s.tar.bz2' % branch

  @staticmethod
  def _get_chromium_source_url(chromium_version):
    if chromium_version == 'master':
      return 'https://chromium.googlesource.com/chromium/src.git'
    return 'https://gsdview.appspot.com/chromium-browser-official/chromium-%s.tar.xz' % chromium_version

  @staticmethod
  def _get_file_url(platform, cef_version, file):
    return file['name']

  @staticmethod
  def _get_sha1_url(platform, cef_version, file):
    return file['name'] + '.sha1'

  @staticmethod
  def _get_tooltip_text(platform, cef_version, file):
    if platform.startswith('linux'):
      sample_app = 'cefsimple'
    else:
      sample_app = 'cefclient'
    return {
      'standard': 'Standard binary distribution. Includes header files, libcef_dll_wrapper source code, binary files, CMake configuration files and source code for the cefclient and cefsimple sample applications. See the included README.txt file for usage and build requirements.',
      'minimal': 'Minimal binary distribution. Includes header files, libcef_dll_wrapper source code, Release build binary files and CMake configuration files. Does not include Debug build binary files or sample application source code. See the included README.txt file for usage and build requirements.',
      'client': 'Release build of the ' + sample_app + ' sample application. See the included README.txt file for usage requirements.',
      'debug_symbols': 'Debug build symbols. Must be extracted and placed next to the CEF Debug binary file with the same name and version.',
      'release_symbols': 'Release build symbols. Must be extracted and placed next to the CEF Release binary file with the same name and version.'
    }[file['type']]

  def generate(self, json_builder):
    """ Generate HTML output based on the contents of |json_builder|. """
    if not isinstance(json_builder, cef_json_builder):
      raise Exception('Invalid argument')

    # Substitution values are augmented at each nesting level.
    subs = {
      'year': '2016',
      'branding': self._branding,
    }

    # Substitute variables.
    root_str = self._replace_all(self._parts['root'], subs)

    platform_link_strs = []
    platform_strs = []
    for platform in json_builder.get_platforms():
      subs['platform'] = platform
      subs['platform_name'] = self._get_platform_name(platform)

      # Substitute variables.
      platform_link_str = self._replace_all(self._parts['platform_link'], subs)
      platform_str = self._replace_all(self._parts['platform'], subs)

      version_strs = []
      for version in json_builder.get_versions(platform):
        subs['cef_version'] = version['cef_version']
        subs['chromium_version'] = version['chromium_version']
        subs['last_modified'] = self._get_date(version['files'][0]['last_modified'])
        subs['cef_source_url'] = self._get_cef_source_url(version['cef_version'])
        subs['chromium_source_url'] = self._get_chromium_source_url(version['chromium_version'])

        # Substitute variables.
        version_str = self._replace_all(self._parts['version'], subs)

        file_strs = {}
        for file in version['files']:
          subs['last_modified'] = self._get_date(file['last_modified'])
          subs['name'] = file['name']
          subs['sha1'] = file['sha1']
          subs['size'] = self._get_file_size(file['size'])
          subs['type'] = file['type']
          subs['type_name'] = self._get_type_name(file['type'])
          subs['file_url'] = self._get_file_url(platform, version['cef_version'], file)
          subs['sha1_url'] = self._get_sha1_url(platform, version['cef_version'], file)
          subs['tooltip_text'] = self._get_tooltip_text(platform, version['cef_version'], file)

          # Substitute variables.
          file_str = self._replace_all(self._parts['file'], subs)
          file_strs[file['type']] = file_str

        if len(file_strs) > 0:
          # Always output file types in the same order.
          file_out = ''
          type_order = ['standard', 'minimal', 'client', 'debug_symbols', 'release_symbols']
          for type in type_order:
            if type in file_strs:
              file_out = file_out + file_strs[type]

          # Insert files.
          version_str = self._replace(version_str, self._section_key('file'), file_out)
          version_strs.append(version_str)

      if len(version_strs) > 0:
        # Insert versions.
        platform_str = self._replace(platform_str, self._section_key('version'), "".join(version_strs))
        platform_strs.append(platform_str)
        platform_link_strs.append(platform_link_str)

    if len(platform_strs) > 0:
      # Insert platforms.
      root_str = self._replace(root_str, self._section_key('platform_link'), "".join(platform_link_strs))
      root_str = self._replace(root_str, self._section_key('platform'), "".join(platform_strs))

    return root_str


# Program entry point.
if __name__ == '__main__':
  # Verify command-line arguments.
  if len(sys.argv) < 4:
    sys.stderr.write('Usage: %s <json_file_in> <html_file_in> <html_file_out>' % sys.argv[0])
    sys.exit()

  json_file_in = sys.argv[1]
  html_file_in = sys.argv[2]
  html_file_out = sys.argv[3]

  # Create the HTML builder and load the HTML template.
  print '--> Reading %s' % html_file_in
  html_builder = cef_html_builder()
  with open(html_file_in, 'r') as f:
    html_builder.load(f.read())

  # Create the JSON builder and load the JSON file.
  print '--> Reading %s' % json_file_in
  json_builder = cef_json_builder(silent=False)
  with open(json_file_in, 'r') as f:
    json_builder.load(f.read())

  # Write the HTML output file.
  print '--> Writing %s' % html_file_out
  with open(html_file_out, 'w') as f:
    f.write(html_builder.generate(json_builder))
