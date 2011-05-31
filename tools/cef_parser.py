# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

import os
import re
import shutil
import string
import sys
import textwrap
import time


def notify(msg):
    """ Display a message. """
    sys.stdout.write('  NOTE: '+msg+'\n')
    

def read_file(name, normalize = True):
    """ Function for reading a file. """
    try:
        f = open(name, 'r')
        # read the data
        data = f.read()
        if normalize:
            # normalize line endings
            data = data.replace("\r\n", "\n")
        return data
    except IOError, (errno, strerror):
        sys.stderr.write('Failed to read file '+filename+': '+strerror)
        raise
    else:
        f.close()
 
def write_file(name, data):
    """ Function for writing a file. """
    notify('Writing file '+name)
    try:
        f = open(name, 'w')
        # write the data
        f.write(data)
    except IOError, (errno, strerror):
       sys.stderr.write('Failed to write file '+name+': '+strerror)
       raise
    else:
        f.close()
        
def file_exists(name):
    """ Returns true if the file currently exists. """
    return os.path.exists(name)

def backup_file(name):
    """ Renames the file to a name that includes the current time stamp. """
    notify('Creating a backup of file '+name)
    shutil.move(name, name+'.'+time.strftime('%Y-%m-%d-%H-%M-%S'))


def wrap_text(text, indent = '', maxchars = 80):
    """ Wrap the text to the specified number of characters. If
    necessary a line will be broken and wrapped after a word.
    """
    result = ''
    lines = textwrap.wrap(text, maxchars - len(indent))
    for line in lines:
        result += indent+line+'\n'
    return result
    
def wrap_code(code, indent = '    ', maxchars = 80, splitchars = '(=,'):
    """ Wrap the code lines to the specified number of characters. If
    necessary a line will be broken and wrapped after one of the split
    characters.
    """
    output = ''
    
    # normalize line endings
    code = code.replace("\r\n", "\n")
    
    # break the code chunk into lines
    lines = string.split(code, '\n')
    for line in lines:
        if len(line) <= maxchars:
            # line is short enough that it doesn't need to be wrapped
            output += line + '\n'
            continue
        
        # retrieve the whitespace at the beginning of the line for later use
        # as padding
        ws = ''
        for char in line:
            if char.isspace():
                ws += char
            else:
                break
            
        # iterate over all characters in the string keeping track of where the
        # last valid break character was found and wrapping the line
        # accordingly
        lastsplit = 0
        nextsplit = -1
        splitct = 0
        pos = 0
        for char in line:
            if splitchars.find(char) >= 0:
                # a new split position has been found
                nextsplit = pos
            size = pos - lastsplit + 1
            if splitct > 0:
                size += len(ws) + len(indent)
            if size >= maxchars:
                # the line is too long
                section = line[lastsplit:nextsplit+1]
                if len(section) > 0:
                    # output the line portion between the last split and the
                    # next split
                    if splitct > 0:
                        # start a new line and trim the line section
                        output += '\n'+ws+indent
                        section = string.strip(section)
                    output += section
                    lastsplit = nextsplit + 1
                    splitct += 1
            pos += 1
        if len(line) - lastsplit > 0:
            # output the remainder of the line
            section = line[lastsplit:]
            if splitct > 0:
                # start a new line and trim the line section
                output += '\n'+ws+indent
                section = string.strip(section)
            output += section
        output += '\n'
    
    return output

def get_capi_name(cppname, isclassname, prefix = None):
    """ Convert a C++ CamelCaps name to a C API underscore name. """
    result = ''
    lastchr = ''
    for chr in cppname:
        # add an underscore if the current character is an upper case letter
        # and the last character was a lower case letter
        if len(result) > 0 and not chr.isdigit() \
            and string.upper(chr) == chr \
            and not string.upper(lastchr) == lastchr:
            result += '_'
        result += string.lower(chr)
        lastchr = chr
    
    if isclassname:
        result += '_t'
        
    if not prefix is None:
        if prefix[0:3] == 'cef':
            # if the prefix name is duplicated in the function name
            # remove that portion of the function name
            subprefix = prefix[3:]
            pos = result.find(subprefix)
            if pos >= 0:
                result = result[0:pos]+ result[pos+len(subprefix):]
        result = prefix+'_'+result
        
    return result

def get_prev_line(body, pos):
    """ Retrieve the start and end positions and value for the line immediately
    before the line containing the specified position.
    """
    end = string.rfind(body, '\n', 0, pos)
    start = body.rfind('\n', 0, end)+1
    line = body[start:end]
    return { 'start' : start, 'end' : end, 'line' : line }
    
