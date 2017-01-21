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

#include "file_dumper.h"

#include <string.h>


using namespace std;


FileDumper::FileDumper(const string& filename) {
  file_.rdbuf()->pubsetbuf(buffer_, kFileWriteBufSize);
  file_.open(filename);
  if (file_.fail()) {
    throw runtime_error("file open failed: " + filename + " Error: " + strerror(errno));
  }
}

FileDumper::~FileDumper() {
  file_.close();
}

void FileDumper::write(const string& line) {
  // using '\n' instead of endl because we don't want it to flush too often
  file_ << line << '\n';
}
