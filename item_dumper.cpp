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
#include "item_dumper.h"
#include <fstream>


using namespace std;


ItemDumper::ItemDumper(): 
  file_dumper_(nullptr),
  cas_min_(0),
  cas_max_(numeric_limits<uint64_t>::max()),
  size_min_(0),
  size_max_(kDefaultMaxItemSize) {
  processor_summary_ = "Dump all keys and their meta info will filters of category, cas version or size";
  processor_name_ = "item dumper";
  args_.emplace_back("--category-to-dump=$CATEGORY_NAME", "category filter, multiple of this arguments is allowed", "(ALL IF NOT SPECIFIED)");
  args_.emplace_back("--category-to-dump-list=$FILE_NAME", "category filter from file while each line is a category", "(NOT SPECIFIED)");
  args_.emplace_back("--category-dump-file=$FILE_NAME", "file name to dump into", "(REQUIRED)");
  args_.emplace_back("--dump-cas-min=$CAS_VALUE", "Min CAS version of items to be dumped", "0");
  args_.emplace_back("--dump-cas-max=$CAS_VALUE", "Max CAS version of items to be dumped", "uint64_max");
  args_.emplace_back("--dump-size-min=$BYTES", "Min size(key len + val len) of items to be dumped", "0");
  args_.emplace_back("--dump-size-max=$BYTES", "Max size(key len + val len) of items to be dumped", "16777216");
}


bool ItemDumper::set_arg(const char *argv) {
  const char *val = nullptr;
  if ((val = is_arg(argv, "--category-dump-file="))) {
    filename_ = val;
  } else if ((val = is_arg(argv, "--category-to-dump="))) {
    categories_.insert(val);
  } else if ((val = is_arg(argv, "--category-to-dump-list="))) {
    categories_list_filename_ = val;
  } else if ((val = is_arg(argv, "--dump-cas-min="))) {
    cas_min_ = atol(val);
  } else if ((val = is_arg(argv, "--dump-cas-max="))) {
    cas_max_ = atol(val);
  } else if ((val = is_arg(argv, "--dump-size-min="))) {
    size_min_ = atol(val);
  } else if ((val = is_arg(argv, "--dump-size-max="))) {
    size_max_ = atol(val);
  } else {
    return false;
  }
  return true;
}


bool ItemDumper::init() { 
  if (filename_.empty()) {
    fprintf(stderr, "category_dump_file can not be empty.\n");
    return false;
  } else {
    try {
      file_dumper_.reset(new FileDumper(filename_));
      if (!categories_list_filename_.empty()) {
        string line;
        // categories list file is specified
        ifstream infile(categories_list_filename_.c_str());
        while (getline(infile, line)) {
          categories_.insert(line);
        }
      }
    } catch (runtime_error &e) {
      fprintf(stderr, "%s\n", e.what());
      return false;
    }
    return true;
  }
}


void ItemDumper::process_item(unsigned int cur_time,
                              const string &key,
                              const string &category,
                              unsigned int touch_time,
                              unsigned int exp_time,
                              unsigned int nbytes,
                              int slab_id,
                              uint64_t cas) {
  if (categories_.count(category)
      && cas >= cas_min_
      && cas <= cas_max_
      && key.size() + nbytes >= size_min_
      && key.size() + nbytes <= size_max_) {
    static const int kLineSize = 1024;  // output line length limit
    char buf[kLineSize];
    snprintf(buf, kLineSize, "%s keysize: %d valsize: %d expire_in_secs: %d last_touch_secs_ago: %d cas: %" PRIu64,
             key.c_str(), (int)key.size(), nbytes, exp_time - cur_time, cur_time - touch_time, cas);
    file_dumper_->write(buf);
  }
}
