// Copyright 2023 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "base/scoped_observation.h"
#include "ui/views/widget/widget_observer.h"

// Tracks if a given widget has been destroyed.
class WidgetDestructionObserver : public views::WidgetObserver {
 public:
  explicit WidgetDestructionObserver(views::Widget* widget) : widget_(widget) {
    DCHECK(widget);
    observation_.Observe(widget);
  }
  WidgetDestructionObserver(const WidgetDestructionObserver&) = delete;
  WidgetDestructionObserver& operator=(const WidgetDestructionObserver&) =
      delete;
  ~WidgetDestructionObserver() override = default;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    DCHECK(widget_);
    widget_ = nullptr;
    observation_.Reset();
  }

  views::Widget* widget() const { return widget_; }

 private:
  views::Widget* widget_;

  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};