def get_comment(body, name):
    """ Retrieve the comment for a class or function. """
    result = []
    
    pos = body.find(name)
    while pos > 0:
        data = get_prev_line(body, pos)
        line = string.strip(data['line'])
        pos = data['start']
        if len(line) == 0:
            # check if the next previous line is a comment
            prevdata = get_prev_line(body, pos)
            if string.strip(prevdata['line'])[0:2] == '//':
                result.append(None)
            else:
                break
        elif line[0:2] == '/*':
            continue
        elif line[0:2] == '//':
            # keep the comment line including any leading spaces
            result.append(line[2:])
        else:
            break
    
    result.reverse()
    return result

def format_comment(comment, indent, translate_map = None, maxchars = 80):
    """ Return the comments array as a formatted string. """
    result = ''
    wrapme = ''
    hasemptyline = False
    for line in comment:
        # if the line starts with a leading space, remove that space
        if not line is None and len(line) > 0 and line[0:1] == ' ':
            line = line[1:]
            didremovespace = True
        else:
            didremovespace = False

        if line is None or len(line) == 0 or line[0:1] == ' ' \
            or line[0:1] == '/':
            # the previous paragraph, if any, has ended
            if len(wrapme) > 0:
                if not translate_map is None:
                    # apply the translation
                    for key in translate_map.keys():
                        wrapme = wrapme.replace(key, translate_map[key])
                # output the previous paragraph
                result += wrap_text(wrapme, indent+'// ', maxchars)
                wrapme = ''
        
        if not line is None:
            if len(line) == 0 or line[0:1] == ' ' or line[0:1] == '/':
                # blank lines or anything that's further indented should be
                # output as-is
                result += indent+'//'
                if len(line) > 0:
                    if didremovespace:
                        result += ' '+line
                    else:
                        result += line;
                result += '\n'
            else:
                # add to the current paragraph
                wrapme += line+' '
        else:
            # output an empty line
            hasemptyline = True
            result += '\n'
            
    if len(wrapme) > 0:
        if not translate_map is None:
            # apply the translation
            for key in translate_map.keys():
                wrapme = wrapme.replace(key, translate_map[key])
        # output the previous paragraph
        result += wrap_text(wrapme, indent+'// ', maxchars)
        
    if hasemptyline:
        # an empty line means a break between comments, so the comment is
        # probably a section heading and should have an extra line before it
        result = '\n' + result
    return result

def format_translation_changes(old, new):
    """ Return a comment stating what is different between the old and new
    function prototype parts.
    """
    changed = False
    result = ''
    
    # normalize C API attributes
    oldargs = [x.replace('struct _', '') for x in old['args']]
    oldretval = old['retval'].replace('struct _', '')
    newargs = [x.replace('struct _', '') for x in new['args']]
    newretval = new['retval'].replace('struct _', '')
    
    # check if the prototype has changed
    oldset = set(oldargs)
    newset = set(newargs)
    if len(oldset.symmetric_difference(newset)) > 0:
        changed = True
        result += '\n  // WARNING - CHANGED ATTRIBUTES'
        
        # in the implementation set only
        oldonly = oldset.difference(newset)
        for arg in oldonly:
            result += '\n  //   REMOVED: '+arg
        
        # in the current set only
        newonly = newset.difference(oldset)
        for arg in newonly:
            result += '\n  //   ADDED:   '+arg
    
    # check if the return value has changed
    if oldretval != newretval:
        changed = True
        result += '\n  // WARNING - CHANGED RETURN VALUE'+ \
                  '\n  //   WAS: '+old['retval']+ \
                  '\n  //   NOW: '+new['retval']
        
    if changed:
        result += '\n  #pragma message("Warning: "__FILE__": '+new['name']+ \
                  ' prototype has changed")\n'
    
    return result

def format_translation_includes(body):
    """ Return the necessary list of includes based on the contents of the
    body.
    """
    result = ''
    
    # identify what CppToC classes are being used
    p = re.compile('([A-Za-z0-9_]{1,})CppToC')
    list = sorted(set(p.findall(body)))
    for item in list:
        result += '#include "libcef_dll/cpptoc/'+ \
                  get_capi_name(item[3:], False)+'_cpptoc.h"\n'
    
    # identify what CToCpp classes are being used
    p = re.compile('([A-Za-z0-9_]{1,})CToCpp')
    list = sorted(set(p.findall(body)))
    for item in list:
        result += '#include "libcef_dll/ctocpp/'+ \
                  get_capi_name(item[3:], False)+'_ctocpp.h"\n'
    
    if body.find('transfer_') > 0:
        result += '#include "libcef_dll/transfer_util.h"\n'
        
    return result

def str_to_dict(str):
    """ Convert a string to a dictionary. """
    dict = {}
    parts = string.split(str, ',')
    for part in parts:
        part = string.strip(part)
        if len(part) == 0:
            continue
        sparts = string.split(part, '=')
        if len(sparts) != 2:
            raise Exception('Invalid dictionary pair format: '+part)
        dict[string.strip(sparts[0])] = string.strip(sparts[1])
    return dict

