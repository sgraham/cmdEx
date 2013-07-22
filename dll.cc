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

#include "git2.h"
#include "util.h"

#define GIT2_FUNCTIONS \
  X(git_branch_name) \
  X(git_oid_tostr) \
  X(git_reference_free) \
  X(git_reference_name) \
  X(git_reference_name_to_id) \
  X(git_reference_shorthand) \
  X(git_repository_discover) \
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

bool IsDirectory(const std::string& path) {
  struct _stat buffer;
  return _stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & _S_IFDIR);
}

bool IsFile(const std::string& path) {
  struct _stat buffer;
  return _stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & _S_IFREG);
}

std::string JoinPath(const std::string& a, const std::string& b) {
  // TODO: normpath.
  return a + "/" + b;
}

// Reads |path| into |buffer| which is assumed to be large enough to hold
// _MAX_PATH. Trailing spaces are trimmed.
void ReadInto(const std::string& path, char* buffer) {
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

// Replacement for WNetGetConnectionW that gets the git branch for the
// specified local path instead.
// Somewhat based on:
// https://github.com/git/git/blob/master/contrib/completion/git-prompt.sh
DWORD APIENTRY GetGitBranch(
    const wchar_t*,  // This is only the drive, not the directory.
    wchar_t* remote,
    DWORD* length) {
  char local_path[_MAX_PATH];
  // We never return an error as far as the caller is concerned. This is
  // because the calling code adds a space in the success case (which we don't
  // want) so we always suffix $M with $H to remove it. But in that case, the
  // error case doesn't work (because the space isn't added) so just return
  // the error for printing in the string and don't indicate failure at the
  // API level.
  remote[0] = 0;
  if (GetCurrentDirectory(sizeof(local_path), local_path) == 0)
    return NO_ERROR;
  char git_dir[_MAX_PATH];
  if (g_git_repository_discover(git_dir, sizeof(git_dir), local_path, 0, "") !=
      0)
    return NO_ERROR;
  git_repository* repo;
  if (g_git_repository_open(&repo, git_dir) != 0)
    return NO_ERROR;

  git_reference* head_ref = NULL;
  if (g_git_repository_head(&head_ref, repo) != 0) {
    // TODO: More useful/fancy here?
    wcscpy(remote, L"[(no head)] ");
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
  sprintf(entire, "[%s%s] ", name, extra);

  // Not sure if this is ACP or UTF-8.
  MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, entire, -1, remote, *length);
  // No error to check; if it fails we return empty.

  g_git_reference_free(head_ref);
  return NO_ERROR;
}

void* RvaToAddr(void* base, unsigned int rva) {
  return reinterpret_cast<char*>(rva) + reinterpret_cast<uintptr_t>(base);
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
  // so that we ReadConsoleInput one char ali ta time so that we get the
  // events we want to handle, and then on \n, \t, etc. do pass back to cmd.
  // I'm not sure where doskey editing fits in. If if's before it gets to us,
  // that would work pretty well because we could just handle our keys as long
  // as they get to us.
  //
  // Further option would be to wholesale replace it, then we could fix tab
  // completion, etc. to be less dumb.

#if 0
  for (;;) {
    INPUT_RECORD input_record;
    DWORD num_read;
    BOOL ret = PeekConsoleInput(input, &input_record, 1, &num_read);
    printf("(%d) RCI.EventType: %d\n", ret, input_record.EventType);
    if (!ret)
      break;
  }
#endif

  Log("in ReadConsoleReplacement");
  BOOL ret = ReadConsoleW(input, buffer, buffer_size, chars_read, control);
  Log("returning %d", ret);
  return ret;
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

  // Patch ReadConsoleW in the same way. This is currently unused, but we'll
  // need it to improve directory completion.
  imp = GetImportByName(GetModuleHandle(NULL), NULL, "ReadConsoleW");
  if (!imp)
    imp = GetImportByName(GetModuleHandle(NULL), NULL, "ReadConsoleWStub");
  if (!imp)
    Error("couldn't get import for ReadConsoleW");
  func_addr = ReadConsoleReplacement;
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
