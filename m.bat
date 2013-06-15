:: Copyright 2013 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.
@echo off
setlocal

::if exist C:\Python27_amd64\python.exe goto :x64

call python m.py %*
goto :eof

:x64
call C:\Python27_amd64\python.exe m.py %*
goto :eof