def dict_to_str(dict):
    """ Convert a dictionary to a string. """
    str = []
    for name in dict.keys():
        str.append(name+'='+dict[name])
    return string.join(str, ',')


# regex for matching comment-formatted attributes
_cre_attrib = '/\*--cef\(([A-Za-z0-9_ ,=]{0,})\)--\*/'
# regex for matching class and function names
_cre_cfname = '([A-Za-z0-9_]{1,})'
# regex for matching typedef values and function return values
_cre_retval = '([A-Za-z0-9_<>:,\*\&]{1,})'
# regex for matching function return value and name combination
_cre_func   = '([A-Za-z][A-Za-z0-9_<>:,\*\& ]{1,})'
# regex for matching virtual function modifiers
_cre_vfmod = '([A-Za-z0-9_]{0,})'
# regex for matching arbitrary whitespace
_cre_space =  '[\s]{1,}'


def get_function_impls(content, ident):
    """ Retrieve the function parts from the specified contents as a set of
    return value, name, arguments and body. Ident must occur somewhere in
    the value.
    """
    # extract the functions
    p = re.compile('\n'+_cre_func+'\((.*?)\)([A-Za-z0-9_\s]{0,})'+
                   '\{(.*?)\n\}',
                   re.MULTILINE | re.DOTALL)
    list = p.findall(content)

    # build the function map with the function name as the key
    result = []
    for retval, argval, vfmod, body in list:
        if retval.find(ident) < 0:
            # the identifier was not found
            continue
        
        # remove the identifier
        retval = string.replace(retval, ident, '')
        retval = string.strip(retval)
        
        # retrieve the function name
        parts = string.split(retval, ' ')
        name = parts[-1]
        del parts[-1]
        retval = string.join(parts, ' ')
        
        # parse the arguments
        args = []
        for v in string.split(argval, ','):
            v = string.strip(v)
            if len(v) > 0:
                args.append(v)
        
        result.append({
            'retval' : string.strip(retval),
            'name' : name,
            'args' : args,
            'vfmod' : string.strip(vfmod),
            'body' : body
        })
    
    return result

def get_next_function_impl(existing, name):
    result = None
    for item in existing:
        if item['name'] == name:
            result = item
            existing.remove(item)
            break
    return result

class obj_header:
    """ Class representing a C++ header file. """
    
    def __init__(self, filename):
        self.filename = filename;
        
        # read the input file into memory
        data = read_file(filename)
        
        # remove space from between template definition end brackets
        data = data.replace("> >", ">>")
        
        # extract global typedefs
        p = re.compile('\ntypedef'+_cre_space+_cre_retval+
                       _cre_space+_cre_cfname+';',
                       re.MULTILINE | re.DOTALL)
        list = p.findall(data)
        
        # build the global typedef objects
        self.typedefs = []
        for value, alias in list:
            self.typedefs.append(obj_typedef(self, value, alias))
            
        # extract global functions
        p = re.compile('\n'+_cre_attrib+'\n'+_cre_func+'\((.*?)\)',
                       re.MULTILINE | re.DOTALL)
        list = p.findall(data)
        
        # build the global function objects
        self.funcs = []
        for attrib, retval, argval in list:
            comment = get_comment(data, retval+'('+argval+');')
            self.funcs.append(obj_function(self, attrib, retval, argval,
                                           comment))
        
        # extract classes
        p = re.compile('\n'+_cre_attrib+
                       '\nclass'+_cre_space+_cre_cfname+_cre_space+
                       ':'+_cre_space+'public'+_cre_space+'virtual'+
                       _cre_space+'CefBase'+
                       '\n{(.*?)};', re.MULTILINE | re.DOTALL)
        list = p.findall(data)
        
        # build the class objects
        self.classes = []
        for attrib, name, body in list:
            comment = get_comment(data, name+' : public virtual CefBase')
            self.classes.append(
                obj_class(self, attrib, name, body, comment))

    def __repr__(self):
        result = ''
        
        if len(self.typedefs) > 0:
            strlist = []
            for cls in self.typedefs:
                strlist.append(str(cls))
            result += string.join(strlist, "\n") + "\n\n"
            
        if len(self.funcs) > 0:
            strlist = []
            for cls in self.funcs:
                strlist.append(str(cls))
            result += string.join(strlist, "\n") + "\n\n"
            
        if len(self.classes) > 0:
            strlist = []
            for cls in self.classes:
                strlist.append(str(cls))
            result += string.join(strlist, "\n")
        
        return result
    
    def get_file_name(self):
        """ Return the file name. """
        return self.filename
    
    def get_typedefs(self):
        """ Return the array of typedef objects. """
        return self.typedefs
    
    def get_funcs(self):
        """ Return the array of function objects. """
        return self.funcs
    
    def get_classes(self):
        """ Return the array of class objects. """
        return self.classes
    
    def get_class(self, classname, defined_structs = None):
        """ Return the specified class or None if not found. """
        for cls in self.classes:
            if cls.get_name() == classname:
                return cls
            elif not defined_structs is None:
                defined_structs.append(cls.get_capi_name())
        return None
    
    def get_class_names(self):
        """ Returns the names of all classes in this object. """
        result = []
        for cls in self.classes:
            result.append(cls.get_name())
        return result
    
    def get_types(self, list):
        """ Return a dictionary mapping data types to analyzed values. """
        for cls in self.typedefs:
            cls.get_types(list)
            
        for cls in self.classes:
            cls.get_types(list)
            
    def get_alias_translation(self, alias):
        """ Return a translation of alias to value based on typedef
            statements. """
        for cls in self.typedefs:
            if cls.alias == alias:
                return cls.value
        return None
    
    def get_analysis(self, value, named = True):
        """ Return an analysis of the value based the header file context. """
        return obj_analysis([self], value, named)
    
    def get_defined_structs(self):
        """ Return a list of names already defined structure names. """
        return ['cef_print_info_t', 'cef_window_info_t',
                'cef_handler_menuinfo_t', 'cef_base_t']
    
    def get_capi_translations(self):
        """ Return a dictionary that maps C++ terminology to C API terminology.
        """
        # strings that will be changed in C++ comments
        map = {
            'class' : 'structure',
            'Class' : 'Structure',
            'interface' : 'structure',
            'Interface' : 'Structure',
            'true' : 'true (1)',
            'false' : 'false (0)',
            'empty' : 'NULL',
            'method' : 'function'
        }
        
        # add mappings for all classes and functions
        funcs = self.get_funcs()
        for func in funcs:
            map[func.get_name()+'()'] = func.get_capi_name()+'()'
            
        classes = self.get_classes()
        for cls in classes:
            map[cls.get_name()] = cls.get_capi_name()
            
            funcs = cls.get_virtual_funcs()
            for func in funcs:
                map[func.get_name()+'()'] = func.get_capi_name()+'()'
                
            funcs = cls.get_static_funcs()
            for func in funcs:
                map[func.get_name()+'()'] = func.get_capi_name()+'()'
        
        return map


