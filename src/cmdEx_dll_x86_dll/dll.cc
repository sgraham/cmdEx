// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <DelayImp.h>
#include <DbgHelp.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#pragma warning(disable: 4530)
#include <string>
#include <vector>

#include "cmdEx/directory_history.h"
#include "cmdEx/line_editor.h"
#include "cmdEx/string_util.h"
#include "common/util.h"
#include "git2.h"

#define GIT2_FUNCTIONS \
  X(git_branch_name) \
  X(git_oid_tostr) \
  X(git_reference_foreach_glob) \
  X(git_reference_free) \
  X(git_reference_name) \
  X(git_reference_name_to_id) \
  X(git_reference_shorthand) \
  X(git_repository_discover) \
  X(git_repository_free) \
  X(git_repository_head) \
  X(git_repository_open) \


#define DBGHELP_FUNCTIONS \
  X(SymCleanup) \
  X(SymFromName) \
  X(SymInitialize) \
  X(SymLoadModule64) \
  X(SymSetOptions) \
  X(SymSetSearchPath) \


#define X(name) decltype(name)* g_ ## name;
GIT2_FUNCTIONS
DBGHELP_FUNCTIONS
#undef X

bool IsDirectory(const string& path) {
  struct _stat buffer;
  return _stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & _S_IFDIR);
}

bool IsFile(const string& path) {
  struct _stat buffer;
  return _stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & _S_IFREG);
}

string JoinPath(const string& a, const string& b) {
  // TODO: normpath.
  return a + "\\" + b;
}

wstring JoinPath(const wstring& a, const wstring& b) {
  // TODO: normpath.
  return a + L"\\" + b;
}

// Reads |path| into |buffer| which is assumed to be large enough to hold
// _MAX_PATH. Trailing spaces are trimmed.
void ReadInto(const string& path, char* buffer) {
  FILE *fp = fopen(path.c_str(), "rb");
  if (!fp) {
    buffer[0] = 0;
    return;
  }
  size_t read = fread(buffer, sizeof(char), _MAX_PATH, fp);
  fclose(fp);
  buffer[read--] = 0;
  while (isspace(buffer[read])) {
    buffer[read] = 0;
    --read;
  }
}

HMODULE LoadLibraryInSameLocation(HMODULE self, const char* dll_name) {
  char module_location[_MAX_PATH];
  GetModuleFileName(self, module_location, sizeof(module_location));
  char* slash = strrchr(module_location, '\\');
  if (!slash) {
    Error("couldn't find self location");
    return NULL;
  }
  *slash = 0;
  strcat(module_location, "\\");
  strcat(module_location, dll_name);
  return LoadLibrary(module_location);
}

// We are in cmd's directory, so loading git2.dll (delayed or otherwise) may
// fail. Instead, LoadLibrary and GetProcAddress the functions we need from
// the same directory as our dll is in.
void LoadGit2FunctionPointers(HMODULE self) {
  HMODULE git2 = LoadLibraryInSameLocation(self, "git2.dll");
  if (!git2)
    return;
#define X(name) \
  g_##name = reinterpret_cast<decltype(name)*>(GetProcAddress(git2, #name));
  GIT2_FUNCTIONS
#undef X
}

// TODO: I'm not sure this works for symsrv as it's lazily loaded by dbghelp.
void LoadDbgHelpFunctionPointers(HMODULE self) {
  HMODULE dbghelp = LoadLibraryInSameLocation(self, "dbghelp.dll");
  if (!dbghelp)
    return;
#define X(name) \
  g_##name = reinterpret_cast<decltype(name)*>(GetProcAddress(dbghelp, #name));
  DBGHELP_FUNCTIONS
#undef X
}

void TrimRefsHead(char* head) {
  const char* prefix = "refs/heads/";
  const size_t length = strlen(prefix);
  if (strncmp(head, prefix, length) == 0)
    memmove(head, &head[length], strlen(&head[length]) + 1);
}

static bool FindGitRepo(char* git_dir,
                        size_t git_dir_size,
                        git_repository** repo) {
  char local_path[_MAX_PATH];
  if (GetCurrentDirectory(sizeof(local_path), local_path) == 0)
    return false;
  if (g_git_repository_discover(git_dir, git_dir_size, local_path, 0, "") !=
      0)
    return false;
  if (g_git_repository_open(repo, git_dir) != 0)
    return false;
  return true;
}

