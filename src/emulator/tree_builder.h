/*
 *  Created on: May 14, 2019
 *  Author: Subhadeep, Papon
 */
#ifndef _TREE_BUILDER_H_
#define _TREE_BUILDER_H_
#include "emu_environment.h"
#include "memtable.h"
#include "sstfile.h"
#include "query_stats.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std;
using namespace emulator;

extern QueryStats *qstats;

namespace tree_builder
{
  // QueryStats *qstats = QueryStats::getQueryStats();

  extern SSTFile *trivialFileMoveForIterator(SSTFile *head, vector<Entry *> &entries_to_flush, int level_to_flush_in, int file_count, int entries_per_file);
  extern std::string getMinKeyFromSSTFile(SSTFile *sst_file);
  extern std::string getMaxKeyFromSSTFile(SSTFile *sst_file);

  class MemoryBuffer
  {
  private:
    MemoryBuffer(EmuEnv *_env);
    static MemoryBuffer *buffer_instance;
    static long max_buffer_size;

  public:
    static float buffer_flush_threshold;
    static long current_buffer_entry_count;
    static long current_buffer_size;
    static float current_buffer_saturation;
    static int buffer_flush_count;
    static MemTable *buffer;
    static int verbosity;

    static MemoryBuffer *getBufferInstance(EmuEnv *_env);
    static int getCurrentBufferStatistics();
    static int setCurrentBufferStatistics(int entry_count_change, int buffer_flush_threshold);
    static int initiateBufferFlush(int level_to_flush_in);
    static int printBufferEntries();
  };

  class DiskMetaFile
  {
  private:
    static pair<long, long> getMatchingKeyFile(long min_key, long max_key, int key_level); //??? Definition change hobe
    static void shiftLevelAndIncreaseSize(int level);  // This will shift level down and add an empty level at `level`

  public:
    // static SSTFile* head_level_1;
    static int checkAndAdjustLevelSaturation(int level);
    static long getLevelEntryCount(int level);
    static int getLevelFileCount(int level);
    static int getTotalLevelCount();
    static std::vector<int> getNonEmptyLevels();
    static int checkOverlapping(SSTFile *file, int level);

    static int setSSTFileHead(SSTFile *arg, int level);
    static SSTFile *getSSTFileHead(int level);

    static void getMetaStatistics();
    static int printAllEntries(int only_file_meta_data);
    static int getTotalPageCount();
    static int getTotalEntriesCount();
    static int clearAllEntries();

    static SSTFile *level_head[32];

    static long level_min_key[32];
    static long level_max_key[32];
    static long level_file_count[32];
    static long level_entry_count[32];
    static long level_current_size[32];
    static int last_level;

    static long level_max_size[32];
    static long level_max_file_count[32];
    static long global_level_file_counter[32];
    static float disk_run_flush_threshold[32];

    static int compaction_counter[32];
    static int compaction_through_sortmerge_counter[32];
    static int compaction_through_point_manipulation_counter[32];
    static int compaction_file_counter[32];
  };

  /*
   * Level Iterator does whatever its name says. It helps in
   * iterating all the entries present in that `level_`. It also
   * calls forward `seek()` functionality which helps in fast-forwarding
   * iterator to some specific target, if exists.
   */
  class LevelIterator
  {
  private:
    int level_;
    std::string target_;
    EntryIterator entry_iterator;

  public:
    LevelIterator(int level, std::string target)
        : level_(level), target_(target), entry_iterator(DiskMetaFile::level_head[level], nullptr) {}

    // Dereference operator
    const Entry &operator*() const
    {
      return *entry_iterator;
    }

    // Pre-increment operator
    LevelIterator &operator++()
    {
      ++entry_iterator;
      return *this;
    }

    // Equality operator
    bool operator==(const LevelIterator &other) const
    {
      return (level_ == other.level_) &&
             (target_ == other.target_) &&
             (entry_iterator == other.entry_iterator);
    }

    // Inequality operator
    bool operator!=(const LevelIterator &other) const
    {
      return !(*this == other);
    }

    bool operator!=(const LevelIterator *other) const
    {
      return !(this == other);
    }

    // Getter for level
    int getLevel()
    {
      return this->level_;
    }