class obj_class:
    """ Class representing a C++ class. """
    
    def __init__(self, parent, attrib, name, body, comment):
        if not isinstance(parent, obj_header):
            raise Exception('Invalid parent object type')
        
        self.parent = parent
        self.attribs = str_to_dict(attrib)
        self.name = name
        self.comment = comment
        
        # extract typedefs
        p = re.compile('\n'+_cre_space+'typedef'+_cre_space+_cre_retval+
                       _cre_space+_cre_cfname+';',
                       re.MULTILINE | re.DOTALL)
        list = p.findall(body)
        
        # build the typedef objects
        self.typedefs = []
        for value, alias in list:
            self.typedefs.append(obj_typedef(self, value, alias))
            
        # extract static functions
        p = re.compile('\n'+_cre_space+_cre_attrib+'\n'+_cre_space+'static'+
                       _cre_space+_cre_func+'\((.*?)\)',
                       re.MULTILINE | re.DOTALL)
        list = p.findall(body)
        
        # build the static function objects
        self.staticfuncs = []
        for attrib, retval, argval in list:
            comment = get_comment(body, retval+'('+argval+')')
            self.staticfuncs.append(
                obj_function_static(self, attrib, retval, argval, comment))
        
        # extract virtual functions
        p = re.compile('\n'+_cre_space+_cre_attrib+'\n'+_cre_space+'virtual'+
                       _cre_space+_cre_func+'\((.*?)\)'+_cre_space+_cre_vfmod,
                       re.MULTILINE | re.DOTALL)
        list = p.findall(body)
        
        # build the virtual function objects
        self.virtualfuncs = []
        for attrib, retval, argval, vfmod in list:
            comment = get_comment(body, retval+'('+argval+')')
            self.virtualfuncs.append(
                obj_function_virtual(self, attrib, retval, argval, comment,
                                     vfmod))
        
    def __repr__(self):
        result = '/* '+dict_to_str(self.attribs)+' */ class '+self.name+"\n{"
        
        if len(self.typedefs) > 0:
            result += "\n\t"
            strlist = []
            for cls in self.typedefs:
                strlist.append(str(cls))
            result += string.join(strlist, "\n\t")
            
        if len(self.staticfuncs) > 0:
            result += "\n\t"
            strlist = []
            for cls in self.staticfuncs:
                strlist.append(str(cls))
            result += string.join(strlist, "\n\t")
            
        if len(self.virtualfuncs) > 0:
            result += "\n\t"
            strlist = []
            for cls in self.virtualfuncs:
                strlist.append(str(cls))
            result += string.join(strlist, "\n\t")
        
        result += "\n};\n"
        return result
    
    def get_name(self):
        """ Return the class name. """
        return self.name;
    
    def get_capi_name(self):
        """ Return the CAPI structure name for this class. """
        return get_capi_name(self.name, True)
    
    def get_comment(self):
        """ Return the class comment as an array of lines. """
        return self.comment
    
    def get_attribs(self):
        """ Return the class attributes as a dictionary. """
        return self.attribs;
    
    def get_typedefs(self):
        """ Return the array of typedef objects. """
        return self.typedefs;
    
    def get_static_funcs(self):
        """ Return the array of static function objects. """
        return self.staticfuncs;
    
    def get_virtual_funcs(self):
        """ Return the array of virtual function objects. """
        return self.virtualfuncs;
    
    def get_types(self, list):
        """ Return a dictionary mapping data types to analyzed values. """
        for cls in self.typedefs:
            cls.get_types(list)
            
        for cls in self.staticfuncs:
            cls.get_types(list)
            
        for cls in self.virtualfuncs:
            cls.get_types(list)
    
    def get_alias_translation(self, alias):
        for cls in self.typedefs:
            if cls.alias == alias:
                return cls.value
        return None
    
    def get_analysis(self, value, named = True):
        """ Return an analysis of the value based on the class definition
        context.
        """
        return obj_analysis([self, self.parent], value, named)
    
    def is_library_side(self):
        """ Returns true if the class is implemented by the library. """
        return self.attribs['source'] == 'library'

    def is_client_side(self):
        """ Returns true if the class is implemented by the client. """
        return self.attribs['source'] == 'client'


