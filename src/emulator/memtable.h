#include <vector>

#include "entry.h"

#ifndef _MEMTABLE_H_
#define _MEMTABLE_H_

class MemTable
{
protected:
    void virtual checkMemTableFull() = 0;

public:
    std::vector<Entry *> entries{}; // Change this to generic container

    /*
     * Insert or Update an entry into Memtable (The update would work only in-memory)
     * The implementation coud be different based on type of memetable
     * Type can be skiplist, vector, linklist any
     */
    bool virtual insert(Entry &entry) = 0;

    /*
     * Delete an entry from the Memtable (If the entry exists in-memory remove
     * and add tombstone for older, otherwise just add tombstone for older)
     */
    bool virtual remove(Entry &entry) = 0;

    /*
     * Get the key index and value if exists in MemTable
     */
    std::pair<int, std::string> virtual get(Entry &entry) = 0;

    virtual ~MemTable() {}
};

#endif // _MEMTABLE_H_