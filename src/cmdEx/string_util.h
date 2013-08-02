// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_STRING_UTIL_H_
#define CMDEX_STRING_UTIL_H_

#include <string>
#include <vector>
using namespace std;

vector<wstring> StringSplit(const wstring& str, wchar_t break_at);

#endif  // CMDEX_STRING_UTIL_H_
