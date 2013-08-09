// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_COMMAND_HISTORY_H_
#define CMDEX_COMMAND_HISTORY_H_

#include <string>
#include <vector>
using namespace std;

class CommandHistory {
 public:
  CommandHistory();

  void Populate(const vector<wstring>& commands);
  vector<wstring> GetListForSaving();

  void AddCommand(const wstring& command);

  bool MoveInHistory(int direction, const wstring& prefix, wstring* result);

 private:
  // Most recent are at the end.
  vector<wstring> commands_;
  int position_;
};

#endif  // CMDEX_COMMAND_HISTORY_H_
