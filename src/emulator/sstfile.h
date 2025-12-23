#include <string>
#include <vector>

#include "entry.h"
#include "page.h"

#ifndef _SSTFILE_H_
#define _SSTFILE_H_

namespace tree_builder
{
    class SSTFile
    {
    public:
        int file_level;
        std::string file_id;
        std::string min_sort_key;
        std::string max_sort_key;

        std::vector<Page *> pages;
        struct SSTFile *next_file_ptr;

        SSTFile();
        SSTFile(const SSTFile &other);

        static struct SSTFile *createNewSSTFile(int total_entries, int level_to_flush_in);
        static int PopulateFile(SSTFile *file, std::vector<Entry *> vector_to_populate_file, int level_to_flush_in);
        static int PopulatePage(SSTFile *file, std::vector<Entry *> entries_to_write, int index, int level_to_flush_in);
    };

    /*
     * Entry Iterator iterates through all SSTFilles that are connected
     * together with pointer `next_file_ptr` and return entries stored in them
     */
    class EntryIterator
    {
    private:
        const SSTFile *end_sst_file_;
        const SSTFile *start_sst_file;
        SSTFile *prev_sst_file_;             // Keeps track of prev of start_sst_file_ before seek
        SSTFile *start_sst_file_;            // Keeps track of current sst_file
        int page_index_;
        int entry_index_;

    public:
        EntryIterator(SSTFile *start_sst_file, const SSTFile *end_sst_file)
            : start_sst_file(start_sst_file), start_sst_file_(start_sst_file), end_sst_file_(end_sst_file), page_index_(0), entry_index_(0), prev_sst_file_(nullptr) {}

        // Dereference operator
        const Entry &operator*() const
        {
            return start_sst_file_->pages[page_index_]->entries_vector[entry_index_];
        }

        // Pre-increment operator
        EntryIterator &operator++()
        {
            // Move to the next entry
            ++entry_index_;

            // Check if we reached the end of the current page
            if (entry_index_ >= start_sst_file_->pages[page_index_]->entries_vector.size())
            {
                // Move to the next page
                if (page_index_ + 1 < start_sst_file_->pages.size())
                {
                    page_index_++;
                }
                else if (page_index_ + 1 >= start_sst_file_->pages.size() && start_sst_file_->next_file_ptr != end_sst_file_)
                {
                    start_sst_file_ = start_sst_file_->next_file_ptr;
                    page_index_ = 0;
                }
                else
                {
                    *this = EntryIterator(nullptr, end_sst_file_);
                }
                entry_index_ = 0;
            }

            return *this;
        }

        // Equality operator
        bool operator==(const EntryIterator &other) const
        {
            return (start_sst_file_ == other.start_sst_file_) &&
                   (end_sst_file_ == other.end_sst_file_);
        }

        // Inequality operator
        bool operator!=(const EntryIterator &other) const
        {
            return !(*this == other);
        }

        // getter method for prev_sst_file pointer
        SSTFile *getPrevSSTFile()
        {
            return this->prev_sst_file_;
        }

        // getter method for start_sst_file_ ptr
        SSTFile *getCurrentSSTFile()
        {
            return this->start_sst_file_;
        }

        // Iterator methods
        EntryIterator begin() const
        {
            if (start_sst_file_)
            {
                return EntryIterator(start_sst_file_, end_sst_file_);
            }
            return end();
        }

        EntryIterator end() const
        {
            return EntryIterator(nullptr, end_sst_file_);
        }

        // fast-forward iterator
        void seek(std::string target)
        {
            if (!start_sst_file_)
            {
                return;
            }

            while (start_sst_file_)
            {
                if (start_sst_file_->max_sort_key.compare(target) < 0)
                {
                    prev_sst_file_ = start_sst_file_;
                    start_sst_file_ = start_sst_file_->next_file_ptr;
                }
                else
                {
                    break;
                }
            }

            if (!start_sst_file_)
            {
                return;
            }
            auto pages_iterator = std::lower_bound(start_sst_file_->pages.begin(), start_sst_file_->pages.end(), target, [](Page *stored_page, std::string key)
                                                   { return stored_page->max_sort_key.compare(key) < 0; });
            if (pages_iterator != start_sst_file_->pages.end())
            {
                page_index_ = pages_iterator - start_sst_file_->pages.begin();
                Page *page = *pages_iterator;
                auto entries_iterator = std::lower_bound(page->entries_vector.begin(), page->entries_vector.end(), target, [](Entry &stored_entry, std::string key)
                                                         { return stored_entry.getKey().compare(key) < 0; });

                if (entries_iterator != page->entries_vector.end())
                {
                    Entry stored_entry = *entries_iterator;
                    entry_index_ = entries_iterator - page->entries_vector.begin();
                }
                else
                {
                    entry_index_ = page->entries_vector.size();
                }
            }
            else
            {
                page_index_ = start_sst_file_->pages.size();
                entry_index_ = start_sst_file_->pages[page_index_ - 1]->entries_vector.size();
            }
        }
    };
}

#endif // _SSTFILE_H_