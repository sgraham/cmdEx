// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "directory_history.h"

class MockWorkingDirectory {
 public:
  bool Set(const std::string& dir) OVERRIDE {
    dir_ = dir;
  }
  std::string Get() OVERRIDE {
    return dir_;
  }

 private:
  std::string dir_;
};

TEST(DirectoryHistory, Basic) {
  MockWorkingDirectory wd;

  DirectoryHistory dh(&wd);
}
