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
  return commands_;
}

void CommandHistory::AddCommand(const wstring& command) {
}

bool CommandHistory::MoveInHistory(int direction, const wstring& prefix, wstring* result) {
  position_ += direction;
  if (!prefix.empty()) {
  }
  return false;
}
