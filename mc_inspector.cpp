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
 *
 * This file contains codes from Memcached:
 *
 * Copyright (c) 2003, Danga Interactive, Inc.
 * All rights reserved.
 *
 * Full license of Memcached:
 * https://github.com/memcached/memcached/blob/master/LICENSE
 *
 */

#include "common.h"
#include "file_dumper.h"
#include "timer.h"

#include "item_aggregator.h"
#include "item_processor.h"
#include "item_dumper.h"
#include "expired_item_dumper.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>


using namespace std;


// Copied from memcached.h in memcached-1.4.32 and slightly changed
#define MAX_NUMBER_OF_SLAB_CLASSES 63
#define ITEM_clsid(item) ((item)->slabs_clsid & ~(3<<6))

typedef struct _stritem {
  struct _stritem *next;
  struct _stritem *prev;
  struct _stritem *h_next;    /* hash chain next */
  unsigned int time;       /* least recent access */
  unsigned int exptime;    /* expire time */
  unsigned int nbytes;     /* size of data */
  unsigned short  refcount;
  uint8_t         nsuffix;    /* length of flags-and-length string */
  uint8_t         it_flags;   /* ITEM_* above */
  uint8_t         slabs_clsid;/* which slab class we're in */
  uint8_t         nkey;       /* key length, w/terminating null and padding */
  /* this odd type prevents type-punning issues when we do
   * the little shuffle to save space when not using CAS. */
  union {
      uint64_t cas;
      char end;
  } data[];
  /* if it_flags & ITEM_CAS we have 8 bytes CAS */
  /* then null-terminated key */
  /* then " flags length\r\n" (no terminating null) */
  /* then data with terminating \r\n (no terminating null; it's binary!) */
} item;
// Copy end


namespace {
  // global variables

  // memcached uses its own clock (secs since server started), so need to maintain relative time
  time_t server_start_unixtime;
  pid_t pid;

  bool cas_enabled = true;
  const int kMaxSlabId = MAX_NUMBER_OF_SLAB_CLASSES + 1;
  uint64_t key_cnt_in_mc = 0;
  uint64_t key_cnt_found = 0;

  vector<string> split_line(const string& line) {
    stringstream ss(line);
    vector<string> tokens;
    string buf;
    while (ss >> buf) {
      tokens.push_back(buf);
    }
    return tokens;
  }

  SlabInfo slabs_info[kMaxSlabId];
}


int compute_item_datafield_offset() {
  // 'cas' field in item is defined as uint64_t
  int cas_field_size = cas_enabled ? sizeof(uint64_t) : 0;
  return offsetof(item, data) + cas_field_size;
}


struct Area {
  const char *lo;
  const char *hi;

  Area(const char * const lo, const char * const hi) : lo(lo), hi(hi) {}

  // construct an area from "7f7f14000000-7f7f17ffa000" liked string
  Area(const string &s) {
    uint64_t addr[2] = {0};
    sscanf(s.c_str(), "%" PRIx64 "-%" PRIx64, &addr[0], &addr[1]);
    lo = (const char *)(addr[0]);
    hi = (const char *)(addr[1]);
  }

  bool operator<(const Area &r) const {
    // assuming the Area objects have no overlap
    return lo < r.lo && hi <= r.lo;
  }

  uint64_t size() const {
    return hi - lo;
  }
};


vector<Area> get_area_list(pid_t pid) {
  // get address spaces of a pid by reading system file in /proc
  ifstream infile("/proc/" + to_string(pid) + "/maps");
  string line;

  vector<Area> area_list;

  while (getline(infile, line)) {
    auto tokens = split_line(line);
    if (tokens.size() >= 5 && tokens[1] == "rw-p" && tokens[4] == "0") {
      // not all memory region store data that we want.
      // we are looking for private heap area
      // which has r/w permission and is not mapped from file
      Area mem_area(tokens[0]);
      if (mem_area.size() < 128 * KB) {
        continue;
      }
      area_list.push_back(mem_area);
    }
  }
  return area_list;
}


