# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys


def Run(args):
  subprocess.check_call(args, shell=True)


def Build(release):
  if os.path.exists('out'):
    Run(['rmdir', '/s', '/q', 'out'])
  os.makedirs('out')
  cflags = [
      '/nologo',
      '/Zi',
      '/W4',
      '/WX',
      '/Ithird_party/libgit2/include',
      '/D_WIN32_WINNT=0x0501',
      '/D_CRT_SECURE_NO_WARNINGS',
    ]
  if release:
    cflags.append('/Ox')
    cflags.append('/DNDEBUG')
  Run(['cl'] + cflags +
      ['dll.cc', 'util.cc',
       '/link',
       '/dll',
       '/out:out\\cmdEx_dll_x86.dll'])
  Run(['cl'] + cflags +
      ['inject.cc', 'util.cc',
       '/link',
       '/out:out\\cmdEx_x86.exe'])
  Run(['rc'
       '/nologo',
       '/r',
       'resource.rc'])
  Run(['cl'] + cflags +
      ['main.cc', 'util.cc',
       'resource.res',
       '/link',
       '/out:out\\cmdEx.exe'])
  shutil.copyfile('third_party\\libgit2\\Release\\git2.dll',
                  'out\\git2.dll')
  return 0


def TestInNew():
  Run(['C:\\Windows\\SysWOW64\\cmd.exe', '/k', 'out\\cmdEx.exe'])


def Test():
  Run([sys.executable, 'test.py'])


def main():
  cmds = {
    'debug': lambda: Build(False),
    'release': lambda: Build(True),
    'new': TestInNew,
    'test': Test,
  }
  if len(sys.argv) < 2:
    return cmds['debug']()
  for cmd in sys.argv[1:]:
    ret = cmds[cmd]()
    if ret != 0:
      return ret
  return 0


if __name__ == '__main__':
  sys.exit(main())
