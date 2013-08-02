// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/string_util.h"
#include "gtest/gtest.h"

TEST(StringUtil, StringSplitStandard) {
  vector<wstring> s = StringSplit(L"a;bcd;;efghi", L';');
  EXPECT_EQ(4, s.size());
  EXPECT_EQ(L"a", s[0]);
  EXPECT_EQ(L"bcd", s[1]);
  EXPECT_EQ(L"", s[2]);
  EXPECT_EQ(L"efghi", s[3]);
}

TEST(StringUtil, StringSplitEndingEmpty) {
  vector<wstring> s = StringSplit(L"a;", L';');
  EXPECT_EQ(2, s.size());
  EXPECT_EQ(L"a", s[0]);
  EXPECT_EQ(L"", s[1]);
}

TEST(StringUtil, StringSplitStartingEmpty) {
  vector<wstring> s = StringSplit(L";b", L';');
  EXPECT_EQ(2, s.size());
  EXPECT_EQ(L"", s[0]);
  EXPECT_EQ(L"b", s[1]);
}

TEST(StringUtil, StringSplitNoSplits) {
  vector<wstring> s = StringSplit(L"blorpy", L';');
  EXPECT_EQ(1, s.size());
  EXPECT_EQ(L"blorpy", s[0]);
}
