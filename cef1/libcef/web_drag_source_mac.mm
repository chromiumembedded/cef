// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/param.h>

#include "libcef/browser_impl.h"
#import "libcef/browser_webview_mac.h"
#include "libcef/cef_context.h"
#include "libcef/drag_download_util.h"
#include "libcef/download_util.h"
#import "libcef/web_drag_source_mac.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/mac/nsimage_cache.h"
#include "webkit/glue/webdropdata.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;
using net::FileStream;
using WebKit::WebDragOperationNone;
using WebKit::WebView;

namespace {

// An unofficial standard pasteboard title type to be provided alongside the
// |NSURLPboardType|.
NSString* const kNSURLTitlePboardType = @"public.url-name";

// Converts a string16 into a FilePath. Use this method instead of
// -[NSString fileSystemRepresentation] to prevent exceptions from being thrown.
// See http://crbug.com/78782 for more info.
FilePath FilePathFromFilename(const string16& filename) {
  NSString* str = SysUTF16ToNSString(filename);
  char buf[MAXPATHLEN];
  if (![str getFileSystemRepresentation:buf maxLength:sizeof(buf)])
    return FilePath();
  return FilePath(buf);
}

// Returns a filename appropriate for the drop data
// TODO(viettrungluu): Refactor to make it common across platforms,
// and move it somewhere sensible.
FilePath GetFileNameFromDragData(const WebDropData& drop_data) {
  FilePath file_name(FilePathFromFilename(drop_data.file_description_filename));
  std::string extension = file_name.Extension();
  file_name = file_name.BaseName().RemoveExtension();

  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.empty()) {
    // Retrieve the name from the URL.
    string16 suggested_filename =
        net::GetSuggestedFilename(drop_data.url, "", "", "", "", "");
    file_name = FilePathFromFilename(suggested_filename);
  }

  return file_name.ReplaceExtension(extension);
}

// This helper's sole task is to write out data for a promised file; the caller
// is responsible for opening the file. It takes the drop data and an open file
// stream.
void PromiseWriterHelper(const WebDropData& drop_data,
                         net::FileStream* file_stream) {
  DCHECK(file_stream);
  file_stream->WriteSync(drop_data.file_contents.data(),
                         drop_data.file_contents.length());

  file_stream->CloseSync();
}

}  // namespace


@interface WebDragSource(Private)

- (void)fillPasteboard;
- (NSImage*)dragImage;

@end  // @interface WebDragSource(Private)


@implementation WebDragSource

- (id)initWithWebView:(BrowserWebView*)view
             dropData:(const WebDropData*)dropData
                image:(NSImage*)image
               offset:(NSPoint)offset
           pasteboard:(NSPasteboard*)pboard
    dragOperationMask:(NSDragOperation)dragOperationMask {
  if ((self = [super init])) {
    view_ = view;
    DCHECK(view_);
    
    dropData_.reset(new WebDropData(*dropData));
    DCHECK(dropData_.get());

    if (image == nil) {
      // No drag image was provided so create one.
      FilePath path = _Context->GetResourcesFilePath();
      path = path.AppendASCII("urlIcon.png");
      image = [[NSImage alloc]
               initWithContentsOfFile:SysUTF8ToNSString(path.value())];
    }

    dragImage_.reset([image retain]);
    imageOffset_ = offset;

    pasteboard_.reset([pboard retain]);
    DCHECK(pasteboard_.get());

    dragOperationMask_ = dragOperationMask;

    fileExtension_ = nil;

    [self fillPasteboard];
  }

  return self;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return dragOperationMask_;
}

