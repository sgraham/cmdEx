// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/command_history.h"

CommandHistory::CommandHistory() : position_(-1) {}

void CommandHistory::Populate(const vector<wstring>& commands) {
  commands_ = commands;
  position_ = commands_.size() - 1;
}

vector<wstring> CommandHistory::GetListForSaving() {
  return vector<wstring>();
}

void CommandHistory::AddCommand(const wstring& command) {
}

bool CommandHistory::NavigateInHistory(int direction, const wstring& prefix) {
  return false;
}
