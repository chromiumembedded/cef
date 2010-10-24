// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "resource_util.h"

bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes)
{
  extern HINSTANCE hInst;
	HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(binaryId),
                            MAKEINTRESOURCE(256));
	if(hRes)
	{
		HGLOBAL hGlob = LoadResource(hInst, hRes);
		if(hGlob)
		{
			dwSize = SizeofResource(hInst, hRes);
			pBytes = (LPBYTE)LockResource(hGlob);
			if(dwSize > 0 && pBytes)
				return true;
		}
	}

	return false;
}
