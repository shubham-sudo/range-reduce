#include <string>
#include <chrono>

#ifndef _ENTRY_H_
#define _ENTRY_H_

/*
 * Enum for PointEntry and PointTombston
 */
enum EntryType
{
    NO_TYPE,
    POINT_ENTRY,
    POINT_TOMBSTONE
};

/*
 * Entry could be point entry, point delete(tombstone)
 * Each entry will have:
 *  - enum type [PointEntry, PointTombston]
 *  - string key
 *  - string value
 *  - long timetag
 */
class Entry
{
private:
    EntryType entry_type;
    std::string key;
    std::string value;
    std::chrono::nanoseconds::rep timetag;

public:
    Entry(std::string key, std::string value);
    Entry(std::string key, std::string value, EntryType entry_type); // Constructor
    Entry(const Entry &other);                                       // copy constructor
    Entry(Entry &&other) noexcept;                                      // move constructor

    Entry& operator=(const Entry& other) = default; // Copy assignment operator
    Entry& operator=(Entry&& other) noexcept;       // Move assignment operator

    bool operator==(const Entry &other); // comparison operator
    EntryType getType()
    {
        return this->entry_type;
    }
    std::string getKey() const
    {
        return this->key;
    }
    std::string getValue() const
    {
        return this->value;
    }
    void updateKey(std::string value)
    {
        this->value = value;
    }
    void setTimeTag(std::chrono::nanoseconds::rep time_tag)
    {
        this->timetag = time_tag;
    }
    std::chrono::nanoseconds::rep getTimeTag() const
    {
        return this->timetag;
    }
};

#endif // _ENTRY_H_
