#include <vector>

#include "entry.h"
#include "page.h"

namespace tree_builder
{
    Page::Page() {}
    Page::Page(const Page &other)
        : min_sort_key(other.min_sort_key),
          max_sort_key(other.max_sort_key)
    {
        // Deep copy entries_vector
        for (const auto &entry : other.entries_vector)
        {
            Entry newEntry(entry);
            entries_vector.push_back(newEntry);
        }
    }

    std::vector<Page *> Page::createNewPages(int page_count)
    {
        std::vector<Page *> pages(page_count);
        for (int i = 0; i < pages.size(); i++)
        {
            pages[i] = new Page();
            std::vector<Entry> entries;
            pages[i]->min_sort_key = "";
            pages[i]->max_sort_key = "";
            pages[i]->entries_vector = entries;
        }

        return pages;
    }
}