// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/command_history.h"

#include "common/util.h"

CommandHistory::CommandHistory() : position_(-1) {}

void CommandHistory::Populate(const vector<wstring>& commands) {
  commands_ = commands;
  position_ = commands_.size();
}

vector<wstring> CommandHistory::GetListForSaving() {
  return commands_;
}

void CommandHistory::AddCommand(const wstring& command) {
  // TODO: Maybe remove from old location so there's only one copy of it
  // closest to the end?
  commands_.push_back(command);
  position_ = commands_.size();
}

bool CommandHistory::MoveInHistory(int direction, const wstring& prefix, wstring* result) {
  if (commands_.empty())
    return false;
  CHECK(direction == -1 || direction == 1);
  int original_position = position_ % commands_.size();
  position_ += direction;
  for (;;) {
    if (position_ < 0)
      position_ = commands_.size() - 1;
    if (position_ >= static_cast<int>(commands_.size()))
      position_ = 0;
    if (prefix.empty())
      break;
    if (commands_[position_].substr(0, prefix.size()) == prefix)
      break;
    if (position_ == original_position)
      return false;
    position_ += direction;
  }
  *result = commands_[position_];
  return true;
}