int get_mc_server_info(const char *stats_file) {
  ifstream infile(stats_file);
  string line;
  time_t now = 0;
  int uptime = 0;
  auto set_slab_info = [](int id, const string& key, const string& val) {
    if (id >= kMaxSlabId) {
      return;
    }
    if (key == "age") {
      slabs_info[id].oldest_age = atoi(val.c_str());
    } else if (key == "chunk_size") {
      slabs_info[id].unit_size = atol(val.c_str());
    } else if (key == "total_chunks") {
      slabs_info[id].slot_cnt = atol(val.c_str());
    } else if (key == "mem_requested") {
      slabs_info[id].allocated_size = atol(val.c_str());
    }
  };

  while (getline(infile, line)) {
    // make ':' in the mc console output into a space so easier to parse
    replace(line.begin(), line.end(), ':', ' ');
    auto tokens = split_line(line);
    if (tokens[0] == "STAT" && tokens.size() >= 3) {
      if (tokens[1] == "pid") {
        // STAT pid 3245
        pid = atoi(tokens[2].c_str());
      } else if (tokens[1] == "cas_enabled") {
        // STAT cas_enabled true
        cas_enabled = (tokens[2] == "yes");
      } else if (tokens[1] == "time") {
        // STAT time 1461002109
        now = atoi(tokens[2].c_str());
      } else if (tokens[1] == "curr_items") {
        // STAT curr_items 127132063
        key_cnt_in_mc = atol(tokens[2].c_str());
      } else if (tokens[1] == "uptime") {
        // STAT uptime 3880664
        uptime = atoi(tokens[2].c_str());
      } else if (tokens[1] == "items" && tokens.size() >= 5) {
        // STAT items:1:number 786384
        int slab_id = atoi(tokens[2].c_str());
        set_slab_info(slab_id, tokens[3], tokens[4]);
      } else if (isdigit(tokens[1][0]) && tokens.size() >= 4) {
        // STAT 1:chunk_size 96
        int slab_id = atoi(tokens[1].c_str());
        set_slab_info(slab_id, tokens[2], tokens[3]);
      }
    }
  }
  if (now && uptime) {
    server_start_unixtime = now - uptime;
    return 0;
  } else {
    return -1;
  }
}


namespace {
  unordered_map<string, ItemProcessor *> all_processors;
  unordered_map<ItemProcessor *, string> item_processors;
}


void show_usage(const char *exec) {
  static const Args args = {
    make_tuple("--processor=$PROCESSOR_NAME", "Processor to use on each detected item.", "(REQUIRED)"),
    make_tuple("--stats-file=$FILE_NAME", "Stats file generated from memcache console.", "(REQUIRED)"),
    make_tuple("--keys-limit=$NUM", "Stop the inspector after seen this number of keys", "no upper limit"),
    make_tuple("--mem-limit-mb=$NUM", "Memory use hard limit of this inspector, in MB", "256 (MB)"),
    make_tuple("--category-delimitor=$char", "Specify a prefix delimiter for key string", ":"),
    make_tuple("--mem-scan-block-size-mb=$NUM", "Memory scan batch size, in MB", "64 (MB)"),
  };

  fprintf(stderr, "The inspector has to run with PTRACE_ATTACH privilege on the memcached process.\n");
  fprintf(stderr, "Usage: %s --stats-file=$PATH --processor=$PROC1 [--processor=$PROC2 .. ] [arguments]\n", exec);
  fprintf(stderr, "stats file can be generated by shell command:\n\t'printf \"stats\\nstats slabs\\nstats items\\"
                  "nstats settings\\n\" | netcat 127.0.0.1 11211 > $STATS_FILE'\n");
  fprintf(stderr, "Available global arguments:\n");
  for (auto& arg : args) {
    fprintf(stderr, "  %-30s default: %-20s %s\n", get<0>(arg), get<2>(arg), get<1>(arg));
  }

  fprintf(stderr, "\n\nAvailable processors and their arguments (multiple processors can be used together):\n");
  for (auto it : all_processors) {
    fprintf(stderr, "\n--processor=%s\n", it.first.c_str());
    it.second->print_options();
  }
}


void prepare_item_processors() {
  all_processors.emplace("item-aggregator", new ItemAggregator(slabs_info, kMaxSlabId));
  all_processors.emplace("item-dumper", new ItemDumper());
  all_processors.emplace("expired-dumper", new ExpiredItemDumper());
}


