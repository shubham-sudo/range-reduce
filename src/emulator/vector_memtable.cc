#include <algorithm>
#include <iostream>
#include <chrono>

#include "tree_builder.h"
#include "emu_environment.h"
#include "workload_executor.h"
#include "vector_memtable.h"

using namespace tree_builder;
using namespace workload_exec;

void VectorMemTable::checkMemTableFull()
{
    if (MemoryBuffer::current_buffer_saturation >= MemoryBuffer::buffer_flush_threshold)
    {
        if (MemoryBuffer::verbosity == 2)
        {
            MemoryBuffer::getCurrentBufferStatistics();
            DiskMetaFile::printAllEntries(0); // To print all levels entries
        }

        if (MemoryBuffer::verbosity == 1)
        {
            std::cout << ":::: Buffer full :: Flushing buffer to Level 1 " << std::endl;
            MemoryBuffer::printBufferEntries();
        }

        int status = MemoryBuffer::initiateBufferFlush(1);
        if (status)
        {
            if (MemoryBuffer::verbosity == 2)
                std::cout << "Buffer flushed :: Resizing buffer ( size = " << MemoryBuffer::buffer->entries.size() << " ) ";

            MemoryBuffer::buffer->entries.clear();
            if (MemoryBuffer::verbosity == 2)
                std::cout << ":::: Buffer resized ( size = " << MemoryBuffer::buffer->entries.size() << " ) " << std::endl;

            MemoryBuffer::current_buffer_entry_count = 0;
            MemoryBuffer::current_buffer_saturation = 0;
            MemoryBuffer::current_buffer_size = 0;
        }
    }
}

bool VectorMemTable::insert(Entry &entry)
{
    if (entry.getType() != EntryType::POINT_ENTRY)
    {
        std::cout << "ERROR: Entry should be Point Entry" << std::endl;
        exit(1);
    }

    std::vector<Entry *>::iterator it;
    it = std::find_if(entries.begin(), entries.end(), [&](const Entry *stored_entry)
                      { return stored_entry->getKey().compare(entry.getKey()) == 0; });

    // Check if entry already exists & UPDATE
    if (it != entries.end())
    {
        MemoryBuffer::setCurrentBufferStatistics(0, (entry.getValue().size() - entries[it - entries.begin()]->getValue().size()));
        Entry *new_entry = new Entry(entry);
        auto now = std::chrono::system_clock::now();
        new_entry->setTimeTag(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        entries[it - entries.begin()] = new_entry;
        // std::cout << "Value updated : " << entry.getKey() << std::endl;
        WorkloadExecutor::total_insert_count++;
        WorkloadExecutor::buffer_insert_count++;
    }
    else
    {
        size_t entry_key_size = sizeof static_cast<uint32_t>(std::stoi(entry.getKey()));
        MemoryBuffer::setCurrentBufferStatistics(1, (entry_key_size + entry.getValue().size()));
        Entry *new_entry = new Entry{entry};
        auto now = std::chrono::system_clock::now();
        new_entry->setTimeTag(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        entries.push_back(new_entry);
        // std::cout << "Key inserted : " << entry.getKey() << std::endl;
        WorkloadExecutor::total_insert_count++;
        WorkloadExecutor::buffer_insert_count++;
    }
    WorkloadExecutor::counter++;

    checkMemTableFull();

    return 1;
}

bool VectorMemTable::remove(Entry &entry)
{
    if (entry.getType() != EntryType::POINT_TOMBSTONE)
    {
        std::cout << "ERROR: Entry should be Point Tombstone" << std::endl;
        exit(1);
    }

    std::vector<Entry *>::iterator it;
    it = std::find_if(entries.begin(), entries.end(), [&](const Entry *stored_entry)
                      { return stored_entry->getKey().compare(entry.getKey()) == 0; });
    size_t entry_key_size = sizeof static_cast<uint32_t>(std::stoi(entry.getKey()));

    // Check if entry already exists in MemTable
    if (it != entries.end())
    {
        MemoryBuffer::setCurrentBufferStatistics(-1, (-(entries[it - entries.begin()]->getValue().size() + entry_key_size)));
        entries.erase(it);
        // std::cout << "Key removed from buffer : " << entry.getKey() << std::endl;
        WorkloadExecutor::total_insert_count--;
        WorkloadExecutor::buffer_insert_count--;
    }
    Entry *new_entry = new Entry{entry};
    MemoryBuffer::setCurrentBufferStatistics(1, entry_key_size);
    auto now = std::chrono::system_clock::now();
    new_entry->setTimeTag(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
    entries.push_back(new_entry);
    // std::cout << "Point Tombstone Added for Key: " << entry.getKey() << std::endl;
    WorkloadExecutor::total_insert_count++;
    WorkloadExecutor::buffer_insert_count++;
    WorkloadExecutor::counter++;

    checkMemTableFull();
    return 1;
}

std::pair<int, std::string> VectorMemTable::get(Entry &entry)
{
    if (entry.getType() != EntryType::NO_TYPE)
    {
        std::cout << "ERROR: Entry should be of No Type" << std::endl;
        exit(1);
    }

    auto it = std::find_if(entries.rbegin(), entries.rend(), [&](const Entry *stored_entry)
                           { return stored_entry->getKey().compare(entry.getKey()) == 0; });

    if (it != entries.rend())
    {
        auto index = std::distance(it, entries.rend()) - 1;
        Entry *stored_entry = *it;
        if (stored_entry->getType() == EntryType::POINT_TOMBSTONE)
        {
            return std::make_pair(index, "Key : " + stored_entry->getKey() + " NOT FOUND!");
        }
        return std::make_pair(index, entries[index]->getValue());
    }
    return std::make_pair(-1, "Key : " + entry.getKey() + " NOT FOUND!");
}


Entry& Entry::operator=(Entry&& other) noexcept
{
    if (this != &other)
    {
        entry_type = std::move(other.entry_type);
        key = std::move(other.key);
        value = std::move(other.value);
        timetag = std::move(other.timetag);

        // Set the moved-from object to a valid but unspecified state
        other.entry_type = EntryType::NO_TYPE;
        other.key = "";
        other.value = "";
        other.timetag = 0;
    }
    return *this;
}

