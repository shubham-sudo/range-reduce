/*
 *  Created on: May 14, 2019
 *  Author: Subhadeep, Papon
 */

#include <iostream>
#include <cmath>
#include <sys/time.h>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <iomanip>

#include "emu_environment.h"
#include "tree_builder.h"
#include "workload_executor.h"
#include "query_stats.h"

using namespace std;
using namespace tree_builder;
using namespace workload_exec;

long WorkloadExecutor::buffer_insert_count = 0;
long WorkloadExecutor::buffer_update_count = 0;
long WorkloadExecutor::total_insert_count = 0;
uint32_t WorkloadExecutor::counter = 0;

inline void showProgress(const uint32_t &workload_size, const uint32_t &counter)
{
  if (counter / (workload_size / 100) >= 1)
  {
    for (int i = 0; i < 104; i++)
    {
      std::cout << "\b";
      fflush(stdout);
    }
  }
  for (int i = 0; i < counter / (workload_size / 100); i++)
  {
    std::cout << "=";
    fflush(stdout);
  }
  std::cout << std::setfill(' ') << std::setw(101 - counter / (workload_size / 100));
  std::cout << counter * 100 / workload_size << "%";
  fflush(stdout);

  if (counter == workload_size)
  {
    std::cout << "\n";
    return;
  }
}

// Utility

bool Utility::sortbysortkey(const Entry *a, const Entry *b)
{
  return (a->getKey() < b->getKey());
}

int Utility::minInt(int a, int b)
{
  if (a <= b)
    return a;
  else
  {
    return b;
  }
}

SSTFile *Utility::trivialFileMove(SSTFile *head, vector<Entry *> entries_to_flush, int level_to_flush_in, int file_count, int entries_per_file)
{
  SSTFile *moving_head = head;
  for (int i = 0; i < file_count; i++) // Though this is not required because on each entry saturation is checked and this would trigger compaction
  {
    vector<Entry *> vector_to_populate_file;
    entries_per_file = Utility::minInt(entries_per_file, entries_to_flush.size());

    for (int j = 0; j < entries_per_file; ++j)
    {
      vector_to_populate_file.push_back(entries_to_flush[j]);
    }

    entries_to_flush.erase(entries_to_flush.begin(), entries_to_flush.begin() + entries_per_file);
    SSTFile *new_file = SSTFile::createNewSSTFile(vector_to_populate_file.size(), level_to_flush_in);
    if (level_to_flush_in == 1)
    {
      qstats->num_trivial_file_move_from_buffer += 1;
    }
    else
    {
      qstats->num_trivial_file_moves += 1;
    }

    if (moving_head == nullptr)
    {
      moving_head = new_file;
      DiskMetaFile::setSSTFileHead(moving_head, level_to_flush_in);
      int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
    }
    else
    {
      int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
      moving_head->next_file_ptr = new_file;
      moving_head = new_file;
    }
    vector_to_populate_file.clear();
  }
  return moving_head;
}

