// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include "cmdEx/directory_history.h"

void LineEditor::Init(HANDLE console_handle, DirectoryHistory* history) {
  console_ = console_handle;
  CONSOLE_SCREEN_BUFFER_INFO screen_buffer_info;
  GetConsoleScreenBufferInfo(console_, &screen_buffer_info);
  start_x_ = screen_buffer_info.dwCursorPosition.X;
  start_y_ = screen_buffer_info.dwCursorPosition.Y;
  history_ = history;
  history_->StartingEdit();
}

LineEditor::HandleAction LineEditor::HandleKeyEvent(
    const KEY_EVENT_RECORD& key_event) {
  if (key_event.bKeyDown) {
    bool alt_down = (key_event.dwControlKeyState & LEFT_ALT_PRESSED) ||
                    (key_event.dwControlKeyState & RIGHT_ALT_PRESSED);
    bool ctrl_down = (key_event.dwControlKeyState & LEFT_CTRL_PRESSED) ||
                     (key_event.dwControlKeyState & RIGHT_CTRL_PRESSED);
    if (alt_down && !ctrl_down && key_event.wVirtualKeyCode == VK_UP) {
      fake_command_ = L"cd..\x0d\x0a";
      return kReturnToCmdThenResume;
    } else if (alt_down && !ctrl_down &&
               (key_event.wVirtualKeyCode == VK_LEFT ||
                key_event.wVirtualKeyCode == VK_RIGHT)) {
      // Navigate back or forward.
      bool changed = history_->NavigateInHistory(
          key_event.wVirtualKeyCode == VK_LEFT ? -1 : 1);
      if (changed) {
        fake_command_ = L"\x0d\x0a";
        return kReturnToCmdThenResume;
      }
    } else if (!alt_down && !ctrl_down && key_event.uChar.AsciiChar == 0x0d) {
      line_ += L"\x0d\x0a";
      return kReturnToCmd;
    } else if (key_event.uChar.AsciiChar == VK_BACK) {
    } else if (isprint(key_event.uChar.AsciiChar)) {
      line_ += key_event.uChar.AsciiChar;
      printf("%c", key_event.uChar.AsciiChar);  // XXX
    }
  }
  return kIncomplete;
}

void LineEditor::ToCmdBuffer(wchar_t* buffer,
                             DWORD buffer_size,
                             DWORD* num_chars) {
  wcscpy_s(buffer, buffer_size, line_.c_str());
  *num_chars = line_.size();
}
