#include "perf_counter.h"

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

static int perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

PerfCounter::PerfCounter() {
  struct perf_event_attr pe;
  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  // Instructions
  pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  fd_instr = perf_event_open(&pe, 0, -1, -1, 0);

  // CPU cycles
  pe.config = PERF_COUNT_HW_CPU_CYCLES;
  fd_cycles = perf_event_open(&pe, 0, -1, -1, 0);

  // L1D Cache Misses
  pe.type = PERF_TYPE_HW_CACHE;
  pe.config = (PERF_COUNT_HW_CACHE_L1D |
               (PERF_COUNT_HW_CACHE_OP_READ << 8) |
               (PERF_COUNT_HW_CACHE_RESULT_MISS << 16));
  fd_cache_miss = perf_event_open(&pe, 0, -1, -1, 0);
}

PerfCounter::~PerfCounter() {
  if (fd_instr >= 0) close(fd_instr);
  if (fd_cycles >= 0) close(fd_cycles);
  if (fd_cache_miss >= 0) close(fd_cache_miss);
}

void PerfCounter::start() {
  ioctl(fd_instr, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd_instr, PERF_EVENT_IOC_ENABLE, 0);

  ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, 0);

  ioctl(fd_cache_miss, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd_cache_miss, PERF_EVENT_IOC_ENABLE, 0);
}

void PerfCounter::stop() {
  ioctl(fd_instr, PERF_EVENT_IOC_DISABLE, 0);
  ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
  ioctl(fd_cache_miss, PERF_EVENT_IOC_DISABLE, 0);
}

void PerfCounter::read() {
    ssize_t ret;
  
    ret = ::read(fd_instr, &instructions, sizeof(uint64_t));
    if (ret != sizeof(uint64_t)) {
      std::cerr << "Failed to read instructions counter\n";
    }
  
    ret = ::read(fd_cycles, &cycles, sizeof(uint64_t));
    if (ret != sizeof(uint64_t)) {
      std::cerr << "Failed to read cycles counter\n";
    }
  
    ret = ::read(fd_cache_miss, &cache_misses, sizeof(uint64_t));
    if (ret != sizeof(uint64_t)) {
      std::cerr << "Failed to read cache miss counter\n";
    }
  }

void PerfCounter::reportCSV(std::unique_ptr<Buffer> &buffer) {
  (*buffer) << ", " << instructions << ", " << cycles << ", " << cache_misses;
}
