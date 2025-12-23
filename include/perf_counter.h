#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>

#include "buffer.h"

class PerfCounter {
public:
  PerfCounter();
  ~PerfCounter();

  void start();
  void stop();
  void read();
  void reportCSV(std::unique_ptr<Buffer> &buffer);

  uint64_t instructions = 0;
  uint64_t cycles = 0;
  uint64_t cache_misses = 0;

private:
  int fd_instr = -1;
  int fd_cycles = -1;
  int fd_cache_miss = -1;
};
