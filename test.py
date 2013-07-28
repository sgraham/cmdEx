# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
import time


BASE_DIR = os.path.abspath(os.path.dirname(__file__))
CMD_EX_PATH = os.path.join(BASE_DIR, 'out\\cmdEx.exe')


def Run(args, quiet=False):
  stdout = None
  if quiet:
    stdout = open(os.devnull, 'w')
  subprocess.check_call(args, shell=True, stdout=stdout)


def EnsureDirExists(dir_name):
  if not os.path.exists(dir_name):
    os.makedirs(dir_name)


class Test(object):
  """Creates a temporary directory, initializes a git repo there, and starts
  cmdEx in that directory."""

  tests_started_ = 0
  tests_passed_ = 0
  tests_failed_ = 0
  cmd_binary_ = None

  def __init__(self, name, repo):
    self.name = name
    self.orig_dir = os.getcwd()
    self.temp_dir = tempfile.mkdtemp()
    shutil.copytree(os.path.join(BASE_DIR, 'test_repos', repo),
                    os.path.join(self.temp_dir, repo))
    os.rename(os.path.join(self.temp_dir, repo, '_git'),
              os.path.join(self.temp_dir, repo, '.git'))
    # The empty repo won't have a refs/heads dir, because, ironically git
    # can't store empty dirs but relies on the existence of .git/refs/heads to
    # determine if it's in a no-initial-commit repo. So, make that dir if it
    # doesn't exist.
    EnsureDirExists(os.path.join(self.temp_dir, repo, '.git', 'refs', 'heads'))
    EnsureDirExists(os.path.join(self.temp_dir, repo, '.git', 'objects'))
    os.chdir(os.path.join(self.temp_dir, repo))

  def Interact(self, commands, expect, callback_before_communicate):
    # depot_tools does some wacky wrapping, so we need to spawn 'git' in a
    # separate sub-shell.
    commands = ['%COMSPEC% /c ' + c if c.startswith('git ') else c
                for c in commands]
    with tempfile.NamedTemporaryFile(suffix='.bat', delete=False) as bat:
      bat.write(CMD_EX_PATH + '\n')
      for command in commands:
        bat.write(command + '\n')
      bat.write('ver >nul\n')
    env = os.environ.copy()
    env['PROMPT'] = '###'
    popen = subprocess.Popen([Test.cmd_binary_, '/c', bat.name],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             env=env)
    if callback_before_communicate:
      callback_before_communicate(popen)
    out, _ = popen.communicate()
    os.unlink(bat.name)
    outlines = out.splitlines()
    # Ignore the first two respones, they're cmdEx being installed and running
    # the first command (which will contain a $P which we don't want).
    outlines = [line for i, line in enumerate(outlines) if i % 2 == 1]
    assert outlines[0].startswith('###')
    outlines = outlines[2:]
    def fail(msg):
      print 'FAILED', self.name, '->', msg
      print 'commands:', commands
      print 'expect:', expect
      print 'outlines:', outlines
      print ''
      Test.tests_failed_ += 1
    if len(outlines) != len(expect):
      fail('length of output and expected don\'t match')
      return
    for line, (i, exp) in zip(outlines, enumerate(expect)):
      if exp is None:
        continue
      if '#' in line:
        line = line[:line.index('#')]
      if '#' in exp:
        exp = exp[:exp.index('#')]
      if line != exp:
        fail("got '%s' vs expected '%s'" % (line, exp))
        return
    print 'ok - ' + self.name
    Test.tests_passed_ += 1

  def __enter__(self):
    Test.tests_started_ += 1
    return self

  def __exit__(self, type, value, traceback):
    os.chdir(self.orig_dir)
    Run(['rmdir', '/s', '/q', self.temp_dir])

  @staticmethod
  def Report():
    assert Test.tests_started_ == Test.tests_passed_ + Test.tests_failed_
    print '%d/%d passed.' % (Test.tests_passed_, Test.tests_started_)

  @staticmethod
  def SetCmdBinary(binary):
    Test.cmd_binary_ = binary


def Interact(name, repo, commands, expect, callback_before_communicate=None):
  with Test(name, repo) as t:
    t.Interact(commands, expect, callback_before_communicate)


def PromptTests():
  Interact(
      'before initial commit',
      'empty',
      commands=[
        'prompt $M#',
      ],
      expect=[
        '[(no head)]  #',
      ])

  Interact(
      'single commit on master',
      'single_commit',
      commands=[
        'prompt $M#',
      ],
      expect=[
        '[master]  #',
      ])

  Interact(
      'prompt completely empty if not in working dir',
      'empty',
      commands=[
        'prompt $M#',
        'cd ..',
      ],
      expect=[
        None,
        ' ',
      ])

  Interact(
      'detached head',
      'four_linear_commits',
      commands=[
        'prompt $M#',
        'git checkout HEAD~2 >nul 2>nul',
      ],
      expect=[
        '[master]  #',
        '[7b4f1ae...]  #',
      ])

  Interact(
      'rebase in progress',
      'conflict_rebase',
      commands=[
        'prompt $M#',
        'git rebase master >nul 2>nul',
      ],
      expect=[
        '[child]  #',
        '[child 1/1|REBASE]  #',
      ])


def TerminateBatchJobTests():
  # TODO: Test this somehow. I think it doesn't work because cmd's in
  # non-interactive mode (?) so it ignores the Ctrl-C.
  return
  # Run a batch file containing "pause\npause\n", wait a bit, send a Ctrl-C,
  # and make sure that it exits cleanly, and that the output doesn't contain
  # the prompt we don't want.
  def send_ctrl_c(proc):
    time.sleep(1)  # Just to get to the first pause, probably not necessary.
    ctypes.windll.kernel32.GenerateConsoleCtrlEvent(0, proc.pid) # 0 => Ctrl-C
  Interact(
      'no terminate batch job prompt',
      'empty',  # Don't actually need any.
      commands=[
        'prompt #',
        'pause',
        'pause',
        ],
      expect=[
        '',
        '',
      ],
      callback_before_communicate=send_ctrl_c)


def DoTests(cmd_binary):
  Test.SetCmdBinary(cmd_binary)
  PromptTests()
  TerminateBatchJobTests()


def main():
  print 'x86:'
  DoTests('C:\\Windows\\SysWOW64\\cmd.exe')
  # TODO(x64)
  """
  isx64python = sys.maxsize > 2**32
  if isx64python:
    print 'x64:'
    DoTests('C:\\Windows\\System32\\cmd.exe')
  else:
    print 'not running on x64 python, can\'t test x64 cmd.'
  """
  Test.Report()


if __name__ == '__main__':
  main()