bool create_item_processor(const string &name) {
  auto it = all_processors.find(name);
  if (it == all_processors.end()) {
    return false;
  } else {
    item_processors.emplace(it->second, name);
  }
  return true;
}


int main(int argc, char *argv[]) {
  prepare_item_processors();

  uint64_t mem_limit = 256 * MB;
  uint64_t keys_limit = numeric_limits<uint64_t>::max();
  uint64_t mem_scan_block_size = 64 * MB;
  char category_delimiter = ':';
  const char *stats_file = nullptr;

  if (argc <= 1) {
    show_usage(argv[0]);
    return 1;
  }

  for (int x = 1; x < argc; x++) {
    const char *val = nullptr;
    if ((val = is_arg(argv[x], "--processor="))) {
      if (!create_item_processor(val)) {
        fprintf(stderr, "Can not create processor of '%s'\n", val);
        return 1;
      }
    } else if ((val = is_arg(argv[x], "--stats-file="))) {
      stats_file = val;
    } else if ((val = is_arg(argv[x], "--keys-limit="))) {
      keys_limit = atol(val);
    } else if ((val = is_arg(argv[x], "--mem-limit-mb="))) {
      mem_limit = atol(val) * MB;
    } else if ((val = is_arg(argv[x], "--category-delimitor="))) {
      category_delimiter = val[0];
    } else if ((val = is_arg(argv[x], "--mem-scan-block-size-mb="))) {
      mem_scan_block_size = atol(val) * MB;
    } else {
      bool captured = false;
      for (auto ip : item_processors) {
        if ((captured = ip.first->set_arg(argv[x]))) {
          break;
        }
      }

      if (!captured) {
        fprintf(stderr, "error: unknown command-line option: %s\n\n", argv[x]);
        show_usage(argv[0]);
        return 1;
      }
    }
  }

  if (!stats_file) {
    fprintf(stderr, "Stats file is required.\n");
    fprintf(stderr, "It can be generated by shell command:\n");
    fprintf(stderr, "\tprintf \"stats\\nstats slabs\\nstats items\\nstats settings\\n\""
                    "| netcat 127.0.0.1 11211 > $STATS_FILE.\n");
    return 1;
  }

  if (item_processors.empty()) {
    fprintf(stderr, "Have to specify at least one item processor\n");
    return 1;
  }

  for (auto ip : item_processors) {
    if (!ip.first->init()) {
      fprintf(stderr, "Item processor [%s] failed to initialize.\n", ip.second.c_str());
      return 1;
    }
  }

  // this is a mc box, don't OOM and pull down the box!
  struct rlimit st_mem_limit = {mem_limit, mem_limit};
  setrlimit(RLIMIT_AS, &st_mem_limit);
  if (get_mc_server_info(stats_file) < 0) {
    fprintf(stderr, "%s parse failed\n", stats_file);
    return 1;
  }

  const auto kBufSize = mem_scan_block_size;
  char *pbuf = new char[kBufSize];
  uint64_t datafield_off = compute_item_datafield_offset();

  Timer timer;
  uint64_t calculation_time_us = 0;
  uint64_t memscan_time_us = 0;

  const char *current_remote_address = 0;
  uint64_t total_read = 0;
  for (;;) {
    // in every iteration get updated address spaces (though it's should rarely change for mc)
    auto area_list = get_area_list(pid);
    uint64_t total_mem_size = 0;
    for (const auto &area : area_list) {
      total_mem_size += area.size();
    }

    // continue from the place where stopped in last iteration
    Area needle = {current_remote_address, current_remote_address};
    auto it = lower_bound(area_list.begin(), area_list.end(), needle);
    if (it == area_list.end()) {
      break;
    }
    current_remote_address = max(it->lo, current_remote_address);

    // process_vm_readv accepts reading multiple region in one batch
    // below is to make up the batch with total size of kBufSize
    vector<struct iovec> read_region_list;
    struct iovec local_region = {(void *)pbuf, kBufSize};
    size_t remote_block_size = min<size_t>(kBufSize, size_t(it->hi - current_remote_address));
    struct iovec iov = {(void *)current_remote_address, remote_block_size};
    read_region_list.emplace_back(iov);
    int64_t bytes_to_read = remote_block_size;
    it++;
    for (; it < area_list.end() && bytes_to_read < (signed)kBufSize; it++) {
      remote_block_size = min<size_t>(kBufSize - bytes_to_read, it->size());
      struct iovec iov = {(void *)it->lo, remote_block_size};
      read_region_list.emplace_back(iov);
      bytes_to_read += remote_block_size;
    }

    timer.reset();
    // key function of memory copy from external process
    int read_bytes = process_vm_readv(pid,
                                      &local_region,
                                      1,  // one local region
                                      &read_region_list[0],
                                      read_region_list.size(),
                                      0);
    memscan_time_us += timer.get_us();
    if (read_bytes) {
      total_read += read_bytes;
    }
    fprintf(stderr, "read %lu KBytes (%.1f%%)\n", read_bytes / KB, total_read * 100.0 / total_mem_size);

    timer.reset();
    uint32_t bytes_left = read_bytes;
    for (const auto &i : read_region_list) {
      if (i.iov_len > bytes_left) {
        current_remote_address = (char *)((uint64_t)i.iov_base + bytes_left);
        break;
      } else {
        current_remote_address = (char *)((uint64_t)i.iov_base + i.iov_len);
        bytes_left -= i.iov_len;
      }
    }

    unsigned int cur_time = time(nullptr) - server_start_unixtime;
    for (int i = 0; i < read_bytes - 1; i++) {
      if (pbuf[i] == ' ' && isdigit(pbuf[i + 1])) {
        // precondition of there being an item around here: ' ' + a digit
        int p = i - 2;  // jump over current ' ' and 'null-termination-char' (actually may not be null) of key
        int possible_key_len = 0;
        while(p > (int)datafield_off && isprint(pbuf[p]) && pbuf[p] != ' ') {
          // currently it's assuming the byte just before the key starts is not a printable ascii.
          // NOTICE: this key boundary detection logic may need to be improved in some cases:
          // it may miss some keys if the cas is disabled when mc server was started,
          // or the mc server has been running very very long time, that global cas in mc server
          // is several times of 2^56, or the machine is in big-endian.
          possible_key_len++;
          p--;
        }
        p++;
        item *probed = reinterpret_cast<item*>(pbuf + p - datafield_off);
        if (possible_key_len < 3 || probed->nkey != possible_key_len) {
          // key length in struct does not equal to the detected length, it's false positive
          continue;
        }

        string detected_key(pbuf + p, probed->nkey);
        string category_name;
        size_t delimiter_pos = detected_key.find(category_delimiter);
        if (delimiter_pos == string::npos) {
          category_name = "__UNKNOWN_CATEGORY__";
        } else {
          category_name = detected_key.substr(0, detected_key.find(category_delimiter));
        }
        if (probed->time > 365 * 86400 * 10 || probed->time >= cur_time + 50
            || probed->nbytes + probed->nkey > slabs_info[ITEM_clsid(probed)].unit_size) {
          // since the item came from raw memory scan, there might be some corrupted entries.
          // so some sanity checks are applied to filter out them
          continue;
        }

        key_cnt_found++;
        for (auto ip : item_processors) {
          ip.first->process_item(cur_time,
                           detected_key,
                           category_name,
                           probed->time,
                           probed->exptime,
                           probed->nbytes,
                           ITEM_clsid(probed),
                           probed->data[0].cas);
        }
        i += probed->nbytes;
      }
    }
    calculation_time_us += timer.get_us();

    if (key_cnt_found > keys_limit) {
      // for test of small samples
      break;
    }
  }

  for (auto ip : item_processors) {
    delete ip.first;
  }

  fprintf(stderr, "Time spent: %lu us_on_mem_scan + %lu us_on_calcuation\n"
                  "Scanned %lu KB memory, detected %lu keys, that are %.1f%% of keys known by mc server\n",
          memscan_time_us,
          calculation_time_us,
          total_read / KB,
          key_cnt_found,
          key_cnt_in_mc ? key_cnt_found * 100.0 / key_cnt_in_mc : 0);
  delete [] pbuf;
  return 0;
}