void Utility::mergeFilesAndFlush(SSTFile *head, vector<Entry *> entries_to_flush, int level_to_flush_in, int file_count, int entries_per_file)
{
  SSTFile *prev_file_ptr = head; // this will be used for linking previous file to new files
  SSTFile *moving_head = head;   // moving_head would move while merging

  if (level_to_flush_in != 1)
  {
    qstats->total_num_compaction += 1;
    qstats->total_entries_written_while_compaction += entries_to_flush.size();
  }

  bool make_first_file_head = false;

  if (prev_file_ptr == nullptr)
  {
    make_first_file_head = true;
    moving_head = DiskMetaFile::getSSTFileHead(level_to_flush_in);
  }
  else
  {
    moving_head = moving_head->next_file_ptr;
  }

  SSTFile *end_head = moving_head; // last file overlapping with new entries
  std::string largest_key = entries_to_flush[entries_to_flush.size() - 1]->getKey();

  while (end_head)
  {
    if (end_head->min_sort_key.compare(largest_key) <= 0)
    {
      end_head = end_head->next_file_ptr;
    }
    else
    {
      break;
    }
  }

  SSTFile *next_file_after_merge = end_head == moving_head ? end_head->next_file_ptr : end_head;

  EntryIterator eit(moving_head, next_file_after_merge);
  vector<Entry *>::iterator it = entries_to_flush.begin();
  eit = eit.begin();

  while (eit != eit.end() || it != entries_to_flush.end())
  {
    vector<Entry *> vector_to_populate_file;
    int j = 0;

    while (j < entries_per_file && (eit != eit.end() || it != entries_to_flush.end()))
    {
      if (eit != eit.end() && it != entries_to_flush.end())
      {
        const Entry &entry = *eit;
        const Entry *new_entry = *it;
        if (entry.getKey().compare(new_entry->getKey()) < 0)
        {
          Entry *ent = new Entry(entry);
          vector_to_populate_file.push_back(ent);
          ++eit;
        }
        else if (new_entry->getKey().compare(entry.getKey()) <= 0)
        {
          if (new_entry->getKey().compare(entry.getKey()) == 0)
          { // If new values have same key, priortize the new key
            ++eit;
          }
          Entry *ent = new Entry(*new_entry);
          vector_to_populate_file.push_back(ent);
          ++it;
        }
      }
      else if (it == entries_to_flush.end())
      {
        const Entry &entry = *eit;
        Entry *ent = new Entry(entry);
        vector_to_populate_file.push_back(ent);
        ++eit;
      }
      else if (eit == eit.end())
      {
        const Entry *new_entry = *it;
        Entry *ent = new Entry(*new_entry);
        vector_to_populate_file.push_back(ent);
        ++it;
      }

      j++;
    }

    SSTFile *new_file = SSTFile::createNewSSTFile(vector_to_populate_file.size(), level_to_flush_in);
    if (level_to_flush_in != 1)
    {
      qstats->total_num_files_written_while_compaction += 1;
      qstats->total_num_pages_written_while_compaction += new_file->pages.size();
    }

    if (make_first_file_head)
    {
      prev_file_ptr = new_file;
      DiskMetaFile::setSSTFileHead(prev_file_ptr, level_to_flush_in);
      int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
      make_first_file_head = false;
    }
    else
    {
      int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
      prev_file_ptr->next_file_ptr = new_file;
      prev_file_ptr = new_file;
    }
  }
  prev_file_ptr->next_file_ptr = next_file_after_merge;
}

