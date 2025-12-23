#include "entry.h"

Entry::Entry(std::string key, std::string value) : key{key}, value{value}, entry_type{EntryType::POINT_ENTRY} {}

Entry::Entry(std::string key, std::string value, EntryType entry_type) : key{key}, value{value}, entry_type{entry_type} {}

Entry::Entry(const Entry &other) : entry_type(other.entry_type),
                                   key(other.key),
                                   value(other.value),
                                   timetag(other.timetag) {}

bool Entry::operator==(const Entry &other)
{
    if (this == &other)
    {
        return true;
    }
    else if (this->key == other.key && this->entry_type == other.entry_type && this->value == other.value)
    {
        return true;
    }
    return false;
}

Entry::Entry(Entry &&other) noexcept
    : entry_type(std::move(other.entry_type)),
      key(std::move(other.key)),
      value(std::move(other.value)),
      timetag(std::move(other.timetag))
{
    other.entry_type = EntryType::NO_TYPE;
    other.key = "";
    other.value = "";
    other.timetag = 0;
}
