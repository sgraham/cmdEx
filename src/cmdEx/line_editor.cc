// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include <windows.h>

#include "cmdEx/directory_history.h"
#include "common/util.h"

void LineEditor::Init(ConsoleInterface* console, DirectoryHistory* history) {
  console_ = console;
  console->GetCursorLocation(&start_x_, &start_y_);
  history_ = history;
  history_->StartingEdit();
  RedrawConsole();
}

LineEditor::HandleAction LineEditor::HandleKeyEvent(bool pressed,
                                                    bool ctrl_down,
                                                    bool alt_down,
                                                    bool shift_down,
                                                    unsigned char ascii_char,
                                                    unsigned short unicode_char,
                                                    int vk) {
  if (pressed) {
    if (alt_down && !ctrl_down && vk == VK_UP) {
      fake_command_ = L"cd..\x0d\x0a";
      return kReturnToCmdThenResume;
    } else if (alt_down && !ctrl_down && (vk == VK_LEFT || vk == VK_RIGHT)) {
      // Navigate back or forward.
      bool changed = history_->NavigateInHistory(vk == VK_LEFT ? -1 : 1);
      if (changed) {
        fake_command_ = L"\x0d\x0a";
        printf("\n");
        return kReturnToCmdThenResume;
      }
      return kIncomplete;
    } else if (!alt_down && ctrl_down && vk == 'L') {
      fake_command_ = L"cls\x0d\x0a";
      return kReturnToCmdThenResume;
    } else if (!alt_down && !ctrl_down && vk == VK_RETURN) {
      line_ += L"\x0d\x0a";
      return kReturnToCmd;
    } else if (!alt_down && !ctrl_down && vk == VK_BACK) {
      if (position_ == 0 || line_.empty())
        return kIncomplete;
      position_--;
      line_.erase(position_, 1);
    } else if (!alt_down && ctrl_down && vk == 'W') {
      int from = FindBackwards(position_, " ");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && ctrl_down && vk == VK_BACK) {
      int from = FindBackwards(position_, " /\\");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && !ctrl_down && vk == VK_DELETE) {
      if (position_ == static_cast<int>(line_.size()) || line_.empty())
        return kIncomplete;
      line_.erase(position_, 1);
    } else if (!alt_down && !ctrl_down && vk == VK_LEFT) {
      position_ = std::max(0, position_ - 1);
    } else if (!alt_down && !ctrl_down && vk == VK_RIGHT) {
      position_ = std::min(static_cast<int>(line_.size()), position_ + 1);
    } else if ((!alt_down && !ctrl_down && vk == VK_HOME) ||
               (!alt_down && ctrl_down && vk == 'A')) {
      position_ = 0;
    } else if ((!alt_down && !ctrl_down && vk == VK_END) ||
               (!alt_down && ctrl_down && vk == 'E')) {
      position_ = line_.size();
    } else if (isprint(ascii_char)) {
      line_.insert(position_, 1, ascii_char);
      position_++;
    }
    RedrawConsole();
  }
  return kIncomplete;
}

void LineEditor::ToCmdBuffer(wchar_t* buffer,
                             unsigned long buffer_size,
                             unsigned long* num_chars) {
  if (!fake_command_.empty()) {
    wcscpy_s(buffer, buffer_size, fake_command_.c_str());
    *num_chars = fake_command_.size();
    fake_command_ = L"";
  } else {
    wcscpy_s(buffer, buffer_size, line_.c_str());
    *num_chars = line_.size();
  }
}

void LineEditor::RedrawConsole() {
  int width = console_->GetWidth();
  CHECK(width > 0);
  int x = start_x_;
  int y = start_y_;
  int offset = 0;
  for (;;) {
    int num_chars_to_draw =
        std::min(width - x, static_cast<int>(line_.size()) - offset);
    console_->DrawString(&line_[offset], num_chars_to_draw, x, y);
    if (position_ >= offset && position_ < offset + num_chars_to_draw)
      console_->SetCursorLocation(position_ - offset + x, y);
    if (offset + num_chars_to_draw >= static_cast<int>(line_.size())) {
      int fillx = x + offset + num_chars_to_draw;
      console_->FillChar(L' ', width - fillx - 1, fillx, y);
      break;
    }
    offset += num_chars_to_draw;
    ++y;
    x = 0;
  }
  console_->SetCursorLocation(position_ - offset + x, y);
}

int LineEditor::FindBackwards(int start_at, const char* until) {
  int result = start_at;
  while (result >= 0 && strchr(until, line_[result]) != NULL)
    result--;
  while (result >= 0 && strchr(until, line_[result]) == NULL)
    result--;
  result++;
  return result;
}
