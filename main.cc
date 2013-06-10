// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

void Fatal(const char* msg, ...) {
  va_list ap;
  fprintf(stderr, "cmdEx: FATAL: ");
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fprintf(stderr, "\n");
#ifdef _WIN32
  __debugbreak();
#endif
  exit(1);
}

typedef LONG(
    WINAPI* NtQueryInformationProcessType)(HANDLE, ULONG, PVOID, ULONG, PULONG);

// Retrieves the pid of the parent of the current process. In our case, this
// should be the cmd.exe that ran this program.
DWORD GetParentPid() {
  NtQueryInformationProcessType function_pointer =
      reinterpret_cast<NtQueryInformationProcessType>(GetProcAddress(
          LoadLibraryA("ntdll.dll"), "NtQueryInformationProcess"));
  if (function_pointer) {
    ULONG_PTR pbi[6];
    ULONG size;
    LONG ret =
        function_pointer(GetCurrentProcess(), 0, &pbi, sizeof(pbi), &size);
    if (ret >= 0 && size == sizeof(pbi))
      return static_cast<DWORD>(pbi[5]);
  }

  return 0;
}

}  // namespace

int main() {
  char* processor_architecture = getenv("PROCESSOR_ARCHITECTURE");
  if (!processor_architecture)
    Fatal("couldn't determine architecture");
  DWORD parent_pid = GetParentPid();
  char exe_name[1024];
  if (GetModuleFileName(NULL, exe_name, sizeof(exe_name)) == 0)
    Fatal("couldn't GetModuleFileName");
  char* extension = strrchr(exe_name, '.');
  if (!extension)
    Fatal("exe name seems malformed");
  *extension = 0;
  const char* arch = "x64";
  if (_stricmp(processor_architecture, "x86") == 0)
    arch = "x86";
  // TODO: Embedded resource-y stuff.
  // http://blog.kowalczyk.info/article/zy/Embedding-binary-resources-on-Windows.html
  // If we can FindResource our sub binaries, extract them to a temporary
  // folder and run from there.
  char buf[1024];
  sprintf(buf, "%s_%s.exe %d", exe_name, arch, parent_pid);
  return system(buf);
}
