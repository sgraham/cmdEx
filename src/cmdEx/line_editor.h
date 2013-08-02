// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_LINE_EDITOR_H_
#define CMDEX_LINE_EDITOR_H_

#include <string>
#include <vector>

#include "cmdEx/completion.h"

class DirectoryHistory;

class ConsoleInterface {
 public:
  virtual void GetCursorLocation(int* x, int* y) = 0;
  virtual int GetWidth() = 0;
  // |str| not null terminated.
  virtual void DrawString(const wchar_t* str, int count, int x, int y) = 0;
  virtual void FillChar(wchar_t ch, int count, int x, int y) = 0;
  virtual void SetCursorLocation(int x, int y) = 0;
};

class LineEditor {
 public:
  LineEditor()
      : console_(NULL),
        start_x_(0),
        start_y_(0),
        position_(0),
        history_(NULL),
        completion_index_(-1) {}

  // Called initially and on each editing resumption. |history| is not owned.
  void Init(ConsoleInterface* console, DirectoryHistory* history);

  enum HandleAction {
    kIncomplete,
    kReturnToCmd,
    kReturnToCmdThenResume,
  };

  // Returns whether a command is now ready for return to cmd.
  HandleAction HandleKeyEvent(bool pressed,
                              bool ctrl_down,
                              bool alt_down,
                              bool shift_down,
                              unsigned char ascii_char,
                              unsigned short unicode_char,
                              int vk);

  void ToCmdBuffer(wchar_t* buffer,
                   unsigned long buffer_size,
                   unsigned long* num_chars);

  // So tests can inject non-filesystem ones. More specific ones should be
  // registered first.
  void RegisterCompleter(Completer completer);

  bool IsCompleting() const;

 private:
  void RedrawConsole();
  int FindBackwards(int start_at, const char* until);
  void TabComplete(bool forward_cycle);

  ConsoleInterface* console_;
  int start_x_;
  int start_y_;
  wstring line_;
  int position_;
  wstring fake_command_;
  DirectoryHistory* history_;  // Weak.

  vector<Completer> completers_;

  int completion_word_started_begin_;
  int completion_index_;
  vector<wstring> completion_results_;
};

#endif  // CMDEX_LINE_EDITOR_H_
