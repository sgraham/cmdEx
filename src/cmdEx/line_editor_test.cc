// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include <windows.h>

#include "cmdEx/command_history.h"
#include "cmdEx/directory_history.h"
#include "gtest/gtest.h"

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
    ASSERT_LE(x + count, width);
    ASSERT_LT(y, height);
    memcpy(&screen_data[y * width + x], str, count * sizeof(wchar_t));
  }
  virtual void FillChar(wchar_t ch, int count, int x, int y) override {
    ASSERT_GE(x, 0);
    ASSERT_GE(y, 0);
    ASSERT_LE(x + count, width);
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
    le.Init(&console, &dir_history, &cmd_history);
    wd.Set("c:\\some\\stuff");
  }

  void ReInit() {
	wchar_t buf[256];
	unsigned long num_chars;
	le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
    le.Init(&console, &dir_history, &cmd_history);
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
  CommandHistory cmd_history;
  LineEditor le;
};

}  // namespace


TEST_F(LineEditorTest, AltUp) {
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, false, true, false, 0, 0, VK_UP));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, wstring(L"cd..\x0d\x0a"));
}

TEST_F(LineEditorTest, EndCommand) {
  TypeLetters("abc");
  EXPECT_EQ(LineEditor::kReturnToCmd,
            le.HandleKeyEvent(true, false, false, false, '\x0d', 0, VK_RETURN));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, wstring(L"abc\x0d\x0a"));
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
  EXPECT_EQ(buf, wstring(L"cls\x0d\x0a"));
}

TEST_F(LineEditorTest, EscClearLine) {
  TypeLetters("stuff");
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_ESCAPE));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(' ', console.GetCharAt(0, 0));
  EXPECT_EQ(' ', console.GetCharAt(4, 0));
}

TEST_F(LineEditorTest, CtrlD) {
  TypeLetters("a");
  // No exit if there's stuff.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'D'));
  EXPECT_EQ(1, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(0, console.cursor_x);

  // exit if empty.
  EXPECT_EQ(LineEditor::kReturnToCmdThenResume,
            le.HandleKeyEvent(true, true, false, false, 0, 0, 'D'));
  EXPECT_EQ('e', console.GetCharAt(0, 0));
  EXPECT_EQ('x', console.GetCharAt(1, 0));
  EXPECT_EQ('i', console.GetCharAt(2, 0));
  EXPECT_EQ('t', console.GetCharAt(3, 0));
  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, wstring(L"exit\x0d\x0a"));
}

TEST_F(LineEditorTest, CtrlArrows) {
  TypeLetters("this is some stuff");

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(13, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(8, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(5, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(0, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_RIGHT));
  EXPECT_EQ(5, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_RIGHT));
  EXPECT_EQ(8, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_RIGHT));
  EXPECT_EQ(13, console.cursor_x);

  // Not sure I love this, but it matches cmd.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_RIGHT));
  EXPECT_EQ(17, console.cursor_x);
}

TEST_F(LineEditorTest, EndLineKill) {
  TypeLetters("blorpy florpbag zorp");
  for (int i = 0; i < 7; ++i)
    EXPECT_EQ(LineEditor::kIncomplete,
              le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(13, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 'K', 0, 'K'));
  for (int i = 13; i < 20; ++i)
    EXPECT_EQ(' ', console.GetCharAt(i, 0));
  EXPECT_EQ(LineEditor::kIncomplete,
      le.HandleKeyEvent(true, true, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, VK_END, 0, VK_END));
  for (int i = 7; i < 20; ++i)
    EXPECT_EQ(' ', console.GetCharAt(i, 0));
}

