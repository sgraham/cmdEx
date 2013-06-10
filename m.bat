@echo off
setlocal
set FLAGS=/nologo /Zi /wd4530 /W4 /Ithird_party/libgit2/include /DARCH=x86 /D_WIN32_WINNT=0x0501 /D_CRT_SECURE_NO_WARNINGS
cl %FLAGS% dll.cc /link third_party\libgit2\Release\git2.lib /dll /out:cmdEx_dll_x86.dll
cl %FLAGS% inject.cc /link /out:cmdEx_x86.exe
cl %FLAGS% main.cc /link /out:cmdEx.exe
