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
  MockConsoleInterface() : width(50), height(10), cursor_x(0), cursor_y(0) {
    screen_data = new wchar_t[width * height];
  }
  ~MockConsoleInterface() {
    delete[] screen_data;
  }
  virtual void GetCursorLocation(int* x, int* y) override {
    *x = cursor_x;
    *y = cursor_y;
  }
  virtual int GetWidth() override {
    return width;
  }
  virtual void DrawString(const wchar_t* str, int count, int x, int y)
      override {
    ASSERT_GE(x, 0);
    ASSERT_GE(y, 0);
    ASSERT_LT(x + count, width);
    ASSERT_LT(y, height);
    memcpy(&screen_data[y * width + x], str, count * sizeof(wchar_t));
  }
  virtual void SetCursorLocation(int x, int y) {
    cursor_x = x;
    cursor_y = y;
  }

  wchar_t GetCharAt(int x, int y) {
    return screen_data[y * width + x];
  }

  int width;
  int height;
  int cursor_x;
  int cursor_y;
  wchar_t* screen_data;
};

class LineEditorTest : public ::testing::Test {
 public:
  LineEditorTest() : dir_history(&wd) {
    le.Init(&console, &dir_history);
    wd.Set("c:\\some\\stuff");
  }

  MockConsoleInterface console;
  MockWorkingDirectory wd;
  DirectoryHistory dir_history;
  LineEditor le;
};

}  // namespace


TEST_F(LineEditorTest, AltUp) {
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_UP));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"cd..\x0d\x0a"));
}

TEST_F(LineEditorTest, EndCommand) {
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

TEST_F(LineEditorTest, CursorArrowSingleLine) {
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'a', 0, 'A'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'b', 0, 'B'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'c', 0, 'C'));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ('c', console.GetCharAt(2, 0));

  EXPECT_EQ(3, console.cursor_x);
  EXPECT_EQ(0, console.cursor_y);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(1, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);

  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ('c', console.GetCharAt(2, 0));

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(LineEditor::kIncomplete,
              le.HandleKeyEvent(true, false, false, false, 0, 0, VK_RIGHT));
  }
  EXPECT_EQ(3, console.cursor_x);
}

TEST_F(LineEditorTest, InsertChar) {
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'a', 0, 'A'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'b', 0, 'B'));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'c', 0, 'C'));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ('c', console.GetCharAt(2, 0));
  EXPECT_EQ(3, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 'x', 0, 'X'));

  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('x', console.GetCharAt(1, 0));
  EXPECT_EQ('b', console.GetCharAt(2, 0));
  EXPECT_EQ('c', console.GetCharAt(3, 0));
  EXPECT_EQ(2, console.cursor_x);
}
