// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "string_util.h"
#include <sstream>


std::wstring StringToWString(const std::string& s)
{
	wchar_t* wch;
	UINT bytes = MultiByteToWideChar(CP_ACP, 0, s.c_str(), s.size()+1, NULL, 0);
	wch  = new wchar_t[bytes];
	if(wch)
		bytes = MultiByteToWideChar(CP_ACP, 0, s.c_str(), s.size()+1, wch, bytes);
  std::wstring str = wch;
	delete [] wch;
  return str;
}

std::string WStringToString(const std::wstring& s)
{
	char* ch;
	UINT bytes = WideCharToMultiByte(CP_ACP, 0, s.c_str(), s.size()+1, NULL, 0,
                                   NULL, NULL); 
	ch = new char[bytes];
	if(ch)
		bytes = WideCharToMultiByte(CP_ACP, 0, s.c_str(), s.size()+1, ch, bytes,
                                NULL, NULL);
	std::string str = ch;
	delete [] ch;
  return str;
}

void DumpRequestContents(CefRefPtr<CefRequest> request, std::wstring& str)
{
  std::wstringstream ss;

  ss << L"URL: " << request->GetURL();
  ss << L"\nMethod: " << request->GetMethod();

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if(headerMap.size() > 0) {
    ss << L"\nHeaders:";
    CefRequest::HeaderMap::const_iterator it = headerMap.begin();
    for(; it != headerMap.end(); ++it) {
      ss << L"\n\t" << (*it).first << L": " << (*it).second;
    }
  }

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if(postData.get()) {
    CefPostData::ElementVector elements;
    postData->GetElements(elements);
    if(elements.size() > 0) {
      ss << L"\nPost Data:";
      CefRefPtr<CefPostDataElement> element;
      CefPostData::ElementVector::const_iterator it = elements.begin();
      for(; it != elements.end(); ++it) {
        element = (*it);
        if(element->GetType() == PDE_TYPE_BYTES) {
          // the element is composed of bytes
          ss << L"\n\tBytes: ";
          if(element->GetBytesCount() == 0)
            ss << L"(empty)";
          else {
            // retrieve the data.
            size_t size = element->GetBytesCount();
            char* bytes = new char[size];
            element->GetBytes(size, bytes);
            ss << StringToWString(std::string(bytes, size));
            delete [] bytes;
          }
        } else if(element->GetType() == PDE_TYPE_FILE) {
          ss << L"\n\tFile: " << element->GetFile();
        }
      }
    }
  }

  str = ss.str();
}

std::wstring StringReplace(const std::wstring& str, const std::wstring& from,
                           const std::wstring& to)
{
  std::wstring result = str;
  std::wstring::size_type pos = 0;
  std::wstring::size_type from_len = from.length();
  std::wstring::size_type to_len = to.length();
  do {
    pos = result.find(from, pos);
    if(pos != std::wstring::npos) {
      result.replace(pos, from_len, to);
      pos += to_len;
    }
  } while(pos != std::wstring::npos);
  return result;
}
