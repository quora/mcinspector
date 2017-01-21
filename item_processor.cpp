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

#include "item_processor.h"


using namespace std;


void ItemProcessor::print_options() const {
  fprintf(stderr, "%s.\n", processor_summary_.c_str());
  fprintf(stderr, "Options for %s:\n", processor_name_.c_str());
  for (auto& arg : args_) {
    fprintf(stderr, "  %-30s default: %-20s %s\n", get<0>(arg), get<2>(arg), get<1>(arg));
  }
}

