/*
 *  Created on: May 13, 2019
 *  Author: Subhadeep
 */

#ifndef EMU_ENVIRONMENT_H_
#define EMU_ENVIRONMENT_H_

#include <iostream>

using namespace std;

class EmuEnv
{
private:
  EmuEnv();
  static EmuEnv *instance;

public:
  static EmuEnv *getInstance();

  int size_ratio; // T

  int buffer_size_in_pages; // P
  int entries_per_page;     // B
  int entry_size;           // E
  long buffer_size;         // M = P*B*E ; in Bytes

  int delete_tile_size_in_pages; // h
  long file_size;                // B/s ; in Bytes
  int correlation;               // c

  float buffer_flush_threshold;
  float disk_run_flush_threshold;

  long num_inserts;              // I
  long num_updates;              // U
  long num_point_deletes;        // D
  long num_point_queries;        // Q
  long num_range_queries;        // S
  float range_query_selectivity; // Y

  int verbosity;
  int lethe_new; // 0 for classical lethe, 1 for new lethe

  int srd_count;
  int epq_count;
  int pq_count;
  int srq_count;
  int level_count;

  int variable_delete_tile_size_in_pages[20] = {-1, 1, 2, 3, 4, 5};

  bool enable_rq_compaction;  // enable range query driven compaction
  bool enable_level_shifting; // shift level upon saturation instead of doing compaction

  int getDeleteTileSize(int level);
};

#endif /*EMU_ENVIRONMENT_H_*/
