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

#include "item_aggregator.h"


using namespace std;


ItemAggregator::ItemAggregator(SlabInfo *slabs_info, int max_slab_id) {
  slabs_info_ = slabs_info;
  max_slab_id_ = max_slab_id;
  min_cat_rec_num_ = 100;
  min_cat_size_ = MB;

  processor_summary_ = "Get a summary of all items in the pool";
  processor_name_ = "item aggregator";
  args_.emplace_back("--min-cat-rec-num=$NUM", "Minimum number of keys in a category to be shown", "100");
  args_.emplace_back("--min-cat-size-mb=$NUM", "Minimum total size of a category to be shown, in MB", "1 (MB)");
}


ItemAggregator::~ItemAggregator() {
  printf("key\t"
         "Count\t"
         "avg_key_size\t"
         "avg_val_size\t"
         "mem_used_total\t"
         "%%_touched_in_5min\t"
         "%%_touched_in_1h\t"
         "%%_touched_in_1d\t"
         "avg_since_last_touched\t"
         "p95_age\t"
         "avg_ttl\t"
         "%%_of_expired\n");

  for (auto it: stats_) {
    if ((it.second.key_cnt < min_cat_rec_num_) && (it.second.mem_used_total < min_cat_size_)) {
      continue;
    }
    printf("CATEGORY %s\t%lu\t%.1f\t%.1f\t%lu\t%.1f\t%.1f\t%.1f\t%d\t%d\t%d\t%.1f\n",
           it.first.c_str(),
           it.second.key_cnt,
           it.second.raw_keysize_total * 1.0 / it.second.key_cnt,
           it.second.raw_valsize_total * 1.0 / it.second.key_cnt,
           it.second.mem_used_total,
           it.second.touch_5min_cnt * 100.0 / it.second.key_cnt,
           it.second.touch_1h_cnt * 100.0 / it.second.key_cnt,
           it.second.touch_1d_cnt * 100.0 / it.second.key_cnt,
           int(it.second.since_last_touch_total / it.second.key_cnt),
           it.second.age_p95.top(),
           int(it.second.ttl_total  / (it.second.key_cnt - it.second.expired_cnt + 1)),
           it.second.expired_cnt * 100.0 / it.second.key_cnt);
  }
  printf("\nOldest item touched per slab: \n");
  printf("slab_id\t"
         "slot_size\t"
         "slot_count\t"
         "total_size\t"
         "oldest_touched_secs_ago\n");
  for (int i = 0; i < max_slab_id_; i++) {
    if (slabs_info_[i].unit_size) {  // a slab id with valid size
      printf("SLAB %d\t%lu\t%lu\t%lu\t%d\n",
             i,
             slabs_info_[i].unit_size,
             slabs_info_[i].slot_cnt,
             slabs_info_[i].allocated_size,
             slabs_info_[i].oldest_age);
    }
  }
}


bool ItemAggregator::set_arg(const char *argv) {
  const char *val = nullptr;
  if ((val = is_arg(argv, "--min-cat-rec-num="))) {
    min_cat_rec_num_ = atol(val);
  } else if ((val = is_arg(argv, "--min-cat-size-mb="))) {
    min_cat_size_ = atol(val) * MB;
  } else {
    return false;
  }
  return true;
}


void ItemAggregator::process_item(unsigned int cur_time,
                                  const string &key,
                                  const string &category,
                                  unsigned int touch_time,
                                  unsigned int exp_time,
                                  unsigned int nbytes,
                                  int slab_id, uint64_t cas) {
  if (category.empty()) {
    return;
  }

  auto &category_stats = stats_[category];
  category_stats.raw_valsize_total += nbytes;
  category_stats.raw_keysize_total += key.size();
  category_stats.key_cnt++;
  category_stats.mem_used_total += slabs_info_[slab_id].unit_size;

  if (touch_time + 5 * 60 >= cur_time) {
    category_stats.touch_5min_cnt++;
  }
  if (touch_time + 60 * 60 >= cur_time) {
    category_stats.touch_1h_cnt++;
  }
  if (touch_time + 24 * 60 * 60 >= cur_time) {
    category_stats.touch_1d_cnt++;
  }

  int secs_touched_ago = cur_time - touch_time;
  category_stats.since_last_touch_total += secs_touched_ago;
  auto &age_p95 = category_stats.age_p95;
  if (age_p95.size() < 10 || age_p95.size() < category_stats.key_cnt * 5 / 100) {
    age_p95.push(secs_touched_ago);
  } else {
    if (age_p95.top() < secs_touched_ago) {
      age_p95.push(secs_touched_ago);
      age_p95.pop();
    }
  }

  if (cur_time < exp_time) {
    category_stats.ttl_total += exp_time - cur_time;
  } else if (exp_time) {
    category_stats.expired_cnt++;
  }
}