- (void)lazyWriteToPasteboard:(NSPasteboard*)pboard forType:(NSString*)type {
  // NSHTMLPboardType requires the character set to be declared. Otherwise, it
  // assumes US-ASCII. Awesome.
  static const string16 kHtmlHeader =
      ASCIIToUTF16("<meta http-equiv=\"Content-Type\" "
                   "content=\"text/html;charset=UTF-8\">");

  // Be extra paranoid; avoid crashing.
  if (!dropData_.get()) {
    NOTREACHED() << "No drag-and-drop data available for lazy write.";
    return;
  }

  // HTML.
  if ([type isEqualToString:NSHTMLPboardType]) {
    DCHECK(!dropData_->html.is_null() && !dropData_->html.string().empty());
    // See comment on |kHtmlHeader| above.
    [pboard setString:SysUTF16ToNSString(kHtmlHeader + dropData_->html.string())
              forType:NSHTMLPboardType];

  // URL.
  } else if ([type isEqualToString:NSURLPboardType]) {
    DCHECK(dropData_->url.is_valid());
    NSString* urlStr = SysUTF8ToNSString(dropData_->url.spec());
    NSURL* url = [NSURL URLWithString:urlStr];
    // If NSURL creation failed, check for a badly-escaped javascript URL.
    if (!url && urlStr && dropData_->url.SchemeIs("javascript")) {
      NSString *escapedStr =
        [urlStr stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
      url = [NSURL URLWithString:escapedStr];
    }
    [url writeToPasteboard:pboard];
  // URL title.
  } else if ([type isEqualToString:kNSURLTitlePboardType]) {
    [pboard setString:SysUTF16ToNSString(dropData_->url_title)
              forType:kNSURLTitlePboardType];

  // File contents.
  } else if ([type isEqualToString:NSFileContentsPboardType] ||
      (fileExtension_ &&
       [type isEqualToString:NSCreateFileContentsPboardType(fileExtension_)])) {
    // TODO(viettrungluu: find something which is known to accept
    // NSFileContentsPboardType to check that this actually works!
    scoped_nsobject<NSFileWrapper> file_wrapper(
        [[NSFileWrapper alloc] initRegularFileWithContents:[NSData
                dataWithBytes:dropData_->file_contents.data()
                       length:dropData_->file_contents.length()]]);
    [file_wrapper setPreferredFilename:SysUTF8ToNSString(
            GetFileNameFromDragData(*dropData_).value())];
    [pboard writeFileWrapper:file_wrapper];

  // TIFF.
  } else if ([type isEqualToString:NSTIFFPboardType]) {
    // TODO(viettrungluu): This is a bit odd since we rely on Cocoa to render
    // our image into a TIFF. This is also suboptimal since this is all done
    // synchronously. I'm not sure there's much we can easily do about it.
    scoped_nsobject<NSImage> image(
        [[NSImage alloc] initWithData:[NSData
                dataWithBytes:dropData_->file_contents.data()
                       length:dropData_->file_contents.length()]]);
    [pboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];

  // Plain text.
  } else if ([type isEqualToString:NSStringPboardType]) {
    DCHECK(!dropData_->text.is_null() && !dropData_->text.string().empty());
    [pboard setString:SysUTF16ToNSString(dropData_->text.string())
              forType:NSStringPboardType];

  // Oops!
  } else {
    NOTREACHED() << "Asked for a drag pasteboard type we didn't offer.";
  }
}

- (NSPoint)convertScreenPoint:(NSPoint)screenPoint {
  NSPoint basePoint = [[view_ window] convertScreenToBase:screenPoint];
  return [view_ convertPoint:basePoint fromView:nil];
}

- (void)startDrag {
  NSEvent* currentEvent = [NSApp currentEvent];

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [view_ window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  if (dragImage_) {
    position.x -= imageOffset_.x;
    // Deal with Cocoa's flipped coordinate system.
    position.y -= [dragImage_.get() size].height - imageOffset_.y;
  }
  // Per kwebster, offset arg is ignored, see -_web_DragImageForElement: in
  // third_party/WebKit/Source/WebKit/mac/Misc/WebNSViewExtras.m.
  [window dragImage:[self dragImage]
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:pasteboard_
             source:view_
          slideBack:YES];
}

- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation {
  // Convert |screenPoint| to view coordinates and flip it.
  NSWindow* window = [view_ window];
  NSPoint localPoint = [self convertScreenPoint:screenPoint];
  NSRect viewFrame = [window frame];
  localPoint.y = viewFrame.size.height - localPoint.y;
  // Flip |screenPoint|.
  NSRect screenFrame = [[window screen] frame];
  screenPoint.y = screenFrame.size.height - screenPoint.y;
  
  // If AppKit returns a copy and move operation, mask off the move bit
  // because WebCore does not understand what it means to do both, which
  // results in an assertion failure/renderer crash.
  if (operation == (NSDragOperationMove | NSDragOperationCopy))
    operation &= ~NSDragOperationMove;

  WebView* webview = view_.browser->UIT_GetWebView();
  
  gfx::Point client(localPoint.x, localPoint.y);
  gfx::Point screen(screenPoint.x, screenPoint.y);
  webview->dragSourceEndedAt(client, screen,
                             static_cast<WebKit::WebDragOperation>(operation));
  
  // Make sure the pasteboard owner isn't us.
  [pasteboard_ declareTypes:[NSArray array] owner:nil];
  
  webview->dragSourceSystemDragEnded();
}

- (void)moveDragTo:(NSPoint)screenPoint {
  // Convert |screenPoint| to view coordinates and flip it.
  NSWindow* window = [view_ window];
  NSPoint localPoint = [self convertScreenPoint:screenPoint];
  NSRect viewFrame = [window frame];
  localPoint.y = viewFrame.size.height - localPoint.y;
  // Flip |screenPoint|.
  NSRect screenFrame = [[window screen] frame];
  screenPoint.y = screenFrame.size.height - screenPoint.y;

  WebView* webview = view_.browser->UIT_GetWebView();
  
  gfx::Point client(localPoint.x, localPoint.y);
  gfx::Point screen(screenPoint.x, screenPoint.y);
  webview->dragSourceMovedTo(client, screen, WebDragOperationNone);
}

- (NSString*)dragPromisedFileTo:(NSString*)path {
  // Be extra paranoid; avoid crashing.
  if (!dropData_.get()) {
    NOTREACHED() << "No drag-and-drop data available for promised file.";
    return nil;
  }

  FilePath fileName = downloadFileName_.empty() ?
      GetFileNameFromDragData(*dropData_) : downloadFileName_;
  FilePath filePath(SysNSStringToUTF8(path));
  filePath = filePath.Append(fileName);

  // CreateFileStreamForDrop() will call file_util::PathExists(),
  // which is blocking.  Since this operation is already blocking the
  // UI thread on OSX, it should be reasonable to let it happen.
  base::ThreadRestrictions::ScopedAllowIO allowIO;
  FileStream* fileStream =
      drag_download_util::CreateFileStreamForDrop(&filePath);
  if (!fileStream)
    return nil;

  if (downloadURL_.is_valid()) {
    WebView* webview = view_.browser->UIT_GetWebView();
    const GURL& page_url = webview->mainFrame()->document().url();
    const std::string& page_encoding =
        webview->mainFrame()->document().encoding().utf8();
    
    scoped_refptr<DragDownloadFile> dragFileDownloader(new DragDownloadFile(
        filePath,
        linked_ptr<net::FileStream>(fileStream),
        downloadURL_,
        page_url,
        page_encoding,
        view_.browser->UIT_GetWebViewDelegate()));

    // The finalizer will take care of closing and deletion.
    dragFileDownloader->Start(
        new drag_download_util::PromiseFileFinalizer(dragFileDownloader));
  } else {
    // The writer will take care of closing and deletion.
    CefThread::PostTask(CefThread::FILE, FROM_HERE,
        base::Bind(PromiseWriterHelper, *dropData_, base::Owned(fileStream)));
  }

  // Once we've created the file, we should return the file name.
  return SysUTF8ToNSString(filePath.BaseName().value());

  return nil;
}

@end  // @implementation WebDragSource


@implementation WebDragSource (Private)

- (void)fillPasteboard {
  DCHECK(pasteboard_.get());

  [pasteboard_ declareTypes:[NSArray array] owner:view_];

  // HTML.
  if (!dropData_->html.is_null() && !dropData_->html.string().empty())
    [pasteboard_ addTypes:[NSArray arrayWithObject:NSHTMLPboardType]
                    owner:view_];

  // URL (and title).
  if (dropData_->url.is_valid())
    [pasteboard_ addTypes:[NSArray arrayWithObjects:NSURLPboardType,
                                                    kNSURLTitlePboardType, nil]
                    owner:view_];

  std::string fileExtension;

  // File.
  if (!dropData_->file_contents.empty() ||
      !dropData_->download_metadata.empty()) {
    if (dropData_->download_metadata.empty()) {
      fileExtension = GetFileNameFromDragData(*dropData_).Extension();
    } else {
      string16 mimeType;
      FilePath fileName;
      if (drag_download_util::ParseDownloadMetadata(
              dropData_->download_metadata,
              &mimeType,
              &fileName,
              &downloadURL_)) {
        download_util::GenerateFileName(
            downloadURL_,
            std::string(),
            std::string(),
            UTF16ToUTF8(mimeType),
            fileName.value(),
            &downloadFileName_);
        fileExtension = downloadFileName_.Extension();
      }
    }

    if (!fileExtension.empty()) {
      // Strip the leading dot.
      fileExtension_ = SysUTF8ToNSString(fileExtension.substr(1));
      // File contents (with and without specific type), and file (HFS) promise.
      // TODO(viettrungluu): others?
      NSArray* types = [NSArray arrayWithObjects:
          NSFileContentsPboardType,
          NSCreateFileContentsPboardType(fileExtension_),
          NSFilesPromisePboardType,
          nil];
      [pasteboard_ addTypes:types owner:view_];

      if (!dropData_->file_contents.empty()) {
        [pasteboard_ addTypes:[NSArray arrayWithObject:NSTIFFPboardType]
                        owner:view_];
      }

      // For the file promise, we need to specify the extension.
      [pasteboard_ setPropertyList:[NSArray arrayWithObject:fileExtension_]
                           forType:NSFilesPromisePboardType];
    }
  }

  // Plain text.
  if (!dropData_->text.is_null() && !dropData_->text.string().empty())
    [pasteboard_ addTypes:[NSArray arrayWithObject:NSStringPboardType]
                    owner:view_];
}

- (NSImage*)dragImage {
  if (dragImage_)
    return dragImage_;

  // Default to returning a generic image.
  return gfx::GetCachedImageWithName(@"nav.pdf");
}

@end  // @implementation WebDragSource (Private)
