// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CMDEX_COMMON_UTIL_H_
#define CMDEX_COMMON_UTIL_H_

void Fatal(const char* msg, ...);
void Error(const char* msg, ...);
void Log(const char* msg, ...);
#define CHECK(c) \
  if (!(c)) Fatal("CHECK '" #c "' failed at %s, line %d", __FILE__, __LINE__);

#endif  // CMDEX_COMMON_UTIL_H_
