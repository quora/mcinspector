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
#include "file_dumper.h"
#include "item_processor.h"

#include <inttypes.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <unordered_set>


class ItemDumper: public ItemProcessor {
public:
  ItemDumper();
  bool set_arg(const char *argv);
  bool init();
  void process_item(unsigned int cur_time,
                    const std::string &key,
                    const std::string &category,
                    unsigned int touch_time,
                    unsigned int exp_time,
                    unsigned int nbytes,
                    int slab_id,
                    uint64_t cas);

private:
  static const int kDefaultMaxItemSize = 16 * MB;
  std::unique_ptr<FileDumper> file_dumper_;
  std::string filename_;
  std::unordered_set<std::string> categories_;
  std::string categories_list_filename_;
  uint64_t cas_min_;
  uint64_t cas_max_;
  uint64_t size_min_;
  uint64_t size_max_;
};



