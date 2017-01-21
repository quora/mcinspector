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

// The function of this program can be done by shell commands:
// user@box$ cat mc_expired_keys.txt | awk '{print "get "$0}' | nc 127.0.0.1 11211
// The program is to get lower cpu_sys and better rate control
#include "common.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <thread>


using namespace std;


volatile bool stop_read_thread = false;


int socket_connect(int port) {
  struct sockaddr_in remote;
  remote.sin_family = AF_INET;
  remote.sin_port = htons(port);
  memset(&remote.sin_zero, 0, 8);

  remote.sin_addr.s_addr = inet_addr("127.0.0.1");
  int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock_fd == -1) {
    fprintf(stderr, "socket() failed. Message: %s.\n", strerror(errno));
    return -1;
  }

  if (connect(sock_fd, (struct sockaddr*)&remote, sizeof(struct sockaddr_in)) == -1) {
    fprintf(stderr, "connect() to %d failed. Message: %s.\n", port, strerror(errno));
    close(sock_fd);
    return -1;
  }
  return sock_fd;
}


void recv_proc(int fd) {
  // the procedure runs in separate thread and just consumes whatever mc sends
  static const uint32_t kBufSize = 32 * KB;
  char recv_buf[kBufSize];
  auto ignore_ret = [](int){};

  struct timeval timeout = {1, 0};
  // set a read timeout so that the thread get a chance to check termination flag
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

  while(!stop_read_thread) {
    // don't care about succeed or not here.
    // If the socket became unavailable, write would fail
    ignore_ret(read(fd, recv_buf, kBufSize));
  }
  return;
}


int flush_send_buf(int fd, const string& send_buf) {
  // try to send whole buf in blocking mode
  // the program is supposed to run on localhost,
  // we don't expect the socket to fail.
  // so there is no connection retry logic in case of socket is broken
  if (send_buf.size() < 4) {
    return 0;
  }

  for (unsigned bytes_sent = 0; bytes_sent < send_buf.size(); ) {
    int ret = write(fd, send_buf.c_str() + bytes_sent, send_buf.size() - bytes_sent);
    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    } else {
      bytes_sent += ret;
    }
  }

  return 0;
}


void show_usage(const char *exec) {
  static const Args args = {
    make_tuple("--expired-keys-file=$PATH", "File of expired keys list.", "(REQUIRED)"),
    make_tuple("--mc-port=$PORT", "Port of localhost memcached running on", "11211"),
    make_tuple("--clean-batch=$NUM", "Number of keys in a 'get' batch command", "1000"),
    make_tuple("--sleep-interval=$MS", "Number of milliseconds to sleep between each batch", "50 (ms)"),
  };

  fprintf(stderr, "Purge expired keys from memcached by sending 'get' command.\n");
  fprintf(stderr, "Usage: %s args\n", exec);
  fprintf(stderr, "Possible args:\n");
  for (auto& arg : args) {
    fprintf(stderr, "  %-30s default: %-20s %s\n", get<0>(arg), get<2>(arg), get<1>(arg));
  }
}


int main(int argc, char *argv[]) {
  int port = 11211;
  // sleep 0.05s between every purge of 1000 keys by default
  int clean_batch_size = 1000;
  int sleep_interval_ms = 50;

  if (argc <= 1) {
    show_usage(argv[0]);
    return 1;
  }

  const char *filename = nullptr;
  for (int x = 1; x < argc; x++) {
    const char *val = nullptr;
    if ((val = is_arg(argv[x], "--expired-keys-file="))) {
      filename = val;
    } else if ((val = is_arg(argv[x], "--mc-port="))) {
      port = atoi(val);
    } else if ((val = is_arg(argv[x], "--clean-batch="))) {
      clean_batch_size = atoi(val);
    } else if ((val = is_arg(argv[x], "--sleep-interval="))) {
      sleep_interval_ms = atoi(val);
    } else {
      fprintf(stderr, "error: unknown command-line option: %s\n\n", argv[x]);
      show_usage(argv[0]);
      return 1;
    }
  }

  if (!filename) {
    fprintf(stderr, "Must specify the file of expired key list\n");
    return 1;
  }

  int fd = socket_connect(port);
  if (fd < 0) {
    fprintf(stderr, "Memcached connect failed.\n");
    return 1;
  }

  static const uint32_t kFileReadBufSize = 256 * KB;
  char buffer[kFileReadBufSize];
  ifstream infile;
  infile.rdbuf()->pubsetbuf(buffer, kFileReadBufSize);
  infile.open(filename);

  thread recv_thread(recv_proc, fd);

  string key;
  string send_buf = "get";
  uint32_t cnt = 0;
  while (infile >> key) {
    send_buf.append(1, ' ');
    send_buf.append(key);
    cnt++;
    if (cnt % clean_batch_size == 0) {
      // mc's implementation has not hard-coded limit on
      // recv buffersize.  So a little bit large batch is fine
      send_buf.append("\r\n");
      if (flush_send_buf(fd, send_buf) < 0) {
        // just abort due to simplicity
        fprintf(stderr, "Connection to Memcached broke\n");
        stop_read_thread = true;
        recv_thread.join();
        return 1;
      }
      send_buf = "get";
      fprintf(stderr, "%u keys are checked\n", cnt);
      usleep(sleep_interval_ms * 1000);
    }
  }
  flush_send_buf(fd, send_buf);
  stop_read_thread = true;
  recv_thread.join();
  fprintf(stderr, "Done! %u keys are checked\n", cnt);
  return 0;
}

