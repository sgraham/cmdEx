# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile


BASE_DIR = os.path.abspath(os.path.dirname(__file__))
CMD_EX_PATH = os.path.join(BASE_DIR, 'out\\cmdEx.exe')


def Run(args, quiet=False):
  stdout = None
  if quiet:
    stdout = open(os.devnull, 'w')
  subprocess.check_call(args, shell=True, stdout=stdout)


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
    os.chdir(os.path.join(self.temp_dir, repo))

  def Interact(self, commands, expect):
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
    popen = subprocess.Popen([
        Test.cmd_binary_, '/c', bat.name],
      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
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
        fail("'%s' vs '%s'" % (line, exp))
        return
    print 'ok - ' + self.name
    Test.tests_passed_ += 1

  def __enter__(self):
    Test.tests_started_ += 1
    return self

  def __exit__(self, type, value, traceback):
    os.chdir(self.orig_dir)
    #Run(['rmdir', '/s', '/q', self.temp_dir])

  @staticmethod
  def Report():
    assert Test.tests_started_ == Test.tests_passed_ + Test.tests_failed_
    print '%d/%d passed.' % (Test.tests_passed_, Test.tests_started_)

  @staticmethod
  def SetCmdBinary(binary):
    Test.cmd_binary_ = binary


def Interact(name, repo, commands, expect):
  with Test(name, repo) as t:
    t.Interact(commands, expect)


def DoTests(cmd_binary):
  Test.SetCmdBinary(cmd_binary)
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


def main():
  print 'x86:'
  DoTests('C:\\Windows\\SysWOW64\\cmd.exe')
  isx64python = sys.maxsize > 2**32
  if isx64python:
    print 'x64:'
    DoTests('C:\\Windows\\System32\\cmd.exe')
  else:
    print 'not running on x64 python, can\'t test x64 cmd.'
  Test.Report()


if __name__ == '__main__':
  main()
