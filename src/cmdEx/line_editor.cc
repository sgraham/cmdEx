// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmdEx/line_editor.h"

#include <windows.h>

#include <algorithm>

#include "cmdEx/command_history.h"
#include "cmdEx/directory_history.h"
#include "common/util.h"

#pragma comment(lib, "user32.lib")

void LineEditor::Init(ConsoleInterface* console,
                      DirectoryHistory* directory_history,
                      CommandHistory* command_history) {
  console_ = console;
  console->GetCursorLocation(&start_x_, &start_y_);
  largest_y_ = start_y_;
  directory_history_ = directory_history;
  directory_history_->StartingEdit();
  command_history_ = command_history;
  RedrawConsole();
}

bool IsModifierKey(int vk) {
  // We don't want to do much when these are actually pressed, just when the
  // other key they're pressed with is pressed. TODO: I'm sure there's some
  // missing, not sure what the better way to do this is.
  switch (vk) {
    case VK_APPS:
    case VK_CAPITAL:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_LMENU:
    case VK_LSHIFT:
    case VK_LWIN:
    case VK_MENU:
    case VK_NUMLOCK:
    case VK_PAUSE:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_RSHIFT:
    case VK_RWIN:
    case VK_SCROLL:
    case VK_SHIFT:
      return true;
  }
  return false;
}