// Replacement for WNetGetConnectionW that gets the git branch for the
// specified local path instead.
// Somewhat based on:
// https://github.com/git/git/blob/master/contrib/completion/git-prompt.sh
DWORD APIENTRY GetGitBranch(
    const wchar_t*,  // This is only the drive, not the directory.
    wchar_t* remote,
    DWORD* length) {
  // We never return an error as far as the caller is concerned. This is
  // because the calling code adds a space in the success case (which we don't
  // want) so we always suffix $M with $H to remove it. But in that case, the
  // error case doesn't work (because the space isn't added) so just return
  // the error for printing in the string and don't indicate failure at the
  // API level.
  remote[0] = 0;
  char git_dir[_MAX_PATH];
  git_repository* repo;
  if (!FindGitRepo(git_dir, sizeof(git_dir), &repo))
    return NO_ERROR;

  git_reference* head_ref = NULL;
  if (g_git_repository_head(&head_ref, repo) != 0) {
    // TODO: More useful/fancy here?
    wcscpy(remote, L"[(no head)]");
    g_git_repository_free(repo);
    return NO_ERROR;
  }

  char name[_MAX_PATH] = {0};
  char extra[_MAX_PATH] = {0};

  if (IsDirectory(JoinPath(git_dir, "rebase-merge"))) {
    ReadInto(JoinPath(git_dir, "rebase-merge/head-name"), name);
    TrimRefsHead(name);
    char step[_MAX_PATH], total[_MAX_PATH];
    ReadInto(JoinPath(git_dir, "rebase-merge/msgnum"), step);
    ReadInto(JoinPath(git_dir, "rebase-merge/end"), total);
    sprintf(extra, " %s/%s", step, total);
  } else if (IsDirectory(JoinPath(git_dir, "rebase-apply"))) {
    char step[_MAX_PATH], total[_MAX_PATH];
    ReadInto(JoinPath(git_dir, "rebase-apply/next"), step);
    ReadInto(JoinPath(git_dir, "rebase-apply/last"), total);
    const char* suffix = "";
    if (IsFile(JoinPath(git_dir, "rebase-apply/rebasing"))) {
      ReadInto(JoinPath(git_dir, "rebase-apply/head-name"), name);
      TrimRefsHead(name);
      suffix = "|REBASE";
    } else if (IsFile(JoinPath(git_dir, "rebase-apply/applying"))) {
      suffix = "|AM";
    } else {
      suffix = "|AM/REBASE";
    }
    sprintf(extra, " %s/%s%s", step, total, suffix);
  } else if (IsDirectory(JoinPath(git_dir, "MERGE_HEAD"))) {
    strcpy(extra, "|MERGING");
  } else if (IsDirectory(JoinPath(git_dir, "CHERRY_PICK_HEAD"))) {
    strcpy(extra, "|CHERRY-PICKING");
  } else if (IsDirectory(JoinPath(git_dir, "REVERT_HEAD"))) {
    strcpy(extra, "|REVERTING");
  } else if (IsDirectory(JoinPath(git_dir, "BISECT_LOG"))) {
    strcpy(extra, "|BISECTING");
  } else {
    const char* head_name = "";
    if (g_git_branch_name(&head_name, head_ref) != 0) {
      git_oid oid;
      if (g_git_reference_name_to_id(&oid, repo, "HEAD") == 0) {
        char truncated[8];
        strcpy(name, g_git_oid_tostr(truncated, sizeof(truncated), &oid));
        strcat(name, "...");
      } else {
        strcpy(name, "(unknown)");
      }
    } else {
      strcpy(name, head_name);
    }
  }

  char entire[_MAX_PATH];
  sprintf(entire, "[%s%s]", name, extra);

  // Not sure if this is ACP or UTF-8.
  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, entire, -1, remote, *length);
  // No error to check; if it fails we return empty.

  g_git_reference_free(head_ref);
  g_git_repository_free(repo);
  return NO_ERROR;
}

void* RvaToAddr(void* base, unsigned int rva) {
  return reinterpret_cast<char*>(rva) + reinterpret_cast<uintptr_t>(base);
}

// Directory history navigation. Given a directory history like this (from
// oldest to newest):
//
// c:\a
// c:\b
// c:\x
// c:\y   <--
//
// Alt-Left goes 'up' and Alt-Right goes 'down'. If the current directory
// becomes a directory other than the current item, the item before, or the
// item after, then everything below the current iterator is dropped, and the
// new location added (and becomes current). In the history above, Alt-Left
// goes to c:\x, then Alt-Left again goes to c:\b. Alt-Right or "cd c:\x"
// would move to c:\x and point the iterator at c:\x. Then, "cd c:\z": because
// that isn't navigating back or forward, it starts a 'new' forward tree, so
// c:\y (and all subsequent entries) are dropped, c:\z is added and becomes
// current.
//
// TODO: This doesn't feel quite right. See vim jumplist docs; in particular
// if revisiting an old location it goes at the end, but is removed from the
// middle.

class RealWorkingDirectory : public WorkingDirectoryInterface {
 public:
  virtual bool Set(const string& dir) override {
    return SetCurrentDirectory(dir.c_str());
  }
  virtual string Get() override {
    char cur_path[_MAX_PATH];
    if (GetCurrentDirectory(sizeof(cur_path), cur_path))
      return cur_path;
    return "";
  }
};

class RealConsole : public ConsoleInterface {
 public:
  RealConsole() : console_(INVALID_HANDLE_VALUE) {}

  void SetConsole(HANDLE console) {
    console_ = console;
  }

  virtual void GetCursorLocation(int* x, int* y) override {
    CONSOLE_SCREEN_BUFFER_INFO screen_buffer_info;
    PCHECK(GetConsoleScreenBufferInfo(console_, &screen_buffer_info));
    *x = screen_buffer_info.dwCursorPosition.X;
    *y = screen_buffer_info.dwCursorPosition.Y;
  }

  virtual int GetWidth() override {
    CONSOLE_SCREEN_BUFFER_INFO screen_buffer_info;
    PCHECK(GetConsoleScreenBufferInfo(console_, &screen_buffer_info));
    return screen_buffer_info.dwSize.X;
  }

