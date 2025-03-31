# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

from __future__ import absolute_import
from cef_parser import *
import functools


def make_ctocpp_impl_proto(clsname, name, func, parts):
  const = ''

  proto = 'NO_SANITIZE("cfi-icall") '
  if func.has_attrib('no_stack_protector'):
    proto += 'NO_STACK_PROTECTOR '
  if clsname is None:
    proto += 'CEF_GLOBAL ' + parts['retval'] + ' '
  else:
    proto += parts['retval'] + ' ' + clsname
    if isinstance(func, obj_function_virtual):
      proto += 'CToCpp'
      if func.is_const():
        const = ' const'

    proto += '::'

  proto += name + '(' + ', '.join(parts['args']) + ')' + const
  return proto


def make_ctocpp_function_impl_existing(cls, name, func, impl, version):
  clsname = None if cls is None else cls.get_name(version=version)
  notify_name = name if clsname is None else '%sCToCpp::%s' % (clsname, name)

  notify(notify_name + ' has manual edits')

  # retrieve the C++ prototype parts
  parts = func.get_cpp_parts(True)

  changes = format_translation_changes(impl, parts)
  if len(changes) > 0:
    notify(notify_name + ' prototype changed')

  return make_ctocpp_impl_proto(clsname, name, func, parts)+'{'+ \
         changes+impl['body']+'\n}\n'


