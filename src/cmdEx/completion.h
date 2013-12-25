// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_COMPLETION_H_
#define CMDEX_COMPLETION_H_

#include <string>
#include <vector>
using namespace std;

// |original_word| will not have any of its escape characters interpreted.
struct WordData {
  wstring original_word;
  int original_offset;
  wstring deescaped_word;
};

struct CompleterInput {
  vector<WordData> word_data;
  int word_index;
  int position_in_word;
};

struct CompleterOutput {
  vector<wstring> results;
  bool trailing_space;
  void Reset() {
    results.clear();
    trailing_space = true;
  }
};

// Return false if can't complete for |input|. Otherwise, fill out |results|.
typedef bool (*Completer)(const CompleterInput& input,
                          CompleterOutput* output);

void CompletionBreakIntoWords(const wstring& line,
                              vector<WordData>* word_data);

int CompletionWordIndex(const vector<WordData>& word_data, int position);

wstring QuoteWord(const wstring& argument);

#endif  // CMDEX_COMPLETION_H_