    // Getter for PrevSSTFile from entry_iterator
    SSTFile *getPrevSSTFile()
    {
      return entry_iterator.getPrevSSTFile();
    }

    // Getter for StartSSTFile from entry_iterator
    SSTFile *getCurrentSSTFile()
    {
      return entry_iterator.getCurrentSSTFile();
    }

    // Iterator methods
    LevelIterator begin()
    {
      if (entry_iterator != entry_iterator.end())
      {
        this->entry_iterator.seek(target_);
        return *this;
      }
      return end();
    }

    LevelIterator end() const
    {
      auto level_end = LevelIterator(level_, target_);
      level_end.entry_iterator = level_end.entry_iterator.end();
      return level_end;
    }
  };

  class RangeIterator
  {
  protected:
    Entry *current_entry;

  public:
    const Entry &operator*() const { return *current_entry; }
    virtual bool isValid() const = 0;
    virtual RangeIterator *next() = 0;
    virtual RangeIterator *begin() = 0;
    RangeIterator() : current_entry(nullptr) {}
    virtual ~RangeIterator() {}
  };

  /*
   * Range Iterator helps in iterating through start of any range query
   * from across the LSM tree and skips invalid keys as well as tombstones.
   * This use the LevelIterator internally to iterate through all entries.
   */
  class RangeQueryIterator : public RangeIterator
  {
  private:
    std::string target_;
    std::string end_target_;
    std::vector<LevelIterator *> level_iterators;
    // Entry *current_entry;

    struct IteratorComparator
    {
      bool operator()(const LevelIterator &lhs, const LevelIterator &rhs) const
      {
        // Compare the current entries pointed by the iterators
        if ((*lhs).getKey().compare((*rhs).getKey()) == 0)
        {
          return (*lhs).getTimeTag() < (*rhs).getTimeTag();
        }
        return (*lhs).getKey().compare((*rhs).getKey()) > 0;
      }
    };

    priority_queue<LevelIterator, std::vector<LevelIterator>, IteratorComparator> level_iterators_queue;

  public:
    RangeQueryIterator(std::vector<LevelIterator *> level_iterators, std::string target, std::string end_target)
        : level_iterators(level_iterators), target_(target), end_target_(end_target) {}
    RangeQueryIterator() : level_iterators(), target_(), end_target_() {}

    // Pre-increment operator
    RangeIterator &operator++()
    {
      if (!level_iterators_queue.empty())
      {
        LevelIterator level_itr = level_iterators_queue.top();
        Entry level_entry = *level_itr;
        if (level_entry.getKey().compare(end_target_) > 0)
        {
          *this = RangeQueryIterator();
          return *this;
        }

        // increment the level iterator and push back if still valid
        ++level_itr;
        level_iterators_queue.pop();
        if (level_itr != level_itr.end())
        {
          level_iterators_queue.push(level_itr);
        }

        // if not tombstone just return and increment the required iterator
        while (!level_iterators_queue.empty() && level_entry.getKey().compare((*level_iterators_queue.top()).getKey()) == 0)
        {
          if (MemoryBuffer::verbosity == 2)
          {
            auto level_ = level_iterators_queue.top();
            std::cout << "Skipping logically invalidated ... Key: " << (*level_iterators_queue.top()).getKey() << " from Level: " << level_.getLevel() << std::endl;
          }
          level_itr = level_iterators_queue.top();
          ++level_itr;
          level_iterators_queue.pop();
          if (level_itr != level_itr.end())
          {
            level_iterators_queue.push(level_itr);
          }
        }

        if (level_entry.getType() != EntryType::POINT_TOMBSTONE)
        {
          current_entry = new Entry(level_entry); // FIXME: This could lead to memory leak
          return *this;
        }
        else
        {
          if (MemoryBuffer::verbosity == 2)
          {
            std::cout << "Skipping ... Tombstone for Key : " << level_entry.getKey() << std::endl;
          }
          // level entry is tombstone
          return ++(*this);
        }
      }
      else
      {
        *this = RangeQueryIterator();
      }
      return *this;
    }

