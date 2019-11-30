// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmdEx_exe/resource.h"
#include "common/util.h"

namespace {

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

void ChdirToTemp(char* into, size_t size) {
  if (!GetTempPath(size, into))
    return;
#ifndef NDEBUG
  fprintf(stderr, "chdir to %s\n", into);
#endif
  _chdir(into);
  // TODO: Possibly make a subdir, but then we need to be able to clean up.
}

bool GetFileResource(int resource_id, unsigned char** data, size_t* size) {
  HRSRC res = FindResource(NULL, MAKEINTRESOURCE(resource_id), RT_RCDATA);
  if (!res)
    return false;
  HGLOBAL res_handle = LoadResource(NULL, res);
  if (!res_handle)
    return false;
  if (data)
    *data = reinterpret_cast<unsigned char*>(LockResource(res_handle));
  if (size)
    *size = SizeofResource(NULL, res);
  return true;
}

FILETIME GetMtimeOfFile(const char* filename) {
  HANDLE file = CreateFile(
      filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  FILETIME ft_write;
  if (!GetFileTime(file, NULL, NULL, &ft_write)) {
    FILETIME none = {0};
    return none;
  }
  CloseHandle(file);
  return ft_write;
}

void SetMtimeOfFile(const char* filename, FILETIME mtime) {
  HANDLE file = CreateFile(
      filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (!SetFileTime(file, NULL, NULL, &mtime))
    Fatal("Couldn't SetFileTime of '%s'\n", filename);
  CloseHandle(file);
}

FILETIME GetOwnMtime() {
  char self_name[_MAX_PATH];
  if (!GetModuleFileName(NULL, self_name, sizeof(self_name)))
    Fatal("Couldn't GetModuleFileName\n");
  return GetMtimeOfFile(self_name);
}

bool MtimeMatchesSelf(const char* filename) {
  FILETIME own = GetOwnMtime();
  FILETIME file = GetMtimeOfFile(filename);
  return own.dwLowDateTime == file.dwLowDateTime &&
         own.dwHighDateTime == file.dwHighDateTime;
}

void ExtractFileResource(int resource_id, const char* filename) {
  unsigned char* data;
  size_t size;
#ifndef NDEBUG
    fprintf(stderr, "Extract: %d %s\n", resource_id, filename);
#endif
  if (MtimeMatchesSelf(filename)) {
#ifndef NDEBUG
    fprintf(stderr, "  mtime unchanged, skipping\n");
#endif
    return;
  } else {
#ifndef NDEBUG
    fprintf(stderr, "  mtime doesn't match, trying extract\n");
#endif
  }
  if (GetFileResource(resource_id, &data, &size)) {
#ifndef NDEBUG
    fprintf(
        stderr, "writing %d to %s (%d bytes)\n", resource_id, filename, size);
#endif
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
      const size_t kBufSize = 10<<20;
      void* buf = malloc(kBufSize);
      FILE* readfp = fopen(filename, "rb");
      if (readfp) {
        size_t bytes_read = fread(buf, 1, kBufSize, readfp);
        fclose(readfp);
        if (bytes_read == size && memcmp(data, buf, size) == 0) {
          // It's locked (in use), but it's the same as what we have in
          // our binary, so we're OK.
          free(buf);
          return;
        }
      } else {
        Fatal("couldn't open %s for read", filename);
      }
      Fatal("couldn't open %s, and it's out of date", filename);
    }

    fwrite(data, 1, size, fp);
    fclose(fp);
    SetMtimeOfFile(filename, GetOwnMtime());
  } else {
    Fatal("couldn't get %s as %d", filename, resource_id);
  }
}

void RunProcess(char* command, const char* in_dir) {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {0};
  if (!CreateProcess(
        NULL, command, NULL, NULL, TRUE, 0, NULL, in_dir, &si, &pi))
    Fatal("CreateProcess '%s' failed: %d", command, GetLastError());
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

}  // namespace

int main() {
  DWORD parent_pid = GetParentPid();

  // TODO: I think this doesn't work at all because this program is
  // compiled as x86 or x64, and PROCESSOR_ARCHITECTURE is modified from
  // the parent. We need to retrieve ParentPid's arch.
  char* processor_architecture = getenv("PROCESSOR_ARCHITECTURE");
  if (!processor_architecture)
    Fatal("couldn't determine architecture");
  const char* arch = "x64";
  if (_stricmp(processor_architecture, "x86") == 0)
    arch = "x86";
  char buf[1024];

  // If we have embedded resources, extract them to a temporary folder and run
  // from there, otherwise, try to launch in a reasonable way from the same
  // directory as we're in.
  if (GetFileResource(CMDEX_X86_EXE, NULL, NULL)) {
    char temp_dir[1024];
    ChdirToTemp(temp_dir, sizeof(temp_dir));
    if (strcmp(arch, "x86") == 0) {
      //ULONGLONG start_time = GetTickCount64();
      ExtractFileResource(CMDEX_X86_EXE, "cmdEx_x86.exe");
      ExtractFileResource(CMDEX_DLL_X86_DLL, "cmdEx_dll_x86.dll");
      ExtractFileResource(GIT2_X86_DLL, "git2.dll");
      ExtractFileResource(DBGHELP_X86_DLL, "dbghelp.dll");
      ExtractFileResource(SYMSRV_X86_DLL, "symsrv.dll");
      //ExtractFileResource(ANSI32_DLL_X86, "ansi32.dll");
      /*
      ULONGLONG end_time = GetTickCount64();
      ULONGLONG elapsed = end_time - start_time;
      fprintf(stderr,
              "extract time: %lldm%.03fs\n",
              elapsed / (60 * 1000),
              (elapsed % (60 * 1000)) / 1000.0);
              */
      sprintf(buf, "%s\\cmdEx_x86.exe %d", temp_dir, parent_pid);
      RunProcess(buf, temp_dir);
    } else {
      Fatal("todo; extract x64");
    }
  } else {
    char module_location[1024];
    if (GetModuleFileName(NULL, module_location, sizeof(module_location)) == 0)
      Fatal("couldn't GetModuleFileName");
    char* slash = strrchr(module_location, '\\');
    if (!slash)
      Fatal("exe name seems malformed");
    *slash = 0;
#ifndef NDEBUG
    fprintf(stderr, "loc: %s\n", module_location);
#endif

    sprintf(buf, "%s\\cmdEx_%s.exe %d", module_location, arch, parent_pid);
    return system(buf);
  }
}
