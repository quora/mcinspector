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

#include <string>


class ItemProcessor {
protected:
  ItemProcessor() {}
  std::string processor_summary_;
  std::string processor_name_;
  Args args_;

public:
  virtual ~ItemProcessor() {}
  void print_options() const;
  virtual bool set_arg(const char *argv) { return false; }
  virtual bool init() { return true; }
  virtual void process_item(unsigned int cur_time,
                              const std::string &key,
                              const std::string &category,
                              unsigned int touch_time,
                              unsigned int exp_time,
                              unsigned int nbytes,
                              int slab_id,
                              uint64_t cas) = 0;
};
