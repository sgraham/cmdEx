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

  // Hack for completing
  //   "C:\Program Files (x86)"\
  // CommandLineToArgvW will split the trailing \ into a second argument. In
  // order to match cmd's completion behaviour, merge an argument that's
  // immediately attached and starts with a \ back into the one before it.
  for (size_t i = 0; i < word_data->size() - 1; ++i) {
    WordData* first = &word_data->at(i);
    WordData* second = &word_data->at(i + 1);
    if (second->original_word[0] == L'\\' &&
        first->original_offset + first->original_word.size() ==
            static_cast<size_t>(second->original_offset)) {
      // Merge second onto first and remove second.
      first->original_word += second->original_word;
      first->deescaped_word += second->deescaped_word;
      word_data->erase(word_data->begin() + i + 1);
    }
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

// ref: http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
// This only does the CommandLineToArgvW part, because when you're editing a
// cmd command, it's not clear when you want metachar escaping.
wstring QuoteWord(const std::wstring& argument) {
  // Don't quote unless we actually need to.
  if (!argument.empty() &&
      argument.find_first_of(L" \t\n\v\"") == argument.npos) {
    return argument;
  } else {
    wstring result;
    result.push_back(L'"');
    for (std::wstring::const_iterator it = argument.begin();; ++it) {
      int num_backslashes = 0;
      while (it != argument.end() && *it == L'\\') {
        ++it;
        ++num_backslashes;
      }
      if (it == argument.end()) {
        // Escape all backslashes, but let the terminating double quotation
        // mark we add below be interpreted as a metacharacter.
        result.append(num_backslashes * 2, L'\\');
        break;
      } else if (*it == L'"') {
        // Escape all backslashes and the following double quotation mark.
        result.append(num_backslashes * 2 + 1, L'\\');
        result.push_back(*it);
      } else {
        // Backslashes aren't special here.
        result.append(num_backslashes, L'\\');
        result.push_back(*it);
      }
    }
    result.push_back(L'"');
    return result;
  }
}

vector<vector<WordData>> CompletionBreakWordsIntoCommands(
    const vector<WordData>& words) {
  vector<vector<WordData>> result;
  vector<WordData> current;
  for (const auto& i : words) {
    if (i.original_word == L"&&" || i.original_word == L"||" ||
        i.original_word == L"&") {
      result.push_back(current);
      current = vector<WordData>();
      continue;
    }
    current.push_back(i);
  }
  result.push_back(current);
  return result;
}
