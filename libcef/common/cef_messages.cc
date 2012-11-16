// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/cef_messages.h"

namespace IPC {

// Extracted from chrome/common/automation_messages.cc.

// Only the net::UploadData ParamTraits<> definition needs this definition, so
// keep this in the implementation file so we can forward declare UploadData in
// the header.
template <>
struct ParamTraits<net::UploadElement> {
  typedef net::UploadElement param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    switch (p.type()) {
      case net::UploadElement::TYPE_BYTES: {
        m->WriteData(p.bytes(), static_cast<int>(p.bytes_length()));
        break;
      }
      default: {
        DCHECK(p.type() == net::UploadElement::TYPE_FILE);
        WriteParam(m, p.file_path());
        WriteParam(m, p.file_range_offset());
        WriteParam(m, p.file_range_length());
        WriteParam(m, p.expected_file_modification_time());
        break;
      }
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    switch (type) {
      case net::UploadElement::TYPE_BYTES: {
        const char* data;
        int len;
        if (!m->ReadData(iter, &data, &len))
          return false;
        r->SetToBytes(data, len);
        break;
      }
      default: {
        DCHECK(type == net::UploadElement::TYPE_FILE);
        FilePath file_path;
        uint64 offset, length;
        base::Time expected_modification_time;
        if (!ReadParam(m, iter, &file_path))
          return false;
        if (!ReadParam(m, iter, &offset))
          return false;
        if (!ReadParam(m, iter, &length))
          return false;
        if (!ReadParam(m, iter, &expected_modification_time))
          return false;
        r->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
        break;
      }
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadElement>");
  }
};

void ParamTraits<scoped_refptr<net::UploadData> >::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->is_chunked());
    WriteParam(m, p->last_chunk_appended());
  }
}

bool ParamTraits<scoped_refptr<net::UploadData> >::Read(const Message* m,
                                                        PickleIterator* iter,
                                                        param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  ScopedVector<net::UploadElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64 identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool is_chunked = false;
  if (!ReadParam(m, iter, &is_chunked))
    return false;
  bool last_chunk_appended = false;
  if (!ReadParam(m, iter, &last_chunk_appended))
    return false;
  *r = new net::UploadData;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_is_chunked(is_chunked);
  (*r)->set_last_chunk_appended(last_chunk_appended);
  return true;
}

void ParamTraits<scoped_refptr<net::UploadData> >::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<net::UploadData>");
}

}  // namespace IPC
