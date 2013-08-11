// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/command_history.h"
#include "gtest/gtest.h"

TEST(CommandHistoryTest, NotFound) {
  CommandHistory ch;
  vector<wstring> entries;
  entries.push_back(L"abc");
  entries.push_back(L"def");
  entries.push_back(L"ghi");
  ch.Populate(entries);
  wstring result;
  EXPECT_FALSE(ch.MoveInHistory(-1, L"py", &result));
}

TEST(CommandHistoryTest, FoundSimple) {
  CommandHistory ch;
  vector<wstring> entries;
  entries.push_back(L"abc");
  entries.push_back(L"def");
  entries.push_back(L"ghi");
  ch.Populate(entries);
  wstring result;
  EXPECT_TRUE(ch.MoveInHistory(-1, L"g", &result));
  EXPECT_EQ(L"ghi", result);
}

TEST(CommandHistoryTest, FoundSimpleFarther) {
  CommandHistory ch;
  vector<wstring> entries;
  entries.push_back(L"abc");
  entries.push_back(L"def");
  entries.push_back(L"ghi");
  ch.Populate(entries);
  wstring result;
  EXPECT_TRUE(ch.MoveInHistory(-1, L"d", &result));
  EXPECT_EQ(L"def", result);
}

TEST(CommandHistoryTest, FoundSimpleFirst) {
  CommandHistory ch;
  vector<wstring> entries;
  entries.push_back(L"abc");
  entries.push_back(L"def");
  entries.push_back(L"ghi");
  ch.Populate(entries);
  wstring result;
  EXPECT_TRUE(ch.MoveInHistory(-1, L"a", &result));
  EXPECT_EQ(L"abc", result);
}