TEST_F(LineEditorTest, StartLineKill) {
  TypeLetters("blorpy florpbag zorp");
  for (int i = 0; i < 13; ++i)
    EXPECT_EQ(LineEditor::kIncomplete,
              le.HandleKeyEvent(true, false, false, false, 0, 0, VK_LEFT));
  EXPECT_EQ(7, console.cursor_x);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 'U', 0, 'U'));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ('f', console.GetCharAt(0, 0));
  EXPECT_EQ('l', console.GetCharAt(1, 0));
  EXPECT_EQ('o', console.GetCharAt(2, 0));
  EXPECT_EQ('r', console.GetCharAt(3, 0));
  EXPECT_EQ('p', console.GetCharAt(4, 0));
  EXPECT_EQ('b', console.GetCharAt(5, 0));
  EXPECT_EQ('a', console.GetCharAt(6, 0));
  EXPECT_EQ('g', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));
  EXPECT_EQ('z', console.GetCharAt(9, 0));
  EXPECT_EQ('o', console.GetCharAt(10, 0));
  EXPECT_EQ('r', console.GetCharAt(11, 0));
  EXPECT_EQ('p', console.GetCharAt(12, 0));
  EXPECT_EQ(LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, 0, 0, VK_END));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, VK_HOME, 0, VK_HOME));
  EXPECT_EQ(0, console.cursor_x);
  for (int i = 0; i < 20; ++i)
    EXPECT_EQ(' ', console.GetCharAt(i, 0));
}

TEST_F(LineEditorTest, EnteringCommandHasNewline) {
  TypeLetters("wee");
  EXPECT_EQ(LineEditor::kReturnToCmd,
            le.HandleKeyEvent(true, false, false, false, '\x0d', 0, VK_RETURN));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(1, console.cursor_y);
  EXPECT_EQ('w', console.GetCharAt(0, 0));
  EXPECT_EQ('e', console.GetCharAt(1, 0));
  EXPECT_EQ('e', console.GetCharAt(2, 0));

  wchar_t buf[256];
  unsigned long num_chars;
  le.ToCmdBuffer(buf, sizeof(buf) / sizeof(wchar_t), &num_chars);
  EXPECT_EQ(buf, wstring(L"wee\x0d\x0a"));
}

TEST_F(LineEditorTest, MultilineWrap) {
  // Assuming console is 50 per mock setup.
  TypeLetters("01234567890123456789012345678901234567890123456789");
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(1, console.cursor_y);
}

TEST_F(LineEditorTest, MultilineClearOnKillBack) {
  // Assuming console is 50 per mock setup.
  TypeLetters("01234567890123456789012345678901234567890123456789");
  TypeLetters("01234567890123456789012345678901234567890123456789");
  TypeLetters("0123456789012345678901234567890123456789012345678");
  EXPECT_EQ(49, console.cursor_x);
  EXPECT_EQ(2, console.cursor_y);

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, true, false, false, 0, 0, VK_BACK));
  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(0, console.cursor_y);
  for (int x = 0; x < 50; ++x)
    for (int y = 0; y < 3; ++y)
      EXPECT_EQ(' ', console.GetCharAt(x, y));
}

bool MockCompleterBasic(const CompleterInput& input, vector<wstring>* results) {
  EXPECT_EQ(L"hi", input.word_data[0].original_word);
  EXPECT_EQ(L"ab", input.word_data[1].original_word);
  EXPECT_EQ(1, input.word_index);
  EXPECT_EQ(2, input.position_in_word);
  results->push_back(L"abxxx");
  results->push_back(L"abyyyyy");
  results->push_back(L"abz");
  return true;
}

TEST_F(LineEditorTest, TabCompleteBasic) {
  le.RegisterCompleter(MockCompleterBasic);
  TypeLetters("hi ab");

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('x', console.GetCharAt(5, 0));
  EXPECT_EQ('x', console.GetCharAt(6, 0));
  EXPECT_EQ('x', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));

  // Cycle next.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(10, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('y', console.GetCharAt(5, 0));
  EXPECT_EQ('y', console.GetCharAt(6, 0));
  EXPECT_EQ('y', console.GetCharAt(7, 0));
  EXPECT_EQ('y', console.GetCharAt(8, 0));
  EXPECT_EQ('y', console.GetCharAt(9, 0));
  EXPECT_EQ(' ', console.GetCharAt(10, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(6, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('z', console.GetCharAt(5, 0));
  EXPECT_EQ(' ', console.GetCharAt(6, 0));

  // Cycle loop.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('x', console.GetCharAt(5, 0));
  EXPECT_EQ('x', console.GetCharAt(6, 0));
  EXPECT_EQ('x', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));
}

TEST_F(LineEditorTest, TabCompleteReverseLoop) {
  le.RegisterCompleter(MockCompleterBasic);
  TypeLetters("hi ab");

  // Start backwards.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, true, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(6, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('z', console.GetCharAt(5, 0));
  EXPECT_EQ(' ', console.GetCharAt(6, 0));

  // To natural first.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('x', console.GetCharAt(5, 0));
  EXPECT_EQ('x', console.GetCharAt(6, 0));
  EXPECT_EQ('x', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));

  // Cycle next.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(10, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('y', console.GetCharAt(5, 0));
  EXPECT_EQ('y', console.GetCharAt(6, 0));
  EXPECT_EQ('y', console.GetCharAt(7, 0));
  EXPECT_EQ('y', console.GetCharAt(8, 0));
  EXPECT_EQ('y', console.GetCharAt(9, 0));
  EXPECT_EQ(' ', console.GetCharAt(10, 0));

  // Back two to last one.
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, true, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, true, VK_TAB, 0, VK_TAB));
  EXPECT_EQ(6, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('z', console.GetCharAt(5, 0));
  EXPECT_EQ(' ', console.GetCharAt(6, 0));
}