  virtual void DrawString(const wchar_t* str, int count, int x, int y)
      override {
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    DWORD written;
    WriteConsoleOutputCharacterW(console_, str, count, coord, &written);
  }

  virtual void FillChar(wchar_t ch, int count, int x, int y) override {
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    DWORD written;
    FillConsoleOutputCharacterW(console_, ch, count, coord, &written);
  }

  virtual void SetCursorLocation(int x, int y) override {
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    PCHECK(SetConsoleCursorPosition(console_, coord));
  }

 private:
  HANDLE console_;
};

// TODO: Search for git-xyz in path too. Should include .sh in addition to
// PATHEXT
static const wchar_t* kGitCommandsPorcelain[] = {
  L"add", L"am", L"archive", L"bisect", L"branch", L"bundle", L"checkout",
  L"cherry-pick", L"citool", L"clean", L"clone", L"commit", L"describe",
  L"diff", L"fetch", L"format-patch", L"gc", L"grep", L"gui", L"init", L"log",
  L"merge", L"mv", L"notes", L"pull", L"push", L"rebase", L"reset", L"revert",
  L"rm", L"shortlog", L"show", L"stash", L"status", L"submodule", L"tag",
};

static bool GitCommandNameCompleter(const CompleterInput& input,
                                    vector<wstring>* results) {
  if (input.word_data.size() > 1 &&
      input.word_data[0].deescaped_word == L"git") {
    bool in_word_one = input.word_index == 1;
    bool no_command = input.word_data.size() == 1;
    if (no_command || in_word_one) {
      const wstring prefix =
          no_command ? L"" : input.word_data[1].deescaped_word;
      for (size_t i = 0; i < ARRAYSIZE(kGitCommandsPorcelain); ++i) {
        wstring tmp = kGitCommandsPorcelain[i];
        if (tmp.substr(0, prefix.size()) == prefix)
          results->push_back(tmp + L" ");
      }
      return true;
    }
  }
  return false;
}

static bool CompletePrefixArray(const CompleterInput& input,
                                const wstring& prefix,
                                const wchar_t* candidates[],
                                size_t candidates_size,
                                vector<wstring>* results) {
  for (size_t i = 0; i < candidates_size; ++i) {
    wstring tmp(candidates[i]);
    if (tmp.substr(0, prefix.size()) == prefix)
      results->push_back(tmp + L" ");
  }
  return !results->empty();
}

static bool CompletePrefixVector(const CompleterInput& input,
                                 const wstring& prefix,
                                 const vector<wstring>& candidates,
                                 vector<wstring>* results) {
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (candidates[i].substr(0, prefix.size()) == prefix)
      results->push_back(candidates[i] + L" ");
  }
  return !results->empty();
}

// See:
// https://github.com/git/git/blob/master/contrib/completion/git-completion.bash#L342
static int ForeachRefCallback(const char* name, void* payload) {
  vector<wstring>* results = reinterpret_cast<vector<wstring>*>(payload);
  wchar_t wide[_MAX_PATH];
  CHECK(strncmp(name, "refs/", 5) == 0);
  const char* second_slash = strchr(&name[5], '/');
  CHECK(second_slash);
  MultiByteToWideChar(
      CP_ACP, MB_PRECOMPOSED, second_slash + 1, -1, wide, sizeof(wide));
  results->push_back(wide);
  return 0;
}

static bool GitRefsHelper(const CompleterInput& input,
                          const wstring& prefix,
                          vector<wstring>* results) {
  CHECK(input.word_data.size() > 2 &&
        input.word_data[0].deescaped_word == L"git");
  char git_dir[_MAX_PATH];
  git_repository* repo;
  if (!FindGitRepo(git_dir, sizeof(git_dir), &repo))
    return false;
  vector<wstring> candidates;
  bool ok = g_git_reference_foreach_glob(
             repo, "refs/tags/*", ForeachRefCallback, &candidates) == 0 &&
         g_git_reference_foreach_glob(
             repo, "refs/heads/*", ForeachRefCallback, &candidates) == 0 &&
         g_git_reference_foreach_glob(
             repo, "refs/remotes/*", ForeachRefCallback, &candidates) == 0;
  g_git_repository_free(repo);
  CompletePrefixVector(input, prefix, candidates, results);
  return ok && !results->empty();
}

static bool GitCommandArgCompleter(const CompleterInput& input,
                                   vector<wstring>* results) {
  if (input.word_data.size() > 2 &&
      input.word_data[0].deescaped_word == L"git") {
    if (input.word_data[1].deescaped_word == L"checkout") {
      const wchar_t* kCheckoutLongArgs[] = {
        L"--quiet", L"--ours", L"--theirs", L"--no-track", L"--merge",
        L"--conflict=", L"--orphan", L"--patch",
      };
      if (input.word_data[2].deescaped_word[0] == L'-' &&
          CompletePrefixArray(input,
                              input.word_data[2].deescaped_word,
                              kCheckoutLongArgs,
                              ARRAYSIZE(kCheckoutLongArgs),
                              results))
        return true;
      if (GitRefsHelper(input, input.word_data[2].deescaped_word, results))
        return true;
    }
  }
  return false;
}

