/*
 *  Created on: May 14, 2019
 *  Author: Subhadeep, Papon
 */

#ifndef AWESOME_H_
#define AWESOME_H_

#include "emu_environment.h"
#include "sstfile.h"

#include <iostream>
#include <vector>

using namespace std;
using namespace tree_builder;

namespace workload_exec
{

  class WorkloadExecutor
  {
  public:
    static long total_insert_count;
    static long buffer_update_count;
    static long buffer_insert_count;
    static uint32_t counter;

    static int insert(std::string key, std::string value);
    static int remove(std::string key);
    static std::string get(std::string key);
    static RangeIterator* getRange(std::string start_key, std::string end_key);
    static int getWorkloadStatictics(EmuEnv *_env);
  };

  class Utility
  {
  private:
    static SSTFile *trivialFileMove(SSTFile *head, vector<Entry *> entries_to_flush, int level_to_flush_in, int file_count, int entries_per_file);
    static void mergeFilesAndFlush(SSTFile *head, vector<Entry *> entries_to_flush, int level_to_flush_in, int file_count, int entries_per_file);

  public:
    static void sortAndWrite(vector<Entry *> vector_to_compact, int level_to_flush_in);
    static void compactAndFlush(vector<Entry *> vector_to_compact, int level_to_flush_in);
    static bool sortbysortkey(const Entry *a, const Entry *b);
    static int minInt(int a, int b);
  };

}

#endif /*EMU_ENVIRONMENT_H_*/
