# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile


CMD_EX_PATH = \
    os.path.abspath(os.path.join(os.path.dirname(__file__), 'out\\cmdEx.exe'))


def Run(args, quiet=False):
  stdout = None
  if quiet:
    stdout = open('os.devnull', 'w')
  subprocess.check_call(args, shell=True, stdout=stdout)


class Test(object):
  """Creates a temporary directory, initializes a git repo there, and starts
  cmdEx in that directory."""

  tests_started_ = 0
  tests_passed_ = 0
  tests_failed_ = 0

  def __init__(self, init=True):
    self.orig_dir = os.getcwd()
    self.temp_dir = tempfile.mkdtemp()
    os.chdir(self.temp_dir)
    if init:
      Run(['git', 'init'], quiet=True)

  def Interact(self, commands, expect):
    with tempfile.NamedTemporaryFile(suffix='.bat', delete=False) as bat:
      bat.write(CMD_EX_PATH + '\n')
      for command in commands:
        bat.write(command + '\n')
      bat.write('echo.\n')
    env = os.environ.copy()
    env['PROMPT'] = '###'
    popen = subprocess.Popen([
        'C:\\Windows\\SysWOW64\\cmd.exe', '/c', bat.name],
      stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
      env=env, cwd=self.temp_dir)
    out, _ = popen.communicate()
    lines = out.splitlines()
    # Ignore the first two respones, they're cmdEx being installed and running
    # the first command (which will contain a $P which we don't want).
    lines = [line for i, line in enumerate(lines) if i % 2 == 1]
    assert lines[0].startswith('###')
    lines = lines[2:]
    for line, (i, exp) in zip(lines, enumerate(expect)):
      if not exp:
        continue
      if i == len(expect) - 1:
        exp = exp + 'echo.'
      if line != exp:
        print 'FAILED', line, exp
        print 'commands:', commands
        print 'expect:', expect
        Test.tests_failed_ += 1
        return
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


def TestBasic():
  with Test() as t:
    t.Interact(
        commands=[
          'prompt $M#',
        ],
        expect=[
          '[(no head)]  #',
        ])

  with Test() as t:
    t.Interact(
        commands=[
          'prompt $M#',
          'echo. > a_file',
          'git add a_file',
          'git commit -m "yolo"',
        ],
        expect=[
          '',
          '',
          '',
          '[(master)]  #',
        ])


def main():
  TestBasic()
  Test.Report()


if __name__ == '__main__':
  main()