def make_ctocpp_function_impl_new(cls, name, func, base_scoped, version,
                                  version_finder):
  # Special handling for the CefShutdown global function.
  is_cef_shutdown = name == 'CefShutdown' and isinstance(
      func.parent, obj_header)

  clsname = None if cls is None else cls.get_name(version=version)
  notify_name = name if clsname is None else '%sCToCpp::%s' % (clsname, name)

  invalid = []

  # retrieve the function arguments
  args = func.get_arguments()

  # determine the argument types
  for arg in args:
    if arg.get_arg_type() == 'invalid':
      invalid.append(arg.get_name())

  # retrieve the function return value
  retval = func.get_retval()
  retval_type = retval.get_retval_type()
  if retval_type == 'invalid':
    invalid.append('(return value)')
    retval_default = ''
  else:
    retval_default = retval.get_retval_default(False)
    if len(retval_default) > 0:
      retval_default = ' ' + retval_default

  # build the C++ prototype
  parts = func.get_cpp_parts(True)
  result = make_ctocpp_impl_proto(clsname, name, func, parts) + ' {'

  is_virtual = isinstance(func, obj_function_virtual)

  if is_virtual and not version is None and not func.exists_at_version(version):
    notreached = format_notreached(
        True,
        '" should not be called at version %d"' % version,
        default_retval=retval_default.strip())
    result += '\n  // AUTO-GENERATED CONTENT' + \
              '\n  ' + notreached + \
              '\n}\n'
    return result

  if isinstance(func.parent, obj_class) and \
      not func.parent.has_attrib('no_debugct_check') and \
      not base_scoped:
    result += '\n  shutdown_checker::AssertNotShutdown();\n'

  if is_virtual:
    # determine how the struct should be referenced
    if cls.get_name() == func.parent.get_name():
      result += '\n  auto* _struct = GetStruct();'
    else:
      result += '\n  auto* _struct = reinterpret_cast<'+\
                func.parent.get_capi_name(version=version)+'*>(GetStruct());'

    result += '\n  if (!_struct->' + func.get_capi_name() + ') {'\
              '\n    return' + retval_default + ';'\
              '\n  }\n'

  # add API hash check
  if func.has_attrib('api_hash_check'):
    result += '\n  const char* api_hash = cef_api_hash(CEF_API_VERSION, 0);'\
              '\n  CHECK(!strcmp(api_hash, CEF_API_HASH_PLATFORM)) <<'\
              '\n      "API hashes for libcef and libcef_dll_wrapper do not match.";\n'

  if len(invalid) > 0:
    notify(notify_name + ' could not be autogenerated')
    # code could not be auto-generated
    result += '\n  // BEGIN DELETE BEFORE MODIFYING'
    result += '\n  // AUTO-GENERATED CONTENT'
    result += '\n  // COULD NOT IMPLEMENT DUE TO: ' + ', '.join(invalid)
    result += '\n  #pragma message("Warning: " __FILE__ ": ' + name + ' is not implemented")'
    result += '\n  // END DELETE BEFORE MODIFYING'
    result += '\n}\n\n'
    return result

  result += '\n  // AUTO-GENERATED CONTENT - DELETE THIS COMMENT BEFORE MODIFYING\n'

  result_len = len(result)

  optional = []

  # parameter verification
  for arg in args:
    arg_type = arg.get_arg_type()
    arg_name = arg.get_type().get_name()

    # skip optional params
    optional_params = arg.parent.get_attrib_list('optional_param')
    if not optional_params is None and arg_name in optional_params:
      optional.append(arg_name)
      continue

    comment = '\n  // Verify param: ' + arg_name + '; type: ' + arg_type

    if arg_type == 'simple_byaddr' or arg_type == 'bool_byaddr':
      result += comment+\
                '\n  DCHECK('+arg_name+');'\
                '\n  if (!'+arg_name+') {'\
                '\n    return'+retval_default+';'\
                '\n  }'
    elif arg_type == 'refptr_same' or arg_type == 'refptr_diff' or \
         arg_type == 'ownptr_same' or arg_type == 'ownptr_diff':
      result += comment+\
                '\n  DCHECK('+arg_name+'.get());'\
                '\n  if (!'+arg_name+'.get()) {'\
                '\n    return'+retval_default+';'\
                '\n  }'
    elif arg_type == 'rawptr_same' or arg_type == 'rawptr_diff':
      result += comment+\
                '\n  DCHECK('+arg_name+');'\
                '\n  if (!'+arg_name+') {'\
                '\n    return'+retval_default+';'\
                '\n  }'
    elif arg_type == 'string_byref_const':
      result += comment+\
                '\n  DCHECK(!'+arg_name+'.empty());'\
                '\n  if ('+arg_name+'.empty()) {'\
                '\n    return'+retval_default+';'\
                '\n  }'

    # check index params
    index_params = arg.parent.get_attrib_list('index_param')
    if not index_params is None and arg_name in index_params:
      result += comment+\
                '\n  DCHECK_GE('+arg_name+', 0);'\
                '\n  if ('+arg_name+' < 0) {'\
                '\n    return'+retval_default+';'\
                '\n  }'

  if len(optional) > 0:
    # Wrap the comment at 80 characters.
    str = '\n  // Unverified params: ' + optional[0]
    for name in optional[1:]:
      str += ','
      if len(str) + len(name) + 1 > 80:
        result += str
        str = '\n  //'
      str += ' ' + name
    result += str

  if len(result) != result_len:
    result += '\n'
  result_len = len(result)

  # parameter translation
  params = []
  if isinstance(func, obj_function_virtual):
    params.append('_struct')

  for arg in args:
    arg_type = arg.get_arg_type()
    arg_name = arg.get_type().get_name()

    comment = '\n  // Translate param: ' + arg_name + '; type: ' + arg_type

    if arg_type == 'simple_byval' or arg_type == 'simple_byaddr' or \
       arg_type == 'bool_byval':
      params.append(arg_name)
    elif arg_type == 'simple_byref' or arg_type == 'simple_byref_const' or \
        arg_type == 'struct_byref_const' or arg_type == 'struct_byref':
      params.append('&' + arg_name)
    elif arg_type == 'bool_byref':
      result += comment+\
                '\n  int '+arg_name+'Int = '+arg_name+';'
      params.append('&' + arg_name + 'Int')
    elif arg_type == 'bool_byaddr':
      result += comment+\
                '\n  int '+arg_name+'Int = '+arg_name+'?*'+arg_name+':0;'
      params.append('&' + arg_name + 'Int')
    elif arg_type == 'string_byref_const':
      params.append(arg_name + '.GetStruct()')
    elif arg_type == 'string_byref':
      params.append(arg_name + '.GetWritableStruct()')
    elif arg_type == 'refptr_same':
      ptr_class = arg.get_type().get_ptr_type()
      params.append(ptr_class + 'CToCpp_Unwrap(' + arg_name + ')')
    elif arg_type == 'ownptr_same':
      ptr_class = arg.get_type().get_ptr_type()
      params.append(ptr_class + 'CToCpp_UnwrapOwn(std::move(' + arg_name + '))')
    elif arg_type == 'rawptr_same':
      ptr_class = arg.get_type().get_ptr_type()
      params.append(ptr_class + 'CToCpp_UnwrapRaw(' + arg_name + ')')
    elif arg_type == 'refptr_diff':
      ptr_class = arg.get_type().get_ptr_type()
      params.append(ptr_class + 'CppToC_Wrap(' + arg_name + ')')
    elif arg_type == 'ownptr_diff':
      ptr_class = arg.get_type().get_ptr_type()
      params.append(ptr_class + 'CppToC_WrapOwn(std::move(' + arg_name + '))')
    elif arg_type == 'rawptr_diff':
      ptr_class = arg.get_type().get_ptr_type()
      result += comment+\
                '\n  auto ['+arg_name+'Ptr, '+arg_name+'Struct] = '+ptr_class+'CppToC_WrapRaw('+arg_name+');'
      params.append(arg_name + 'Struct')
    elif arg_type == 'refptr_same_byref' or arg_type == 'refptr_diff_byref':
      ptr_class = arg.get_type().get_ptr_type()
      ptr_struct = arg.get_type().get_result_ptr_type_root()
      if not version_finder is None:
        ptr_struct = version_finder(ptr_struct)
      if arg_type == 'refptr_same_byref':
        assign = ptr_class + 'CToCpp_Unwrap(' + arg_name + ')'
      else:
        assign = ptr_class + 'CppToC_Wrap(' + arg_name + ')'
      result += comment+\
                '\n  '+ptr_struct+'* '+arg_name+'Struct = NULL;'\
                '\n  if ('+arg_name+'.get()) {'\
                '\n    '+arg_name+'Struct = '+assign+';'\
                '\n  }'\
                '\n  auto* '+arg_name+'Orig = '+arg_name+'Struct;'
      params.append('&' + arg_name + 'Struct')
    elif arg_type == 'string_vec_byref' or arg_type == 'string_vec_byref_const':
      result += comment+\
                '\n  cef_string_list_t '+arg_name+'List = cef_string_list_alloc();'\
                '\n  DCHECK('+arg_name+'List);'\
                '\n  if ('+arg_name+'List) {'\
                '\n    transfer_string_list_contents('+arg_name+', '+arg_name+'List);'\
                '\n  }'
      params.append(arg_name + 'List')
    elif arg_type == 'string_map_single_byref' or arg_type == 'string_map_single_byref_const':
      result += comment+\
                '\n  cef_string_map_t '+arg_name+'Map = cef_string_map_alloc();'\
                '\n  DCHECK('+arg_name+'Map);'\
                '\n  if ('+arg_name+'Map) {'\
                '\n    transfer_string_map_contents('+arg_name+', '+arg_name+'Map);'\
                '\n  }'
      params.append(arg_name + 'Map')
    elif arg_type == 'string_map_multi_byref' or arg_type == 'string_map_multi_byref_const':
      result += comment+\
                '\n  cef_string_multimap_t '+arg_name+'Multimap = cef_string_multimap_alloc();'\
                '\n  DCHECK('+arg_name+'Multimap);'\
                '\n  if ('+arg_name+'Multimap) {'\
                '\n    transfer_string_multimap_contents('+arg_name+', '+arg_name+'Multimap);'\
                '\n  }'
      params.append(arg_name + 'Multimap')
    elif arg_type == 'simple_vec_byref' or arg_type == 'bool_vec_byref' or \
         arg_type == 'refptr_vec_same_byref' or arg_type == 'refptr_vec_diff_byref':
      count_func = arg.get_attrib_count_func()
      vec_type = arg.get_type().get_result_vector_type_root()
      if not version_finder is None:
        vec_type = version_finder(vec_type)

      if arg_type == 'refptr_vec_same_byref':
        ptr_class = arg.get_type().get_ptr_type()
        assign = ptr_class + 'CToCpp_Unwrap(' + arg_name + '[i])'
      elif arg_type == 'refptr_vec_diff_byref':
        ptr_class = arg.get_type().get_ptr_type()
        assign = ptr_class + 'CppToC_Wrap(' + arg_name + '[i])'
      else:
        assign = arg_name + '[i]'
      result += comment+\
                '\n  size_t '+arg_name+'Size = '+arg_name+'.size();'\
                '\n  size_t '+arg_name+'Count = std::max('+count_func+'(), '+arg_name+'Size);'\
                '\n  '+vec_type+'* '+arg_name+'List = NULL;'\
                '\n  if ('+arg_name+'Count > 0) {'\
                '\n    '+arg_name+'List = new '+vec_type+'['+arg_name+'Count];'\
                '\n    DCHECK('+arg_name+'List);'\
                '\n    if ('+arg_name+'List) {'\
                '\n       memset('+arg_name+'List, 0, sizeof('+vec_type+')*'+arg_name+'Count);'\
                '\n    }'\
                '\n    if ('+arg_name+'List && '+arg_name+'Size > 0) {'\
                '\n      for (size_t i = 0; i < '+arg_name+'Size; ++i) {'\
                '\n        '+arg_name+'List[i] = '+assign+';'\
                '\n      }'\
                '\n    }'\
                '\n  }'
      params.append('&' + arg_name + 'Count')
      params.append(arg_name + 'List')
    elif arg_type == 'simple_vec_byref_const' or arg_type == 'bool_vec_byref_const' or \
         arg_type == 'refptr_vec_same_byref_const' or arg_type == 'refptr_vec_diff_byref_const' or \
         arg_type == 'rawptr_vec_same_byref_const' or arg_type == 'rawptr_vec_diff_byref_const':
      count_func = arg.get_attrib_count_func()
      vec_type = arg.get_type().get_result_vector_type_root()
      if not version_finder is None:
        vec_type = version_finder(vec_type)

      if arg_type == 'simple_vec_byref_const' or arg_type == 'bool_vec_byref_const':
        assign = arg_name + '[i]'
      else:
        ptr_class = arg.get_type().get_ptr_type()
        if arg_type == 'refptr_vec_same_byref_const':
          assign = ptr_class + 'CToCpp_Unwrap(' + arg_name + '[i])'
        elif arg_type == 'refptr_vec_diff_byref_const':
          assign = ptr_class + 'CppToC_Wrap(' + arg_name + '[i])'
        elif arg_type == 'rawptr_vec_same_byref_const':
          assign = ptr_class + 'CToCpp_UnwrapRaw(' + arg_name + '[i])'
        elif arg_type == 'rawptr_vec_diff_byref_const':
          assign = ptr_class + 'CppToC_WrapRawAndRelease(' + arg_name + '[i])'
      result += comment+\
                '\n  const size_t '+arg_name+'Count = '+arg_name+'.size();'\
                '\n  '+vec_type+'* '+arg_name+'List = NULL;'\
                '\n  if ('+arg_name+'Count > 0) {'\
                '\n    '+arg_name+'List = new '+vec_type+'['+arg_name+'Count];'\
                '\n    DCHECK('+arg_name+'List);'\
                '\n    if ('+arg_name+'List) {'\
                '\n      for (size_t i = 0; i < '+arg_name+'Count; ++i) {'\
                '\n        '+arg_name+'List[i] = '+assign+';'\
                '\n      }'\
                '\n    }'\
                '\n  }'
      params.append(arg_name + 'Count')
      params.append(arg_name + 'List')
    else:
      raise Exception('Unsupported argument type %s for parameter %s in %s' %
                      (arg_type, arg_name, name))

  if len(result) != result_len:
    result += '\n'
  result_len = len(result)

  # execution
  result += '\n  // Execute\n  '

  if retval_type != 'none':
    # has a return value
    if retval_type == 'simple' or retval_type == 'bool':
      result += retval.get_type().get_result_simple_type_root()
    elif retval_type == 'simple_byaddr':
      result += retval.get_type().get_result_simple_type()
    elif retval_type == 'string':
      result += 'cef_string_userfree_t'
    elif retval_type == 'refptr_same' or retval_type == 'refptr_diff' or \
         retval_type == 'ownptr_same' or retval_type == 'ownptr_diff':
      result += 'auto*'
    else:
      raise Exception('Unsupported return type %s in %s' % (retval_type, name))

    result += ' _retval = '

  if isinstance(func, obj_function_virtual):
    result += '_struct->'
  result += func.get_capi_name() + '('

  if len(params) > 0:
    if not isinstance(func, obj_function_virtual):
      result += '\n      '
    result += ',\n      '.join(params)

  result += ');\n'

  if is_cef_shutdown:
    result += '\n\n#if DCHECK_IS_ON()'\
              '\n  shutdown_checker::SetIsShutdown();'\
              '\n#endif\n'

  result_len = len(result)

  # parameter restoration
  for arg in args:
    arg_type = arg.get_arg_type()
    arg_name = arg.get_type().get_name()

    comment = '\n  // Restore param:' + arg_name + '; type: ' + arg_type

    if arg_type == 'bool_byref':
      result += comment+\
                '\n  '+arg_name+' = '+arg_name+'Int?true:false;'
    elif arg_type == 'bool_byaddr':
      result += comment+\
                '\n  if ('+arg_name+') {'\
                '\n    *'+arg_name+' = '+arg_name+'Int?true:false;'\
                '\n  }'
    elif arg_type == 'refptr_same_byref' or arg_type == 'refptr_diff_byref':
      ptr_class = arg.get_type().get_ptr_type()
      if arg_type == 'refptr_same_byref':
        assign = ptr_class + 'CToCpp_Wrap(' + arg_name + 'Struct)'
      else:
        assign = ptr_class + 'CppToC_Unwrap(' + arg_name + 'Struct)'
      result += comment+\
                '\n  if ('+arg_name+'Struct) {'\
                '\n    if ('+arg_name+'Struct != '+arg_name+'Orig) {'\
                '\n      '+arg_name+' = '+assign+';'\
                '\n    }'\
                '\n  } else {'\
                '\n    '+arg_name+' = nullptr;'\
                '\n  }'
    elif arg_type == 'string_vec_byref':
      result += comment+\
                '\n  if ('+arg_name+'List) {'\
                '\n    '+arg_name+'.clear();'\
                '\n    transfer_string_list_contents('+arg_name+'List, '+arg_name+');'\
                '\n    cef_string_list_free('+arg_name+'List);'\
                '\n  }'
    elif arg_type == 'string_vec_byref_const':
      result += comment+\
                '\n  if ('+arg_name+'List) {'\
                '\n    cef_string_list_free('+arg_name+'List);'\
                '\n  }'
    elif arg_type == 'string_map_single_byref':
      result += comment+\
                '\n  if ('+arg_name+'Map) {'\
                '\n    '+arg_name+'.clear();'\
                '\n    transfer_string_map_contents('+arg_name+'Map, '+arg_name+');'\
                '\n    cef_string_map_free('+arg_name+'Map);'\
                '\n  }'
    elif arg_type == 'string_map_single_byref_const':
      result += comment+\
                '\n  if ('+arg_name+'Map) {'\
                '\n    cef_string_map_free('+arg_name+'Map);'\
                '\n  }'
    elif arg_type == 'string_map_multi_byref':
      result += comment+\
                '\n  if ('+arg_name+'Multimap) {'\
                '\n    '+arg_name+'.clear();'\
                '\n    transfer_string_multimap_contents('+arg_name+'Multimap, '+arg_name+');'\
                '\n    cef_string_multimap_free('+arg_name+'Multimap);'\
                '\n  }'
    elif arg_type == 'string_map_multi_byref_const':
      result += comment+\
                '\n  if ('+arg_name+'Multimap) {'\
                '\n    cef_string_multimap_free('+arg_name+'Multimap);'\
                '\n  }'
    elif arg_type == 'simple_vec_byref' or arg_type == 'bool_vec_byref' or \
         arg_type == 'refptr_vec_same_byref' or arg_type == 'refptr_vec_diff_byref':
      count_func = arg.get_attrib_count_func()

      if arg_type == 'refptr_vec_same_byref':
        ptr_class = arg.get_type().get_ptr_type()
        assign = ptr_class + 'CToCpp_Wrap(' + arg_name + 'List[i])'
      elif arg_type == 'refptr_vec_diff_byref':
        ptr_class = arg.get_type().get_ptr_type()
        assign = ptr_class + 'CppToC_Unwrap(' + arg_name + 'List[i])'
      elif arg_type == 'bool_vec_byref':
        assign = arg_name + 'List[i]?true:false'
      else:
        assign = arg_name + 'List[i]'
      result += comment+\
                '\n  '+arg_name+'.clear();'\
                '\n  if ('+arg_name+'Count > 0 && '+arg_name+'List) {'\
                '\n    for (size_t i = 0; i < '+arg_name+'Count; ++i) {'\
                '\n      '+arg_name+'.push_back('+assign+');'\
                '\n    }'\
                '\n    delete [] '+arg_name+'List;'\
                '\n  }'
    elif arg_type == 'simple_vec_byref_const' or arg_type == 'bool_vec_byref_const' or \
         arg_type == 'refptr_vec_same_byref_const' or arg_type == 'refptr_vec_diff_byref_const' or \
         arg_type == 'rawptr_vec_same_byref_const' or arg_type == 'rawptr_vec_diff_byref_const':
      result += comment
      if arg_type == 'rawptr_vec_diff_byref_const':
        result += '\n  if ('+arg_name+'Count > 0) {'\
                  '\n    for (size_t i = 0; i < '+arg_name+'Count; ++i) {'\
                  '\n      delete '+ptr_class+'CppToC_GetWrapper('+arg_name+'List[i]);'\
                  '\n    }'\
                  '\n  }'
      result += '\n  if ('+arg_name+'List) {'\
                '\n    delete [] '+arg_name+'List;'\
                '\n  }'

  if len(result) != result_len:
    result += '\n'
  result_len = len(result)

  if len(result) != result_len:
    result += '\n'
  result_len = len(result)

  # return translation
  if retval_type != 'none':
    # has a return value
    result += '\n  // Return type: ' + retval_type
    if retval_type == 'simple' or retval_type == 'simple_byaddr':
      result += '\n  return _retval;'
    elif retval_type == 'bool':
      result += '\n  return _retval?true:false;'
    elif retval_type == 'string':
      result += '\n  CefString _retvalStr;'\
                '\n  _retvalStr.AttachToUserFree(_retval);'\
                '\n  return _retvalStr;'
    elif retval_type == 'refptr_same' or retval_type == 'ownptr_same':
      ptr_class = retval.get_type().get_ptr_type()
      result += '\n  return ' + ptr_class + 'CToCpp_Wrap(_retval);'
    elif retval_type == 'refptr_diff':
      ptr_class = retval.get_type().get_ptr_type()
      result += '\n  return ' + ptr_class + 'CppToC_Unwrap(_retval);'
    elif retval_type == 'ownptr_diff':
      ptr_class = retval.get_type().get_ptr_type()
      result += '\n  return ' + ptr_class + 'CppToC_UnwrapOwn(_retval);'
    else:
      raise Exception('Unsupported return type %s in %s' % (retval_type, name))

  if len(result) != result_len:
    result += '\n'

  result += '}\n'
  return result


