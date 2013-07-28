// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_LINE_EDITOR_H_
#define CMDEX_LINE_EDITOR_H_

#include <string>

class DirectoryHistory;

class ConsoleInterface {
 public:
  virtual void GetCursorLocation(int* x, int* y) = 0;
};

class LineEditor {
 public:
  LineEditor() : console_(NULL), position_(0) {}

  // Called initially and on each editing resumption. |history| is not owned.
  void Init(ConsoleInterface* console, DirectoryHistory* history);

  enum HandleAction {
    kIncomplete,
    kReturnToCmd,
    kReturnToCmdThenResume,
  };

  // Returns whether a command is now ready for return to cmd.
  HandleAction HandleKeyEvent(bool pressed,
                              bool alt_down,
                              bool ctrl_down,
                              bool shift_down,
                              unsigned char ascii_char,
                              unsigned short unicode_char,
                              int vk);

  void ToCmdBuffer(wchar_t* buffer,
                   unsigned long buffer_size,
                   unsigned long* num_chars);

 private:
  ConsoleInterface* console_;
  int start_x_;
  int start_y_;
  std::wstring line_;
  int position_;
  std::wstring fake_command_;
  DirectoryHistory* history_;  // Weak.
};

#endif  // CMDEX_LINE_EDITOR_H_