// Everything in "help" that "where" doesn't find.
static const wchar_t* kCmdBuiltins[] = {
  L"assoc", L"break", L"bcdedit", L"call", L"cd", L"chdir", L"cls", L"color",
  L"copy", L"date", L"del", L"dir", L"echo", L"endlocal", L"erase", L"exit",
  L"for", L"ftype", L"goto", L"graftabl", L"if", L"md", L"mkdir", L"mklink",
  L"move", L"path", L"pause", L"popd", L"prompt", L"pushd", L"rd", L"rem",
  L"ren", L"rename", L"rmdir", L"set", L"setlocal", L"shift", L"start", L"time",
  L"title", L"type", L"ver", L"verify", L"vol",
};

static void SearchPathByPrefix(const wstring& prefix,
                               vector<wstring>* results) {
  const wchar_t* path_var = _wgetenv(L"PATH");
  if (!path_var)
    return;
  const wchar_t* path_ext_var = _wgetenv(L"PATHEXT");
  if (!path_ext_var)
    return;
  CHECK(wcschr(path_var, L'"') == NULL);
  vector<wstring> paths = StringSplit(path_var, L';');
  vector<wstring> pathexts = StringSplit(path_ext_var, L';');
  for (const auto& builtin : kCmdBuiltins) {
    wstring as_str(builtin);
    if (as_str.substr(0, prefix.size()) == prefix)
      results->push_back(as_str + L" ");  // Don't need quoting here.
  }
  for (const auto& path : paths) {
    for (const auto& pathext : pathexts) {
      WIN32_FIND_DATAW find_data;
      HANDLE handle =
          FindFirstFileW(JoinPath(path, L"*" + pathext).c_str(), &find_data);
      if (handle != INVALID_HANDLE_VALUE) {
        do {
          wstring tmp = find_data.cFileName;
          if (tmp.substr(0, prefix.size()) == prefix) {
            // Strip pathext because it's ugly looking.
            // TODO: Quoting.
            results->push_back(tmp.substr(0, tmp.size() - pathext.size()) +
                               L" ");
          }
        } while (FindNextFileW(handle, &find_data));
        FindClose(handle);
      }
    }
  }
}

static bool CommandInPathCompleter(const CompleterInput& input,
                                   vector<wstring>* results) {
  bool in_word_zero = input.word_index == 0;
  if (in_word_zero) {
    // If there's a slash in the word, the search will fail anyway, but early
    // out so that we don't do a lot of FindFile'ing for no reason.
    if (input.word_data[0].deescaped_word.find(L"\\") != string::npos ||
        input.word_data[0].deescaped_word.find(L"/") != string::npos) {
      return false;
    }
  }
  if (input.word_data.empty() || in_word_zero) {
    const wstring prefix =
        input.word_data.empty() ? L"" : input.word_data[0].deescaped_word;
    SearchPathByPrefix(prefix, results);
    return !results->empty();
  }
  return false;
}

static wstring NormalizeSlashes(const wstring& path, bool command_is_git) {
  wstring result;
  bool in_slashes = false;
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == L'\\' || path[i] == L'/') {
      if (in_slashes)
        continue;
      in_slashes = true;
      result.push_back(command_is_git ? L'/' : L'\\');
    } else {
      in_slashes = false;
      result.push_back(path[i]);
    }
  }
  return result;
}

static void FindFiles(const wstring& prefix,
                      bool dir_only,
                      bool command_is_git,
                      vector<wstring>* results) {
  wchar_t drive[MAX_PATH];
  wchar_t dir[MAX_PATH];
  wchar_t file[MAX_PATH];
  wchar_t ext[MAX_PATH];
  wstring prepend;
  _wsplitpath(prefix.c_str(), drive, dir, file, ext);

  wstring search_prefix = prefix;
  if (drive[0] != 0 && dir[0] == 0 && file[0] == 0 && ext[0] == 0) {
    // For drive-only, we need to add a slash, otherwise it's interpreted as
    // current directory on that drive rather than the root.
    search_prefix = prefix + L"\\";
  }

  if (drive[0] != 0) {
    prepend = drive;
    if (prepend[prepend.size() - 1] != L'\\')
      prepend.push_back(L'\\');
  }
  if (dir[0] != 0)
    prepend += dir;
  prepend = NormalizeSlashes(prepend, command_is_git);

  WIN32_FIND_DATAW find_data;
  HANDLE handle = FindFirstFileW((search_prefix + L"*").c_str(), &find_data);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      // We could keep these but generally they're not too useful.
      if (find_data.cFileName[0] == L'.' &&
          (find_data.cFileName[1] == 0 ||
           (find_data.cFileName[1] == L'.' && find_data.cFileName[2] == 0)))
        continue;
      if (!dir_only || (dir_only && (find_data.dwFileAttributes &
                                     FILE_ATTRIBUTE_DIRECTORY))) {
        // TODO: Quoting.
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          results->push_back(prepend + find_data.cFileName +
                             (command_is_git ? L"/" : L"\\"));
        else
          results->push_back(prepend + find_data.cFileName);
      }
    } while (FindNextFileW(handle, &find_data));
    FindClose(handle);
  }
}