class obj_typedef:
    """ Class representing a typedef statement. """
    
    def __init__(self, parent, value, alias):
        if not isinstance(parent, obj_header) \
            and not isinstance(parent, obj_class):
            raise Exception('Invalid parent object type')
        
        self.parent = parent
        self.alias = alias
        self.value = self.parent.get_analysis(value, False)

    def __repr__(self):
        return 'typedef '+self.value.get_type()+' '+self.alias+';'
    
    def get_alias(self):
        """ Return the alias. """
        return self.alias
    
    def get_value(self):
        """ Return an analysis of the value based on the class or header file
        definition context.
        """
        return self.value
       
    def get_types(self, list):
        """ Return a dictionary mapping data types to analyzed values. """
        name = self.value.get_type()
        if not name in list:
            list[name] = self.value;
    
    
class obj_function:
    """ Class representing a function. """
    
    def __init__(self, parent, attrib, retval, argval, comment):
        self.parent = parent
        self.attribs = str_to_dict(attrib)
        self.retval = obj_argument(self, retval)
        self.name = self.retval.remove_name()
        self.comment = comment
        
        # build the argument objects
        self.arguments = []
        arglist = string.split(argval, ',')
        for arg in arglist:
            arg = string.strip(arg)
            if len(arg) > 0:
                self.arguments.append(obj_argument(self, arg))

    def __repr__(self):
        return '/* '+dict_to_str(self.attribs)+' */ '+self.get_cpp_proto()
    
    def get_name(self):
        """ Return the function name. """
        return self.name
    
    def get_capi_name(self, prefix = None):
        """ Return the CAPI function name. """
        if 'capi_name' in self.attribs:
            return self.attribs['capi_name']
        return get_capi_name(self.name, False, prefix)
    
    def get_comment(self):
        """ Return the function comment as an array of lines. """
        return self.comment
    
    def get_attribs(self):
        """ Return the function attributes as a dictionary. """
        return self.attribs
    
    def get_retval(self):
        """ Return the return value object. """
        return self.retval
    
    def get_arguments(self):
        """ Return the argument array. """
        return self.arguments
    
    def get_types(self, list):
        """ Return a dictionary mapping data types to analyzed values. """
        for cls in self.arguments:
            cls.get_types(list)
            
    def get_capi_parts(self, defined_structs = [], prefix = None):
        """ Return the parts of the C API function definition. """
        retval = ''
        dict = self.retval.get_type().get_capi(defined_structs)
        if dict['format'] == 'single':
            retval = dict['value']
            
        name = self.get_capi_name(prefix)
        args = []
        
        if isinstance(self, obj_function_virtual):
            # virtual functions get themselves as the first argument
            str = 'struct _'+self.parent.get_capi_name()+'* self'
            if isinstance(self, obj_function_virtual) and self.is_const():
                # const virtual functions get const self pointers
                str = 'const '+str
            args.append(str)
        
        if len(self.arguments) > 0:
            for cls in self.arguments:
                type = cls.get_type()
                dict = type.get_capi(defined_structs)
                if dict['format'] == 'single':
                    args.append(dict['value'])
                elif dict['format'] == 'multi-arg':
                    # add an additional argument for the size of the array
                    type = type.get_name()
                    if type[-1] == 's':
                        type = type[:-1]
                    args.append('size_t '+type+'Count')
                    args.append(dict['value'])
                elif dict['format'] == 'multi-func':
                    # change the function to return one value of the
                    # required type based on an index parameter
                    type = type.get_name()
                    if type[-1] == 's':
                        type = type[:-1]
                    args.append('int '+type+'Index')
                    retval = dict['value']
        
        return { 'retval' : retval, 'name' : name, 'args' : args }

    def get_capi_proto(self, defined_structs = [], prefix = None):
        """ Return the prototype of the C API function. """
        parts = self.get_capi_parts(defined_structs, prefix)
        result = parts['retval']+' '+parts['name']+ \
                 '('+string.join(parts['args'], ', ')+')'
        return result
               
    def get_cpp_parts(self, isimpl = False):
        """ Return the parts of the C++ function definition. """
        retval = str(self.retval)
        name = self.name
        
        args = []
        if len(self.arguments) > 0:
            for cls in self.arguments:
                args.append(str(cls))
                
        if isimpl and isinstance(self, obj_function_virtual):
            # enumeration return values must be qualified with the class name
            type = self.get_retval().get_type()
            if type.is_result_struct() and not type.is_byref() \
                and not type.is_byaddr():
                retval = self.parent.get_name()+'::'+retval
        
        return { 'retval' : retval, 'name' : name, 'args' : args }
        
    def get_cpp_proto(self, classname = None):
        """ Return the prototype of the C++ function. """
        parts = self.get_cpp_parts()
        result = parts['retval']+' '
        if not classname is None:
            result += classname+'::'
        result += parts['name']+'('+string.join(parts['args'], ', ')+')'
        if isinstance(self, obj_function_virtual) and self.is_const():
            result += ' const'
        return result


