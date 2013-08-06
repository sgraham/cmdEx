// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/completion.h"

#include <windows.h>

#include "common/util.h"

#pragma comment(lib, "shell32.lib")

static void CopyArg(const wchar_t* line_start,
                    const wchar_t* arg_start,
                    const wchar_t* arg_end,
                    vector<WordData>* word_data) {
  WordData wd;
  wd.original_offset = arg_start - line_start;
  wd.original_word.reserve(arg_end - arg_start);
  for (const wchar_t* i = arg_start; i != arg_end; ++i)
    wd.original_word.push_back(*i);
  word_data->push_back(wd);
}

static void SkipWhitespace(const wchar_t** p) {
  while (**p == L' ' || **p == L'\t')
    ++(*p);
}
// We want to use CommandLineToArgvW here, but we need to be able to say in
// which word and offset the given |position| in source falls into, and we
// don't want to de-escape anything for completion. So, we need to reimplement
// the somewhat subtle and complex splitting behaviour of CommandLineToArgvW
// for quote escaping and quote doubling:
// - " groups over space and tab
// - \" -> "
// - multiple \ before a quote (only; otherwise backslashes aren't magic):
//   2n -> n backslashes, quote as argument delimiter
//   2n+1 -> n backslashes, quote is literal
// - consecutive quotes are / 3:
//   3n -> n
//   3n+1 -> n, and close string
//   3n+2 -> n+1 and close string

void CompletionBreakIntoWords(const wstring& line,
                              vector<WordData>* word_data) {
  if (line.empty()) {
    // See comment about appending empty arg at end of function.
    CopyArg(NULL, NULL, NULL, word_data);
    return;
  }

  const wchar_t* p = line.c_str();
  const wchar_t* line_start = p;
  const wchar_t* arg_start = p;
  // The executable gets special rules per CreateProcess docs: no quote
  // escaping if it's quoted, and no space escaping if it's not.
  if (*p == L'"') {
    ++p;
    while (*p)
      if (*p++ == L'"')
        break;
  } else {
    while (*p && *p != L' ' && *p != L'\t')
      ++p;
  }
  CopyArg(line_start, arg_start, p, word_data);

  // Skip to the first argument.
  SkipWhitespace(&p);
  arg_start = p;

  int num_active_quotes = 0;
  int num_backslashes = 0;
  while (*p) {
    if ((*p == L' ' || *p == L'\t') && num_active_quotes == 0) {
      // We hit a space and aren't in an active quote, skip to the next
      // argument.
      CopyArg(line_start, arg_start, p, word_data);
      SkipWhitespace(&p);
      arg_start = p;
      num_backslashes = 0;
    } else if (*p == L'\\') {
      ++num_backslashes;
      ++p;
    } else if (*p == L'"') {
      if (num_backslashes % 2 == 0)  // Unescaped quote.
        ++num_active_quotes;
      ++p;
      num_backslashes = 0;
      while (*p == L'"') {
        ++num_active_quotes;
        ++p;
      }
      num_active_quotes = num_active_quotes % 3;
      if (num_active_quotes == 2)
        num_active_quotes = 0;
    } else {
      num_backslashes = 0;
      ++p;
    }
  }
  if (arg_start != p)
    CopyArg(line_start, arg_start, p, word_data);

  int num_args;
  LPWSTR* escaped = CommandLineToArgvW(line_start, &num_args);
  CHECK(num_args == static_cast<int>(word_data->size()));
  for (int i = 0; i < num_args; ++i) {
    word_data->at(i).deescaped_word = escaped[i];
  }
  LocalFree(escaped);

  // If the cursor was spaced past the end of the last argument, add an empty
  // argument so that calling code knows we're not on any word, past the end
  // of existing arguments. Similarly if the command line is completely empty.
  if (p - line_start >
      static_cast<int>(word_data->at(num_args - 1).original_offset +
                       word_data->at(num_args - 1).original_word.size())) {
    CopyArg(line_start, p, p, word_data);
  }
}

int CompletionWordIndex(const vector<WordData>& word_data, int position) {
  int result = -1;
  for (const auto& i : word_data) {
    if (position < i.original_offset)
      break;
    ++result;
  }
  return result;
}