static bool DirectoryCompleter(const CompleterInput& input,
                               vector<wstring>* results) {
  if (input.word_data.size() < 1)
    return false;
  const wstring& command = input.word_data[0].deescaped_word;
  if (command == L"md" || command == L"rd" || command == L"cd" ||
      command == L"mkdir" || command == L"rmdir" || command == L"chdir") {
    bool in_word_one = input.word_index == 1;
    if (input.word_data.size() == 1 || in_word_one) {
      const wstring prefix =
          input.word_data.size() == 1 ? L"" : input.word_data[1].deescaped_word;
      FindFiles(prefix, true, false, results);
      return !results->empty();
    }
  }
  return false;
}

static bool FilenameCompleter(const CompleterInput& input,
                              vector<wstring>* results) {
  const wstring prefix = input.word_data.empty()
                             ? L""
                             : input.word_data[input.word_index].deescaped_word;
  FindFiles(prefix,
            false,
            input.word_data.size() >= 1 &&
                input.word_data[0].deescaped_word == L"git",
            results);
  return !results->empty();
}

static DirectoryHistory* g_directory_history;

static LineEditor* g_editor;
static RealConsole g_real_console;

static void (*g_original_exit)(int);

// Write command history on shell exit. TODO: This needs to only be when the
// shell was interactive.
void ExitReplacement(int exit_code) {
  printf("ExitReplacement stub, exit_code: %d\n", exit_code);

  g_original_exit(exit_code);
}

BOOL WINAPI ReadConsoleReplacement(HANDLE input,
                                   wchar_t* buffer,
                                   DWORD buffer_size,
                                   LPDWORD chars_read,
                                   PCONSOLE_READCONSOLE_CONTROL control) {

  // It'd be nice to handle Alt-Up for "cd .." here. ReadConsoleInput or
  // PeekConsoleInput can get those I think. But we are called with mode set
  // to ENABLE_LINE_INPUT so ReadConsoleW doesn't return until a full line is
  // typed.
  //
  // Hmm, how is tab completion handled? It turns out the |control| is an
  // escaper mechanism. The dwCtrlWakeupMask field contains |1 << control key|
  // and this will also trigger a return. So, relatively easy workaround would
  // be make the cd.. command bound to Ctrl-U (instead of Alt-Up), or that
  // into the control.dwCtrlWakeupMask, and then if that was the last thing
  // read then modify the working directory, and resume the read without
  // notifying cmd.
  //
  // It'd be nice to have it work while in the middle of a command rather than
  // only at the beginning of the line. That would probably mean returning
  // just "\n" to cmd after setting the new working directory, and then
  // reechoing the existing command. That in turn would mean having to
  // actually keep a copy of what's actually been typed which probably isn't
  // going to be too easy/bug free.
  //
  // Another option would be to more completely replace the functionality here
  // so that we ReadConsoleInput one char at a time so that we get the
  // events we want to handle, and then on \n, \t, etc. do pass back to cmd.
  // I'm not sure where doskey editing fits in. If if's before it gets to us,
  // that would work pretty well because we could just handle our keys as long
  // as they get to us. [Tried this, ReadConsoleInput is pretty raw compared
  // with ReadConsole and seems to be before doskey (or maybe doskey is even
  // dispatched from ReadConsole?).]
  //
  // Further option would be to wholesale replace it, then we could fix
  // various other editing things to be less annoying (quotes, tab completion
  // in middle of line, etc.). But, kind of a lot of work. Would need to do:
  // - typing & backspace
  // - history? maybe and search in history (is that in doskey?)
  // - ctrl-arrow skipping
  // - tab completion
  // And could add:
  // - smarter tab completion (. prefixes, git branches, PATH, etc.)
  // - Alt-Up
  // - Alt-Left for "back" (save at each prompt)
  // - history across sessions with better history search
  // - smarter insert when tab in middle
  // - ...

  // printf and backspace, etc. get a little of the way here, but it seems
  // like it's going to be better to use console apis. the main thing that
  // printf gets is that it handles wrapping, scrolling the buffer up, etc.
  // instead, figure out where we are when we start (cursor-wise), keep a
  // buffer of the current line, scroll the console up to make space if
  // necessary, and then output the line, reposition the cursor as necessary.
  // store the cursor in line position and convert to screen as necessary.

  if (getenv("CMDEX_READCONSOLE")) {
    HANDLE conout =
        CreateFile("CONOUT$",
                   GENERIC_WRITE | GENERIC_READ,
                   FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                   NULL,
                   OPEN_EXISTING,
                   0,
                   NULL);
    PCHECK(conout != INVALID_HANDLE_VALUE);
    // TODO: I'm not completely sure what's going on here. Using |input| as
    // the handle for console operations doesn't work (the various functions
    // return err=5 -- "The handle is invalid", but I'm not sure what handle
    // cmd is giving us here. It's probably wrong to just grab CONOUT$
    // because we'll be doing interactive editing while things are redirected.
    //CHECK(conout == GetStdHandle(STD_OUTPUT_HANDLE));
    *chars_read = 0;
    if (!g_directory_history) {
      RealWorkingDirectory* working_directory = new RealWorkingDirectory;
      g_directory_history = new DirectoryHistory(working_directory);
    }
    if (!g_editor) {
      g_editor = new LineEditor;
      g_editor->RegisterCompleter(GitCommandNameCompleter);
      g_editor->RegisterCompleter(GitCommandArgCompleter);
      g_editor->RegisterCompleter(CommandInPathCompleter);
      g_editor->RegisterCompleter(DirectoryCompleter);
      g_editor->RegisterCompleter(FilenameCompleter);
    }
    g_real_console.SetConsole(conout);
    g_editor->Init(&g_real_console, g_directory_history);
    for (;;) {
      INPUT_RECORD input_record;
      DWORD num_read;
      if (*chars_read >= buffer_size - 2) { // Lame, probably wrong.
        delete g_editor;
        g_editor = NULL;
        goto done;
      }
      BOOL ret = ReadConsoleInput(input, &input_record, 1, &num_read);
      if (!ret) {
        delete g_editor;
        g_editor = NULL;
        CloseHandle(conout);
        return ret;
      }
      if (input_record.EventType == KEY_EVENT) {
        const KEY_EVENT_RECORD& key_event = input_record.Event.KeyEvent;
        bool alt_down = (key_event.dwControlKeyState & LEFT_ALT_PRESSED) ||
                        (key_event.dwControlKeyState & RIGHT_ALT_PRESSED);
        bool ctrl_down = (key_event.dwControlKeyState & LEFT_CTRL_PRESSED) ||
                         (key_event.dwControlKeyState & RIGHT_CTRL_PRESSED);
        bool shift_down = key_event.dwControlKeyState & SHIFT_PRESSED;
        if (key_event.bKeyDown && !alt_down && ctrl_down && !shift_down &&
            key_event.wVirtualKeyCode == VK_RETURN) {
          char buf[_MAX_PATH + 100];
          sprintf(buf,
                  "explorer /e,\"%s\"",
                  g_directory_history->GetWorkingDirectoryInterface()->Get()
                      .c_str());
          system(buf);
          continue;
        }
        LineEditor::HandleAction action =
            g_editor->HandleKeyEvent(key_event.bKeyDown,
                                     ctrl_down,
                                     alt_down,
                                     shift_down,
                                     key_event.uChar.AsciiChar,
                                     key_event.uChar.UnicodeChar,
                                     key_event.wVirtualKeyCode);
        if (action == LineEditor::kReturnToCmd) {
          g_editor->ToCmdBuffer(buffer, buffer_size, chars_read);
          delete g_editor;
          g_editor = NULL;
          goto done;
        } else if (action == LineEditor::kReturnToCmdThenResume) {
          g_editor->ToCmdBuffer(buffer, buffer_size, chars_read);
          // Don't delete g_editor, and resume the command.
          goto done;
        }
      }
    }
    (void)control;
  done:
    CloseHandle(conout);
    return 1;
  } else {
    Log("non-overridden ReadConsole");
    BOOL ret = ReadConsoleW(input, buffer, buffer_size, chars_read, control);
    Log("returning %d", ret);
    return ret;
  }
}