class obj_function_static(obj_function):
    """ Class representing a static function. """
    
    def __init__(self, parent, attrib, retval, argval, comment):
        if not isinstance(parent, obj_class):
            raise Exception('Invalid parent object type')
        obj_function.__init__(self, parent, attrib, retval, argval, comment)

    def __repr__(self):
        return 'static '+obj_function.__repr__(self)+';'

    def get_capi_name(self, prefix = None):
        """ Return the CAPI function name. """
        if prefix is None:
            # by default static functions are prefixed with the class name
            prefix = get_capi_name(self.parent.get_name(), False)
        return obj_function.get_capi_name(self, prefix)
        
class obj_function_virtual(obj_function):
    """ Class representing a virtual function. """
    
    def __init__(self, parent, attrib, retval, argval, comment, vfmod):
        if not isinstance(parent, obj_class):
            raise Exception('Invalid parent object type')
        obj_function.__init__(self, parent, attrib, retval, argval, comment)
        if vfmod == 'const':
            self.isconst = True
        else:
            self.isconst = False
    
    def __repr__(self):
        return 'virtual '+obj_function.__repr__(self)+';'
    
    def is_const(self):
        """ Returns true if the method declaration is const. """
        return self.isconst


class obj_argument:
    """ Class representing a function argument. """
    
    def __init__(self, parent, argval):
        if not isinstance(parent, obj_function):
            raise Exception('Invalid parent object type')
        
        self.parent = parent
        self.type = self.parent.parent.get_analysis(argval)
        
    def __repr__(self):
        result = ''
        if self.type.is_const():
            result += 'const '
        result += self.type.get_type()
        if self.type.is_byref():
            result += '&'
        elif self.type.is_byaddr():
            result += '*'
        if self.type.has_name():
            result += ' '+self.type.get_name()
        return result
    
    def remove_name(self):
        """ Remove and return the name value. """
        name = self.type.get_name()
        self.type.name = None
        return name
 
    def get_type(self):
        """ Return an analysis of the argument type based on the class
        definition context.
        """
        return self.type

    def get_types(self, list):
        """ Return a dictionary mapping data types to analyzed values. """
        name = self.type.get_type()
        if not name in list:
            list[name] = self.type
            
    