def make_ctocpp_function_impl(cls, funcs, existing, base_scoped, version,
                              version_finder):
  impl = ''

  customized = False

  for func in funcs:
    name = func.get_name()
    value = get_next_function_impl(existing, name)

    if version is None:
      pre, post = get_version_surround(func, long=True)
    else:
      pre = post = ''

    if not value is None \
        and value['body'].find('// AUTO-GENERATED CONTENT') < 0:
      # an implementation exists that was not auto-generated
      customized = True
      impl += pre + make_ctocpp_function_impl_existing(cls, name, func, value,
                                                       version) + post + '\n'
    else:
      impl += pre + make_ctocpp_function_impl_new(
          cls, name, func, base_scoped, version, version_finder) + post + '\n'

  if not customized and impl.find('// COULD NOT IMPLEMENT') >= 0:
    customized = True

  return (impl, customized)


def make_ctocpp_virtual_function_impl(header, cls, existing, base_scoped,
                                      version, version_finder):
  impl, customized = make_ctocpp_function_impl(
      cls,
      cls.get_virtual_funcs(), existing, base_scoped, version, version_finder)

  cur_cls = cls
  while True:
    parent_name = cur_cls.get_parent_name()
    if is_base_class(parent_name):
      break
    else:
      parent_cls = header.get_class(parent_name)
      if parent_cls is None:
        raise Exception('Class does not exist: ' + parent_name)
      pimpl, pcustomized = make_ctocpp_function_impl(
          cls,
          parent_cls.get_virtual_funcs(), existing, base_scoped, version,
          version_finder)
      impl += pimpl
      if pcustomized and not customized:
        customized = True
    cur_cls = header.get_class(parent_name)

  return (impl, customized)


