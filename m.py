# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys


def Run(args):
  subprocess.check_call(args, shell=True)


def Build():
  if os.path.exists('out'):
    Run(['rmdir', '/s', '/q', 'out'])
  os.makedirs('out')
  flags = [
      '/nologo',
      '/Ox',
      '/Zi',
      '/W4',
      '/Ithird_party/libgit2/include',
      '/D_WIN32_WINNT=0x0501',
      '/D_CRT_SECURE_NO_WARNINGS',
    ]
  Run(['cl'] + flags +
      ['dll.cc',
       '/link',
       'third_party\\libgit2\\Release\\git2.lib',
       '/dll',
       '/out:out\\cmdEx_dll_x86.dll'])
  Run(['cl'] + flags +
      ['inject.cc',
       '/link',
       '/out:out\\cmdEx_x86.exe'])
  Run(['cl'] + flags +
      ['main.cc',
       '/link',
       '/out:out\\cmdEx.exe'])
  shutil.copyfile('third_party\\libgit2\\Release\\git2.dll',
                  'out\\git2.dll')
  return 0


def Package():
  """Glom cmdEx_dll_{x86,x64}.dll, cmdEx_{x86,x64}.exe, git2_{x86,x64}.dll
  into cmdEx.exe for better distribution. TODO: cmdEx doesn't know how to
  do this yet."""
  pass


def TestInNew():
  Run(['C:\\Windows\\SysWOW64\\cmd.exe', '/k', 'out\\cmdEx.exe'])


def main():
  cmds = {
    'build': Build,
    'testnew': TestInNew,
    'package': Package,
  }
  if len(sys.argv) < 2:
    return cmds['build']()
  for cmd in sys.argv[1:]:
    ret = cmds[cmd]()
    if ret != 0:
      return ret
  return 0


if __name__ == '__main__':
  sys.exit(main())
