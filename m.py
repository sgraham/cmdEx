# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys


def Run(args):
  subprocess.check_call(args, shell=True)


COMMON_CFLAGS = [
  '/nologo',
  '/Zi',
  '/W4',
  '/WX',
  '/D_WIN32_WINNT=0x0501',
  '/D_CRT_SECURE_NO_WARNINGS',
]

def Build(release):
  if os.path.exists('out'):
    Run(['rmdir', '/s', '/q', 'out'])
  os.makedirs('out')
  cflags = COMMON_CFLAGS + [
      '/Ithird_party/libgit2/include',
    ]
  if release:
    cflags += [
        '/Ox',
        '/DNDEBUG',
      ]
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


def RunInNew():
  Run(['C:\\Windows\\SysWOW64\\cmd.exe', '/k', 'out\\cmdEx.exe'])


def Test():
  UnitTest()
  Run([sys.executable, 'test.py'])


def UnitTest():
  Run(['cl'] + COMMON_CFLAGS + [
        '/Ithird_party/gtest/include',
        '/Ithird_party/gtest',
        '/EHsc',
        '/D_VARIADIC_MAX=10',
      ] +
      ['*_test.cc',
       'third_party/gtest/src/gtest-all.cc',
       'third_party/gtest/src/gtest_main.cc',
       '/link',
       '/out:out\\cmdEx_test.exe'])
  Run(['out\\cmdEx_test.exe'])


def main():
  cmds = {
    'debug': lambda: Build(False),
    'release': lambda: Build(True),
    'new': RunInNew,
    'test': Test,
    'utest': UnitTest,
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
