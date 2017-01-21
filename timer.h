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
#include <stdint.h>
#include <sys/time.h>


class Timer {
public:
  Timer() {
    reset();
  }

  void reset() {
    interval_ = 0;
    gettimeofday(&ts_begin_, nullptr);
  }

  void stop() {
    struct timeval ts_end;
    gettimeofday(&ts_end, nullptr);
    interval_ = (ts_end.tv_sec - ts_begin_.tv_sec) * 1000000
      + ts_end.tv_usec - ts_begin_.tv_usec;
  }

  uint64_t get_us() const {
    if (interval_) {
      return interval_;
    } else {
      struct timeval ts_end;
      gettimeofday(&ts_end, nullptr);
      return (ts_end.tv_sec - ts_begin_.tv_sec) * 1000000
        + ts_end.tv_usec - ts_begin_.tv_usec;
    }
  }

  uint64_t get_ms() const {
    return get_us() / 1000;
  }

  uint64_t get_s() const {
    return get_us() / 1000000;
  }

private:
  struct timeval ts_begin_;
  uint64_t interval_;
};