def make_ctocpp_unwrap_derived(header, cls, base_scoped, version):
  # identify all classes that derive from cls
  derived_classes = []
  clsname = cls.get_name()

  if version is None:
    capiname = cls.get_capi_name()
  else:
    capiname = cls.get_capi_name(version=version)

  allclasses = header.get_classes()
  for cur_cls in allclasses:
    if cur_cls.get_name() == clsname:
      continue
    if cur_cls.has_parent(clsname):
      derived_classes.append(cur_cls.get_name())

  derived_classes = sorted(derived_classes)

  if base_scoped:
    impl = ['', '']
    for clsname in derived_classes:
      derived_cls = header.get_class(clsname)
      if version is None:
        pre, post = get_version_surround(derived_cls)
      else:
        if not derived_cls.exists_at_version(version):
          continue
        pre = post = ''

      impl[0] += pre + '  if (type == '+get_wrapper_type_enum(clsname)+') {\n'+\
                 '    return reinterpret_cast<'+capiname+'*>('+\
                 clsname+'CToCpp_UnwrapOwn(CefOwnPtr<'+clsname+'>(reinterpret_cast<'+clsname+'*>(c.release()))));\n'+\
                 '  }\n' + post
      impl[1] += pre + '  if (type == '+get_wrapper_type_enum(clsname)+') {\n'+\
                 '    return reinterpret_cast<'+capiname+'*>('+\
                 clsname+'CToCpp_UnwrapRaw(CefRawPtr<'+clsname+'>(reinterpret_cast<'+clsname+'*>(c))));\n'+\
                 '  }\n' + post
  else:
    impl = ''
    for clsname in derived_classes:
      derived_cls = header.get_class(clsname)
      if version is None:
        pre, post = get_version_surround(derived_cls)
      else:
        if not derived_cls.exists_at_version(version):
          continue
        pre = post = ''

      impl += pre + '  if (type == '+get_wrapper_type_enum(clsname)+') {\n'+\
              '    return reinterpret_cast<'+capiname+'*>('+\
              clsname+'CToCpp_Unwrap(reinterpret_cast<'+clsname+'*>(c)));\n'+\
              '  }\n' + post
  return impl