TEST_F(LineEditorTest, TabCompleteStopsAfterNonTab) {
  le.RegisterCompleter(MockCompleterBasic);
  TypeLetters("hi ab");

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_TRUE(le.IsCompleting());
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('x', console.GetCharAt(5, 0));
  EXPECT_EQ('x', console.GetCharAt(6, 0));
  EXPECT_EQ('x', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));

  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, ' ', 0, VK_SPACE));
  EXPECT_FALSE(le.IsCompleting());
}

bool MockCompleterInMiddle(const CompleterInput& input,
                           vector<wstring>* results) {
  EXPECT_EQ(L"hi", input.word_data[0].original_word);
  EXPECT_EQ(L"ab", input.word_data[1].original_word);
  EXPECT_EQ(L"cdefghi", input.word_data[2].original_word);
  EXPECT_EQ(1, input.word_index);
  EXPECT_EQ(1, input.position_in_word);
  results->push_back(L"abxxx");
  results->push_back(L"abyyyyy");
  results->push_back(L"abz");
  return true;
}

TEST_F(LineEditorTest, TabCompleteInMiddle) {
  le.RegisterCompleter(MockCompleterInMiddle);
  TypeLetters("hi ab cdefghi");

  // Back to 'b'.
  for (int i = 0; i < 9; ++i) {
    EXPECT_EQ(
        LineEditor::kIncomplete,
        le.HandleKeyEvent(true, false, false, false, VK_LEFT, 0, VK_LEFT));
  }
  EXPECT_EQ(LineEditor::kIncomplete,
            le.HandleKeyEvent(true, false, false, false, VK_TAB, 0, VK_TAB));
  EXPECT_TRUE(le.IsCompleting());
  EXPECT_EQ(8, console.cursor_x);
  EXPECT_EQ('h', console.GetCharAt(0, 0));
  EXPECT_EQ('i', console.GetCharAt(1, 0));
  EXPECT_EQ(' ', console.GetCharAt(2, 0));
  EXPECT_EQ('a', console.GetCharAt(3, 0));
  EXPECT_EQ('b', console.GetCharAt(4, 0));
  EXPECT_EQ('x', console.GetCharAt(5, 0));
  EXPECT_EQ('x', console.GetCharAt(6, 0));
  EXPECT_EQ('x', console.GetCharAt(7, 0));
  EXPECT_EQ(' ', console.GetCharAt(8, 0));
  EXPECT_EQ('c', console.GetCharAt(9, 0));
  EXPECT_EQ('d', console.GetCharAt(10, 0));
}

TEST_F(LineEditorTest, CommandHistoryUpDown) {
  TypeLetters("abc");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("defz");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("ghi");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();

  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(3, console.cursor_y);

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('g', console.GetCharAt(0, 3));
  EXPECT_EQ('h', console.GetCharAt(1, 3));
  EXPECT_EQ('i', console.GetCharAt(2, 3));
  EXPECT_EQ(3, console.cursor_x);

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('d', console.GetCharAt(0, 3));
  EXPECT_EQ('e', console.GetCharAt(1, 3));
  EXPECT_EQ('f', console.GetCharAt(2, 3));
  EXPECT_EQ('z', console.GetCharAt(3, 3));
  EXPECT_EQ(4, console.cursor_x);

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('a', console.GetCharAt(0, 3));
  EXPECT_EQ('b', console.GetCharAt(1, 3));
  EXPECT_EQ('c', console.GetCharAt(2, 3));
  EXPECT_EQ(3, console.cursor_x);

  // Wrap.
  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('g', console.GetCharAt(0, 3));
  EXPECT_EQ('h', console.GetCharAt(1, 3));
  EXPECT_EQ('i', console.GetCharAt(2, 3));

  // Wrap down.
  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_DOWN, 0, VK_DOWN));
  EXPECT_EQ('a', console.GetCharAt(0, 3));
  EXPECT_EQ('b', console.GetCharAt(1, 3));
  EXPECT_EQ('c', console.GetCharAt(2, 3));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_DOWN, 0, VK_DOWN));
  EXPECT_EQ('d', console.GetCharAt(0, 3));
  EXPECT_EQ('e', console.GetCharAt(1, 3));
  EXPECT_EQ('f', console.GetCharAt(2, 3));
}

