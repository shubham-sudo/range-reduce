#include <iostream>
#include <cmath>
#include <sys/time.h>
#include "emu_environment.h"

/*Set up the singleton object with the experiment wide options*/
EmuEnv *EmuEnv::instance = 0;

EmuEnv::EmuEnv()
{
  size_ratio = 2;

  buffer_size_in_pages = 2;                                           // default 2 buffers
  entries_per_page = 8;                                               // setting to 8 entries per page
  entry_size = 8;                                                     // default which is specified in load_gen in Bytes
  buffer_size = buffer_size_in_pages * entries_per_page * entry_size; // M = P*B*E = 8 * 8 * 2 B = 128B

  delete_tile_size_in_pages = 2; // h
  file_size = buffer_size;       // file_size = M/s = 2 MB / 8 = 256 KB

  buffer_flush_threshold = disk_run_flush_threshold = 1;

  num_inserts = 0;
  num_updates = 0;
  num_point_queries = 0;
  num_range_queries = 0;
  range_query_selectivity = 0;

  verbosity = 1; // print all entries by default
  lethe_new = 0; // 0 for classical lethe, 1 for optimal Kiwi, 2 for Kiwi++
  srd_count = 1;
  epq_count = 1;
  pq_count = 1;
  srq_count = 1;
  level_count = 1;
  enable_rq_compaction = true;
  enable_level_shifting = false;
}
int EmuEnv::getDeleteTileSize(int level)
{
  if (lethe_new == 2)
  {
    return variable_delete_tile_size_in_pages[level];
  }
  else
  {
    return delete_tile_size_in_pages;
  }
}

EmuEnv *EmuEnv::getInstance()
{
  if (instance == 0)
    instance = new EmuEnv();

  return instance;
}
