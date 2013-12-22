// Copyright 2012 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Adapted from http://github.com/martine/ninja/

#ifndef CMDEX_SUBPROCESS_H_
#define CMDEX_SUBPROCESS_H_

#include <string>
#include <vector>
#include <queue>
using namespace std;

#include <windows.h>

enum ExitStatus {
  ExitSuccess,
  ExitFailure,
  ExitInterrupted
};

// Subprocess wraps a single async subprocess.  It is entirely
// passive: it expects the caller to notify it when its fds are ready
// for reading, as well as call Finish() to reap the child once done()
// is true.
class Subprocess {
 public:
  ~Subprocess();

  // Returns ExitSuccess on successful process exit, ExitInterrupted if
  // the process was interrupted, ExitFailure if it otherwise failed.
  ExitStatus Finish();

  bool Done() const;

  const wstring& GetOutput() const;

 private:
  Subprocess();
  bool Start(class SubprocessSet* set, const wstring& command);
  void OnPipeReady();

  wstring buf_;

  // Set up pipe_ as the parent-side pipe of the subprocess; return the
  // other end of the pipe, usable in the child process.
  HANDLE SetupPipe(HANDLE ioport);

  HANDLE child_;
  HANDLE pipe_;
  OVERLAPPED overlapped_;
  char overlapped_buf_[4 << 10];
  bool is_reading_;

  friend class SubprocessSet;
};

// SubprocessSet runs a ppoll/pselect() loop around a set of Subprocesses.
// DoWork() waits for any state change in subprocesses; finished_
// is a queue of subprocesses as they finish.
class SubprocessSet {
 public:
  SubprocessSet();
  ~SubprocessSet();

  Subprocess* Add(const wstring& command);
  bool DoWork();
  Subprocess* NextFinished();
  void Clear();

  vector<Subprocess*> running_;
  queue<Subprocess*> finished_;

  static BOOL WINAPI NotifyInterrupted(DWORD dwCtrlType);
  static HANDLE ioport_;
};

#endif // CMDEX_SUBPROCESS_H_