def make_ctocpp_version_wrappers(header, cls, base_scoped, versions):
  assert len(versions) > 0

  clsname = cls.get_name()
  typename = clsname + 'CToCpp'
  structname = cls.get_capi_name(version=versions[0])

  rversions = sorted(versions, reverse=True)

  notreached = format_notreached(
      True,
      '" called with invalid version " << version',
      default_retval='nullptr')

  impl = ''

  if base_scoped:
    impl += 'CefOwnPtr<' + clsname + '> ' + typename + '_Wrap(' + structname + '* s) {\n' + \
            '  const int version = cef_api_version();\n'

    for version in rversions:
      vstr = str(version)
      impl += '  if (version >= ' + vstr + ') {\n'
      if versions[0] == version:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::Wrap(s);\n'
      else:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::Wrap(reinterpret_cast<' + cls.get_capi_name(
            version) + '*>(s));\n'
      impl += '  }\n'

    impl += '  ' + notreached + '\n'+ \
            '}\n\n' + \
            structname + '* ' + typename + '_UnwrapOwn(CefOwnPtr<' + clsname + '> c) {\n' + \
            '  const int version = cef_api_version();\n'

    for version in rversions:
      vstr = str(version)
      impl += '  if (version >= ' + vstr + ') {\n'
      if versions[0] == version:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::UnwrapOwn(std::move(c));\n'
      else:
        impl += '    return reinterpret_cast<' + structname + '*>(' + clsname + '_' + vstr + '_CToCpp::UnwrapOwn(std::move(c)));\n'
      impl += '  }\n'

    impl += '  ' + notreached + '\n'+ \
            '}\n\n' + \
            structname + '* ' + typename + '_UnwrapRaw(CefRawPtr<' + clsname + '> c) {\n' + \
            '  const int version = cef_api_version();\n'

    for version in rversions:
      vstr = str(version)
      impl += '  if (version >= ' + vstr + ') {\n'
      if versions[0] == version:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::UnwrapRaw(std::move(c));\n'
      else:
        impl += '    return reinterpret_cast<' + structname + '*>(' + clsname + '_' + vstr + '_CToCpp::UnwrapRaw(std::move(c)));\n'
      impl += '  }\n'

    impl += '  ' + notreached + '\n'+ \
            '}\n'
  else:
    impl += 'CefRefPtr<' + clsname + '> ' + typename + '_Wrap(' + structname + '* s) {\n' + \
            '  const int version = cef_api_version();\n'

    for version in rversions:
      vstr = str(version)
      impl += '  if (version >= ' + vstr + ') {\n'
      if versions[0] == version:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::Wrap(s);\n'
      else:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::Wrap(reinterpret_cast<' + cls.get_capi_name(
            version) + '*>(s));\n'
      impl += '  }\n'

    impl += '  ' + notreached + '\n'+ \
            '}\n\n' + \
            structname + '* ' + typename + '_Unwrap(CefRefPtr<' + clsname + '> c) {\n' + \
            '  const int version = cef_api_version();\n'

    for version in rversions:
      vstr = str(version)
      impl += '  if (version >= ' + vstr + ') {\n'
      if versions[0] == version:
        impl += '    return ' + clsname + '_' + vstr + '_CToCpp::Unwrap(c);\n'
      else:
        impl += '    return reinterpret_cast<' + structname + '*>(' + clsname + '_' + vstr + '_CToCpp::Unwrap(c));\n'
      impl += '  }\n'

    impl += '  ' + notreached + '\n'+ \
            '}\n'

  return impl + '\n'


