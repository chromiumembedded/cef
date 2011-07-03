// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlcore.h>
#include <atlbase.h>
#include <commdlg.h>

#include "base/compiler_specific.h"

#include "third_party/WebKit/Source/WebCore/config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "PlatformContextSkia.h"
MSVC_POP_WARNING();

#include "browser_webkit_glue.h"

#undef LOG
#include "base/logging.h"
#include "base/resource_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/gdi_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebView;

namespace webkit_glue {

string16 GetLocalizedString(int message_id) {
  // Localized resources are provided via webkit_resources.rc and
  // webkit_strings_en-US.rc.
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                message_id);
  if (!image) {
    NOTREACHED();
    return L"No string for this identifier!";
  }
  return string16(image->achString, image->nLength);
}

base::StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                         &data_size)
      ? base::StringPiece(static_cast<char*>(data_ptr), data_size)
      : base::StringPiece();
}

base::StringPiece NetResourceProvider(int key) {
  HMODULE hModule = ::GetModuleHandle(L"libcef.dll");
  if(!hModule)
    hModule = ::GetModuleHandle(NULL);
  return GetRawDataResource(hModule, key);
}

base::StringPiece GetDataResource(int resource_id) {
  return NetResourceProvider(resource_id);
}

bool EnsureFontLoaded(HFONT font) {
  return true;
}

void CaptureWebViewBitmap(HWND mainWnd, WebView* webview, HBITMAP& bitmap,
                          SIZE& size)
{
  WebKit::WebSize webSize = webview->size();
  size.cx = webSize.width;
  size.cy = webSize.height;
  
  skia::PlatformCanvas canvas(size.cx, size.cy, true);
  canvas.drawARGB(255, 255, 255, 255, SkXfermode::kSrc_Mode);
  WebCore::PlatformContextSkia context(&canvas);
  WebKit::WebRect rect(0, 0, size.cx, size.cy);
  webview->layout();
  webview->paint(&canvas, rect);

  HDC hRefDC = GetDC(mainWnd);
  HDC hDC = CreateCompatibleDC(hRefDC);
  bitmap = CreateCompatibleBitmap(hRefDC, size.cx, size.cy);
  DCHECK(bitmap != NULL);
  HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, bitmap);

  // Create a BMP v4 header that we can serialize.
  BITMAPV4HEADER bitmap_header;
  gfx::CreateBitmapV4Header(size.cx, size.cy, &bitmap_header);
  const SkBitmap& src_bmp = canvas.getDevice()->accessBitmap(true);
  SkAutoLockPixels src_lock(src_bmp);
	int retval = StretchDIBits(hDC,
                               0,
                               0,
                               size.cx, size.cy,
                               0, 0,
                               size.cx, size.cy,
                               src_bmp.getPixels(),
                               reinterpret_cast<BITMAPINFO*>(&bitmap_header),
                               DIB_RGB_COLORS,
                               SRCCOPY);
  DCHECK(retval != GDI_ERROR);

  SelectObject(hDC, hOldBmp);
  DeleteDC(hDC);
  ReleaseDC(mainWnd, hRefDC);
}


static PBITMAPINFO BmpCreateInfo(HBITMAP hBmp)
{ 
  BITMAP bmp; 
  PBITMAPINFO pbmi; 
  WORD cClrBits; 

  // Retrieve the bitmap color format, width, and height. 
  if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp)) {
    NOTREACHED();
    return NULL;
  }

  // Convert the color format to a count of bits. 
  cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel); 
  if (cClrBits == 1) {
    cClrBits = 1; 
  } else if (cClrBits <= 4) {
    cClrBits = 4; 
  } else if (cClrBits <= 8) {
    cClrBits = 8; 
  } else if (cClrBits <= 16) {
    cClrBits = 16; 
  } else if (cClrBits <= 24) {
    cClrBits = 24; 
  } else {
    cClrBits = 32; 
  }

  // Allocate memory for the BITMAPINFO structure. (This structure 
  // contains a BITMAPINFOHEADER structure and an array of RGBQUAD 
  // data structures.) 
  if (cClrBits != 24) {
    pbmi = (PBITMAPINFO) LocalAlloc(LPTR, 
      sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1<< cClrBits)); 
  } else { // There is no RGBQUAD array for the 24-bit-per-pixel format. 
    pbmi = (PBITMAPINFO) LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER)); 
  }
  
  // Initialize the fields in the BITMAPINFO structure. 
  pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
  pbmi->bmiHeader.biWidth = bmp.bmWidth; 
  pbmi->bmiHeader.biHeight = bmp.bmHeight; 
  pbmi->bmiHeader.biPlanes = bmp.bmPlanes; 
  pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel; 
  if (cClrBits < 24) {
    pbmi->bmiHeader.biClrUsed = (1<<cClrBits);
  }

  // If the bitmap is not compressed, set the BI_RGB flag. 
  pbmi->bmiHeader.biCompression = BI_RGB; 

  // Compute the number of bytes in the array of color 
  // indices and store the result in biSizeImage. 
  // For Windows NT, the width must be DWORD aligned unless 
  // the bitmap is RLE compressed. This example shows this. 
  // For Windows 95/98/Me, the width must be WORD aligned unless the 
  // bitmap is RLE compressed.
  pbmi->bmiHeader.biSizeImage =
    ((pbmi->bmiHeader.biWidth * cClrBits +31) & ~31) /8
    * pbmi->bmiHeader.biHeight; 
  
  // Set biClrImportant to 0, indicating that all of the 
  // device colors are important. 
  pbmi->bmiHeader.biClrImportant = 0; 
  return pbmi; 
} 

