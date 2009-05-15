// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/stream_cpptoc.h"


size_t CEF_CALLBACK stream_reader_read(struct _cef_stream_reader_t* stream,
                                       void *ptr, size_t size, size_t n)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamReaderCppToC::Struct* impl =
      reinterpret_cast<CefStreamReaderCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Read(ptr, size, n);
}

int CEF_CALLBACK stream_reader_seek(struct _cef_stream_reader_t* stream,
                                    long offset, int whence)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamReaderCppToC::Struct* impl =
      reinterpret_cast<CefStreamReaderCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Seek(offset, whence);
}

long CEF_CALLBACK stream_reader_tell(struct _cef_stream_reader_t* stream)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamReaderCppToC::Struct* impl =
      reinterpret_cast<CefStreamReaderCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Tell();
}

int CEF_CALLBACK stream_reader_eof(struct _cef_stream_reader_t* stream)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamReaderCppToC::Struct* impl =
      reinterpret_cast<CefStreamReaderCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Eof();
}


CefStreamReaderCppToC::CefStreamReaderCppToC(CefStreamReader* cls)
    : CefCppToC<CefStreamReader, cef_stream_reader_t>(cls)
{
  struct_.struct_.read = stream_reader_read;
  struct_.struct_.seek = stream_reader_seek;
  struct_.struct_.tell = stream_reader_tell;
  struct_.struct_.eof = stream_reader_eof;
}

#ifdef _DEBUG
long CefCppToC<CefStreamReader, cef_stream_reader_t>::DebugObjCt = 0;
#endif


size_t CEF_CALLBACK stream_writer_write(struct _cef_stream_writer_t* stream,
                                        const void *ptr, size_t size, size_t n)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamWriterCppToC::Struct* impl =
      reinterpret_cast<CefStreamWriterCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Write(ptr, size, n);
}

int CEF_CALLBACK stream_writer_seek(struct _cef_stream_writer_t* stream,
                                    long offset, int whence)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamWriterCppToC::Struct* impl =
      reinterpret_cast<CefStreamWriterCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Seek(offset, whence);
}

long CEF_CALLBACK stream_writer_tell(struct _cef_stream_writer_t* stream)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamWriterCppToC::Struct* impl =
      reinterpret_cast<CefStreamWriterCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Tell();
}

int CEF_CALLBACK stream_writer_flush(struct _cef_stream_writer_t* stream)
{
  DCHECK(stream);
  if(!stream)
    return 0;

  CefStreamWriterCppToC::Struct* impl =
      reinterpret_cast<CefStreamWriterCppToC::Struct*>(stream);
  return impl->class_->GetClass()->Flush();
}


CefStreamWriterCppToC::CefStreamWriterCppToC(CefStreamWriter* cls)
    : CefCppToC<CefStreamWriter, cef_stream_writer_t>(cls)
{
  struct_.struct_.write = stream_writer_write;
  struct_.struct_.seek = stream_writer_seek;
  struct_.struct_.tell = stream_writer_tell;
  struct_.struct_.flush = stream_writer_flush;
}

#ifdef _DEBUG
long CefCppToC<CefStreamWriter, cef_stream_writer_t>::DebugObjCt = 0;
#endif