void* GetDataDirectory(void* base, int index, int* size) {
  IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  IMAGE_NT_HEADERS* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
      reinterpret_cast<char*>(base) + dos_header->e_lfanew);
  IMAGE_DATA_DIRECTORY* data_dir =
      nt_headers->OptionalHeader.DataDirectory + index;
  if (!data_dir)
    return NULL;
  if (!data_dir->VirtualAddress)
    return NULL;
  if (size)
    *size = data_dir->Size;
  return RvaToAddr(base, data_dir->VirtualAddress);
}

void** IterateNames(void* base,
                    IMAGE_IMPORT_DESCRIPTOR* iid,
                    const char* func_name) {
  void** at = reinterpret_cast<void**>(RvaToAddr(base, iid->FirstThunk));
  intptr_t* nt =
      reinterpret_cast<intptr_t*>(RvaToAddr(base, iid->OriginalFirstThunk));
  while (*at && *nt) {
    IMAGE_IMPORT_BY_NAME* iin = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
        RvaToAddr(base, static_cast<unsigned int>(*nt)));
    if (_stricmp(iin->Name, func_name) == 0)
      return at;
    ++at;
    ++nt;
  }
  return NULL;
}

// Bah, exactly the same except different member names.
void** IterateNamesDelay(void* base,
                         ImgDelayDescr* idd,
                         const char* func_name) {
  void** at = reinterpret_cast<void**>(RvaToAddr(base, idd->rvaIAT));
  intptr_t* nt =
      reinterpret_cast<intptr_t*>(RvaToAddr(base, idd->rvaINT));
  while (*at && *nt) {
    IMAGE_IMPORT_BY_NAME* iin = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
        RvaToAddr(base, static_cast<unsigned int>(*nt)));
    if (_stricmp(iin->Name, func_name) == 0)
      return at;
    ++at;
    ++nt;
  }
  return NULL;
}

void** GetImportByName(void* base, const char* dll, const char* func_name) {
  size_t len = dll ? strlen(dll) : 0;

  {
    // Try to find it in regular imports.
    IMAGE_IMPORT_DESCRIPTOR* iid = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        GetDataDirectory(base, IMAGE_DIRECTORY_ENTRY_IMPORT, NULL));
    if (!iid) {
      Error("failed to find import descriptor for %p", base);
      return NULL;
    }
    while (iid->Name) {
      char* name = reinterpret_cast<char*>(RvaToAddr(base, iid->Name));
      if (!dll || _strnicmp(name, dll, len) == 0) {
        void** ret = IterateNames(base, iid, func_name);
        if (ret)
          return ret;
      }
      ++iid;
    }
  }

  {
    // Then in delayloads.
    ImgDelayDescr* idd = reinterpret_cast<ImgDelayDescr*>(
        GetDataDirectory(base, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT, NULL));
    if (!idd) {
      Error("failed to find delay import descriptor for %p", base);
      return NULL;
    }
    while (idd->rvaDLLName) {
      char* name = reinterpret_cast<char*>(RvaToAddr(base, idd->rvaDLLName));
      if (!dll || _strnicmp(name, dll, len) == 0) {
        void** ret = IterateNamesDelay(base, idd, func_name);
        if (ret)
          return ret;
      }
      ++idd;
    }
  }

  return NULL;
}