def _version_finder(header, version, name):
  assert version is None or isinstance(version, int), version

  # normalize ptr values
  if name[-1] == '*':
    name = name[0:-1]
    suffix = '*'
  else:
    suffix = ''

  cls = header.get_capi_class(name)
  if not cls is None:
    name = cls.get_capi_name(first_version=True)

  return name + suffix


def make_ctocpp_class_impl(header, clsname, impl):
  cls = header.get_class(clsname)
  if cls is None:
    raise Exception('Class does not exist: ' + clsname)

  base_class_name = header.get_base_class_name(clsname)
  base_scoped = True if base_class_name == 'CefBaseScoped' else False
  if base_scoped:
    template_class = 'CefCToCppScoped'
  else:
    template_class = 'CefCToCppRefCounted'

  with_versions = cls.is_client_side()
  versions = list(cls.get_all_versions()) if with_versions else (None,)

  # retrieve the existing static function implementations
  existing_static = get_function_impls(impl, clsname + '::')

  staticout = virtualout = ''
  customized = False
  first = True
  idx = 0

  for version in versions:
    version_finder = functools.partial(_version_finder, header,
                                       version) if with_versions else None

    if first:
      first = False

      # generate static functions
      staticimpl, scustomized = make_ctocpp_function_impl(
          cls,
          cls.get_static_funcs(), existing_static, base_scoped, version,
          version_finder)
      if len(staticimpl) > 0:
        staticout += '\n// STATIC METHODS - Body may be edited by hand.\n\n' + staticimpl
      if scustomized:
        customized = True

      if len(versions) > 1:
        staticout += '// HELPER FUNCTIONS - Do not edit by hand.\n\n'
        staticout += make_ctocpp_version_wrappers(header, cls, base_scoped,
                                                  versions)

    comment = '' if version is None else (' FOR VERSION %d' % version)
    typename = cls.get_name(version=version) + 'CToCpp'

    # retrieve the existing virtual function implementations
    existing_virtual = get_function_impls(impl, typename + '::')

    # generate virtual functions
    virtualimpl, vcustomized = make_ctocpp_virtual_function_impl(
        header, cls, existing_virtual, base_scoped, version, version_finder)
    if len(virtualimpl) > 0:
      virtualout += '\n// VIRTUAL METHODS' + comment + ' - Body may be edited by hand.\n\n' + virtualimpl
    if vcustomized:
      customized = True

    # any derived classes can be unwrapped
    unwrapderived = make_ctocpp_unwrap_derived(header, cls, base_scoped,
                                               version)

    capiname = cls.get_capi_name(version=version)

    const =  '// CONSTRUCTOR' + comment + ' - Do not edit by hand.\n\n'+ \
             typename+'::'+typename+'() {\n'

    if not version is None:
      if idx < len(versions) - 1:
        condition = 'version < %d || version >= %d' % (version, versions[idx
                                                                         + 1])
      else:
        condition = 'version < %d' % version

      const += '  const int version = cef_api_version();\n' + \
               '  LOG_IF(FATAL, ' + condition + ') << __func__ << " called with invalid version " << version;\n'

    const += '}\n\n'+ \
             '// DESTRUCTOR' + comment + ' - Do not edit by hand.\n\n'+ \
             typename+'::~'+typename+'() {\n'

    if not cls.has_attrib('no_debugct_check') and not base_scoped:
      const += '  shutdown_checker::AssertNotShutdown();\n'

    const += '}\n\n'

    parent_sig = template_class + '<' + typename + ', ' + clsname + ', ' + capiname + '>'
    notreached = format_notreached(
        with_versions,
        '" called with unexpected class type " << type',
        default_retval='nullptr')

    if base_scoped:
      const += 'template<> '+capiname+'* '+parent_sig+'::UnwrapDerivedOwn(CefWrapperType type, CefOwnPtr<'+clsname+'> c) {\n'+ \
               unwrapderived[0] + \
               '  ' + notreached + '\n'+ \
               '}\n\n' + \
               'template<> '+capiname+'* '+parent_sig+'::UnwrapDerivedRaw(CefWrapperType type, CefRawPtr<'+clsname+'> c) {\n'+ \
               unwrapderived[1] + \
               '  ' + notreached + '\n'+ \
               '}\n\n'
    else:
      const += 'template<> '+capiname+'* '+parent_sig+'::UnwrapDerived(CefWrapperType type, '+clsname+'* c) {\n'+ \
               unwrapderived + \
               '  ' + notreached + '\n'+ \
               '}\n\n'

    const += 'template<> CefWrapperType ' + parent_sig + '::kWrapperType = ' + get_wrapper_type_enum(
        clsname) + ';\n\n'

    virtualout += const
    idx += 1

  out = staticout + virtualout

  # determine what includes are required by identifying what translation
  # classes are being used
  includes = format_translation_includes(
      header,
      out + (unwrapderived[0] if base_scoped else unwrapderived),
      with_versions=with_versions)

  # build the final output
  result = get_copyright()

  result += includes + '\n'

  if with_versions:
    pre = post = ''
  else:
    pre, post = get_version_surround(cls, long=True)
    if len(pre) > 0:
      result += pre + '\n'

  result += out + '\n'

  if len(post) > 0:
    result += post + '\n'

  return (result, customized)


