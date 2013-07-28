// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include <windows.h>

#include "cmdEx/directory_history.h"

void LineEditor::Init(ConsoleInterface* console, DirectoryHistory* history) {
  console_ = console;
  console->GetCursorLocation(&start_x_, &start_y_);
  history_ = history;
  history_->StartingEdit();
}

LineEditor::HandleAction LineEditor::HandleKeyEvent(bool pressed,
                                                    bool alt_down,
                                                    bool ctrl_down,
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
        return kReturnToCmdThenResume;
      }
    } else if (!alt_down && !ctrl_down && ascii_char == 0x0d) {
      line_ += L"\x0d\x0a";
      return kReturnToCmd;
    } else if (vk == VK_BACK) {
    } else if (isprint(ascii_char)) {
      line_ += ascii_char;
      printf("%c", ascii_char);  // XXX
    }
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