void ReadMemory(void* addr, void* dest, size_t bytes) {
  DWORD bytes_written;
  BOOL ret =
      ReadProcessMemory(GetCurrentProcess(), addr, dest, bytes, &bytes_written);
  if (!ret)
    Error("couldn't WriteProcessMemory");
}

void WriteMemory(void* addr, const void* src, size_t bytes) {
  DWORD previous_protect;
  if (VirtualProtect(addr, bytes, PAGE_EXECUTE_READWRITE, &previous_protect)) {
    DWORD bytes_written;
    BOOL ret = WriteProcessMemory(
        GetCurrentProcess(), addr, src, bytes, &bytes_written);
    if (!ret)
      Error("couldn't WriteProcessMemory");

    DWORD unused;
    ret = VirtualProtect(addr, bytes, previous_protect, &unused);
    if (!ret)
      Error("couldn't VirtualProtect to previous_protect");
    FlushInstructionCache(GetCurrentProcess(), 0, 0);
  } else {
    Error("couldn't VirtualProtect to P_E_RW");
  }
}

void PatchTerminateBatchJobPrompt() {
  // Ctrl-C during a batch file looks like:
  //
  // CtrlCAbort:
  //   ... some stuff ...
  //   push 2328h
  //   push 237bh
  //   push 0
  //   call PromptUser
  //   cmp eax, 1
  //   ... if PromptUser returns 1, then terminate ...
  //
  // So, we need to find the location for _CtrlCAbort@0, and replace the pushes
  // and call with nops and mov eax, 1. This avoids both the printing of the
  // prompt and also the need to answer 'y'.
  //
  // CtrlCAbort is not an exported function, so we pull down cmd's pdb with
  // DbgHelp/SymSrv to get its address, and then find the push+call to patch.

  Log("downloading pdb to patch Ctrl-C, might take a while the first time...");

  g_SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_DEBUG);
  HANDLE process = GetCurrentProcess();

  char temp_path[1024];
  if (!GetTempPath(sizeof(temp_path), temp_path)) {
    Log("couldn't GetTempPath");
    return;
  }

  g_SymInitialize(process, NULL, FALSE);
  char search_path[_MAX_PATH];
  sprintf(search_path, "SRV*%s*http://msdl.microsoft.com/download/symbols",
      temp_path);
  g_SymSetSearchPath(process, search_path);

  char module_location[_MAX_PATH];
  HMODULE self = GetModuleHandle(NULL);
  GetModuleFileName(self, module_location, sizeof(module_location));
  DWORD64 base_address =
      g_SymLoadModule64(process, NULL, module_location, NULL, 0, 0);
  if (base_address == 0) {
    Log("SymLoadModule64 failed: %d", GetLastError());
    g_SymCleanup(process);
    return;
  }

  char symbol_name[MAX_SYM_NAME];
  ULONG64 buffer[(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) +
                  sizeof(ULONG64) - 1) /
                 sizeof(ULONG64)];
  strcpy(symbol_name, "CtrlCAbort");
  PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;
  if (g_SymFromName(process, symbol_name, symbol)) {
    DWORD64 offset_from_base = symbol->Address - symbol->ModBase;
    DWORD64 relative_to_cmd =
        reinterpret_cast<DWORD64>(self) + offset_from_base;
    Log("CtrlCAbort is at: %p", relative_to_cmd);
    // Search up to 100 bytes for the thing we want to patch (it should be
    // only a few away from the beginning of the function).
    unsigned char* code_mem = reinterpret_cast<unsigned char*>(relative_to_cmd);
    for (int i = 0; i < 100; ++i) {
      if ((code_mem[i + 0] == 0x68 && code_mem[i + 1] == 0x28 &&
           code_mem[i + 2] == 0x23 && code_mem[i + 3] == 0x00 &&
           code_mem[i + 4] == 0x00) &&  // push 2328h
          (code_mem[i + 5] == 0x68 && code_mem[i + 6] == 0x7b &&
           code_mem[i + 7] == 0x23 && code_mem[i + 8] == 0x00 &&
           code_mem[i + 9] == 0x00) &&  // push 237bh
          (code_mem[i + 10] == 0x6a && code_mem[i + 11] == 0x00) &&  // push 0
          (code_mem[i + 12] == 0xe8)  // call PromptUser (w/o target address)
          ) {
        unsigned char replace_with[17] = {
          0x90, 0x90, 0x90, 0x90, 0x90,  // first push
          0x90, 0x90, 0x90, 0x90, 0x90,  // second push
          0x90, 0x90,                    // third push
          0xb8, 0x01, 0x00, 0x00, 0x00,  // call, replace with mov eax, 1
        };
        WriteMemory(&code_mem[i], replace_with, sizeof(replace_with));
        break;
      }
    }
  } else {
    Log("SymFromName failed: %d", GetLastError());
    g_SymCleanup(process);
    return;
  }

  g_SymCleanup(process);
}

