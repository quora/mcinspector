/*
 * Copyright 2016 Quora, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include "common.h"

#include <stdint.h>

#include <fstream>
#include <string>


class FileDumper {
public:
  FileDumper() = delete;
  FileDumper(const std::string& filename);
  ~FileDumper();
  void write(const std::string& line);

private:
  static const uint32_t kFileWriteBufSize = 256 * KB;
  char buffer_[kFileWriteBufSize];
  std::ofstream file_;
};
