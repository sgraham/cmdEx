// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/directory_history.h"

#include <algorithm>

#include "common/util.h"

DirectoryHistory::DirectoryHistory(WorkingDirectoryInterface* working_dir)
    : working_dir_(working_dir), position_(0) {
  last_known_ = working_dir_->Get();
}

void DirectoryHistory::StartingEdit() {
  string current = working_dir_->Get();
  if (last_known_ != current) {
    CommitLastKnown();
    last_known_ = current;
  }
}

bool DirectoryHistory::NavigateInHistory(int direction) {
  CHECK(direction == 1 || direction == -1);
  if (position_ == static_cast<int>(dirs_.size()) && direction == -1) {
    CommitLastKnown();
    position_--;
  }
  int original = position_;
  position_ += direction;
  position_ = max(position_, 0);
  position_ = min(position_, static_cast<int>(dirs_.size() - 1));
  // TODO: If a directory has since been removed, remove from history list,
  // and either go to the next or say something maybe?
  working_dir_->Set(dirs_[position_]);
  last_known_ = dirs_[position_];
  return original != position_;
}

bool DirectoryHistory::CommitLastKnown() {
  for (vector<string>::const_iterator i(dirs_.begin()); i != dirs_.end(); ++i) {
    if (*i == last_known_) {
      dirs_.erase(i);
      break;
    }
  }
  dirs_.push_back(last_known_);
  position_ = static_cast<int>(dirs_.size());
  return true;
}