class obj_analysis:
    """ Class representing an analysis of a data type value. """
    
    def __init__(self, scopelist, value, named):
        self.value = value
        self.result_type = 'unknown'
        self.result_value = None
        
        # parse the argument string
        partlist = string.split(string.strip(value))
        
        if named == True:
            # extract the name value
            self.name = partlist[-1]
            del partlist[-1]
        else:
            self.name = None
            
        if len(partlist) == 0:
            raise Exception('Invalid argument value: '+value)
        
        # check const status
        if partlist[0] == 'const':
            self.isconst = True
            del partlist[0]
        else:
            self.isconst = False
            
        # combine the data type
        self.type = string.join(partlist, ' ')
            
        # extract the last character of the data type
        endchar = self.type[-1]
        
        # check if the value is passed by reference
        if endchar == '&':
            self.isbyref = True
            self.type = self.type[:-1]
        else:
            self.isbyref = False
        
        # check if the value is passed by address
        if endchar == '*':
            self.isbyaddr = True
            self.type = self.type[:-1]
        else:
            self.isbyaddr = False
        
        # see if the value is directly identifiable
        if self._check_advanced(self.type) == True:
            return
        
        # not identifiable, so look it up
        translation = None
        for scope in scopelist:
            if not isinstance(scope, obj_header) \
                and not isinstance(scope, obj_class):
                raise Exception('Invalid scope object type')
            translation = scope.get_alias_translation(self.type)
            if not translation is None:
                break
            
        if translation is None:
            raise Exception('Failed to translate type: '+self.type)
        
        # the translation succeeded so keep the result
        self.result_type = translation.result_type
        self.result_value = translation.result_value
        
    def _check_advanced(self, value):
        # check for vectors
        if value.find('std::vector') == 0:
            self.result_type = 'vector'
            val = value[12:-1]
            self.result_value = [
                self._get_basic(val)
            ]
            return True
            
        # check for maps
        if value.find('std::map') == 0:
            self.result_type = 'map'
            vals = string.split(value[9:-1], ',')
            if len(vals) == 2:
                self.result_value = [
                    self._get_basic(string.strip(vals[0])),
                    self._get_basic(string.strip(vals[1]))
                ]
                return True
        
        # check for basic types
        basic = self._get_basic(value)
        if not basic is None:
            self.result_type = basic['result_type']
            self.result_value = basic['result_value']
            return True
        
        return False
        
    def _get_basic(self, value):
        # check for string values
        if value == "CefString":
            return {
                'result_type' : 'string',
                'result_value' : None
            }
        
        # check for simple direct translations
        simpletypes = {
            'void' : 'void',
            'int' : 'int',
            'int64' : 'int64',
            'uint64' : 'uint64',
            'double' : 'double',
            'long' : 'long',
            'unsigned long' : 'unsigned long',
            'size_t' : 'size_t',
            'time_t' : 'time_t',
            'bool' : 'int',
            'CefCursorHandle' : 'cef_cursor_handle_t',
            'CefWindowHandle' : 'cef_window_handle_t',
            'CefRect' : 'cef_rect_t',
            'CefThreadId' : 'cef_thread_id_t',
            'CefTime' : 'cef_time_t',
        }
        if value in simpletypes.keys():
            return {
                'result_type' : 'simple',
                'result_value' : simpletypes[value]
            }
        
        # check if already a C API structure
        if value[-2:] == '_t':
            return {
                'result_type' : 'structure',
                'result_value' : value
            }
        
        # check for CEF reference pointers
        p = re.compile('^CefRefPtr<(.*?)>$', re.DOTALL)
        list = p.findall(value)
        if len(list) == 1:
            return {
                'result_type' : 'refptr',
                'result_value' : get_capi_name(list[0], True)+'*'
            }

        # check for CEF structure types
        if value[0:3] == 'Cef' and value[-4:] != 'List':
            return {
                'result_type' : 'structure',
                'result_value' : get_capi_name(value, True)
            }
        
        return None
        
    def __repr__(self):
        return '('+self.result_type+') '+str(self.result_value)
    
    def has_name(self):
        """ Returns true if a name value exists. """
        return (not self.name is None)
    
    def get_name(self):
        """ Return the name. """
        return self.name
    
    def get_value(self):
        """ Return the name. """
        return self.value
    
    def get_type(self):
        """ Return the type. """
        return self.type
    
    def is_const(self):
        """ Returns true if the argument value is constant. """
        return self.isconst
    
    def is_byref(self):
        """ Returns true if the argument is passed by reference. """
        return self.isbyref
    
    def is_byaddr(self):
        """ Returns true if the argument is passed by address. """
        return self.isbyaddr
    
    def is_result_simple(self):
        """ Returns true if this is a simple argument type. """
        return (self.result_type == 'simple')
    
    def get_result_simple_type(self):
        """ Return the simple type. """
        result = ''
        if self.is_const():
            result += 'const '
        result += self.result_value
        if self.is_byaddr() or self.is_byref():
            result += '*'
        return result
    
    def is_result_refptr(self):
        """ Returns true if this is a reference pointer type. """
        return (self.result_type == 'refptr')
    
    def get_result_refptr_type(self, defined_structs = []):
        """ Return the refptr type. """
        result = ''
        if not self.result_value[:-1] in defined_structs:
            result += 'struct _'
        result += self.result_value
        if self.is_byref() or self.is_byaddr():
            result += '*'
        return result
    
    def is_result_struct(self):
        """ Returns true if this is a structure type. """
        return (self.result_type == 'structure')
    
    def get_result_struct_type(self, defined_structs = []):
        """ Return the structure or enumeration type. """
        result = ''
        # structure values that are passed by reference or address must be
        # structures and not enumerations
        if self.is_byref() or self.is_byaddr():
            if self.is_const():
                result += 'const '
            if not self.result_value in defined_structs:
                result += 'struct _'
        else:
            result += 'enum '
        result += self.result_value
        if self.is_byref() or self.is_byaddr():
            result += '*'
        return result
    
    def is_result_string(self):
        """ Returns true if this is a string type. """
        return (self.result_type == 'string')
    
    def get_result_string_type(self):
        """ Return the string type. """
        if not self.has_name():
            # Return values are string structs that the user must free. Use
            # the name of the structure as a hint.
            return 'cef_string_userfree_t'
        elif not self.is_const() and (self.is_byref() or self.is_byaddr()):
            # Parameters passed by reference or address. Use the normal
            # non-const string struct.
            return 'cef_string_t*'
        # Const parameters use the const string struct.
        return 'const cef_string_t*'

    def is_result_vector(self):
        """ Returns true if this is a vector type. """
        return (self.result_type == 'vector')
    
    def get_result_vector_type(self, defined_structs = []):
        """ Return the vector type. """
        if not self.has_name():
            raise Exception('Cannot use vector as a return type')
        
        type = self.result_value[0]['result_type']
        value = self.result_value[0]['result_value']
        
        result = {}
        if type == 'string':
            result['value'] = 'cef_string_list_t'
            result['format'] = 'single'
            return result
        
        if type == 'simple':
            result['value'] = value
        elif type == 'refptr':
            str = ''
            if not value[:-1] in defined_structs:
                str += 'struct _'
            str += value
            if self.is_const():
                str += ' const*'
            result['value'] = str
        else:
            raise Exception('Unsupported vector type: '+type)
        
        if self.is_const():
            # const vector values must be passed as the value array parameter
            # and a size parameter
            result['format'] = 'multi-arg'
        else:
            # non-const vector values must be passed as one function to get the
            # size and another function to get the element at a specified index
            result['format'] = 'multi-func'
        return result
    
    def is_result_map(self):
        """ Returns true if this is a map type. """
        return (self.result_type == 'map')
    
    def get_result_map_type(self, defined_structs = []):
        """ Return the map type. """
        if not self.has_name():
            raise Exception('Cannot use map as a return type')
        if self.result_value[0]['result_type'] == 'string' \
            and self.result_value[1]['result_type'] == 'string':
            return {
                'value' : 'cef_string_map_t',
                'format' : 'single'
            }
        raise Exception('Only mappings of strings to strings are supported')

    def get_capi(self, defined_structs = []):
        """ Format the value for the C API. """
        result = ''
        format = 'single'
        if self.is_result_simple():
            result += self.get_result_simple_type()
        elif self.is_result_refptr():
            result += self.get_result_refptr_type(defined_structs)
        elif self.is_result_struct():
            result += self.get_result_struct_type(defined_structs)
        elif self.is_result_string():
            result += self.get_result_string_type()
        elif self.is_result_map():
            resdict = self.get_result_map_type(defined_structs)
            if resdict['format'] == 'single':
                result += resdict['value']
            else:
                raise Exception('Only single-value map types are supported')
        elif self.is_result_vector():
            resdict = self.get_result_vector_type(defined_structs)
            if resdict['format'] != 'single':
                format = resdict['format']
            result += resdict['value']
        
        if self.has_name() and format != 'multi-func':
            result += ' '+self.get_name();
        
        return {'format' : format, 'value' : result}