    // Equality operator
    bool operator==(const RangeQueryIterator &other) const
    {
      return (target_ == other.target_) && (((!level_iterators_queue.empty() && !other.level_iterators_queue.empty()) &&
                                             (level_iterators_queue.top() == other.level_iterators_queue.top())) ||
                                            (level_iterators_queue.empty() & other.level_iterators_queue.empty()));
    }

    // Inequality operator
    bool operator!=(const RangeQueryIterator &other) const
    {
      bool valid = !(*this == other);
      if (!valid)
      {
        // cleaning up the 0th level data (this was pushed for range query)
        DiskMetaFile::setSSTFileHead(nullptr, 0);
      }
      return valid;
    }

    virtual bool isValid() const override
    {
      return *this != RangeQueryIterator();
    }

    virtual RangeIterator *next() override
    {
      return &++(*this);
    }

    // Iterator methods
    virtual RangeIterator *begin() override
    {
      if (level_iterators.size() > 0)
      {
        while (!this->level_iterators_queue.empty())
        {
          this->level_iterators_queue.pop();
        }

        for (auto level_itr : this->level_iterators)
        {
          this->level_iterators_queue.push(*level_itr);
        }

        return &++(*this);
      }
      return nullptr;
    }

    virtual ~RangeQueryIterator() {}
  };

  /*
   * Range query driven compaction Iterator does the same thing as RangeIterator
   * Additionally it also writes files back to those level while compacting keys
   * and skipping invalid and tombstones entries
   *
   * ======= HOW ? ========
   * ... comming soon ...
   */
  class RangeQueryDrivenCompactionIterator : public RangeIterator
  {
  private:
    const EmuEnv *_env = EmuEnv::getInstance();
    std::string target_;
    std::string end_target_;
    int last_level_;
    std::vector<LevelIterator *> level_iterators_;
    std::unordered_map<int, SSTFile *> level_start_sst_file_copies; // level to start_sst_file copy map
    std::unordered_map<int, SSTFile *> level_prev_sst_file_ptrs;    // this would keep prev_file_ptr for each level
    vector<Entry *> valid_entries;
    int entries_per_file = _env->entries_per_page * _env->buffer_size_in_pages;
    struct IteratorComparator
    {
      bool operator()(const LevelIterator *lhs, const LevelIterator *rhs) const
      {
        // Compare the current entries pointed by the iterators
        if ((**lhs).getKey().compare((**rhs).getKey()) == 0)
        {
          return (**lhs).getTimeTag() < (**rhs).getTimeTag();
        }
        return (**lhs).getKey().compare((**rhs).getKey()) > 0;
      }
    };

    priority_queue<LevelIterator *, std::vector<LevelIterator *>, IteratorComparator> level_iterators_queue;

  public:
    RangeQueryDrivenCompactionIterator(std::vector<LevelIterator *> level_iterators, std::string target, std::string end_target)
        : level_iterators_(level_iterators), target_(target), end_target_(end_target) {}
    RangeQueryDrivenCompactionIterator() : level_iterators_(), target_(), end_target_() {}

    // closing steps
    void linkEverythingBack()
    {
      // check if valid_entries vector is full
      if (valid_entries.size() > 0)
      {
        int file_count = ceil(valid_entries.size() / (entries_per_file * 1.0));
        level_prev_sst_file_ptrs[last_level_] = trivialFileMoveForIterator(level_prev_sst_file_ptrs[last_level_], valid_entries, last_level_, file_count, entries_per_file);
      }

      // connect pointer back to the newly created files
      for (auto level_itr : this->level_iterators_)
      {
        if (level_itr->getLevel() != 0)
        {
          if (level_itr->getCurrentSSTFile() == nullptr && level_prev_sst_file_ptrs[level_itr->getLevel()] == nullptr)
          {
            DiskMetaFile::setSSTFileHead(nullptr, level_itr->getLevel());
          }
          else if (level_itr->getCurrentSSTFile() == nullptr && level_prev_sst_file_ptrs[level_itr->getLevel()] != nullptr)
          {
            level_prev_sst_file_ptrs[level_itr->getLevel()]->next_file_ptr = nullptr;
          }
          else if (level_prev_sst_file_ptrs[level_itr->getLevel()] != nullptr && level_itr->getCurrentSSTFile() != nullptr && level_prev_sst_file_ptrs[level_itr->getLevel()]->file_id == level_itr->getCurrentSSTFile()->file_id)
          {
            level_prev_sst_file_ptrs[level_itr->getLevel()]->next_file_ptr = level_itr->getCurrentSSTFile()->next_file_ptr;
          }
          else
          {
            // clear entries from start till end_target_ of last file accessed and save back while linking with current previous file
            if (level_prev_sst_file_ptrs[level_itr->getLevel()] == nullptr)
            {
              DiskMetaFile::setSSTFileHead(processLastFileOfEachLevelInRange(level_itr->getCurrentSSTFile()), level_itr->getLevel());
            }
            else
            {
              level_prev_sst_file_ptrs[level_itr->getLevel()]->next_file_ptr = processLastFileOfEachLevelInRange(level_itr->getCurrentSSTFile());
            }
          }
        }
      }

      // Once everything is done check if last level is bigger than T ratio size
      int saturation = DiskMetaFile::checkAndAdjustLevelSaturation(last_level_);
    }

