// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_LINE_EDITOR_H_
#define CMDEX_LINE_EDITOR_H_

#include <windows.h>
#include <string>

class DirectoryHistory;

class LineEditor {
 public:
  LineEditor() : console_(INVALID_HANDLE_VALUE), position_(0) {}

  // Called initially and on each editing resumption. |history| is not owned.
  void Init(HANDLE console_handle, DirectoryHistory* history);

  enum HandleAction {
    kIncomplete,
    kReturnToCmd,
    kReturnToCmdThenResume,
  };

  // Returns whether a command is now ready for return to cmd.
  HandleAction HandleKeyEvent(const KEY_EVENT_RECORD& key_event);

  void ToCmdBuffer(wchar_t* buffer, DWORD buffer_size, DWORD* num_chars);

 private:
  HANDLE console_;
  int start_x_;
  int start_y_;
  std::wstring line_;
  int position_;
  std::wstring fake_command_;
  DirectoryHistory* history_;  // Weak.
};

#endif  // CMDEX_LINE_EDITOR_H_
