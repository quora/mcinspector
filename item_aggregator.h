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
#include "item_processor.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <queue>
#include <unordered_map>


struct SlabInfo {
  void reset() {
    memset(this, 0, sizeof(*this));
  }
  int oldest_age;
  uint64_t unit_size;
  uint64_t allocated_size;
  uint64_t slot_cnt;
};


class ItemAggregator: public ItemProcessor {
public:
  ItemAggregator(SlabInfo *slabs_info, int max_slab_id);
  ~ItemAggregator();
  bool set_arg(const char *argv);
  void process_item(unsigned int cur_time,
                    const std::string &key,
                    const std::string &category,
                    unsigned int touch_time,
                    unsigned int exp_time,
                    unsigned int nbytes,
                    int slab_id, uint64_t cas);

private:
  struct CategoryStats {
    // basic stats unit of a key category
    CategoryStats():
      raw_valsize_total(0),
      raw_keysize_total(0),
      mem_used_total(0),
      touch_5min_cnt(0),
      touch_1h_cnt(0),
      touch_1d_cnt(0),
      since_last_touch_total(0),
      ttl_total(0),
      expired_cnt(0),
      key_cnt(0) {
    }

    uint64_t raw_valsize_total;
    uint64_t raw_keysize_total;
    uint64_t mem_used_total;
    uint64_t touch_5min_cnt;
    uint64_t touch_1h_cnt;
    uint64_t touch_1d_cnt;
    uint64_t since_last_touch_total;
    uint64_t ttl_total;
    uint64_t expired_cnt;
    uint64_t key_cnt;
    std::priority_queue<int> age_p95;
  };

  SlabInfo *slabs_info_;
  std::unordered_map<std::string, CategoryStats> stats_;
  int max_slab_id_;
  uint64_t min_cat_rec_num_;
  uint64_t min_cat_size_;
};
