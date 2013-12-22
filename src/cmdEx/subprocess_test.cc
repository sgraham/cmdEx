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

#include "cmdEx/subprocess.h"

#include "gtest/gtest.h"

namespace {

const wchar_t* kSimpleCommand = L"cmd /c dir \\";

struct SubprocessTest : public testing::Test {
  SubprocessSet subprocs_;
};

}  // anonymous namespace

// Run a command that fails and emits to stderr.
TEST_F(SubprocessTest, BadCommandStderr) {
  Subprocess* subproc = subprocs_.Add(L"cmd /c cmdex_no_such_command");
  ASSERT_NE((Subprocess *) 0, subproc);

  while (!subproc->Done()) {
    // Pretend we discovered that stderr was ready for writing.
    subprocs_.DoWork();
  }

  EXPECT_EQ(ExitFailure, subproc->Finish());
  EXPECT_NE(L"", subproc->GetOutput());
}

// Run a command that does not exist
TEST_F(SubprocessTest, NoSuchCommand) {
  Subprocess* subproc = subprocs_.Add(L"cmdex_no_such_command");
  ASSERT_NE((Subprocess *) 0, subproc);

  while (!subproc->Done()) {
    // Pretend we discovered that stderr was ready for writing.
    subprocs_.DoWork();
  }

  EXPECT_EQ(ExitFailure, subproc->Finish());
  EXPECT_NE(L"", subproc->GetOutput());
  ASSERT_EQ(
      L"CreateProcess failed: The system cannot find the file specified.\n",
      subproc->GetOutput());
}

TEST_F(SubprocessTest, SetWithSingle) {
  Subprocess* subproc = subprocs_.Add(kSimpleCommand);
  ASSERT_NE((Subprocess *) 0, subproc);

  while (!subproc->Done()) {
    subprocs_.DoWork();
  }
  ASSERT_EQ(ExitSuccess, subproc->Finish());
  ASSERT_NE(L"", subproc->GetOutput());

  ASSERT_EQ(1u, subprocs_.finished_.size());
}

TEST_F(SubprocessTest, SetWithMulti) {
  Subprocess* processes[3];
  const wchar_t* kCommands[3] = {
    kSimpleCommand,
    L"cmd /c echo hi",
    L"cmd /c time /t",
  };

  for (int i = 0; i < 3; ++i) {
    processes[i] = subprocs_.Add(kCommands[i]);
    ASSERT_NE((Subprocess *) 0, processes[i]);
  }

  ASSERT_EQ(3u, subprocs_.running_.size());
  for (int i = 0; i < 3; ++i) {
    ASSERT_FALSE(processes[i]->Done());
    ASSERT_EQ(L"", processes[i]->GetOutput());
  }

  while (!processes[0]->Done() || !processes[1]->Done() ||
         !processes[2]->Done()) {
    ASSERT_GT(subprocs_.running_.size(), 0u);
    subprocs_.DoWork();
  }

  ASSERT_EQ(0u, subprocs_.running_.size());
  ASSERT_EQ(3u, subprocs_.finished_.size());

  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(ExitSuccess, processes[i]->Finish());
    ASSERT_NE(L"", processes[i]->GetOutput());
    delete processes[i];
  }
}