    // Pre-increment operator
    RangeIterator &operator++()
    {
      if (!level_iterators_queue.empty())
      {
        LevelIterator *level_itr = level_iterators_queue.top();
        Entry level_entry = **level_itr;
        int current_level = level_itr->getLevel();

        if (level_entry.getKey().compare(end_target_) > 0)
        {
          linkEverythingBack();
          *this = RangeQueryDrivenCompactionIterator();
          return *this;
        }

        // increment the level iterator and push back if still valid
        ++(*level_itr);
        level_iterators_queue.pop();
        if (*level_itr != level_itr->end())
        {
          level_iterators_queue.push(level_itr);
        }

        // if not tombstone just return and increment the required iterator
        while (!level_iterators_queue.empty() && level_entry.getKey().compare((**(level_iterators_queue.top())).getKey()) == 0)
        {
          if (MemoryBuffer::verbosity == 2)
          {
            LevelIterator *level_ = level_iterators_queue.top();
            std::cout << "Skipping logically invalidated ... Key: " << (**level_).getKey() << " from Level: " << level_->getLevel() << std::endl;
          }
          level_itr = level_iterators_queue.top();
          ++(*level_itr);
          level_iterators_queue.pop();
          if (*level_itr != level_itr->end())
          {
            level_iterators_queue.push(level_itr);
          }
        }

        if (level_entry.getType() != EntryType::POINT_TOMBSTONE)
        {
          Entry *valid_entry = new Entry(level_entry);
          current_entry = valid_entry;

          if (current_level != 0)
          {
            valid_entries.push_back(valid_entry);
          }

          // Write new file at last level ... this could be background job
          if (valid_entries.size() == entries_per_file)
          {
            int file_count = ceil(valid_entries.size() / (entries_per_file * 1.0));
            level_prev_sst_file_ptrs[last_level_] = trivialFileMoveForIterator(level_prev_sst_file_ptrs[last_level_], valid_entries, last_level_, file_count, entries_per_file);
          }
          return *this;
        }
        else
        {
          if (MemoryBuffer::verbosity == 2)
          {
            std::cout << "Skipping ... Tombstone for Key : " << level_entry.getKey() << std::endl;
          }
          // level entry is tombstone
          return ++(*this);
        }
      }
      else
      {
        linkEverythingBack();
        *this = RangeQueryDrivenCompactionIterator();
      }
      return *this;
    }

