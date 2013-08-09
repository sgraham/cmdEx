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
  largest_y_ = start_y_;
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
    // Assume that it's not going to continue, and let tab handling put it
    // back on if it did continue. TODO: We hang on to results until next
    // completion which is kind of dumb.
    int previous_completion_begin = completion_word_begin_;
    // TODO: This test sucks, need a better way to determine mode.
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU)
      completion_word_begin_ = -1;

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
    } else if (!alt_down && ctrl_down && vk == 'D') {
      if (line_.empty() && position_ == 0) {
        line_ = L"exit";
        RedrawConsole();
        fake_command_ = L"exit\x0d\x0a";
        return kReturnToCmdThenResume;
      }
      return kIncomplete;
    } else if (!alt_down && !ctrl_down && vk == VK_RETURN) {
      line_ += L"\x0d\x0a";
      int x, y;
      console_->GetCursorLocation(&x, &y);
      // TODO: Scrolling seems to work, but I'm not sure why.
      console_->SetCursorLocation(0, y + 1);
      return kReturnToCmd;
    } else if (!alt_down && !ctrl_down && vk == VK_ESCAPE) {
      line_ = L"";
      position_ = 0;
    } else if (!alt_down && !ctrl_down && vk == VK_BACK) {
      if (position_ == 0 || line_.empty())
        return kIncomplete;
      position_--;
      line_.erase(position_, 1);
    } else if (!alt_down && ctrl_down && vk == 'W') {
      int from = FindBackwards(position_, " ");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && !ctrl_down && vk == VK_TAB) {
      // We're continuing completion, keep it on.
      completion_word_begin_ = previous_completion_begin;
      TabComplete(!shift_down);
    } else if (!alt_down && ctrl_down && vk == VK_BACK) {
      int from = FindBackwards(position_, " /\\");
      line_.erase(from, position_ - from);
      position_ = from;
    } else if (!alt_down && !ctrl_down && vk == VK_DELETE) {
      if (position_ == static_cast<int>(line_.size()) || line_.empty())
        return kIncomplete;
      line_.erase(position_, 1);
    } else if (!alt_down && !ctrl_down && vk == VK_LEFT) {
      position_ = max(0, position_ - 1);
    } else if (!alt_down && !ctrl_down && vk == VK_RIGHT) {
      position_ = min(static_cast<int>(line_.size()), position_ + 1);
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

void LineEditor::RegisterCompleter(Completer completer) {
  completers_.push_back(completer);
}

bool LineEditor::IsCompleting() const {
  return !completion_results_.empty() && completion_word_begin_ != -1;
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


void LineEditor::RedrawConsole() {
  int width = console_->GetWidth();
  CHECK(width > 0);
  bool cursor_past_end = position_ == static_cast<int>(line_.size());
  vector<LineChunk> chunks = BreakIntoLineChunks(
      line_ + (cursor_past_end ? wstring(L"\x01") : wstring(L"")),
      start_x_,
      start_y_,
      width);
  int last_drawn_x = start_x_;
  int last_drawn_y = start_y_;
  for (size_t i = 0; i < chunks.size(); ++i) {
    const LineChunk& chunk = chunks[i];
    size_t to_draw = chunk.contents.size();
    if (i == chunks.size() - 1 && cursor_past_end)
      --to_draw;
    console_->DrawString(chunk.contents.c_str(),
                         to_draw,
                         chunk.start_x,
                         chunk.start_y);
    last_drawn_x = chunk.start_x + to_draw;
    last_drawn_y = chunk.start_y;
    if (position_ >= chunk.start_offset &&
        position_ <
            chunk.start_offset + static_cast<int>(chunk.contents.size())) {
      console_->SetCursorLocation(
          chunk.start_x + (position_ - chunk.start_offset), chunk.start_y);
    }
  }

  int clear_from = last_drawn_x;
  for (int y = last_drawn_y; y <= largest_y_; ++y) {
    console_->FillChar(L' ', width - clear_from, clear_from, y);
    clear_from = 0;
  }
  largest_y_ = chunks.empty() ? start_y_ : chunks.back().start_y;
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

void LineEditor::TabComplete(bool forward_cycle) {
  bool started = false;
  if (!IsCompleting()) {
    CompleterInput input;
    CompletionBreakIntoWords(line_, &input.word_data);
    input.word_index = CompletionWordIndex(input.word_data, position_);
    input.position_in_word =
        position_ - input.word_data[input.word_index].original_offset;
    for (vector<Completer>::const_iterator i(completers_.begin());
        i != completers_.end();
        ++i) {
      completion_results_.clear();
      if ((*i)(input, &completion_results_)) {
        // We'll be completing from begin_ to position_ subbing in results_.
        // position_ is updated over time, so old end isn't saved.
        started = true;
        completion_word_begin_ =
            input.word_data[input.word_index].original_offset;
        completion_word_end_ =
            completion_word_begin_ +
            input.word_data[input.word_index].original_word.size();
        break;
      }
    }
  }

  if (!IsCompleting())
    return;

  if (started) {
    completion_index_ = forward_cycle ? 0 : completion_results_.size() - 1;
  } else {
    completion_index_ += forward_cycle ? 1 : -1;
    if (completion_index_ < 0)
      completion_index_ = completion_results_.size() - 1;
    else if (completion_index_ >= static_cast<int>(completion_results_.size()))
      completion_index_ = 0;
  }

  // Remove the old one (or the stub of one if we just started).
  line_.erase(completion_word_begin_,
              completion_word_end_ - completion_word_begin_);
  position_ = completion_word_begin_;

  // Insert the new one.
  line_.insert(completion_word_begin_, completion_results_[completion_index_]);
  position_ =
      completion_word_begin_ + completion_results_[completion_index_].size();
  completion_word_end_ = position_;
  RedrawConsole();
}