void Utility::compactAndFlush(vector<Entry *> vector_to_compact, int level_to_flush_in)
{
  EmuEnv *_env = EmuEnv::getInstance();

  int entries_per_file = _env->entries_per_page * _env->buffer_size_in_pages;
  int file_count = ceil(vector_to_compact.size() / (entries_per_file * 1.0));
  if (MemoryBuffer::verbosity == 2)
    std::cout << "\nwriting " << file_count << " file(s)\n";

  SSTFile *level_head = DiskMetaFile::getSSTFileHead(level_to_flush_in);

  if (!level_head)
  {
    if (MemoryBuffer::verbosity == 2)
    {
      std::cout << "No Files Present at Level : " << level_to_flush_in
                << "  ->  Making Trivial Move"
                << std::endl;
    }
    SSTFile *last_file_ptr = trivialFileMove(level_head, vector_to_compact, level_to_flush_in, file_count, entries_per_file);
  }
  else
  {
    SSTFile *overlapping_files_start = level_head;
    SSTFile *prev_of_overlapping_file_start = level_head;
    std::string smallest_key = vector_to_compact[0]->getKey();
    std::string largest_key = vector_to_compact[vector_to_compact.size() - 1]->getKey();

    while (overlapping_files_start->next_file_ptr && smallest_key.compare(overlapping_files_start->max_sort_key) > 0)
    {
      prev_of_overlapping_file_start = overlapping_files_start;
      overlapping_files_start = overlapping_files_start->next_file_ptr;
    }

    if (overlapping_files_start->next_file_ptr == NULL && smallest_key.compare(overlapping_files_start->max_sort_key) > 0)
    {
      // Trivial move to the last of level
      if (MemoryBuffer::verbosity == 2)
      {
        std::cout << "Performing Trivial Move at Level : " << level_to_flush_in
                  << std::endl;
      }
      SSTFile *last_file_ptr = trivialFileMove(overlapping_files_start, vector_to_compact, level_to_flush_in, file_count, entries_per_file);
    }
    else if (smallest_key.compare(overlapping_files_start->min_sort_key) < 0 && largest_key.compare(overlapping_files_start->min_sort_key) < 0)
    {
      // In between or start of level trivial move
      if (MemoryBuffer::verbosity == 2)
      {
        std::cout << "\nPerforming Trivial Move at Level : " << level_to_flush_in
                  << std::endl;
        std::cout << "\nMin Sort Key of Prev File : " << prev_of_overlapping_file_start->min_sort_key
                  << "\nMax Sort Key of Prev File : " << prev_of_overlapping_file_start->max_sort_key
                  << "\nNew File Min Sort Key : " << smallest_key
                  << "\nNew File Max Sort Key : " << largest_key
                  << "\nNext File Min Sort Key : " << overlapping_files_start->min_sort_key
                  << "\nNext File Max Sort Key : " << overlapping_files_start->max_sort_key
                  << std::endl;
      }
      if (prev_of_overlapping_file_start == overlapping_files_start) // If both are same, it means the start of level
      {
        prev_of_overlapping_file_start = nullptr;
      }
      SSTFile *last_file_ptr = trivialFileMove(prev_of_overlapping_file_start, vector_to_compact, level_to_flush_in, file_count, entries_per_file);
      last_file_ptr->next_file_ptr = overlapping_files_start;
    }
    else
    {
      // Merge new data with existing SST file data on level 1
      if (MemoryBuffer::verbosity == 2)
      {
        std::cout << "Merge new entries with existing files entries at level : " << level_to_flush_in << std::endl;
      }
      if (prev_of_overlapping_file_start == overlapping_files_start) // If both are same, it means the start of level
      {
        prev_of_overlapping_file_start = nullptr;
      }
      mergeFilesAndFlush(prev_of_overlapping_file_start, vector_to_compact, level_to_flush_in, file_count, entries_per_file);
    }
  }
}

void Utility::sortAndWrite(vector<Entry *> vector_to_compact, int level_to_flush_in)
{
  EmuEnv *_env = EmuEnv::getInstance();
  if (level_to_flush_in == 1)
  {
    qstats->num_buffer_flush += 1;
    qstats->num_page_flushed_from_buffer += _env->buffer_size_in_pages;
    qstats->total_entries_written_buffer_flush += vector_to_compact.size();
  }

  std::sort(vector_to_compact.begin(), vector_to_compact.end(), Utility::sortbysortkey);
  if (MemoryBuffer::verbosity == 2)
  {
    std::cout << "Flushing into level: " << level_to_flush_in << std::endl;
  }
  compactAndFlush(vector_to_compact, level_to_flush_in);
  int saturation = DiskMetaFile::checkAndAdjustLevelSaturation(level_to_flush_in);
}

// WorkloadExecutor

int WorkloadExecutor::insert(std::string key, std::string value)
{
  Entry entry(key, value, EntryType::POINT_ENTRY);
  return MemoryBuffer::buffer->insert(entry);
}

int WorkloadExecutor::remove(std::string key)
{
  Entry entry(key, std::string(""), EntryType::POINT_TOMBSTONE);
  return MemoryBuffer::buffer->remove(entry);
}

