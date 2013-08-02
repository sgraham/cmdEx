// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_COMPLETION_H_
#define CMDEX_COMPLETION_H_

#include <string>
#include <vector>

// Return false if |line| and |position| isn't a good match, otherwise fill
// out |results|. |completion_start| is the offset of the word that's being
// completed.
typedef bool (*Completer)(const std::wstring& line,
                          int position,
                          std::vector<std::wstring>* results,
                          int* completion_start);

// |word| will not have any of its escape characters interpreted.
struct WordData {
  std::wstring original_word;
  int original_offset;
  std::wstring deescaped_word;
};

void CompletionBreakIntoWords(const std::wstring& line,
                              std::vector<WordData>* word_data);

int CompletionWordIndex(const std::vector<WordData>& word_data, int position);

#endif  // CMDEX_COMPLETION_H_