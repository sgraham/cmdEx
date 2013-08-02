// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/string_util.h"

vector<wstring> StringSplit(const wstring& str, wchar_t break_at) {
  vector<wstring> result;
  wstring current;
  for (const auto& i : str) {
    if (i == break_at) {
      result.push_back(current);
      current = wstring();
      continue;
    }
    current.push_back(i);
  }
  result.push_back(current);
  return result;
}
