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

#include "common.h"
#include "expired_item_dumper.h"


using namespace std;


ExpiredItemDumper::ExpiredItemDumper():
  file_dumper_(nullptr) {
  processor_summary_ = "Dump keys of all expired items that are wasting space";
  processor_name_ = "item dumper";
  args_.emplace_back("--expired-dump-file=$FILE_NAME", "file name to dump into", "(REQUIRED)");
}


bool ExpiredItemDumper::set_arg(const char *argv) {
  const char *val = nullptr;
  if ((val = is_arg(argv, "--expired-dump-file="))) {
    filename_ = val;
  } else {
    return false;
  }
  return true;
}


bool ExpiredItemDumper::init() { 
  if (filename_.empty()) {
    fprintf(stderr, "expired_dump_file can not be empty.\n");
    return false;
  } else {
    try {
      file_dumper_.reset(new FileDumper(filename_));
    } catch (runtime_error &e) {
      fprintf(stderr, "%s\n", e.what());
      return false;
    }
    return true;
  }
}


void ExpiredItemDumper::process_item(unsigned int cur_time,
                                     const string &key,
                                     const string &category,
                                     unsigned int touch_time,
                                     unsigned int exp_time,
                                     unsigned int nbytes,
                                     int slab_id,
                                     uint64_t cas) {
  if (exp_time && cur_time >= exp_time) {
    file_dumper_->write(key);
  }
}
