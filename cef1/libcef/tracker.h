// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef  CEF_LIBCEF_TRACKER_H_
#define  CEF_LIBCEF_TRACKER_H_
#pragma once

#include "include/cef_base.h"

// Class extended by objects that must be tracked.  After creating a tracked
// object you should add it to the appropriate track manager.
class CefTrackNode {
 public:
  CefTrackNode() {
    track_next_ = NULL;
    track_prev_ = NULL;
  }
  virtual ~CefTrackNode() {
  }

  // Returns true if the object is currently being tracked.
  bool IsTracked() { return (track_prev_ || track_next_); }

 private:
  CefTrackNode* GetTrackPrev() { return track_prev_; }
  void SetTrackPrev(CefTrackNode* base) { track_prev_ = base; }
  CefTrackNode* GetTrackNext() { return track_next_; }
  void SetTrackNext(CefTrackNode* base) { track_next_ = base; }

  // Insert a new object into the tracking list before this object.
  void InsertTrackPrev(CefTrackNode* object) {
    if (track_prev_)
      track_prev_->SetTrackNext(object);
    object->SetTrackNext(this);
    object->SetTrackPrev(track_prev_);
    track_prev_ = object;
  }

  // Insert a new object into the tracking list after this object.
  void InsertTrackNext(CefTrackNode* object) {
    if (track_next_)
      track_next_->SetTrackPrev(object);
    object->SetTrackPrev(this);
    object->SetTrackNext(track_next_);
    track_next_ = object;
  }

  // Remove this object from the tracking list.
  void RemoveTracking() {
    if (track_next_)
      track_next_->SetTrackPrev(track_prev_);
    if (track_prev_)
      track_prev_->SetTrackNext(track_next_);
    track_next_ = NULL;
    track_prev_ = NULL;
  }

 private:
  CefTrackNode* track_next_;
  CefTrackNode* track_prev_;

  friend class CefTrackManager;
};

// Class used to manage tracked objects.  A single instance of this class
// should be created for each intended usage.  Any objects that have not been
// removed by explicit calls to the Destroy() method will be removed when the
// manager object is destroyed.  A manager object can be created as either a
// member variable of another class or by using lazy initialization:
// base::LazyInstance<CefTrackManager> g_singleton = LAZY_INSTANCE_INITIALIZER;
class CefTrackManager : public CefBase {
 public:
  CefTrackManager() : object_count_(0) {}
  virtual ~CefTrackManager() {
    DeleteAll();
  }

  // Add an object to be tracked by this manager.
  void Add(CefTrackNode* object) {
    Lock();
    if (!object->IsTracked()) {
      tracker_.InsertTrackNext(object);
      ++object_count_;
    }
    Unlock();
  }

  // Delete an object tracked by this manager.
  bool Delete(CefTrackNode* object) {
    bool rv = false;
    Lock();
    if (object->IsTracked()) {
      object->RemoveTracking();
      delete object;
      --object_count_;
      rv = true;
    }
    Unlock();
    return rv;
  }

  // Delete all objects tracked by this manager.
  void DeleteAll() {
    Lock();
    CefTrackNode* next;
    do {
      next = tracker_.GetTrackNext();
      if (next) {
        next->RemoveTracking();
        delete next;
      }
    } while (next != NULL);
    object_count_ = 0;
    Unlock();
  }

  // Returns the number of objects currently being tracked.
  int GetCount() { return object_count_; }

 private:
  CefTrackNode tracker_;
  int object_count_;

  IMPLEMENT_REFCOUNTING(CefTrackManager);
  IMPLEMENT_LOCKING(CefTrackManager);
};

#endif  // CEF_LIBCEF_TRACKER_H_
