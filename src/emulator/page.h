#include <string>
#include <vector>

#include "entry.h"

#ifndef _PAGE_H_
#define _PAGE_H_

namespace tree_builder
{
    class Page
    {
    public:
        std::string min_sort_key;
        std::string max_sort_key;
        std::vector<Entry> entries_vector;

        Page();
        Page(const Page &other); // Deep copy constructor
        static std::vector<Page *> createNewPages(int page_count);
    };
}

#endif // _PAGE_H_