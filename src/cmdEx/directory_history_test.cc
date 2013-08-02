// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "cmdEx/directory_history.h"

namespace {

class MockWorkingDirectory : public WorkingDirectoryInterface {
 public:
  bool Set(const string& dir) override {
    dir_ = dir;
    return true;
  }
  string Get() override {
    return dir_;
  }

 private:
  string dir_;
};

}  // namespace

TEST(DirectoryHistoryTest, Basic) {
  MockWorkingDirectory wd;
  wd.Set("c:\\x");

  DirectoryHistory dh(&wd);
  dh.StartingEdit();

  wd.Set("c:\\y");
  dh.StartingEdit();

  wd.Set("c:\\z");
  dh.StartingEdit();

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\y", wd.Get());

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\x", wd.Get());

  dh.NavigateInHistory(1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\y", wd.Get());

  dh.NavigateInHistory(1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\z", wd.Get());
}

TEST(DirectoryHistoryTest, WithSetAfterNavigate) {
  MockWorkingDirectory wd;
  wd.Set("c:\\x");

  DirectoryHistory dh(&wd);
  dh.StartingEdit();

  wd.Set("c:\\y");
  dh.StartingEdit();

  wd.Set("c:\\z");
  dh.StartingEdit();

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\y", wd.Get());

  wd.Set("c:\\a");
  dh.StartingEdit();
  EXPECT_EQ("c:\\a", wd.Get());

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\y", wd.Get());

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\z", wd.Get());

  dh.NavigateInHistory(-1);
  dh.StartingEdit();
  EXPECT_EQ("c:\\x", wd.Get());
}

// TODO
/*
TEST(DirectoryHistory, SetFails) {
}
*/
