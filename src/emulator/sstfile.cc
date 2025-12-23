#include <algorithm>
#include <cmath>

#include "emu_environment.h"
#include "page.h"
#include "sstfile.h"
#include "tree_builder.h"
#include "workload_executor.h"

using namespace workload_exec;

namespace tree_builder
{
    SSTFile::SSTFile() {}
    SSTFile::SSTFile(const SSTFile &other)
        : file_level(other.file_level),
          file_id(other.file_id),
          min_sort_key(other.min_sort_key),
          max_sort_key(other.max_sort_key),
          next_file_ptr(nullptr) // Initialize next_file_ptr to nullptr
    {
        // Deep copy pages vector
        for (const auto *page : other.pages)
        {
            Page *newPage = new Page(*page);
            pages.push_back(newPage);
        }
    }

    SSTFile *SSTFile::createNewSSTFile(int total_entries, int level_to_flush_in)
    {
        EmuEnv *_env = EmuEnv::getInstance();

        SSTFile *new_file = new SSTFile();
        new_file->max_sort_key = "";
        new_file->min_sort_key = "";
        int max_entries_in_file = std::min(total_entries, _env->entries_per_page * _env->buffer_size_in_pages);

        new_file->pages = Page::createNewPages(ceil((max_entries_in_file * 1.0) / (_env->entries_per_page)));

        new_file->file_level = level_to_flush_in;
        new_file->next_file_ptr = NULL;
        DiskMetaFile::global_level_file_counter[level_to_flush_in]++;
        new_file->file_id = "L" + std::to_string(level_to_flush_in) + "F" + std::to_string(DiskMetaFile::global_level_file_counter[level_to_flush_in]);
        return new_file;
    }

    int SSTFile::PopulateFile(SSTFile *file, std::vector<Entry *> vector_to_populate_file, int level_to_flush_in)
    {
        EmuEnv *_env = EmuEnv::getInstance();

        int page_count = file->pages.size();
        file->min_sort_key = vector_to_populate_file[0]->getKey();
        file->max_sort_key = vector_to_populate_file[vector_to_populate_file.size() - 1]->getKey();

        if (MemoryBuffer::verbosity == 2)
        {
            std::cout << "In PopulateFile() ... " << std::endl;
            std::cout << "File to Populate with Entries Count : " << vector_to_populate_file.size() << std::endl;
        }

        for (int i = 0; i < page_count; i++)
        {
            std::vector<Entry *> vector_to_populate_tile;
            int entries_per_page = Utility::minInt(_env->entries_per_page, vector_to_populate_file.size());
            for (int j = 0; j < entries_per_page; ++j)
            {
                vector_to_populate_tile.push_back(vector_to_populate_file[j]);
            }
            std::sort(vector_to_populate_tile.begin(), vector_to_populate_tile.end(), Utility::sortbysortkey);
            vector_to_populate_file.erase(vector_to_populate_file.begin(), vector_to_populate_file.begin() + entries_per_page);

            int status = SSTFile::PopulatePage(file, vector_to_populate_tile, i, level_to_flush_in);
            vector_to_populate_tile.clear();
        }
        return 1;
    }

    int SSTFile::PopulatePage(SSTFile *file, std::vector<Entry *> entries_to_write, int index, int level_to_flush_in)
    {
        if (MemoryBuffer::verbosity == 2)
        {
            std::cout << "In PopulatePage() ..." << std::endl;
        }

        EmuEnv *_env = EmuEnv::getInstance();
        Page *page = file->pages[index];
        page->entries_vector.clear();
        page->min_sort_key = entries_to_write[0]->getKey();
        page->max_sort_key = entries_to_write[entries_to_write.size() - 1]->getKey();

        for (int i = 0; i < entries_to_write.size(); i++)
        {
            Entry *new_entry = new Entry(*entries_to_write[i]);
            page->entries_vector.push_back(*new_entry);
            delete new_entry;
        }

        if (MemoryBuffer::verbosity == 2)
        {
            std::cout << "Populated Page : " << index << " at Level : " << level_to_flush_in << std::endl;
            std::cout << "Page populated Entries Count : " << entries_to_write.size() << std::endl;
        }

        return 1;
    }

}