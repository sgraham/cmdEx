// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Windows.h>
#include <TlHelp32.h>
#include <direct.h>
#include <stdio.h>

#include "common/util.h"

#ifdef _M_IX86
const char g_main_dll_name[] = "cmdEx_dll_x86.dll";
const char g_ansi32_dll_name[] = "ansi32.dll";
#else
#error
#endif

namespace {

// Either SuspendThread or ResumeThread's all thread in the process identified
// by |pid|.
void SetThreadsRunning(DWORD pid, bool run) {
  HANDLE th32 = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, pid);
  THREADENTRY32 te;
  BOOL ok = Thread32First(th32, &te);
  while (ok) {
    HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
    // These are refcounted, so we won't wake ones that were already suspended.
    if (run)
      ResumeThread(thread);
    else
      SuspendThread(thread);
    CloseHandle(thread);
    Thread32Next(th32, &te);
  }
  CloseHandle(th32);
}

// Injects our helper DLL into the process identified by |target_pid|.
void Inject(DWORD target_pid, const char* dll_name) {
  char dll_path[1024];
  GetModuleFileName(NULL, dll_path, sizeof(dll_path));
  char* slash = strrchr(dll_path, '\\');
  if (slash)
    strcpy(slash + 1, dll_name);

  // Get a handle to the target process.
  HANDLE target_process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE,
                  target_pid);
  if (!target_process)
    Fatal("failed to target process");

  // Make sure it matches our architecture.
  BOOL parent_is_wow64, current_is_wow64;
  IsWow64Process(target_process, &parent_is_wow64);
  IsWow64Process(GetCurrentProcess(), &current_is_wow64);
  if (parent_is_wow64 != current_is_wow64)
    Fatal("x86/x64 mismatch, use loader that matches parent architecture");

  // Allocate a buffer in the target to contain the DLL path, and store the
  // DLL we want to load into it.
  LPVOID buffer = VirtualAllocEx(
      target_process, NULL, sizeof(dll_path), MEM_COMMIT, PAGE_READWRITE);
  if (!WriteProcessMemory(
          target_process, buffer, dll_path, sizeof(dll_path), NULL))
    Fatal("WriteProcessMemory failed");

  SetThreadsRunning(target_pid, false);

  // Get LoadLibrary as the entry point for the remote thread (it takes one
  // argument -- the DLL path buffer we created above).
  void* thread_proc =
      GetProcAddress(LoadLibraryA("kernel32.dll"), "LoadLibraryA");
  if (!thread_proc)
    Fatal("couldn't get LoadLibraryA");
  DWORD thread_id;
  HANDLE remote_thread =
      CreateRemoteThread(target_process,
                         NULL,
                         0,
                         reinterpret_cast<LPTHREAD_START_ROUTINE>(thread_proc),
                         buffer,
                         0,
                         &thread_id);
  if (!remote_thread)
    Fatal("CreateRemoteThread failed");

  // Wait until the LoadLibrary has completed (which means injection is done).
  WaitForSingleObject(remote_thread, INFINITE);

  SetThreadsRunning(target_pid, true);

  CloseHandle(remote_thread);
  VirtualFreeEx(target_process, buffer, 0, MEM_RELEASE);
  CloseHandle(target_process);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2)
    Fatal("no target injection pid specified");
  //ULONGLONG start_time = GetTickCount64();
  char* cwd = _getcwd(NULL, 0);
  Log("cwd: %s\n", cwd);
  free(cwd);
  DWORD target_pid = atoi(argv[1]);
  if (target_pid == 0)
    Fatal("argv[1] didn't look like pid");
  Log("injecting into %d", target_pid);
  Inject(target_pid, g_main_dll_name);
  Inject(target_pid, g_ansi32_dll_name);
  /*
  ULONGLONG end_time = GetTickCount64();
  ULONGLONG elapsed = end_time - start_time;
  fprintf(stderr,
          "inject time: %lldm%.03fs\n",
          elapsed / (60 * 1000),
          (elapsed % (60 * 1000)) / 1000.0);
          */
  return 0;
}