    /*
     * Process last SST file in range from each level
     *  - remove entries which are in range from last file
     *  - retrun the file to link back
     */
    SSTFile *processLastFileOfEachLevelInRange(SSTFile *last_sst_file)
    {
      if (last_sst_file == nullptr)
      {
        return last_sst_file;
      }

      // calculate page index
      auto pages_iterator = std::upper_bound(last_sst_file->pages.begin(), last_sst_file->pages.end(), end_target_, [](const std::string &entry, const Page *stored_page)
                                             { return entry.compare(stored_page->max_sort_key) < 0; });

      if (pages_iterator == last_sst_file->pages.end())
      {
        return last_sst_file->next_file_ptr;
      }

      // calculate entry index with-in page
      Page *page;
      int page_index = pages_iterator - last_sst_file->pages.begin();

      // Page to be removed if empty
      std::vector<Page *> empty_pages;

      // clean entries
      do
      {
        page = last_sst_file->pages[page_index];
        auto end_entries_iterator = std::upper_bound(page->entries_vector.begin(), page->entries_vector.end(), end_target_, [](const std::string &entry, const Entry &stored_entry)
                                                     { return entry.compare(stored_entry.getKey()) < 0; });
        page->entries_vector.erase(page->entries_vector.begin(), end_entries_iterator);

        if (page->entries_vector.size() == 0)
        {
          empty_pages.push_back(page);
        }
        else
        {
          page->min_sort_key = page->entries_vector[0].getKey();
          page->max_sort_key = page->entries_vector[page->entries_vector.size() - 1].getKey();
        }

        page_index--;

      } while (page_index >= 0);

      // remove empty pages
      if (empty_pages.size() > 0)
      {
        for (Page *empty_page : empty_pages)
        {
          last_sst_file->pages.erase(std::remove(last_sst_file->pages.begin(), last_sst_file->pages.end(), empty_page));
        }
      }

      // write file back to levels
      if (hasEntriesInSSTFileCopy(last_sst_file))
      {
        last_sst_file->min_sort_key = getMinKeyFromSSTFile(last_sst_file);
        last_sst_file->max_sort_key = getMaxKeyFromSSTFile(last_sst_file);

        qstats->num_files_written_during_range_query += 1;
        std::for_each(last_sst_file->pages.begin(), last_sst_file->pages.end(), [](Page *page){
          qstats->num_pages_written_during_range_query += 1;
          qstats->num_entries_written_during_range_query += page->entries_vector.size();
        });

        return last_sst_file;
      }
      else
      {
        return last_sst_file->next_file_ptr;
      }
    }

    // Equality operator
    bool operator==(const RangeQueryDrivenCompactionIterator &other) const
    {
      return (target_ == other.target_) && (end_target_ == other.end_target_) &&
             (((!level_iterators_queue.empty() && !other.level_iterators_queue.empty()) &&
               (level_iterators_queue.top() == other.level_iterators_queue.top())) ||
              (level_iterators_queue.empty() & other.level_iterators_queue.empty()));
    }

    // Inequality operator
    bool operator!=(const RangeQueryDrivenCompactionIterator &other) const
    {
      return !(*this == other);
    }

    /*
     * Check if entries > 0 in SSTFile copy
     */
    bool hasEntriesInSSTFileCopy(SSTFile *sst_file)
    {
      bool has_entries = false;

      for (Page *page : sst_file->pages)
      {
        if (page->entries_vector.size() > 0)
        {
          has_entries = true;
          break;
        }
      }
      return has_entries;
    }

