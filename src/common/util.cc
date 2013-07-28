// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

void Error(const char* msg, ...) {
  va_list ap;
  fprintf(stderr, "cmdEx: ERROR: ");
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}

void Log(const char* msg, ...) {
#ifndef NDEBUG
  va_list ap;
  fprintf(stderr, "cmdEx: ");
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fprintf(stderr, "\n");
#else
  (void)msg;
#endif
}
