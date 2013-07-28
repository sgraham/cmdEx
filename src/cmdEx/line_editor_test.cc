// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include <windows.h>

#include "cmdEx/directory_history.h"
#include "gtest/gtest.h"

namespace {

class MockWorkingDirectory : public WorkingDirectoryInterface {
 public:
  bool Set(const std::string& dir) override {
    dir_ = dir;
    return true;
  }
  std::string Get() override {
    return dir_;
  }

 private:
  std::string dir_;
};

class MockConsoleInterface : public ConsoleInterface {
 public:
  virtual void GetCursorLocation(int* x, int* y) override {
    *x = 0;
    *y = 0;
  }
};

}  // namespace


TEST(LineEditor, AltUp) {
  MockConsoleInterface console;
  MockWorkingDirectory wd;
  DirectoryHistory dir_history(&wd);
  LineEditor le;
  le.Init(&console, &dir_history);
  wd.Set("c:\\some\\stuff");
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_UP));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"cd..\x0d\x0a"));
}