    /*
     * Process the copy of first SST file in range from each level
     *  - remove entries which are in range
     *  - write the file back and update the prev file pointer
     */
    void processFirstFileOfEachLevelInRange()
    {
      for (auto level_sst_file_copy : level_start_sst_file_copies)
      {
        SSTFile *start_sst_file = level_sst_file_copy.second;
        start_sst_file->next_file_ptr = nullptr; // setting next file ptr to null

        // calculate page index
        auto pages_iterator = std::lower_bound(start_sst_file->pages.begin(), start_sst_file->pages.end(), target_, [](const Page *stored_page, const std::string &entry)
                                               { return stored_page->max_sort_key.compare(entry) < 0; });

        // calculate entry index with-in page
        Page *page = *pages_iterator;

        // Page to be removed if empty
        std::vector<Page *> empty_pages;

        // clean entries
        do
        {
          page = *pages_iterator;
          auto entries_iterator = std::lower_bound(page->entries_vector.begin(), page->entries_vector.end(), target_, [](const Entry &stored_entry, const std::string &entry)
                                                   { return stored_entry.getKey().compare(entry) < 0; });
          auto end_entries_iterator = std::upper_bound(page->entries_vector.begin(), page->entries_vector.end(), end_target_, [](const std::string &entry, const Entry &stored_entry)
                                                       { return entry.compare(stored_entry.getKey()) < 0; });

          if (entries_iterator > end_entries_iterator)
          {
            break; // If nothing found in between target_ and end_target_
          }
          page->entries_vector.erase(entries_iterator, end_entries_iterator);

          if (page->entries_vector.size() == 0)
          {
            empty_pages.push_back(page);
          }
          else
          {
            page->min_sort_key = page->entries_vector[0].getKey();
            page->max_sort_key = page->entries_vector[page->entries_vector.size() - 1].getKey();
          }
          pages_iterator++;

        } while (pages_iterator != start_sst_file->pages.end());

        // remove empty pages
        if (empty_pages.size() > 0)
        {
          for (Page *empty_page : empty_pages)
          {
            start_sst_file->pages.erase(std::remove(start_sst_file->pages.begin(), start_sst_file->pages.end(), empty_page));
            delete empty_page;
            empty_page = nullptr;
          }
        }

        // write file back to levels
        if (hasEntriesInSSTFileCopy(start_sst_file))
        {
          start_sst_file->min_sort_key = getMinKeyFromSSTFile(start_sst_file);
          start_sst_file->max_sort_key = getMaxKeyFromSSTFile(start_sst_file);

          qstats->num_files_written_during_range_query += 1;

          std::for_each(start_sst_file->pages.begin(), start_sst_file->pages.end(), [](Page *page){
            qstats->num_pages_written_during_range_query += 1;
            qstats->num_entries_written_during_range_query += page->entries_vector.size();
          });

          SSTFile *prev_sst_file_pointer = level_prev_sst_file_ptrs[level_sst_file_copy.first];
          if (prev_sst_file_pointer == nullptr)
          {
            DiskMetaFile::setSSTFileHead(start_sst_file, level_sst_file_copy.first);
          }
          else
          {
            prev_sst_file_pointer->next_file_ptr = start_sst_file;
          }
          level_prev_sst_file_ptrs[level_sst_file_copy.first] = start_sst_file;
        }
        else
        {
          delete start_sst_file;
          level_start_sst_file_copies[level_sst_file_copy.first] = nullptr;
          start_sst_file = nullptr;
        }
      }
    }

    virtual bool isValid() const override
    {
      bool valid = *this != RangeQueryDrivenCompactionIterator();
      if (!valid)
      {
        // cleaning up the 0th level data (this was pushed for range query)
        DiskMetaFile::setSSTFileHead(nullptr, 0);
      }
      return valid;
    }

    virtual RangeIterator *next() override
    {
      return &++(*this);
    }

    // Iterator methods
    virtual RangeIterator *begin() override
    {
      if (level_iterators_.size() > 0)
      {
        // clean up
        while (!this->level_iterators_queue.empty())
        {
          level_iterators_queue.pop();
        }
        level_start_sst_file_copies.clear();
        int last_level = -1;

        // reinitialize everything
        for (auto *level_itr : this->level_iterators_)
        {
          if (level_itr->getLevel() != 0)
          {
            last_level = max(last_level, level_itr->getLevel());
            level_start_sst_file_copies[level_itr->getLevel()] = new SSTFile(*(level_itr->getCurrentSSTFile()));
            level_prev_sst_file_ptrs[level_itr->getLevel()] = level_itr->getPrevSSTFile();
            level_iterators_queue.push(level_itr);
          }
        }

        // clear few entries
        processFirstFileOfEachLevelInRange();
        this->last_level_ = last_level;

        return &++(*this);
      }
      return nullptr;
    }

    virtual ~RangeQueryDrivenCompactionIterator()
    {
      for (auto level_itr_ : level_iterators_)
      {
        delete level_itr_;
      }
      level_iterators_.clear();

      for (auto entry : valid_entries)
      {
        delete entry;
      }
      valid_entries.clear();

      while (!level_iterators_queue.empty())
      {
        delete level_iterators_queue.top();
        level_iterators_queue.pop();
      }

      for (auto &entry : level_prev_sst_file_ptrs)
      {
        delete entry.second;
      }

      level_prev_sst_file_ptrs.clear();
      for (auto &entry : level_start_sst_file_copies)
      {
        delete entry.second;
      }

      level_start_sst_file_copies.clear();
    }
  };
} // namespace

#endif // _TREE_BUILDER_H_