# test the module
if __name__ == "__main__":
    import pprint
    import sys
    
    # verify that the correct number of command-line arguments are provided
    if len(sys.argv) != 2:
        sys.stderr.write('Usage: '+sys.argv[0]+' <infile>')
        sys.exit()
        
    pp = pprint.PrettyPrinter(indent=4)
    
    # create the header object
    header = obj_header(sys.argv[1])
    
    # output the type mapping
    types = {}
    header.get_types(types)
    pp.pprint(types)
    sys.stdout.write('\n')
    
    # output the parsed C++ data
    sys.stdout.write(wrap_code(str(header), '\t'))
    
    # output the C API formatted data
    defined_names = header.get_defined_structs()
    result = ''
    
    # global functions
    funcs = header.get_funcs()
    if len(funcs) > 0:
        for func in funcs:
            result += func.get_capi_proto(defined_names)+';\n'
        result += '\n'
        
    classes = header.get_classes()
    for cls in classes:
        # virtual functions are inside a structure
        result += 'struct '+cls.get_capi_name()+'\n{\n'
        funcs = cls.get_virtual_funcs()
        if len(funcs) > 0:
            for func in funcs:
                result += '\t'+func.get_capi_proto(defined_names)+';\n'
        result += '}\n\n'
        
        defined_names.append(cls.get_capi_name())
        
        # static functions become global
        funcs = cls.get_static_funcs()
        if len(funcs) > 0:
            for func in funcs:
                result += func.get_capi_proto(defined_names)+';\n'
            result += '\n'
    sys.stdout.write(wrap_code(result, '\t'))
