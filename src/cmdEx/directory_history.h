// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_DIRECTORY_HISTORY_H_
#define CMDEX_DIRECTORY_HISTORY_H_

#include <string>
#include <vector>

class WorkingDirectoryInterface {
 public:
  virtual bool Set(const std::string& dir) = 0;
  virtual std::string Get() = 0;
};

class DirectoryHistory {
 public:
  DirectoryHistory(WorkingDirectoryInterface* working_dir);

  // Called when we resume editing again. If the directory isn't the same as
  // the last known, then we jumped: add last known to the list.
  void StartingEdit();

  bool NavigateInHistory(int direction);

  WorkingDirectoryInterface* GetWorkingDirectoryInterface() {
    return working_dir_;
  }

 private:
  bool CommitLastKnown();

  WorkingDirectoryInterface* working_dir_;
  std::vector<std::string> dirs_;
  std::string last_known_;
  int position_;
};

#endif  // CMDEX_DIRECTORY_HISTORY_H_