LineEditor::HandleAction LineEditor::HandleKeyEvent(bool pressed,
                                                    bool ctrl_down,
                                                    bool alt_down,
                                                    bool shift_down,
                                                    unsigned char ascii_char,
                                                    unsigned short unicode_char,
                                                    int vk) {
  if (pressed) {
    if (IsModifierKey(vk))
      return kIncomplete;
    // Assume that it's not going to continue, and let tab handling put it
    // back on if it did continue. TODO: We hang on to results until next
    // completion which is kind of dumb.
    int previous_completion_begin = completion_word_begin_;
    completion_word_begin_ = -1;

    bool second_ctrl_v_was_pending =
        second_ctrl_v_pending_saved_position_ != -1;
    if (second_ctrl_v_was_pending) {
      line_ = second_ctrl_v_pending_saved_line_;
      position_ = second_ctrl_v_pending_saved_position_;
    }
    second_ctrl_v_pending_saved_line_.clear();
    second_ctrl_v_pending_saved_position_ = -1;

    if (alt_down && !ctrl_down && vk == VK_UP) {
      fake_command_ = L"cd..\x0d\x0a";
      return kReturnToCmdThenResume;
    } else if (alt_down && !ctrl_down &&
               (vk == VK_LEFT || vk == VK_RIGHT ||
                vk == VK_BROWSER_BACK || vk == VK_BROWSER_FORWARD)) {
      // Navigate back or forward.
      bool changed = directory_history_->NavigateInHistory(
          (vk == VK_LEFT || vk == VK_BROWSER_BACK) ? -1 : 1);
      if (changed) {
        // cd . is necessary to get a newline.
        fake_command_ = L"cd .\x0d\x0a";
        return kReturnToCmdThenResume;
      }
      return kIncomplete;
    } else if (!alt_down && ctrl_down && vk == 'L') {
      fake_command_ = L"cls\x0d\x0a";
      return kReturnToCmdThenResume;
    } else if (!alt_down && ctrl_down && vk == 'D') {
      if (line_.empty() && position_ == 0) {
        line_ = L"exit";
        RedrawConsole();
        fake_command_ = L"exit\x0d\x0a";
        return kReturnToCmdThenResume;
      }
      return kIncomplete;
    } else if (!alt_down && !ctrl_down && vk == VK_RETURN) {
      if (!line_.empty())
        command_history_->AddCommand(line_);
      line_ += L"\x0d\x0a";
      int x, y;
      console_->GetCursorLocation(&x, &y);
      start_y_ += console_->SetCursorLocation(0, y + 1);
      return kReturnToCmd;
    } else if (!alt_down && !ctrl_down && vk == VK_ESCAPE) {
      // During prompt, Escape cancels.
      if (!second_ctrl_v_was_pending) {
        line_ = L"";
        position_ = 0;
      }
    } else if (!alt_down && !ctrl_down && vk == VK_BACK) {
      if (position_ == 0 || line_.empty())
        return kIncomplete;
      position_--;
      line_.erase(position_, 1);
    } else if (!alt_down && ctrl_down && vk == 'W') {
      int from = FindBackwards(max(0, position_ - 1), " ");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && !ctrl_down && vk == VK_TAB) {
      // We're continuing completion, keep it on.
      completion_word_begin_ = previous_completion_begin;
      TabComplete(!shift_down);
    } else if (!alt_down && ctrl_down && vk == VK_BACK) {
      int from = FindBackwards(max(0, position_ - 1), " /\\");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && !ctrl_down && vk == VK_DELETE) {
      if (position_ == static_cast<int>(line_.size()) || line_.empty())
        return kIncomplete;
      line_.erase(position_, 1);
    } else if (!alt_down && ctrl_down && (vk == VK_END || vk == 'K')) {
      line_.erase(position_, line_.size() - position_);
    } else if (!alt_down && ctrl_down && (vk == VK_HOME || vk == 'U')) {
      line_.erase(0, position_);
      position_ = 0;
    } else if (!alt_down && !ctrl_down && vk == VK_LEFT) {
      position_ = max(0, position_ - 1);
    } else if (!alt_down && !ctrl_down && vk == VK_RIGHT) {
      position_ = min(static_cast<int>(line_.size()), position_ + 1);
    } else if (!alt_down && ctrl_down && vk == VK_LEFT) {
      position_ = FindBackwards(max(0, position_ - 1), " ");
    } else if (!alt_down && ctrl_down && vk == VK_RIGHT) {
      position_ = min(static_cast<int>(line_.size() - 1),
                      FindForwards(position_, " ") + 1);
      while (position_ < static_cast<int>(line_.size()) - 1 &&
             line_[position_] == L' ')
        position_++;
    } else if ((!alt_down && !ctrl_down && vk == VK_HOME) ||
               (!alt_down && ctrl_down && vk == 'A')) {
      position_ = 0;
    } else if ((!alt_down && !ctrl_down && vk == VK_END) ||
               (!alt_down && ctrl_down && vk == 'E')) {
      position_ = static_cast<int>(line_.size());
    } else if (!alt_down && !ctrl_down && vk == VK_UP) {
      if (command_history_->MoveInHistory(-1, L"", &line_))
        position_ = static_cast<int>(line_.size());
    } else if (!alt_down && !ctrl_down && vk == VK_DOWN) {
      if (command_history_->MoveInHistory(1, L"", &line_))
        position_ = static_cast<int>(line_.size());
    } else if (!alt_down && !ctrl_down && (vk == VK_PRIOR || vk == VK_F8)) {
      command_history_->MoveInHistory(-1, line_.substr(0, position_), &line_);
    } else if (!alt_down && !ctrl_down && vk == VK_NEXT) {
      command_history_->MoveInHistory(1, line_.substr(0, position_), &line_);
    } else if (!alt_down && ctrl_down && vk == 'V') {
      wstring text;
      if (console_->GetClipboardText(&text)) {
        if (std::count(text.begin(), text.end(), L'\n') > 0 &&
            !second_ctrl_v_was_pending) {
          second_ctrl_v_pending_saved_line_ = line_;
          second_ctrl_v_pending_saved_position_ = position_;
          line_ = L"<Clipboard contains newline, Ctrl-V again to confirm>";
          position_ = 0;
        } else {
          //vector<wstring> to_paste = StringSplit(text, L'\n');
          line_.insert(position_, text);
          position_ += static_cast<int>(text.size());
        }
      }
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
    *num_chars = static_cast<int>(fake_command_.size());
    fake_command_.clear();
  } else {
    wcscpy_s(buffer, buffer_size, line_.c_str());
    *num_chars = static_cast<int>(line_.size());
    line_.clear();
    position_ = 0;
  }
}

void LineEditor::RegisterCompleter(Completer completer) {
  completers_.push_back(completer);
}

bool LineEditor::IsCompleting() const {
  return !completion_output_.results.empty() && completion_word_begin_ != -1;
}

// This code is redonk. Instead:
// - break line_ into N chunks given width, and initial X
// - draw them at initial X and subsequent Y at X=0
// - position the cursor based
// - clear to the end of line of the last Y we previously wrote to
// - save the new Y range

struct LineChunk {
  LineChunk(int offset, int start_x, int start_y)
      : start_offset(offset), start_x(start_x), start_y(start_y) {}

  int start_offset;
  wstring contents;
  int start_x;
  int start_y;
};

vector<LineChunk> BreakIntoLineChunks(const wstring& line,
                                      int startx,
                                      int starty,
                                      int width) {
  int x = startx;
  int y = starty;
  int offset = 0;
  vector<LineChunk> chunks;
  LineChunk chunk(offset, x, y);
  // TODO: This could be better.
  for (const auto c : line) {
    chunk.contents.push_back(c);
    ++offset;
    ++x;
    if (x == width) {
      x = 0;
      ++y;
      chunks.push_back(chunk);
      chunk = LineChunk(offset, x, y);
    }
  }
  if (!chunk.contents.empty())
    chunks.push_back(chunk);
  return chunks;
}

void LineEditor::ScrollByOneLine() {
  // This is a bit of a silly way to scroll, but we set the cursor one
  // past the end, causing scrolling, and then reset it.
  int height = console_->GetHeight();
  int cursor_x, cursor_y;
  console_->GetCursorLocation(&cursor_x, &cursor_y);
  console_->SetCursorLocation(cursor_x, height);
  console_->SetCursorLocation(cursor_x, cursor_y);
}

void LineEditor::RedrawConsole() {
  int width = console_->GetWidth();
  int height = console_->GetHeight();
  CHECK(width > 0);
  bool cursor_past_end = position_ == static_cast<int>(line_.size());
  vector<LineChunk> chunks = BreakIntoLineChunks(
      line_ + (cursor_past_end ? wstring(L"\x01") : wstring(L"")),
      start_x_,
      start_y_,
      width);
  while (chunks[chunks.size() - 1].start_y >= height) {
    ScrollByOneLine();
    for (auto& chunk : chunks)
      --chunk.start_y;
    --start_y_;
  }
  int last_drawn_x = start_x_;
  int last_drawn_y = start_y_;
  int modify_start_y_by = 0;
  for (size_t i = 0; i < chunks.size(); ++i) {
    const LineChunk& chunk = chunks[i];
    size_t to_draw = chunk.contents.size();
    if (i == chunks.size() - 1 && cursor_past_end)
      --to_draw;
    console_->DrawString(chunk.contents.c_str(), static_cast<int>(to_draw),
                         chunk.start_x, chunk.start_y);
    last_drawn_x = static_cast<int>(chunk.start_x + to_draw);
    last_drawn_y = chunk.start_y;
    if (position_ >= chunk.start_offset &&
        position_ <
            chunk.start_offset + static_cast<int>(chunk.contents.size())) {
      modify_start_y_by += console_->SetCursorLocation(
          chunk.start_x + (position_ - chunk.start_offset),
          chunk.start_y + modify_start_y_by);
    }
  }
  start_y_ += modify_start_y_by;
  largest_y_ += modify_start_y_by;

  int clear_from = last_drawn_x;
  for (int y = last_drawn_y; y <= largest_y_; ++y) {
    console_->FillChar(L' ', width - clear_from, clear_from, y);
    clear_from = 0;
  }
  largest_y_ =
      chunks.empty() ? start_y_ : chunks.back().start_y + modify_start_y_by;
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

int LineEditor::FindForwards(int start_at, const char* until) {
  int result = start_at;
  while (result < static_cast<int>(line_.size()) &&
         strchr(until, line_[result]) != NULL)
    result++;
  while (result < static_cast<int>(line_.size()) &&
         strchr(until, line_[result]) == NULL)
    result++;
  result--;
  return result;
}

void LineEditor::TabComplete(bool forward_cycle) {
  bool started = false;
  if (!IsCompleting()) {
    vector<WordData> words;
    CompletionBreakIntoWords(line_, &words);
    CompleterInput input;
    input.word_data = words;  // TODO
    input.word_index = CompletionWordIndex(input.word_data, position_);
    input.position_in_word =
        position_ - input.word_data[input.word_index].original_offset;
    for (vector<Completer>::const_iterator i(completers_.begin());
        i != completers_.end();
        ++i) {
      completion_output_.Reset();
      if ((*i)(input, &completion_output_)) {
        // We'll be completing from begin_ to position_ subbing in results_.
        // position_ is updated over time, so old end isn't saved.
        started = true;
        completion_word_begin_ =
            input.word_data[input.word_index].original_offset;
        completion_word_end_ = static_cast<int>(
            completion_word_begin_ +
            input.word_data[input.word_index].original_word.size());
        break;
      }
    }
  }

  if (!IsCompleting())
    return;

  if (started) {
    completion_index_ =
        forward_cycle ? 0
                      : static_cast<int>(completion_output_.results.size()) - 1;
  } else {
    completion_index_ += forward_cycle ? 1 : -1;
    if (completion_index_ < 0)
      completion_index_ = static_cast<int>(completion_output_.results.size()) - 1;
    else if (completion_index_ >=
             static_cast<int>(completion_output_.results.size()))
      completion_index_ = 0;
  }

  // Remove the old one (or the stub of one if we just started).
  line_.erase(completion_word_begin_,
              completion_word_end_ - completion_word_begin_);
  position_ = completion_word_begin_;

  // Insert the new one.
  wstring quoted = QuoteWord(completion_output_.results[completion_index_]);
  if (completion_output_.trailing_space)
    quoted += L" ";
  line_.insert(completion_word_begin_, quoted);
  position_ = static_cast<int>(completion_word_begin_ + quoted.size());
  completion_word_end_ = position_;
  RedrawConsole();
}