TEST_F(LineEditorTest, CommandHistoryCompleteUpDown) {
  TypeLetters("abc");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("def");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("abxx");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("abzzz");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();

  EXPECT_EQ(0, console.cursor_x);
  EXPECT_EQ(4, console.cursor_y);

  TypeLetters("ab");
  EXPECT_EQ(2, console.cursor_x);

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_PRIOR, 0, VK_PRIOR));
  EXPECT_EQ(2, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('z', console.GetCharAt(2, 4));
  EXPECT_EQ('z', console.GetCharAt(3, 4));
  EXPECT_EQ('z', console.GetCharAt(4, 4));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_PRIOR, 0, VK_PRIOR));
  EXPECT_EQ(2, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('x', console.GetCharAt(2, 4));
  EXPECT_EQ('x', console.GetCharAt(3, 4));
  EXPECT_EQ(' ', console.GetCharAt(4, 4));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_PRIOR, 0, VK_PRIOR));
  EXPECT_EQ(2, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('c', console.GetCharAt(2, 4));
  EXPECT_EQ(' ', console.GetCharAt(3, 4));
  EXPECT_EQ(' ', console.GetCharAt(4, 4));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_NEXT, 0, VK_NEXT));
  EXPECT_EQ(2, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('x', console.GetCharAt(2, 4));
  EXPECT_EQ('x', console.GetCharAt(3, 4));
  EXPECT_EQ(' ', console.GetCharAt(4, 4));

  // F8 is the same as Page Up.
  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_F8, 0, VK_F8));
  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_F8, 0, VK_F8));
  EXPECT_EQ(2, console.cursor_x);
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('z', console.GetCharAt(2, 4));
  EXPECT_EQ('z', console.GetCharAt(3, 4));
  EXPECT_EQ('z', console.GetCharAt(4, 4));
}

TEST_F(LineEditorTest, CommandHistorySaving) {
  TypeLetters("abc");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("def");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("abxx");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();
  TypeLetters("abzzz");
  EXPECT_EQ(
      LineEditor::kReturnToCmd,
      le.HandleKeyEvent(true, false, false, false, VK_RETURN, 0, VK_RETURN));
  ReInit();

  vector<wstring> saved = cmd_history.GetListForSaving();

  cmd_history = CommandHistory();

  cmd_history.Populate(saved);

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('z', console.GetCharAt(2, 4));
  EXPECT_EQ('z', console.GetCharAt(3, 4));
  EXPECT_EQ('z', console.GetCharAt(4, 4));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('a', console.GetCharAt(0, 4));
  EXPECT_EQ('b', console.GetCharAt(1, 4));
  EXPECT_EQ('x', console.GetCharAt(2, 4));
  EXPECT_EQ('x', console.GetCharAt(3, 4));
  EXPECT_EQ(' ', console.GetCharAt(4, 4));

  EXPECT_EQ(
      LineEditor::kIncomplete,
      le.HandleKeyEvent(true, false, false, false, VK_UP, 0, VK_UP));
  EXPECT_EQ('d', console.GetCharAt(0, 4));
  EXPECT_EQ('e', console.GetCharAt(1, 4));
  EXPECT_EQ('f', console.GetCharAt(2, 4));
  EXPECT_EQ(' ', console.GetCharAt(3, 4));
  EXPECT_EQ(' ', console.GetCharAt(4, 4));
}

// Ctrl-C to break line
// Sort complete results somehow?
// Ctrl-V to paste
// Ctrl-Y (?) to copy line