def make_ctocpp_global_impl(header, impl):
  # retrieve the existing global function implementations
  existing = get_function_impls(impl, 'CEF_GLOBAL')

  # generate static functions
  impl, customized = make_ctocpp_function_impl(None,
                                               header.get_funcs(), existing,
                                               False, None, None)
  if len(impl) > 0:
    impl = '\n// GLOBAL METHODS - Body may be edited by hand.\n\n' + impl

  includes = ''

  # include required headers for global functions
  paths = set()
  for func in header.get_funcs():
    filename = func.get_file_name()
    paths.add('include/' + func.get_file_name())
    paths.add('include/capi/' + func.get_capi_file_name())

  # determine what includes are required by identifying what translation
  # classes are being used
  includes += format_translation_includes(header, impl, other_includes=paths)

  # build the final output
  result = get_copyright()

  result += includes + '\n// Define used to facilitate parsing.\n#define CEF_GLOBAL\n\n' + impl

  return (result, customized)


def write_ctocpp_impl(header, clsname, dir):
  if clsname is None:
    # global file
    file = dir
  else:
    # class file
    # give the output file the same directory offset as the input file
    cls = header.get_class(clsname)
    dir = os.path.dirname(os.path.join(dir, cls.get_file_name()))
    file = os.path.join(dir, get_capi_name(clsname[3:], False) + '_ctocpp.cc')

  set_notify_context(file)

  if path_exists(file):
    oldcontents = read_file(file)
  else:
    oldcontents = ''

  if clsname is None:
    newcontents, customized = make_ctocpp_global_impl(header, oldcontents)
  else:
    newcontents, customized = make_ctocpp_class_impl(header, clsname,
                                                     oldcontents)

  set_notify_context(None)

  return (file, newcontents, customized)


# test the module
if __name__ == "__main__":
  import sys

  # verify that the correct number of command-line arguments are provided
  if len(sys.argv) < 4:
    sys.stderr.write('Usage: ' + sys.argv[0] +
                     ' <infile> <classname> <existing_impl>\n')
    sys.exit()

  # create the header object
  header = obj_header()
  header.add_file(sys.argv[1])

  # read the existing implementation file into memory
  try:
    with open(sys.argv[3], 'r') as f:
      data = f.read()
  except IOError as e:
    (errno, strerror) = e.args
    raise Exception('Failed to read file ' + sys.argv[3] + ': ' + strerror)

  # dump the result to stdout
  sys.stdout.write(make_ctocpp_class_impl(header, sys.argv[2], data))
