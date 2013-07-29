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
  virtual void FillChar(wchar_t ch, int count, int x, int y) override {
    ASSERT_GE(x, 0);
    ASSERT_GE(y, 0);
    ASSERT_LT(x + count, width);
    ASSERT_LT(y, height);
    for (int i = 0; i < count; ++i)
      screen_data[y * width + x + i] = ch;
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

  void TypeLetters(const char* str) {
    for (size_t i = 0; i < strlen(str); ++i) {
      EXPECT_EQ(LineEditor::kIncomplete,
                le.HandleKeyEvent(
                    true, false, false, false, str[i], 0, toupper(str[i])));
    }
  }

  MockConsoleInterface console;
  MockWorkingDirectory wd;
  DirectoryHistory dir_history;
  LineEditor le;
};

}  // namespace


TEST_F(LineEditorTest, AltUp) {
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, false, true, false, 0, 0, VK_UP));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"cd..\x0d\x0a"));
}

TEST_F(LineEditorTest, EndCommand) {
  TypeLetters("abc");
  EXPECT_EQ(LineEditor::kReturnToCmd,
            le.HandleKeyEvent(true, false, false, false, '\x0d', 0, VK_RETURN));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"abc\x0d\x0a"));
}

TEST_F(LineEditorTest, CursorArrowSingleLine) {
  TypeLetters("abc");
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
  TypeLetters("abc");
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

TEST_F(LineEditorTest, Backspace) {
  TypeLetters("abc");
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_BACK));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ(2, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
}

TEST_F(LineEditorTest, Delete) {
  TypeLetters("abc");
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_DELETE));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ('c', console.GetCharAt(2, 0));
  EXPECT_EQ(3, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(1, console.cursor_x);
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_DELETE));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('c', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_DELETE));
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ(' ', console.GetCharAt(1, 0));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_DELETE));
  EXPECT_EQ(' ', console.GetCharAt(0, 0));
}

TEST_F(LineEditorTest, HomeEnd) {
  TypeLetters("abc");
  EXPECT_EQ('a', console.GetCharAt(0, 0));
  EXPECT_EQ('b', console.GetCharAt(1, 0));
  EXPECT_EQ('c', console.GetCharAt(2, 0));
  EXPECT_EQ(3, console.cursor_x);
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_HOME));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_END));
  EXPECT_EQ(3, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'A'));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'E'));
  EXPECT_EQ(3, console.cursor_x);
}

TEST_F(LineEditorTest, KillCtrlW) {
  TypeLetters("abc sff   and things");
  EXPECT_EQ(20, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'W'));
  EXPECT_EQ(14, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(14, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'W'));
  EXPECT_EQ(10, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(10, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'W'));
  EXPECT_EQ(4, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(4, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'W'));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(0, 0));
}

TEST_F(LineEditorTest, KillCtrlBack) {
  TypeLetters("abc stu/ff\\and\\things/ ");
  EXPECT_EQ(23, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(15, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(15, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(11, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(11, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(8, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(4, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(4, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(0, 0));
}

TEST_F(LineEditorTest, ClearScreen) {
  console.cursor_x = 10;
  console.cursor_y = 5;
  TypeLetters("stuff");
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'L'));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, std::wstring(L"cls\x0d\x0a"));
}
