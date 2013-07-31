// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/completion.h"

#include "gtest/gtest.h"

// Some tests based on:
// http://source.winehq.org/git/wine.git/blob/HEAD:/dlls/shell32/tests/shlexec.c

TEST(CompletionTest, BreakIntoWordsNoArgs) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe", &words);
  EXPECT_EQ(1, words.size());
  EXPECT_EQ(L"exe", words[0].word);
}

TEST(CompletionTest, BreakIntoWordsBasicQuotes) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe a b \"c d\" 'e f` g\\ h)", &words);
  EXPECT_EQ(8, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"a", words[1].word);
  EXPECT_EQ(L"b", words[2].word);
  EXPECT_EQ(L"\"c d\"", words[3].word);
  EXPECT_EQ(L"'e", words[4].word);
  EXPECT_EQ(L"f`", words[5].word);
  EXPECT_EQ(L"g\\", words[6].word);
  EXPECT_EQ(L"h)", words[7].word);
}

TEST(CompletionTest, BreakIntoWordsQuotesInArgs) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe a\"b\" \"c\"d e", &words);
  EXPECT_EQ(4, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"a\"b\"", words[1].word);
  EXPECT_EQ(L"\"c\"d", words[2].word);
  EXPECT_EQ(L"e", words[3].word);
}

TEST(CompletionTest, BreakIntoWordsUnclosedQuotes) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe a \"b", &words);
  EXPECT_EQ(3, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"a", words[1].word);
  EXPECT_EQ(L"\"b", words[2].word);
}

TEST(CompletionTest, BreakIntoWordsStandaloneQuote) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe a \"", &words);
  EXPECT_EQ(3, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"a", words[1].word);
  EXPECT_EQ(L"\"", words[2].word);
}

TEST(CompletionTest, BreakIntoWordsOnlySpaceAndTab) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe a\tb\rc\nd\ve\ff", &words);
  EXPECT_EQ(3, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"a", words[1].word);
  EXPECT_EQ(L"b\rc\nd\ve\ff", words[2].word);
}

TEST(CompletionTest, BreakIntoWordsBackslashNoQuote) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(L"exe o\\ne t\\\\wo t\\\\\\hree f\\\\\\\\our",
                           &words);
  EXPECT_EQ(5, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"o\\ne", words[1].word);
  EXPECT_EQ(L"t\\\\wo", words[2].word);
  EXPECT_EQ(L"t\\\\\\hree", words[3].word);
  EXPECT_EQ(L"f\\\\\\\\our", words[4].word);
}

TEST(CompletionTest, BreakIntoWordsBackslashNoQuoteInQuotes) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(
      L"exe \"o\\ne\" \"t\\\\wo\" \"t\\\\\\hree\" \"f\\\\\\\\our\"", &words);
  EXPECT_EQ(5, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"\"o\\ne\"", words[1].word);
  EXPECT_EQ(L"\"t\\\\wo\"", words[2].word);
  EXPECT_EQ(L"\"t\\\\\\hree\"", words[3].word);
  EXPECT_EQ(L"\"f\\\\\\\\our\"", words[4].word);
}

TEST(CompletionTest, BreakIntoWordsBackslashQuote) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(
      L"exe \\\"one \\\\\"two\" \\\\\\\"three \\\\\\\\\"four\" after", &words);
  EXPECT_EQ(6, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"\\\"one", words[1].word);
  EXPECT_EQ(L"\\\\\"two\"", words[2].word);
  EXPECT_EQ(L"\\\\\\\"three", words[3].word);
  EXPECT_EQ(L"\\\\\\\\\"four\"", words[4].word);
  EXPECT_EQ(L"after", words[5].word);
}

TEST(CompletionTest, BreakIntoWordsBackslashQuoteSpaced) {
  std::vector<WordData> words;
  CompletionBreakIntoWords(
      L"exe \"one\\\" still\" \"two\\\\\" \"three\\\\\\\" still\" " \
      L"\"four\\\\\\\\\" after",
      &words);
  EXPECT_EQ(6, words.size());
  EXPECT_EQ(L"exe", words[0].word);
  EXPECT_EQ(L"\"one\\\" still\"", words[1].word);
  EXPECT_EQ(L"\"two\\\\\"", words[2].word);
  EXPECT_EQ(L"\"three\\\\\\\" still\"", words[3].word);
  EXPECT_EQ(L"\"four\\\\\\\\\"", words[4].word);
  EXPECT_EQ(L"after", words[5].word);
}

// TODO
// - triple quoting
// - quotes inside quotes
// - escaped quotes inside
// - special executable handling