std::string WorkloadExecutor::get(std::string key)
{
  // First Search key in buffer
  Entry entry(key, std::string(""), EntryType::NO_TYPE);

  std::pair<int, std::string> buffer_response = MemoryBuffer::buffer->get(entry);
  if (buffer_response.first != -1)
  {
    return buffer_response.second;
  }

  // Else check in Levels starting from 1st
  for (auto index : DiskMetaFile::getNonEmptyLevels())
  {
    SSTFile *level_head = DiskMetaFile::getSSTFileHead(index);

    while (level_head)
    {
      if (level_head->min_sort_key.compare(entry.getKey()) > 0)
      {
        break;
      }
      else if (level_head->max_sort_key.compare(entry.getKey()) >= 0)
      {
        auto pages_iterator = std::lower_bound(level_head->pages.begin(), level_head->pages.end(), entry, [](Page *stored_page, Entry entry)
                                               { return stored_page->max_sort_key.compare(entry.getKey()) < 0; });
        if (pages_iterator != level_head->pages.end())
        {
          Page *page = *pages_iterator;
          auto entries_iterator = std::lower_bound(page->entries_vector.begin(), page->entries_vector.end(), entry, [](Entry &stored_entry, Entry entry)
                                                   { return stored_entry.getKey().compare(entry.getKey()) < 0; });
          if (entries_iterator != page->entries_vector.end() && page->entries_vector[entries_iterator - page->entries_vector.begin()].getKey().compare(entry.getKey()) == 0)
          {
            Entry stored_entry = *entries_iterator;
            if (stored_entry.getType() == EntryType::POINT_TOMBSTONE)
            {
              if (MemoryBuffer::verbosity == 2)
              {
                std::cout << "Point Tombstone found for Key : " << stored_entry.getKey() << " Queried with key : " << key << std::endl;
              }
              return "Key : " + key + " NOT FOUND!";
            }
            else if (stored_entry.getType() == EntryType::POINT_ENTRY)
            {
              if (MemoryBuffer::verbosity == 2)
              {
                std::cout << "Found Entry with Key : " << stored_entry.getKey() << " Value : " << stored_entry.getValue() << std::endl;
              }
              return stored_entry.getValue();
            }
          }
        }
      }
      level_head = level_head->next_file_ptr;
    }
  }
  return "Key : " + key + " NOT FOUND!";
}

RangeIterator *WorkloadExecutor::getRange(std::string start_key, std::string end_key)
{
  // Create a Dummy level out of buffer entries to perform the range query
  EmuEnv *_env = EmuEnv::getInstance();

  int level_to_flush_in = 0;

  if (MemoryBuffer::buffer->entries.size() > 0)
  {
    vector<Entry *> vector_to_compact = MemoryBuffer::buffer->entries;
    std::sort(vector_to_compact.begin(), vector_to_compact.end(), Utility::sortbysortkey);
    int entries_per_file = Utility::minInt(_env->entries_per_page * _env->buffer_size_in_pages, vector_to_compact.size());
    int file_count = ceil(vector_to_compact.size() / (entries_per_file * 1.0));
    SSTFile *moving_head = nullptr;

    for (int i = 0; i < file_count; i++) // Though this is not required because on each entry saturation is checked and this would trigger compaction
    {
      SSTFile *new_file = SSTFile::createNewSSTFile(vector_to_compact.size(), level_to_flush_in);
      vector<Entry *> vector_to_populate_file;
      entries_per_file = Utility::minInt(entries_per_file, vector_to_compact.size());

      for (int j = 0; j < entries_per_file; ++j)
      {
        vector_to_populate_file.push_back(vector_to_compact[j]);
      }

      vector_to_compact.erase(vector_to_compact.begin(), vector_to_compact.begin() + entries_per_file);

      new_file->min_sort_key = vector_to_populate_file.at(0)->getKey();
      new_file->max_sort_key = vector_to_populate_file.at(vector_to_populate_file.size()-1)->getKey();

      if (moving_head == nullptr)
      {
        moving_head = new_file;
        DiskMetaFile::setSSTFileHead(moving_head, level_to_flush_in);
        int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
      }
      else
      {
        int status = SSTFile::PopulateFile(new_file, vector_to_populate_file, level_to_flush_in);
        moving_head->next_file_ptr = new_file;
        moving_head = new_file;
      }
      vector_to_populate_file.clear();
    }
  }

  // set-up the level iterators
  vector<LevelIterator *> level_its;

  for (auto index : DiskMetaFile::getNonEmptyLevels())
  {
    LevelIterator *level_it = new LevelIterator(index, start_key);
    level_it->begin();
    if (*level_it != level_it->end())
    {
      level_its.push_back(level_it);
    }
  }

  // If only one level has data for the range query always do state of the art
  if (level_its.size() == 1 or !_env->enable_rq_compaction)
  {
    LevelIterator *first_level = new LevelIterator(level_to_flush_in, start_key); // TODO: Can also flush this once the range query is done or even when iterator is created
    first_level->begin();

    if (*first_level != first_level->end())
    {
      level_its.push_back(first_level);
    }

    if (MemoryBuffer::verbosity == 2)
    {
      std::cout << "Doing Normal Range Query This Time ..." << std::endl;
    }

    if (level_its.size() == 0)
    {
      RangeIterator *merge_itr = new RangeQueryIterator();
      return merge_itr;
    }

    RangeIterator *merge_itr = new RangeQueryIterator(level_its, start_key, end_key);
    qstats->total_vanilla_compaction += 1;

    return merge_itr;
  }
  else
  {
    LevelIterator *first_level = new LevelIterator(level_to_flush_in, start_key); // TODO: Can also flush this once the range query is done or even when iterator is created
    first_level->begin();

    if (*first_level != first_level->end())
    {
      level_its.push_back(first_level);
    }

    // Do the range query compaction
    if (MemoryBuffer::verbosity == 2)
    {
      std::cout << "Doing Range Query Driven Compaction This Time ..." << std::endl;
    }

    if (level_its.size() == 0)
    {
      RangeIterator *merge_itr = new RangeQueryDrivenCompactionIterator();
      return merge_itr;
    }

    RangeIterator *merge_itr = new RangeQueryDrivenCompactionIterator(level_its, start_key, end_key);
    qstats->total_rqd_compaction += 1;

    return merge_itr;
  }
}

