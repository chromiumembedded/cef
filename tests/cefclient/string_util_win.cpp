// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "string_util.h"

#ifdef _WIN32

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

#endif // _WIN32
