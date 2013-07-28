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
  MockConsoleInterface() : width(0), cursor_x(0), cursor_y(0) {}
  virtual void GetCursorLocation(int* x, int* y) override {
    *x = cursor_x;
    *y = cursor_y;
  }
  virtual int GetWidth() override {
    return width;
  }
  virtual void DrawString(const wchar_t* str, int count, int x, int y)
      override {
  }
  virtual void SetCursorLocation(int x, int y) {
    cursor_x = x;
    cursor_y = y;
  }

  int width;
  int cursor_x;
  int cursor_y;
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

TEST(LineEditor, EndCommand) {
  MockConsoleInterface console;
  MockWorkingDirectory wd;
  DirectoryHistory dir_history(&wd);
  LineEditor le;
  le.Init(&console, &dir_history);
  wd.Set("c:\\some\\stuff");
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'a', 0, 'A'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'b', 0, 'B'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'c', 0, 'C'));
  EXPECT_EQ(LineEditor::kReturnToCmd,
            le.HandleKeyEvent(true, false, false, false, '\x0d', 0, VK_RETURN));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"abc\x0d\x0a"));
}