void* g_veh;
unsigned char* g_hook_trap_addr;
unsigned char g_hook_trap_value;

LONG WINAPI HookTrap(EXCEPTION_POINTERS* info) {
  const EXCEPTION_RECORD* er = info->ExceptionRecord;
  // Not a HALT.
  if (er->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
    return EXCEPTION_CONTINUE_SEARCH;
  // Not at the expected address.
  if (er->ExceptionAddress != g_hook_trap_addr)
    return EXCEPTION_CONTINUE_SEARCH;

#ifdef _M_IX86
  void** sp = reinterpret_cast<void**>(info->ContextRecord->Esp);
#else
#error
#endif
  void* caller = *sp;
  unsigned char* bytes_at_callsite = reinterpret_cast<unsigned char*>(caller);
  if (bytes_at_callsite[0] == 0x83 &&
      bytes_at_callsite[1] == 0xf8 &&
      bytes_at_callsite[2] == 0x04 &&
      bytes_at_callsite[3] == 0x0f &&
      bytes_at_callsite[4] == 0x85) {
    // Matches cmp eax, 4
    //         jnz ...
    unsigned char fixed_drive = DRIVE_FIXED;
    WriteMemory(&bytes_at_callsite[2], &fixed_drive, sizeof(fixed_drive));
  }

  // Restore original instruction in GetDriveType.
  WriteMemory(g_hook_trap_addr, &g_hook_trap_value, sizeof(g_hook_trap_value));

  // Patch WNetGetConnectionW's IAT entry.
  void** imp;
  imp = GetImportByName(GetModuleHandle(NULL), NULL, "WNetGetConnectionW");
  if (!imp) {
    imp =
        GetImportByName(GetModuleHandle(NULL), NULL, "WNetGetConnectionWStub");
  }
  if (!imp)
    Error("couldn't get import for WNetGetConnectionW");
  void* func_addr = GetGitBranch;
  WriteMemory(imp, &func_addr, sizeof(imp));

  // Patch ReadConsoleW in the same way.
  imp = GetImportByName(GetModuleHandle(NULL), NULL, "ReadConsoleW");
  if (!imp)
    imp = GetImportByName(GetModuleHandle(NULL), NULL, "ReadConsoleWStub");
  if (!imp)
    Error("couldn't get import for ReadConsoleW");
  func_addr = ReadConsoleReplacement;
  WriteMemory(imp, &func_addr, sizeof(imp));

  // Patch msvcrt!exit in the same way. Save original so we can call it to
  // actually exit.
  imp = GetImportByName(GetModuleHandle(NULL), NULL, "exit");
  if (!imp)
    imp = GetImportByName(GetModuleHandle(NULL), NULL, "exit");
  if (!imp)
    Error("couldn't get import for exit");
  func_addr = ExitReplacement;
  ReadMemory(imp, &g_original_exit, sizeof(imp));
  WriteMemory(imp, &func_addr, sizeof(imp));

  // Patch "Terminate?" prompt code.
  PatchTerminateBatchJobPrompt();

  FlushInstructionCache(GetCurrentProcess(), 0, 0);

  RemoveVectoredExceptionHandler(g_veh);

  g_veh = 0;
  g_hook_trap_addr = 0;
  g_hook_trap_value = 0xcc;

  return EXCEPTION_CONTINUE_EXECUTION;
}

const char* GetKernelDll() {
  OSVERSIONINFOEX osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  osvi.dwMajorVersion = 6;
  osvi.dwMinorVersion = 2;

  DWORDLONG mask = 0;
  VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);

  if (VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, mask))
    return "kernelbase.dll";
  return "kernel32.dll";
}

void OnAttach(HMODULE self) {
  // Push the modified prompt into the environment. The implementation of $M
  // adds a space for no particularly good reason, so remove it with a $H.
  _putenv("PROMPT=$M$H$P$G");

  LoadGit2FunctionPointers(self);
  LoadDbgHelpFunctionPointers(self);

  // Trap in GetDriveTypeW (this guards the call to WNetGetConnectionW we want
  // to override). When it's next called and it matches the callsite we want,
  // patch the compare from DRIVE_REMOTE to DRIVE_FIXED. Then, restore normal
  // execution of the function, and IAT patch WNetGetConnectionW to point to
  // our git-branch-getter.
  HMODULE kernel = GetModuleHandle(GetKernelDll());
  if (!kernel)
    Error("failed to find base for kernel dll");
  unsigned char* get_drive_type =
      reinterpret_cast<unsigned char*>(GetProcAddress(kernel, "GetDriveTypeW"));
  g_hook_trap_addr = get_drive_type;
  g_hook_trap_value = *g_hook_trap_addr;
  g_veh = AddVectoredExceptionHandler(1, HookTrap);
  unsigned char halt = 0xf4;
  WriteMemory(g_hook_trap_addr, &halt, sizeof(halt));
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID unused) {
  (void)instance;
  (void)unused;
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      OnAttach(instance);
      break;
  }
  return TRUE;
}
