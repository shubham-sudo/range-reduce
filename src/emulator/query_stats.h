#ifndef _QUERY_STATS_H_
#define _QUERY_STATS_H_

#include <vector>
#include <chrono>
#include <ostream>
#include <iostream>

namespace emulator
{
    enum QueryType
    {
        POINT,
        INSERT,
        UPDATE,
        DELETE,
        RANGE,
        INVALID
    };

    class QueryStats
    {
        friend std::ostream &operator<<(std::ostream &os, const QueryStats qstats);

    private:
        static QueryStats *_instance;

        QueryStats() = default;

    public:
        long long query_number;
        QueryType query_type;
        std::string key;

        long long total_num_compaction;
        long long total_num_files_written_while_compaction;
        long long total_num_pages_written_while_compaction;
        long long total_entries_written_while_compaction;

        long long num_trivial_file_moves;

        long long num_buffer_flush;
        long long num_page_flushed_from_buffer;
        long long num_trivial_file_move_from_buffer;
        long long total_entries_written_buffer_flush;
        
        long long num_files_written_during_range_query;
        long long num_pages_written_during_range_query;
        long long num_entries_written_during_range_query;

        long long total_vanilla_compaction;
        long long total_rqd_compaction;

        long long entries_count_before_query;
        long long entries_count_after_query;
        double total_time_taken;

        static QueryStats *getQueryStats();

        void reset();
        void print();
    };
}

#endif // _QUERY_STATS_H_