int WorkloadExecutor::getWorkloadStatictics(EmuEnv *_env)
{
  std::cout << "************* PRINTING WORKLOAD STATISTICS *************" << std::endl;
  std::cout << "Total inserts in buffer = " << total_insert_count << " (unique inserts = " << buffer_insert_count << "; in-place updates = " << buffer_update_count << ")" << std::endl;

  std::cout << "Insert stats: ";
  int total_compactions = 0;
  long total_files_in_compcations = 0;
  for (int i = 0; i < 32; i++)
  {
    total_compactions += DiskMetaFile::compaction_through_sortmerge_counter[i];
    total_files_in_compcations += DiskMetaFile::compaction_file_counter[i];
  }
  // int disk_pages_per_file = _env->buffer_size_in_pages/_env->buffer_split_factor;
  int disk_pages_per_file = _env->buffer_size_in_pages; // Doubt
  std::cout << "#compactions = " << total_compactions << ", #files_in_compactions = " << total_files_in_compcations << ", #IOs = " << 2 * total_files_in_compcations * disk_pages_per_file
            << " (#IOs does not include IOs for only writing or pointer manipulation)" << std::endl;

  std::cout << "Disk space amplification = ";
  SSTFile *level_1_head = DiskMetaFile::getSSTFileHead(1);
  long total_entries_in_L1 = 0;
  long duplicate_key_count = 0;
  // while (level_1_head != NULL) {
  //   total_entries_in_L1 += level_1_head->file_instance.size();
  //   for(int i = 0; i<level_1_head->file_instance.size(); ++i) {
  //     for(int j=1; j<32; ++j) {
  //       if( level_1_head->file_instance[i].first <= DiskMetaFile::level_min_key[1]) {
  //         //std::cout << "key smaller than min of L2 \n";
  //         break;
  //       }
  //       if( level_1_head->file_instance[i].first <= DiskMetaFile::level_max_key[j]) {
  //         duplicate_key_count += search(level_1_head->file_instance[i].first, j+1);
  //         break;
  //       }
  //     }
  //   }
  //   level_1_head = level_1_head->next_file_ptr;
  // }
  float space_amplification = (float)duplicate_key_count / total_entries_in_L1;
  std::cout << space_amplification << " (entries in L1 = " << total_entries_in_L1 << " / duplicated entries = " << duplicate_key_count << ")" << std::endl;

  std::cout << "********************************************************" << std::endl;

  return 1;
}