static BOOL BmpSaveFile(LPCTSTR pszFile, PBITMAPINFO pbi, HBITMAP hBMP,
                        HDC hDC, LPBYTE lpBits = NULL) 
{ 
  HANDLE hf = INVALID_HANDLE_VALUE; // file handle 
  BITMAPFILEHEADER hdr;             // bitmap file-header 
  PBITMAPINFOHEADER pbih;           // bitmap info-header 
  DWORD dwTotal;                    // total count of bytes 
  DWORD cb;                         // incremental count of bytes 
  BYTE *hp;                         // byte pointer 
  DWORD dwTmp; 
  BOOL ret = FALSE;
  BOOL bitsAlloc = FALSE;

  pbih = (PBITMAPINFOHEADER) pbi; 

  if(!lpBits) {
    // The bits have not been provided, so retrieve from the bitmap file
    lpBits = (LPBYTE) GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
    if (!lpBits) {
      // Memory could not be allocated
      NOTREACHED();
      return FALSE;
    }

    bitsAlloc = TRUE;

    // Retrieve the color table (RGBQUAD array) and the bits 
    // (array of palette indices) from the DIB. 
    if (!GetDIBits(hDC, hBMP, 0, (WORD) pbih->biHeight, lpBits, pbi,
      DIB_RGB_COLORS)) {
      NOTREACHED();
      goto end;
    }
  }

  // Create the bitmap file. 
  hf = CreateFile(pszFile, 
                  GENERIC_READ | GENERIC_WRITE, 
                  (DWORD) 0, 
                  NULL, 
                  CREATE_ALWAYS, 
                  FILE_ATTRIBUTE_NORMAL, 
                  (HANDLE) NULL);
  if (hf == INVALID_HANDLE_VALUE) {
    // Could not create the bitmap file
    NOTREACHED();
    goto end;
  }
  
  hdr.bfType = 0x4d42; // 0x42 = "B", 0x4d = "M"
  
  // Compute the size of the entire file. 
  hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + 
               pbih->biSize + pbih->biClrUsed 
               * sizeof(RGBQUAD) + pbih->biSizeImage); 
  hdr.bfReserved1 = 0; 
  hdr.bfReserved2 = 0; 

  // Compute the offset to the array of color indices. 
  hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + 
                  pbih->biSize + pbih->biClrUsed 
                  * sizeof (RGBQUAD); 

  // Copy the BITMAPFILEHEADER into the bitmap file. 
  if (!WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER), 
    (LPDWORD) &dwTmp,  NULL)) {
		// Could not write bitmap file header to file
    NOTREACHED();
		goto end;
  }

  // Copy the BITMAPINFOHEADER and RGBQUAD array into the file. 
  if (!WriteFile(hf, (LPVOID) pbih, sizeof(BITMAPINFOHEADER) 
                + pbih->biClrUsed * sizeof (RGBQUAD), 
                (LPDWORD) &dwTmp, NULL)) {
		// Could not write bitmap info header to file
    NOTREACHED();
    goto end;
  }

  // Copy the array of color indices into the .BMP file. 
  dwTotal = cb = pbih->biSizeImage; 
  hp = lpBits; 
  if (!WriteFile(hf, (LPSTR) hp, (int) cb, (LPDWORD) &dwTmp,NULL)) {
    // Could not write bitmap data to file
    NOTREACHED();
    goto end;
  }

  ret = TRUE;

end:
  // Close the bitmap file. 
  if(hf != INVALID_HANDLE_VALUE) {
    CloseHandle(hf);
    if(!ret)
      DeleteFile(pszFile);
  }

  if(bitsAlloc)
  {
    // Free memory. 
    GlobalFree((HGLOBAL)lpBits);
  }

  return ret;
}

BOOL SaveBitmapToFile(HBITMAP hBmp, HDC hDC, LPCTSTR file, LPBYTE lpBits)
{
  PBITMAPINFO pbmi = BmpCreateInfo(hBmp);
  BOOL ret = FALSE;
  if(pbmi) {
    ret = BmpSaveFile(file, pbmi, hBmp, hDC, lpBits);
    LocalFree(pbmi);
  }
  return ret;
}

}  // namespace webkit_glue